/*
*   poolmanager.c - pre-allocated pool manager
*
*   strategy: allocate all resources at PASSIVE_LEVEL during init.
*   each pool type has a fixed-size array of pointers and a simple
*   lock-free stack (interlocked index) for request/return.
*
*   allocation APIs used:
*     - MmAllocateContiguousMemory: for page-aligned, physically-meaningful
*       structures (EPT_DYNAMIC_SPLIT, EPT_HOOKED_PAGE_DETAIL).
*       guarantees page alignment and physical contiguity.
*       IRQL requirement: <= APC_LEVEL (only called during init).
*
*     - ExAllocatePool2(NON_PAGED | EXECUTE): for trampoline buffers.
*       executable memory required for the copied/relocated instructions.
*
*     - ExAllocatePool2(NON_PAGED): for general data structures
*       (EPT_HOOKED_FUNCTION_ENTRY).
*/
#include "hv.h"

typedef struct _EPT_POOL_BUCKET {
    PVOID * items;          // array of pre-allocated pointers
    UINT32  capacity;       // max items
    volatile LONG  count;   // current available count (stack top index)
    SIZE_T  item_size;      // size of each item
    BOOLEAN contiguous;     // use MmAllocateContiguousMemory
    BOOLEAN executable;     // use POOL_FLAG_NON_PAGED_EXECUTE
} EPT_POOL_BUCKET;

static EPT_POOL_BUCKET g_pool_buckets[EptPoolTypeCount];
static BOOLEAN g_pool_initialized = FALSE;

static BOOLEAN
pool_alloc_bucket(EPT_POOL_BUCKET * bucket, UINT32 capacity, SIZE_T item_size,
                  BOOLEAN contiguous, BOOLEAN executable)
{
    bucket->capacity   = capacity;
    bucket->item_size  = item_size;
    bucket->contiguous = contiguous;
    bucket->executable = executable;
    bucket->count      = 0;

    bucket->items = (PVOID *)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, sizeof(PVOID) * capacity, HV_POOL_TAG);
    if (!bucket->items)
        return FALSE;

    RtlZeroMemory(bucket->items, sizeof(PVOID) * capacity);

    for (UINT32 i = 0; i < capacity; i++)
    {
        PVOID ptr = NULL;

        if (contiguous)
        {
            //
            // MmAllocateContiguousMemory: page-aligned, physically contiguous.
            // used for EPT page table structures whose PFN must be derived
            // from MmGetPhysicalAddress. guaranteed page alignment.
            // must be called at IRQL <= APC_LEVEL.
            //
            PHYSICAL_ADDRESS max_phys;
            max_phys.QuadPart = MAXULONG64;
            ptr = MmAllocateContiguousMemory(item_size, max_phys);
        }
        else if (executable)
        {
            //
            // ExAllocatePool2 with EXECUTE flag: for trampoline code buffers.
            // on HVCI systems, NonPaged pool is NX by default — the EXECUTE
            // flag requests executable pages. for allocations >= PAGE_SIZE,
            // the return is page-aligned.
            //
            ptr = ExAllocatePool2(
                POOL_FLAG_NON_PAGED_EXECUTE,
                item_size, HV_POOL_TAG);
        }
        else
        {
            //
            // ExAllocatePool2 NonPaged: general data structures.
            // for allocations >= PAGE_SIZE, the pool manager returns
            // page-aligned memory. for smaller allocations, 16-byte aligned.
            //
            ptr = ExAllocatePool2(POOL_FLAG_NON_PAGED, item_size, HV_POOL_TAG);
        }

        if (!ptr)
        {
            DbgPrintEx(0, 0, "[hv] pool: failed to allocate item %u/%u (size=0x%llx)\n",
                       i, capacity, (UINT64)item_size);
            return FALSE;
        }

        RtlZeroMemory(ptr, item_size);
        bucket->items[i] = ptr;
        InterlockedIncrement(&bucket->count);
    }

    return TRUE;
}

static VOID
pool_free_bucket(EPT_POOL_BUCKET * bucket)
{
    if (!bucket->items)
        return;

    for (UINT32 i = 0; i < bucket->capacity; i++)
    {
        if (bucket->items[i])
        {
            if (bucket->contiguous)
                MmFreeContiguousMemory(bucket->items[i]);
            else
                ExFreePoolWithTag(bucket->items[i], HV_POOL_TAG);
            bucket->items[i] = NULL;
        }
    }

    ExFreePoolWithTag(bucket->items, HV_POOL_TAG);
    bucket->items = NULL;
    bucket->count = 0;
}

BOOLEAN
ept_pool_init(VOID)
{
    BOOLEAN ok = TRUE;

    //
    // EptPoolSplit: VMM_EPT_DYNAMIC_SPLIT
    //   page-aligned (contains PML1[512] array whose PA is used as PFN)
    //   use MmAllocateContiguousMemory for guaranteed page alignment
    //
    ok = ok && pool_alloc_bucket(&g_pool_buckets[EptPoolSplit],
        EPT_POOL_SPLIT_COUNT, sizeof(VMM_EPT_DYNAMIC_SPLIT), TRUE, FALSE);

    //
    // EptPoolPage: EPT_HOOKED_PAGE_DETAIL
    //   metadata only (fake page owned by consumer), no alignment requirement
    //
    ok = ok && pool_alloc_bucket(&g_pool_buckets[EptPoolPage],
        EPT_POOL_PAGE_COUNT, sizeof(EPT_HOOKED_PAGE_DETAIL), FALSE, FALSE);

    //
    // EptPoolFunc: EPT_HOOKED_FUNCTION_ENTRY
    //   small struct, no alignment requirement, ExAllocatePool2 is fine
    //
    ok = ok && pool_alloc_bucket(&g_pool_buckets[EptPoolFunc],
        EPT_POOL_FUNC_COUNT, sizeof(EPT_HOOKED_FUNCTION_ENTRY), FALSE, FALSE);

    //
    // EptPoolExec: trampoline buffers (executable memory)
    //   ExAllocatePool2 with EXECUTE flag
    //
    ok = ok && pool_alloc_bucket(&g_pool_buckets[EptPoolExec],
        EPT_POOL_EXEC_COUNT, EPT_POOL_EXEC_SIZE, FALSE, TRUE);

    if (ok)
    {
        g_pool_initialized = TRUE;
        DbgPrintEx(0, 0, "[hv] pool: initialized (split=%u page=%u func=%u exec=%u)\n",
                   EPT_POOL_SPLIT_COUNT, EPT_POOL_PAGE_COUNT,
                   EPT_POOL_FUNC_COUNT, EPT_POOL_EXEC_COUNT);
    }
    else
    {
        DbgPrintEx(0, 0, "[hv] pool: initialization FAILED\n");
        ept_pool_destroy();
    }

    return ok;
}

VOID
ept_pool_destroy(VOID)
{
    for (UINT32 i = 0; i < EptPoolTypeCount; i++)
        pool_free_bucket(&g_pool_buckets[i]);

    g_pool_initialized = FALSE;
    DbgPrintEx(0, 0, "[hv] pool: destroyed\n");
}

/*
 *  request an item from the pool.
 *  lock-free: uses InterlockedDecrement to pop from the stack.
 *  safe at any IRQL <= DISPATCH_LEVEL.
 */
PVOID
ept_pool_request(EPT_POOL_TYPE type)
{
    EPT_POOL_BUCKET * bucket = &g_pool_buckets[type];
    LONG idx = InterlockedDecrement(&bucket->count);

    if (idx < 0)
    {
        //
        // pool exhausted — restore count and return NULL
        //
        InterlockedIncrement(&bucket->count);
        return NULL;
    }

    PVOID ptr = bucket->items[idx];
    bucket->items[idx] = NULL;

    RtlZeroMemory(ptr, bucket->item_size);
    return ptr;
}

/*
 *  return an item to the pool.
 *  lock-free: uses InterlockedIncrement to push back.
 */
VOID
ept_pool_return(EPT_POOL_TYPE type, PVOID ptr)
{
    if (!ptr)
        return;

    EPT_POOL_BUCKET * bucket = &g_pool_buckets[type];
    LONG idx = InterlockedIncrement(&bucket->count) - 1;

    if (idx < (LONG)bucket->capacity)
    {
        bucket->items[idx] = ptr;
    }
}

UINT32
ept_pool_available(EPT_POOL_TYPE type)
{
    LONG val = InterlockedCompareExchange(&g_pool_buckets[type].count, 0, 0);
    return (val > 0) ? (UINT32)val : 0;
}
