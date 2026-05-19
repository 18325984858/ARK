// HelloSys/HelloSys.c
// 最小 KMDF 演示驱动：DriverEntry 打印 Hello World，DriverUnload 也打印一行。
// 用 sc create / sc start 加载即可在 WinDbg / DebugView 看到输出。
#include <ntddk.h>

DRIVER_UNLOAD HelloSysUnload;

VOID HelloSysUnload(_In_ PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "[HelloSys] Goodbye World\n");
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "[HelloSys] Hello World\n");
    DriverObject->DriverUnload = HelloSysUnload;
    return STATUS_SUCCESS;
}
