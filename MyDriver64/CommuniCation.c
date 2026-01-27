#include "Head.h"
#include "CommunCation.h"
#include "Interface.h"
#include "Struct.h"
#include "Hook/EtwHook.h"
#include "Filter.h"
#include "Ssdt.h"

PFLT_PORT g_AppPort = { 0 };											//存储通信的三环链接端口
PFLT_FILTER g_pFilter = { 0 };											//存储注册的文件过滤器指针
PFLT_PORT g_pServerPort = { 0 };										//存储通信的0环端口
CKerneMessageList g_KernelMessageList = { 0 };							//存储消息链表


CONST FLT_OPERATION_REGISTRATION MyCallbacks[MAX_FLT_NUMBER] = {

	/*{ IRP_MJ_CREATE,
	  0,
	  NULL,
	  MyFltCreatePostOperationCallBack},*/


	{ IRP_MJ_OPERATION_END } };

//注册文件过滤器时回调结构
CONST FLT_REGISTRATION FilterRegistration = {

	sizeof(FLT_REGISTRATION),						//  Size
	FLT_REGISTRATION_VERSION,						//  Version
	0,												//  Flags //FLTFL_REGISTRATION_DO_NOT_SUPPORT_SERVICE_STOP不允许强制卸载

	NULL,											//  Context
	MyCallbacks,									//  Operation callbacks
	FsFilterUnload,									//  卸载回调,替换成驱动回调

	FsFilterInstanceSetup,							//  InstanceSetup
	FsFilterInstanceQueryTeardown,					//  InstanceQueryTeardown
	FsFilterInstanceTeardownStart,					//  InstanceTeardownStart
	FsFilterInstanceTeardownComplete,				//  InstanceTeardownComplete

	NULL,											//  GenerateFileName
	NULL,											//  GenerateDestinationFileName
	NULL											//  NormalizeNameComponent
};


/*初始化驱动所需的注册表操作
 *参数一:文件路径
 *参数二:未使用
 */
ULONG64 InitRegistryInfo(_In_ PDRIVER_OBJECT pDriverObject, _In_ PUNICODE_STRING pRegistryPath)
{
	if (!MmIsAddressValid(pDriverObject) || !MmIsAddressValid(pRegistryPath))
	{
		return FALSE;
	}

	/*************************************************************************************************************************************************/
	ULONG32 dwDebugFlags = 0;
	ULONG32 dwTag = 0xb;
	ULONG32 dwErrorControl = 1;
	ULONG32 dwSupportedFeatures = 3;
	UNICODE_STRING pFltMgr = { 0 };
	RtlInitUnicodeString(&pFltMgr, L"FltMgr");

	UNICODE_STRING pGroup = { 0 };
	RtlInitUnicodeString(&pGroup, L"FSFilter Activity Monitor");

	WCHAR szDescription[256] = { 0 };

	WCHAR szImagePath[256] = { 0 };

	//
	//获取驱动名称
	//
	PWCHAR pName = pRegistryPath->Buffer;
	ULONG64 nNameIndex = 0;
	ULONG64 nNameLen = 0;
	for (ULONG64 i = 0; i < pRegistryPath->Length; i++)
	{
		if (pName[i] == L'\\')
		{
			nNameIndex = i + 1; //记录当前字符'\'的位置
			nNameLen = 0;		//记录'\'长度到下一个'\'的长度
		}
		nNameLen++;
	}

	RtlStringCbPrintfW(szImagePath, sizeof(szImagePath[0]) * 256, L"system32\\DRIVERS\\%ws.sys", &pName[nNameIndex]);
	RtlStringCbPrintfW(szDescription, sizeof(szDescription[0]) * 256, L"%ws Mini-Filter Driver", &pName[nNameIndex]);

	//RtlInitUnicodeString(&pImagePath, L"system32\DRIVERS\");
		//写入路径为 
		//
		//-|计算机
		//	-|HKEY_LOCAL_MACHINE
		//		-|SYSTEM
		//			-|CurrentControlSet
		//				-|Services 
		//					-|MyFristFsFilter
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, pRegistryPath->Buffer, L"DebugFlags", REG_DWORD, &dwDebugFlags, sizeof(dwDebugFlags));
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, pRegistryPath->Buffer, L"ErrorControl", REG_DWORD, &dwErrorControl, sizeof(dwErrorControl));
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, pRegistryPath->Buffer, L"DependOnService", REG_MULTI_SZ, pFltMgr.Buffer, pFltMgr.Length);
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, pRegistryPath->Buffer, L"Group", REG_SZ, pGroup.Buffer, pGroup.Length);
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, pRegistryPath->Buffer, L"Tag", REG_DWORD, &dwTag, sizeof(dwTag));
	//类型设置 2 卸载无反应
	//RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, pRegistryPath->Buffer, L"Type", REG_DWORD, &dwType, sizeof(dwType));
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, pRegistryPath->Buffer, L"ImagePath", REG_EXPAND_SZ, szImagePath, wcslen(szImagePath) * sizeof(WCHAR));
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, pRegistryPath->Buffer, L"SupportedFeatures", REG_DWORD, &dwSupportedFeatures, sizeof(dwSupportedFeatures));
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, pRegistryPath->Buffer, L"Description", REG_SZ, szDescription, wcslen(szDescription) * sizeof(WCHAR));
	/*************************************************************************************************************************************************/
	WCHAR szNewPath[256] = { 0 };
	WCHAR szInstanceName[256] = { 0 };
	//写入路径为 
	//
	//-|计算机
	//	-|HKEY_LOCAL_MACHINE
	//		-|SYSTEM
	//			-|CurrentControlSet
	//				-|Services 
	//					-|MyFristFsFilter
	//						-|Instances 
	RtlStringCbPrintfW(szInstanceName, sizeof(szInstanceName[0]) * 256, L"%ws Instances", &pName[nNameIndex]);
	RtlStringCbPrintfW(szNewPath, sizeof(szNewPath[0]) * 256, L"%ws\\%ws", pRegistryPath->Buffer, L"Instances");
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, szNewPath, L"DefaultInstance", REG_SZ, szInstanceName, (wcslen(szInstanceName) * 2) + 2);
	/*************************************************************************************************************************************************/
	ULONG32 dwFlags = 0;
	UNICODE_STRING pAltitude = { 0 };
	RtlInitUnicodeString(&pAltitude, L"370030");
	//写入路径为 
	//
	//-|计算机
	//	-|HKEY_LOCAL_MACHINE
	//		-|SYSTEM
	//			-|CurrentControlSet
	//				-|Services 
	//					-|MyFristFsFilter
	//						-|Instances 
	//							-|驱动名 + Instances 
	RtlStringCbPrintfW(szNewPath, sizeof(szNewPath[0]) * 256, L"%ws\\%ws", szNewPath, szInstanceName);
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, szNewPath, L"Altitude", REG_SZ, pAltitude.Buffer, pAltitude.Length);
	RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, szNewPath, L"Flags", REG_DWORD, &dwFlags, sizeof(dwFlags));
	/*************************************************************************************************************************************************/

	return STATUS_SUCCESS;
}

NTSTATUS FsFilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{

	UNREFERENCED_PARAMETER(Flags);

	if (g_pServerPort != NULL)
	{
		FltCloseCommunicationPort(g_pServerPort);
		g_pServerPort = NULL;
	}

	if (g_pFilter != NULL)
	{
		FltUnregisterFilter(g_pFilter);
		g_pFilter = NULL;
	}

	//关闭ETWHook
	EtwStop();

	//卸载重载的内核数据
	UnLoadKernelModule();

	//卸载重载的Win32k数据
	UnLoadWin32k();

	//等待所有Api执行完成才返回
	while (g_ApiCallNumber) {}


	//通知三环
	ULONG PreviousState = NULL;
	NTSTATUS nStatus = ZwSetEvent(g_hExitEvent, &PreviousState);
	if (nStatus == STATUS_INVALID_HANDLE)
	{
		MyDbgPrintfEx("提供的 EventHandle 参数无效。\n");
	}
	else if (nStatus == STATUS_INSUFFICIENT_RESOURCES)
	{
		MyDbgPrintfEx("无法分配此函数所需的资源。\n");
	}
	else if (nStatus == STATUS_ACCESS_DENIED)
	{
		MyDbgPrintfEx("调用方没有修改 EventHandle 参数指定的事件所需的权限\n");
	}
	else
	{
		MyDbgPrintfEx("通知成功!Status:%08X\n", ZwClose(g_hExitEvent));
	}

	return STATUS_SUCCESS;
}

VOID ToUserSendMessgae(PULONG64 Paragma, PCFilterUserGetMessageHeadInfo Pack, PULONG64 Paragma0)
{
	//
	if (!MmIsAddressValid(Pack))
	{
		return;
	}

	PCSSDTHookInfo pSSdtInfo = (PCSSDTHookInfo)Pack;
	WriteBufferToProcessStructEx(&pSSdtInfo->ProcessInfo, PsLookUpProcessByProcessId(pSSdtInfo->PID));

	ULONG64 SendSize = 0;
	NTSTATUS status = FltSendMessage(g_pFilter, &g_AppPort, &Pack->PackType, Pack->PackSize - sizeof(FILTER_MESSAGE_HEADER), NULL, &SendSize, 0);
	if (NT_SUCCESS(status))
	{
		//MyDbgPrintfEx("%s Message sent to user mode successfully. SendSize:%016I64X\n", __FUNCTION__, SendSize);
	}
	else
	{
		MyDbgPrintfEx("%s Failed to send message to user mode, status: %08X SendSize:%016I64X\n", __FUNCTION__, status, SendSize);
	}

	ExFreePool(Pack);

	return;
}

ULONG64 ToUserSendGetStructInfoMessgae(const WCHAR* pModule, const WCHAR* pClassType, const WCHAR* memberName)
{
	if (!pModule || !pClassType || !memberName)
	{
		MyDbgPrintfEx("[%ws] 参数错误!\n", __FUNCTIONW__);
		return;
	}

	CFilterUserGetMessageStructInfo Pack = { 0 };

	Pack.StructInfo.nOffset = 0; // 偏移量初始化为0
	Pack.Header.PackType = um_FilterMessageDataType_GetStructOffset; // 设置包类型
	Pack.Header.PackSize = sizeof(CFilterUserGetMessageStructInfo); // 设置包大小

	wcscpy(Pack.StructInfo.szModuleName, pModule);
	wcscpy(Pack.StructInfo.szClassType, pClassType);
	wcscpy(Pack.StructInfo.szmemberName, memberName);


	UCHAR ReplyBuffer[0x1000] = { 0 };
	ULONG ReplyLength = sizeof(ReplyBuffer);


	//设置超时时间为1秒
	LARGE_INTEGER timeout;
	timeout.QuadPart = -20 * 1000 * 1000; // 2秒超时（单位为100纳秒，负值表示相对时间）


	// 发送消息到过滤器
	NTSTATUS status = FltSendMessage(g_pFilter, &g_AppPort, &Pack.Header.PackType, Pack.Header.PackSize - sizeof(FILTER_MESSAGE_HEADER), ReplyBuffer, &ReplyLength, &timeout);
	if (NT_SUCCESS(status))
	{
		if (ReplyLength >= sizeof(FILTER_REPLY_HEADER))
		{
			PCStructInfo pReply = (PCStructInfo)((ULONG64)ReplyBuffer + (sizeof(CFilterGetMessageHeadInfo) - sizeof(FILTER_REPLY_HEADER)));

			if (MmIsAddressValid(pReply))
				return pReply->nOffset; // 返回偏移量
		}
		else
		{
			MyDbgPrintfEx("[%ws] 返回数据长度错误! ReplyLength:%016I64X\n", __FUNCTIONW__, ReplyLength);
		}
	}
	else if (STATUS_TIMEOUT != status/*超时*/)
	{
		MyDbgPrintfEx("[%ws] 包超时! ReplyLength:%016I64X\n", __FUNCTIONW__, ReplyLength);
	}
	return -1;
}

ULONG64 ToUserSendGetStructSizeMessgae(const WCHAR* pModule, const WCHAR* pClassName)
{
	if (!pClassName || !pModule)
	{
		MyDbgPrintfEx("[%ws] 参数错误!\n", __FUNCTIONW__);
		return;
	}

	CFilterUserGetMessageStructSize Pack = { 0 };

	Pack.StructInfo.nSize = 0;											// Size初始化为0
	Pack.Header.PackType = um_FilterMessageDataType_GetStructSize;		// 设置包类型
	Pack.Header.PackSize = sizeof(CFilterUserGetMessageStructSize);		// 设置包大小

	wcscpy(Pack.StructInfo.szClassName, pClassName);
	wcscpy(Pack.StructInfo.szModuleName, pModule);

	UCHAR ReplyBuffer[0x1000] = { 0 };
	ULONG ReplyLength = sizeof(ReplyBuffer);

	//设置超时时间为1秒
	LARGE_INTEGER timeout;
	timeout.QuadPart = -20 * 1000 * 1000; // 2秒超时（单位为100纳秒，负值表示相对时间）

	// 发送消息到过滤器
	NTSTATUS status = FltSendMessage(g_pFilter, &g_AppPort, &Pack.Header.PackType, Pack.Header.PackSize - sizeof(FILTER_MESSAGE_HEADER), ReplyBuffer, &ReplyLength, &timeout);
	if (NT_SUCCESS(status))
	{
		if (ReplyLength >= sizeof(FILTER_REPLY_HEADER))
		{
			PCStructSize pReply = (PCStructSize)((ULONG64)ReplyBuffer + (sizeof(CFilterGetMessageHeadInfo) - sizeof(FILTER_REPLY_HEADER)));

			if (MmIsAddressValid(pReply))
				return pReply->nSize; // 返回偏移量
		}
		else
		{
			MyDbgPrintfEx("[%ws] 返回数据长度错误! ReplyLength:%016I64X\n", __FUNCTIONW__, ReplyLength);
		}
	}
	else if (STATUS_TIMEOUT != status/*超时*/)
	{
		MyDbgPrintfEx("[%ws] 包超时! ReplyLength:%016I64X\n", __FUNCTIONW__, ReplyLength);
	}
	return -1;
}

ULONG64 ToUserSendGetGlobalVariablesMessgae(const WCHAR* pModule, const WCHAR* VarName)
{
	if (!pModule || !VarName)
	{
		MyDbgPrintfEx("[%ws] 参数错误!\n", __FUNCTIONW__);
		return 0;
	}

	CFilterUserGetMessageGlobalVariables Pack = { 0 };

	Pack.VarInfo.nOffset = 0; // 偏移量初始化为0
	Pack.Header.PackType = um_FilterMessageDataType_GetGlobalVariables; // 设置包类型
	Pack.Header.PackSize = sizeof(CFilterUserGetMessageGlobalVariables); // 设置包大小

	wcscpy(Pack.VarInfo.szModuleName, pModule);
	wcscpy(Pack.VarInfo.szVarName, VarName);


	UCHAR ReplyBuffer[0x1000] = { 0 };
	ULONG ReplyLength = sizeof(ReplyBuffer);


	//设置超时时间为1秒
	LARGE_INTEGER timeout;
	timeout.QuadPart = -20 * 1000 * 1000; // 2秒超时（单位为100纳秒，负值表示相对时间）


	// 发送消息到过滤器
	NTSTATUS status = FltSendMessage(g_pFilter, &g_AppPort, &Pack.Header.PackType, Pack.Header.PackSize - sizeof(FILTER_MESSAGE_HEADER), ReplyBuffer, &ReplyLength, &timeout);
	if (NT_SUCCESS(status))
	{
		if (ReplyLength >= sizeof(FILTER_REPLY_HEADER))
		{
			PCGlobalVariables pReply = (PCGlobalVariables)((ULONG64)ReplyBuffer + (sizeof(CFilterGetMessageHeadInfo) - sizeof(FILTER_REPLY_HEADER)));

			if (MmIsAddressValid(pReply))
				return pReply->nOffset; // 返回偏移量
		}
		else
		{
			MyDbgPrintfEx("[%ws] 返回数据长度错误! ReplyLength:%016I64X\n", __FUNCTIONW__, ReplyLength);
		}
	}
	else if (STATUS_TIMEOUT != status/*超时*/)
	{
		MyDbgPrintfEx("[%ws] 包超时! ReplyLength:%016I64X\n", __FUNCTIONW__, ReplyLength);
	}
	return -1;
}


//	通信连接函数
NTSTATUS MyMiniFltConnectNotify(
	_In_ PFLT_PORT ClientPort,
	_In_opt_ PVOID ServerPortCookie,
	_In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
	_In_ ULONG SizeOfContext,
	_Outptr_result_maybenull_ PVOID* ConnectionPortCookie
)
{
	if (g_AppPort == NULL && ClientPort != NULL)
	{

		/*	{
					//初始化存储消息的锁
					KeInitializeSpinLock(&g_KernelMessageList.Lock);
					//初始化通知对象
					KeInitializeEvent(&g_KernelMessageList.Event, NotificationEvent, FALSE);
					//初始化链表
					//InitDoubleLoopList(&g_KernelMessageList.PackList->List);
				}
		*/
		/*
				{
					HANDLE hThread = NULL;
					//创建一个用于HOOK的发送消息系统线程
					OBJECT_ATTRIBUTES ThreadObjectAttributes = { 0 };
					InitializeObjectAttributes(&ThreadObjectAttributes, NULL, OBJ_KERNEL_HANDLE, 0, NULL);

					//设置线程运行标志
					g_ToUserSendHookSystemServiceTableInfoThreadRunFlags = TRUE;

					NTSTATUS nStatus = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS,
						&ThreadObjectAttributes,
						NULL,
						NULL,
						ToUserSendHookSystemServiceTableInfo,
						&g_KernelMessageList);

					//当创创建线程失败时,
					if (!NT_SUCCESS(nStatus))
					{
						return nStatus;
					}

					//关闭线程句柄
					ZwClose(hThread);
				}
		*/

		g_AppPort = ClientPort;
	}

	return STATUS_SUCCESS;
}

//	通信销毁连接函数
VOID MyMiniFltDisconnectNotify(_In_opt_ PVOID ConnectionCookie)
{
	if (g_AppPort == NULL)
	{
		return STATUS_SUCCESS;
	}

	/*
		{
			//当线程运行的时候通知关闭线程清理资源
			if (g_ToUserSendHookSystemServiceTableInfoThreadRunFlags)
			{
				//发送信号
				KeSetEvent(&g_KernelMessageList.Event, EVENT_INCREMENT, TRUE);
				NTSTATUS status = KeWaitForSingleObject(&g_KernelMessageList.Event, Executive, KernelMode, FALSE, NULL);

				if (status != STATUS_SUCCESS)
				{
					MyDbgPrintfEx("未等到线程结束 status: %08X\n", status);
				}
				MyDbgPrintfEx("线程结束 status: %08X\n", status);
			}
		}
	*/


	//LARGE_INTEGER waitTime = { 0 };

	//关闭3环链接
	FltCloseClientPort(g_pFilter, &g_AppPort);
	MyDbgPrintfEx("g_pFilter 关闭连接\n");

	g_AppPort = NULL;
	return STATUS_SUCCESS;
}

// 通信收消息函数
NTSTATUS MyMiniFltMessageNotify(
	_In_opt_ PVOID PortCookie,
	_In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
	_In_ ULONG InputBufferLength,
	_Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer,
	_In_ ULONG OutputBufferLength,
	_Out_ PULONG ReturnOutputBufferLength
)
{
	//判断是否是有效端口
	PCCommunicationInfo pMsg = InputBuffer;
	//__debugbreak();
	if (MmIsAddressValid(pMsg))
	{
		//验证参数是否准确
		if (MmIsAddressValid(&g_CmdFun[pMsg->m_Cmd]) && MmIsAddressValid(g_CmdFun[pMsg->m_Cmd].m_pfn))
		{
			g_CmdFun[pMsg->m_Cmd].m_pfn(pMsg->m_Cmd, pMsg->m_pIndata, pMsg->m_pOutData, pMsg->m_nRet, pMsg->m_pParam);
		}
	}
	return STATUS_SUCCESS;
}

// 注册通信服务函数
ULONG64 RegisteredCominterface(PUNICODE_STRING pPortName)
{
	PSECURITY_DESCRIPTOR security = { 0 };
	OBJECT_ATTRIBUTES Attributes = { 0 };
	NTSTATUS ntstatus = { 0 };

	// 生成FltCreateCommunicationPort的安全描述符
	ntstatus = FltBuildDefaultSecurityDescriptor(&security, FLT_PORT_ALL_ACCESS);
	if (!NT_SUCCESS(ntstatus))
	{
		return ntstatus;
	}
	// 初始化安全描述符
	InitializeObjectAttributes(&Attributes, pPortName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, security);

	//	创建一个通信服务器端口,注册回调
	ntstatus = FltCreateCommunicationPort(g_pFilter,
		&g_pServerPort,
		&Attributes,
		NULL,
		MyMiniFltConnectNotify,			/*连接回调*/
		MyMiniFltDisconnectNotify,		/*连接销毁回调*/
		MyMiniFltMessageNotify,			/*消息接收回调*/
		1/*最大连接数*/);

	//释放安全描述符
	FltFreeSecurityDescriptor(security);

	if (!NT_SUCCESS(ntstatus))
	{
		FltCloseCommunicationPort(g_pServerPort);
		return ntstatus;
	}
	return ntstatus;
}

NTSTATUS
FsFilterInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);
	UNREFERENCED_PARAMETER(VolumeFilesystemType);

	PAGED_CODE();

	return STATUS_SUCCESS;
}

NTSTATUS
FsFilterInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();


	return STATUS_SUCCESS;
}

VOID
FsFilterInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

}

VOID
FsFilterInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

}