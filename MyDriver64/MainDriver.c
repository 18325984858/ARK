#include "Head.h"
#include "CommunCation.h"
#include "DefineArea.h"
#include "Interface.h"
#include "Hook/EtwHook.h"
#include "Ssdt.h"

ULONG64					g_CurDriverObject = NULL;
HANDLE					g_hExitEvent = NULL;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{

	//__debugbreak();

	NTSTATUS status = STATUS_SUCCESS;

	g_CurDriverObject = (ULONG64)DriverObject;

	/*
	{	//筛选版本
		RTL_OSVERSIONINFOEXW version = { 0 };
		RtlGetVersion(&version);


		//版本Win10 19045 版本
		if (version.dwBuildNumber == 19045)
		{

		}
		//Win11
		else if (version.dwBuildNumber == 26100)
		{

		}
		else
		{
			return -1;
		}

	}
	*/


	{	//创建一个事件对象,当三环进程退出时用于通知
		UNICODE_STRING eventName;
		RtlInitUnicodeString(&eventName, L"\\BaseNamedObjects\\MyExitEvent");

		// 初始化安全描述符
		SECURITY_DESCRIPTOR securityDescriptor;
		RtlCreateSecurityDescriptor(&securityDescriptor, SECURITY_DESCRIPTOR_REVISION);

		// 设置DACL，允许所有用户访问
		ULONG daclSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(ULONG) + RtlLengthSid(SeExports->SeWorldSid);
		PACL pDacl = (PACL)ExAllocatePoolWithTag(PagedPool, daclSize, 'dacl');
		RtlCreateAcl(pDacl, daclSize, ACL_REVISION);
		RtlAddAccessAllowedAce(pDacl, ACL_REVISION, EVENT_ALL_ACCESS, SeExports->SeWorldSid);
		RtlSetDaclSecurityDescriptor(&securityDescriptor, TRUE, pDacl, FALSE);

		OBJECT_ATTRIBUTES objAttr;
		InitializeObjectAttributes(&objAttr, &eventName, OBJ_KERNEL_HANDLE, NULL, &securityDescriptor);

		status = ZwCreateEvent(
			&g_hExitEvent,															// 输出句柄
			EVENT_ALL_ACCESS,														// 访问权限
			&objAttr,																// 对象属性
			NotificationEvent,														// 事件类型
			FALSE																	// 初始状态
		);

		if (MmIsAddressValid(pDacl))
		{
			ExFreePool(pDacl);
		}

		if (!NT_SUCCESS(status)) {
			// 处理错误

			MyDbgPrintfEx("创建退出通知事件失败! Status:%08X\n", status);
			return status;
		}
	}

	//初始化需要用到的全局变量
	status = InitGlobalVariable(g_CurDriverObject);
	if (!NT_SUCCESS(status))
	{
		MyDbgPrintfEx("%ws InitGlobalVariable 函数失败！\n", __FUNCTION__);
		return status;
	}

	if (InitRegistryInfo(DriverObject, RegistryPath) != STATUS_SUCCESS)					// 初始化注册表
	{
		return FALSE;
	}

	status = FltRegisterFilter(DriverObject, &FilterRegistration, &g_pFilter);			//注册文件过滤器

	if (NT_SUCCESS(status))
	{

		{//	创建默认端口名
#define MINIFILTER_PORT_NAME L"\\58DF4FB5-D464-4DF9-B14E-3565FDE02AED"
			UNICODE_STRING PortName = { 0 };
			RtlInitUnicodeString(&PortName, MINIFILTER_PORT_NAME);
			RegisteredCominterface(&PortName);											//注册通信函数
		}

		status = FltStartFiltering(g_pFilter);											//运行文件过滤器

		if (!NT_SUCCESS(status)) {

			FltUnregisterFilter(g_pFilter);
			return status;
		}

		/*
		//开启ETW HOOK
		if (EtwInit((ULONG64)&call_back))
		{

			//初始化Hook
			InitRootSsdtHook();

			EtwStart();
		}
		*/
	}

	return status;
}

