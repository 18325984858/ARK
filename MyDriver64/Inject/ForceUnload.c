// MyDriver64/Inject/ForceUnload.c
// 强制卸载驱动：优先用 ZwUnloadDriver（canonical 路径，会做 DriverUnload + MmUnloadSystemImage），
// 失败 / 没服务名再退回手工 DetachDevice + DriverUnload + ObDereferenceObject。
//
// 没 DriverUnload 例程的驱动：默认拒绝；FU_FLAG_FORCE_NO_UNLOAD 跳过 DriverUnload 调用，
// 几乎必蓝屏，UI 必须二次确认。

#include "../Head.h"
#include "../Define.h"

#define FU_TAG 'FuMm'

// ZwUnloadDriver 在 ntoskrnl 导出但 WDK 头里没声明
NTKERNELAPI NTSTATUS NTAPI ZwUnloadDriver(IN PUNICODE_STRING DriverServiceName);

static BOOLEAN MmFuIsValidDriverObject(PVOID p)
{
    if (!p || !MmIsAddressValid(p)) return FALSE;
    __try
    {
        // DRIVER_OBJECT.Type (_DISPATCHER_HEADER.Type) == 4
        CSHORT t = *(CSHORT*)p;
        return (t == 4);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return FALSE; }
}

VOID __vectorcall ForceUnloadDriverInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
    UNREFERENCED_PARAMETER(nCmd);
    UNREFERENCED_PARAMETER(pOutData);
    UNREFERENCED_PARAMETER(pParam);

    if (!MmIsAddressValid((PVOID)pIndata)) return;
    PCForceUnloadInfo p = (PCForceUnloadInfo)pIndata;

    PDRIVER_OBJECT drv    = NULL;
    ULONG          flags  = 0;
    WCHAR          svc[64] = { 0 };
    BOOLEAN        haveSvc = FALSE;
    __try
    {
        drv   = (PDRIVER_OBJECT)(ULONG_PTR)p->DriverObject;
        flags = p->Flags;
        p->Status         = FU_STATUS_OK;
        p->DeviceCount    = 0;
        p->UnloadRoutine  = 0;
        p->ZwUnloadStatus = (ULONG)STATUS_NOT_FOUND;
        for (ULONG i = 0; i < 63 && p->ServiceName[i]; ++i) svc[i] = p->ServiceName[i];
        haveSvc = (svc[0] != 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return; }

    if (!MmFuIsValidDriverObject(drv))
    {
        __try { p->Status = FU_STATUS_BAD_DRIVER; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        goto out;
    }

    PVOID unload = NULL;
    __try { unload = (PVOID)drv->DriverUnload; } __except (EXCEPTION_EXECUTE_HANDLER) {}

    if (!unload && !(flags & FU_FLAG_FORCE_NO_UNLOAD))
    {
        __try { p->Status = FU_STATUS_NO_UNLOAD; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        goto out;
    }

    NTSTATUS zwSt = STATUS_NOT_FOUND;

    // === 优先尝试 ZwUnloadDriver（最干净，会做完 DriverUnload + MmUnloadSystemImage）===
    if (haveSvc && unload)
    {
        WCHAR fullPath[128];
        const WCHAR prefix[] = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\";
        ULONG pi = 0;
        for (ULONG i = 0; prefix[i] && pi < 127; ++i) fullPath[pi++] = prefix[i];
        for (ULONG i = 0; svc[i] && pi < 127; ++i)    fullPath[pi++] = svc[i];
        fullPath[pi] = 0;

        UNICODE_STRING us; RtlInitUnicodeString(&us, fullPath);
        __try { zwSt = ZwUnloadDriver(&us); }
        __except (EXCEPTION_EXECUTE_HANDLER) { zwSt = STATUS_UNHANDLED_EXCEPTION; }

        __try
        {
            p->ZwUnloadStatus = (ULONG)zwSt;
            if (NT_SUCCESS(zwSt))
            {
                p->Status        = FU_STATUS_OK;
                p->DeviceCount   = 0;
                p->UnloadRoutine = (ULONG64)unload;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        if (NT_SUCCESS(zwSt)) goto out;
    }

    // === 兜底：手工 detach + DriverUnload + ObDereference ===
    ULONG deviceCount = 0;
    __try
    {
        PDEVICE_OBJECT dev = drv->DeviceObject;
        while (dev && MmIsAddressValid(dev))
        {
            PDEVICE_OBJECT next = dev->NextDevice;
            PDEVICE_OBJECT att  = dev->AttachedDevice;
            while (att && MmIsAddressValid(att))
            {
                PDEVICE_OBJECT attNext = att->AttachedDevice;
                IoDetachDevice(dev);
                att = attNext;
            }
            IoDeleteDevice(dev);
            ++deviceCount;
            dev = next;
        }

        if (unload) ((PDRIVER_UNLOAD)unload)(drv);
        ObDereferenceObject(drv);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        __try
        {
            p->Status        = FU_STATUS_EXCEPTION;
            p->DeviceCount   = deviceCount;
            p->UnloadRoutine = (ULONG64)unload;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        goto out;
    }

    __try
    {
        p->Status        = FU_STATUS_OK;
        p->DeviceCount   = deviceCount;
        p->UnloadRoutine = (ULONG64)unload;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

out:
    if (MmIsAddressValid((PVOID)pRet))
    {
        __try { *(PULONG64)pRet = (ULONG64)p->Status; } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}
