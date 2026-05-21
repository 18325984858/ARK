/*
*   hostcr3.c - private host page tables for vmx host mode
*
*   deep-copies kernel portion of system page tables to create an isolated
*   cr3 for vmx host mode. protects the hypervisor from guest-mode page
*   table modifications (eg. anti-cheat drivers that unmap/corrupt kernel ptes)
*
*   strategy:
*     1. discover Windows page table self-referencing PML4 index
*     2. access all page table levels through the self-map virtual addresses
*        (no MmGetVirtualForPhysical — it returns garbage for page table pages
*        on Windows 11 because they lack normal PFN database VA mappings)
*     3. deep-copy kernel-space entries (pml4[256..511])
*     4. leaf entries still point to same physical pages — isolation is
*        at the page table level only
*     5. set VMCS_HOST_CR3 to our private pml4 physical address
*/
#include "hv.h"

#define PTE_PRESENT     (1ULL << 0)
#define PTE_LARGE_PAGE  (1ULL << 7)
#define PTE_PFN_MASK    0x000FFFFFFFFFF000ULL

#define MAX_HOST_PT_PAGES 4096

static PVOID   g_host_pt_pages[MAX_HOST_PT_PAGES];
static UINT32  g_host_pt_count = 0;
static PUINT64 g_host_pml4_va  = NULL;
static UINT64  g_host_pml4_pa  = 0;

//
// page table self-map base addresses (computed from self-referencing PML4 index)
//
// windows maps all active page table pages through a recursive PML4 entry.
// given the self-ref index N, the virtual addresses of each level are:
//
//   PML4 page:   VA = {N, N, N, N, offset}  → g_pxe_base
//   PDPT pages:  VA = {N, N, N, i, offset}  → g_ppe_base + (i << 12)
//   PD pages:    VA = {N, N, i, j, offset}  → g_pde_base + (i << 21) + (j << 12)
//   PT pages:    VA = {N, i, j, k, offset}  → g_pte_base + (i << 30) + (j << 21) + (k << 12)
//
static UINT64  g_pte_base = 0;
static UINT64  g_pde_base = 0;
static UINT64  g_ppe_base = 0;
static UINT64  g_pxe_base = 0;

static PVOID
host_alloc_page(VOID)
{
    PVOID page = ExAllocatePool2(POOL_FLAG_NON_PAGED, PAGE_SIZE, HV_POOL_TAG);
    if (!page)
        return NULL;

    RtlZeroMemory(page, PAGE_SIZE);

    if (g_host_pt_count < MAX_HOST_PT_PAGES)
        g_host_pt_pages[g_host_pt_count++] = page;

    return page;
}

//
// sign-extend a 48-bit virtual address to 64 bits (canonical form)
//
static __forceinline UINT64
sign_extend_48(UINT64 va)
{
    if (va & (1ULL << 47))
        return va | 0xFFFF000000000000ULL;
    return va;
}

//
// discover the self-referencing PML4 index by scanning candidates.
// the self-ref entry is the PML4 entry whose PFN == CR3's PFN.
// we try each possible index (256..511), compute where the PML4 page
// would appear in the self-map, and verify the PFN matches.
//
static BOOLEAN
host_discover_self_map(VOID)
{
    UINT64 cr3      = __readcr3();
    UINT64 pml4_pfn = (cr3 & PTE_PFN_MASK) >> 12;

    for (UINT32 sri = 256; sri < 512; sri++)
    {
        //
        // compute candidate PXE_BASE: where PML4 appears if sri is self-ref
        // PML4 page VA = {sri, sri, sri, sri, 0}
        //
        UINT64 candidate = sign_extend_48(
            ((UINT64)sri << 39) | ((UINT64)sri << 30) |
            ((UINT64)sri << 21) | ((UINT64)sri << 12));

        PUINT64 test_pml4 = (PUINT64)candidate;

        //
        // verify: PML4[sri] should be present and point back to PML4 itself
        //
        if (!MmIsAddressValid(&test_pml4[sri]))
            continue;

        if (!(test_pml4[sri] & PTE_PRESENT))
            continue;

        if (((test_pml4[sri] & PTE_PFN_MASK) >> 12) != pml4_pfn)
            continue;

        //
        // found it — compute all self-map base addresses
        //
        g_pxe_base = candidate;
        g_ppe_base = sign_extend_48(((UINT64)sri << 39) | ((UINT64)sri << 30) | ((UINT64)sri << 21));
        g_pde_base = sign_extend_48(((UINT64)sri << 39) | ((UINT64)sri << 30));
        g_pte_base = sign_extend_48(((UINT64)sri << 39));

        DbgPrintEx(0, 0, "[hv] hostcr3: self-ref PML4 index=%u PTE_BASE=0x%llx PXE_BASE=0x%llx\n",
                   sri, g_pte_base, g_pxe_base);
        return TRUE;
    }

    DbgPrintEx(0, 0, "[hv] hostcr3: failed to find self-referencing PML4 entry\n");
    return FALSE;
}

//
// self-map accessors — return VA of each page table level page
// these are always valid for present entries because the self-map
// follows the same page table hierarchy as normal translation
//

static __forceinline PUINT64
selfmap_pml4(VOID)
{
    return (PUINT64)g_pxe_base;
}

static __forceinline PUINT64
selfmap_pdpt(UINT32 pml4_idx)
{
    return (PUINT64)(g_ppe_base + ((UINT64)pml4_idx << 12));
}

static __forceinline PUINT64
selfmap_pd(UINT32 pml4_idx, UINT32 pdpt_idx)
{
    return (PUINT64)(g_pde_base + ((UINT64)pml4_idx << 21) + ((UINT64)pdpt_idx << 12));
}

static __forceinline PUINT64
selfmap_pt(UINT32 pml4_idx, UINT32 pdpt_idx, UINT32 pd_idx)
{
    return (PUINT64)(g_pte_base + ((UINT64)pml4_idx << 30) +
                     ((UINT64)pdpt_idx << 21) + ((UINT64)pd_idx << 12));
}

//
// deep-copy a PT (level 1) — all 512 leaf entries copied as-is
//
static PUINT64
host_clone_pt(UINT32 pml4_idx, UINT32 pdpt_idx, UINT32 pd_idx)
{
    PUINT64 orig_pt = selfmap_pt(pml4_idx, pdpt_idx, pd_idx);
    PUINT64 our_pt  = (PUINT64)host_alloc_page();
    if (!our_pt)
        return NULL;

    RtlCopyMemory(our_pt, orig_pt, PAGE_SIZE);
    return our_pt;
}

//
// deep-copy a PD (level 2) — large pages copied as-is, PT pointers deep-copied
//
static PUINT64
host_clone_pd(UINT32 pml4_idx, UINT32 pdpt_idx)
{
    PUINT64 orig_pd = selfmap_pd(pml4_idx, pdpt_idx);
    PUINT64 our_pd  = (PUINT64)host_alloc_page();
    if (!our_pd)
        return NULL;

    for (UINT32 k = 0; k < 512; k++)
    {
        if (!(orig_pd[k] & PTE_PRESENT))
        {
            our_pd[k] = 0;
            continue;
        }

        if (orig_pd[k] & PTE_LARGE_PAGE)
        {
            our_pd[k] = orig_pd[k];
            continue;
        }

        PUINT64 our_pt = host_clone_pt(pml4_idx, pdpt_idx, k);
        if (!our_pt)
        {
            our_pd[k] = orig_pd[k];
            continue;
        }

        our_pd[k] = (orig_pd[k] & ~PTE_PFN_MASK) | va_to_pa(our_pt);
    }

    return our_pd;
}

//
// deep-copy a PDPT (level 3) — 1GB large pages copied as-is, PD pointers deep-copied
//
static PUINT64
host_clone_pdpt(UINT32 pml4_idx)
{
    PUINT64 orig_pdpt = selfmap_pdpt(pml4_idx);
    PUINT64 our_pdpt  = (PUINT64)host_alloc_page();
    if (!our_pdpt)
        return NULL;

    for (UINT32 j = 0; j < 512; j++)
    {
        if (!(orig_pdpt[j] & PTE_PRESENT))
        {
            our_pdpt[j] = 0;
            continue;
        }

        if (orig_pdpt[j] & PTE_LARGE_PAGE)
        {
            our_pdpt[j] = orig_pdpt[j];
            continue;
        }

        PUINT64 our_pd = host_clone_pd(pml4_idx, j);
        if (!our_pd)
        {
            our_pdpt[j] = orig_pdpt[j];
            continue;
        }

        our_pdpt[j] = (orig_pdpt[j] & ~PTE_PFN_MASK) | va_to_pa(our_pd);
    }

    return our_pdpt;
}

/*
*   build private host page tables by deep-copying kernel pml4 entries.
*   uses the page table self-map to access all levels — no MmGetVirtualForPhysical.
*   must be called after all host-mode allocations (vmm stacks, bitmaps, etc).
*/
BOOLEAN
hostcr3_build(VOID)
{
    if (!host_discover_self_map())
    {
        DbgPrintEx(0, 0, "[hv] hostcr3: self-map discovery failed\n");
        return FALSE;
    }

    UINT64  sys_cr3  = get_system_cr3();
    UINT64  pml4_pa  = sys_cr3 & PTE_PFN_MASK;
    PUINT64 orig_pml4 = selfmap_pml4();

    PUINT64 our_pml4 = (PUINT64)host_alloc_page();
    if (!our_pml4)
        return FALSE;

    //
    // zero user-space entries (never run user code in host mode)
    //
    for (UINT32 i = 0; i < 256; i++)
        our_pml4[i] = 0;

    for (UINT32 i = 256; i < 512; i++)
    {
        if (!(orig_pml4[i] & PTE_PRESENT))
        {
            our_pml4[i] = 0;
            continue;
        }

        //
        // skip self-referencing entry — fix it up after building our pml4
        //
        if ((orig_pml4[i] & PTE_PFN_MASK) == pml4_pa)
        {
            our_pml4[i] = orig_pml4[i];
            continue;
        }

        PUINT64 our_pdpt = host_clone_pdpt(i);
        if (!our_pdpt)
        {
            our_pml4[i] = orig_pml4[i];
            continue;
        }

        our_pml4[i] = (orig_pml4[i] & ~PTE_PFN_MASK) | va_to_pa(our_pdpt);
    }

    g_host_pml4_va = our_pml4;
    g_host_pml4_pa = va_to_pa(our_pml4);

    //
    // fix self-referencing pml4 entry to point to our pml4
    //
    for (UINT32 i = 256; i < 512; i++)
    {
        if ((our_pml4[i] & PTE_PRESENT) &&
            ((our_pml4[i] & PTE_PFN_MASK) == pml4_pa))
        {
            our_pml4[i] = (our_pml4[i] & ~PTE_PFN_MASK) | g_host_pml4_pa;
            break;
        }
    }

    DbgPrintEx(0, 0, "[hv] Private host CR3 built: PA=0x%llx (%u pages allocated)\n",
               g_host_pml4_pa, g_host_pt_count);

    return TRUE;
}

UINT64
hostcr3_get(VOID)
{
    return g_host_pml4_pa;
}

VOID
hostcr3_destroy(VOID)
{
    for (UINT32 i = 0; i < g_host_pt_count; i++)
    {
        if (g_host_pt_pages[i])
            ExFreePoolWithTag(g_host_pt_pages[i], HV_POOL_TAG);
    }

    g_host_pt_count = 0;
    g_host_pml4_va  = NULL;
    g_host_pml4_pa  = 0;
}
