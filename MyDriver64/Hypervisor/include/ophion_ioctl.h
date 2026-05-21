/*
*   ophion_ioctl.h - shared IOCTL definitions for Ophion hypervisor
*   include this in consumer drivers to communicate with Ophion.sys
*/
#pragma once

#define OPHION_DEVICE_NAME  L"\\Device\\Ophion"
#define OPHION_SYMLINK_NAME L"\\DosDevices\\Ophion"

#define IOCTL_BASE              0x800
#define IOCTL_HV_STATUS         CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_EPT_HOOK       CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_EPT_UNHOOK     CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 2, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _EPT_HOOK_REQUEST {
    unsigned __int64 target_address;      // VA of kernel function to hook
    unsigned __int64 handler_address;     // VA of proxy/handler function
    unsigned __int64 origin_out_address;  // VA of (PVOID*) to receive trampoline (0 = don't need)
} EPT_HOOK_REQUEST, *PEPT_HOOK_REQUEST;
