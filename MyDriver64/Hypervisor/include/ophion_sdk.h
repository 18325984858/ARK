/*
*   ophion_sdk.h - Ophion hypervisor SDK for consumer drivers
*
*   consumer drivers include this header to communicate with the Ophion
*   hypervisor via VMCALL. no IOCTL, no device objects.
*
*   requirements:
*     - caller must be Ring 0 (kernel driver)
*     - Ophion hypervisor must be loaded and active
*     - consumer compiles its own copy of AsmVmxOperation.asm for asm_vmx_vmcall
*/
#pragma once

#include <ntddk.h>

/* ================================================================
 *  VMCALL numbers (shared between Ophion and consumers)
 * ================================================================ */

#define VMCALL_TEST             0x00000001
#define VMCALL_VMXOFF           0x00000002
#define VMCALL_EPT_HOOK_APPLY   0x00000003  // apply a prepared hook
#define VMCALL_EPT_HOOK_REMOVE  0x00000004  // remove a single hook by target VA
#define VMCALL_EPT_HOOK_CLEAR   0x00000005  // remove all hooks

/* ================================================================
 *  Hook descriptor — filled by consumer, passed to Ophion via VMCALL
 *
 *  Consumer does all preparation at PASSIVE_LEVEL:
 *    1. MmGetPhysicalAddress(target) → phys_base
 *    2. Allocate fake page, copy original, write VMCALL (0F 01 C1)
 *    3. MmGetPhysicalAddress(fake_page) → fake_page_pfn
 *    4. Build trampoline with Zydis
 *    5. Fill this struct and call ophion_vmcall(VMCALL_EPT_HOOK_APPLY, &desc)
 *
 *  Ophion (VMX-root) only does:
 *    - Split 2MB→4KB if needed (from pre-allocated pool)
 *    - Set PML1 = RW-noX (original page) / X-noRW (fake page)
 *    - Record hook entry for EPT violation / VMCALL RIP matching
 *    - INVEPT
 * ================================================================ */

typedef struct _OPHION_HOOK_DESCRIPTOR {
    UINT64 target_va;           // VA of function to hook (for RIP matching)
    UINT64 handler_va;          // VA of handler function (RIP redirect target)
    UINT64 phys_base;           // physical address of target page (4KB aligned)
    UINT64 fake_page_pa;        // physical address of consumer's fake page
} OPHION_HOOK_DESCRIPTOR, *POPHION_HOOK_DESCRIPTOR;

/* ================================================================
 *  VMCALL wrapper — consumer must link AsmVmxOperation.obj
 *
 *  asm_vmx_vmcall sets R10/R11/R12 = signature, then VMCALL.
 *  RCX = vmcall_number, RDX = param1 (descriptor pointer), R8/R9 = 0
 * ================================================================ */

//
// defined in AsmVmxOperation.asm (consumer compiles their own copy)
//
extern NTSTATUS asm_vmx_vmcall(UINT64 vmcall_num, UINT64 param1, UINT64 param2, UINT64 param3);

//
// convenience wrappers
//
static __forceinline NTSTATUS
ophion_hook_apply(POPHION_HOOK_DESCRIPTOR desc)
{
    return asm_vmx_vmcall(VMCALL_EPT_HOOK_APPLY, (UINT64)desc, 0, 0);
}

static __forceinline NTSTATUS
ophion_hook_remove(UINT64 target_va)
{
    return asm_vmx_vmcall(VMCALL_EPT_HOOK_REMOVE, target_va, 0, 0);
}

static __forceinline NTSTATUS
ophion_hook_clear(void)
{
    return asm_vmx_vmcall(VMCALL_EPT_HOOK_CLEAR, 0, 0, 0);
}

static __forceinline NTSTATUS
ophion_test(void)
{
    return asm_vmx_vmcall(VMCALL_TEST, 0, 0, 0);
}
