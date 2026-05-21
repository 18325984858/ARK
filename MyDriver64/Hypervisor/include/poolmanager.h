/*
*   poolmanager.h - pre-allocated pool manager for EPT hook resources
*
*   all memory needed by EPT hooks is pre-allocated at PASSIVE_LEVEL
*   during ept_pool_init(). hook installation just picks from the pool,
*   zero allocation in hot paths or VMX-root.
*
*   pool types:
*     - SPLIT:  VMM_EPT_DYNAMIC_SPLIT (page-aligned, for 2MB→4KB split)
*     - PAGE:   EPT_HOOKED_PAGE_DETAIL (page-aligned, contains fake page)
*     - FUNC:   EPT_HOOKED_FUNCTION_ENTRY (general NonPaged)
*     - EXEC:   executable trampoline buffers (NonPaged + Execute)
*/
#pragma once

#include <ntddk.h>

//
// pool capacity — max number of pre-allocated items per type
//
#define EPT_POOL_SPLIT_COUNT     64     // max 2MB pages that can be split
#define EPT_POOL_PAGE_COUNT      32     // max hooked 4KB pages
#define EPT_POOL_FUNC_COUNT      64     // max hooked functions
#define EPT_POOL_EXEC_COUNT      64     // max trampoline buffers
#define EPT_POOL_EXEC_SIZE       128    // bytes per trampoline buffer

typedef enum _EPT_POOL_TYPE {
    EptPoolSplit,       // VMM_EPT_DYNAMIC_SPLIT (page-aligned)
    EptPoolPage,        // EPT_HOOKED_PAGE_DETAIL (page-aligned, huge)
    EptPoolFunc,        // EPT_HOOKED_FUNCTION_ENTRY
    EptPoolExec,        // executable trampoline buffer
    EptPoolTypeCount
} EPT_POOL_TYPE;

//
// init/destroy (call at PASSIVE_LEVEL)
//
BOOLEAN ept_pool_init(VOID);
VOID    ept_pool_destroy(VOID);

//
// request/return (safe at any IRQL <= DISPATCH, including DPC broadcast)
//
PVOID   ept_pool_request(EPT_POOL_TYPE type);
VOID    ept_pool_return(EPT_POOL_TYPE type, PVOID ptr);

//
// stats
//
UINT32  ept_pool_available(EPT_POOL_TYPE type);
