#include "Ssdt.h"
#include "Hook/EtwHook.h"
#include "Head.h"
#include "CommunCation.h"
#include "FunctionPtr.h"



typedef enum _MY_KAPC_ENVIRONMENT
{
	OriginalApcEnvironment,
	AttachedApcEnvironment,
	CurrentApcEnvironment,
	InsertApcEnvironment
} MY_KAPC_ENVIRONMENT;

ULONG64 MySSDTTable[SSDT_MAX_NUMBER] = { MyNtAccessCheck ,
										MyNtWorkerFactoryWorkerReady,
										MyNtAcceptConnectPort ,
										MyNtMapUserPhysicalPagesScatter ,
										MyNtWaitForSingleObject ,
										MyNtCallbackReturn ,
										MyNtReadFile ,
										MyNtDeviceIoControlFile,
										MyNtWriteFile,
};


PCEtwHook g_MySSDTTableHookInfo[SSDT_MAX_NUMBER] = { 0 };

ULONG64 g_ApiCallNumber = 0;

UCHAR InitRootSsdtHook()
{
	if (!MmIsAddressValid(KeServiceDescriptorTable))
	{
		return FALSE;
	}

	ULONG64 dqCount = ((PKSYSTEM_SERVICE_TABLE)KeServiceDescriptorTable)->NumberOfService;			//获取SSDT表数量
	PULONG64 TableBaseAddr = ((PKSYSTEM_SERVICE_TABLE)KeServiceDescriptorTable)->ServiceTableBase;	//获取SSDT函数基地址
	if (!MmIsAddressValid(TableBaseAddr))
	{
		return FALSE;
	}

	for (int i = 0; i < dqCount; i++)
	{
		if (MySSDTTable[i] == NULL)
		{
			break;
		}
		g_MySSDTTableHookInfo[i] = HookAddr(KeGroundIndexGetSsdtTableFunAddr(TableBaseAddr, i), MySSDTTable[i], BUILDETWHOOKLEVEL(0, i));
	}
	return TRUE;
}

//判断是否是要HOOK的函数
VOID __fastcall call_back(ULONG32 ssdt_index, PULONG64* ssdt_address)
{
	UNREFERENCED_PARAMETER(ssdt_index);

	if (g_IsOpenEtw)
	{
		switch (ssdt_index)
		{
		case um_SSDT_Monitor_NtAccessCheck:
		case um_SSDT_Monitor_NtMapUserPhysicalPagesScatter:
		case um_SSDT_Monitor_NtWorkerFactoryWorkerReady:
		case um_SSDT_Monitor_NtAcceptConnectPort:
		case um_SSDT_Monitor_NtWaitForSingleObject:
		case um_SSDT_Monitor_NtCallbackReturn:
		case um_SSDT_Monitor_NtReadFile:
		//case um_SSDT_Monitor_NtDeviceIoControlFile:
		case um_SSDT_Monitor_NtWriteFile:
			*ssdt_address = g_MySSDTTableHookInfo[ssdt_index]->CallAddr;
			InterlockedIncrement(&g_ApiCallNumber);
			break;

		}
	}
}


VOID SetPragma(PCPragmaType pPragma, ULONG64 Data, ULONG64 Type)
{
	pPragma->pPragma = Data;
	pPragma->Type = Type;
}

NTSTATUS
MyNtAccessCheck(
	__in PSECURITY_DESCRIPTOR SecurityDescriptor,
	__in HANDLE ClientToken,
	__in ACCESS_MASK DesiredAccess,
	__in PGENERIC_MAPPING GenericMapping,
	__out_bcount(*PrivilegeSetLength) PPRIVILEGE_SET PrivilegeSet,
	__inout PULONG PrivilegeSetLength,
	__out PACCESS_MASK GrantedAccess,
	__out PNTSTATUS AccessStatus
)
{
	//此位置用于写前回调框架
	//前回调好处在于可以拦截参数,修改参数



	//此位置用于调用原先的函数
	NTSTATUS ntStatus = ((PfnNtAccessCheck)g_MySSDTTableHookInfo[um_SSDT_Monitor_NtAccessCheck]->SrcAddr)(SecurityDescriptor, ClientToken, DesiredAccess, GenericMapping, PrivilegeSet, PrivilegeSetLength, GrantedAccess, AccessStatus);

	//此处如果开启了监控就生成包
	if (g_MySSDTTableHookInfo[um_SSDT_Monitor_NtAccessCheck]->nIsMonitor)
	{
		//生成包
		ULONG32 PackSize = sizeof(CSSDTHookInfo);
		PCSSDTHookInfo pData = MyExAllocMemOry(PackSize, POOL_FLAG_NON_PAGED, KernelMode);
		if (pData != NULL)
		{
			{
				pData->Header.PackSize = PackSize;
				pData->Header.PackType = um_FilterMessageDataType_HookSSDT;
				pData->PID = PsGetCurrentProcessId();
				pData->TID = PsGetCurrentThreadId();
				pData->FunCallNumber = um_SSDT_Monitor_NtAccessCheck;
				pData->FunAddr = g_MySSDTTableHookInfo[um_SSDT_Monitor_NtAccessCheck]->SrcAddr;
				pData->RetValue = ntStatus;
				pData->ParagmaNumber = 8;
				wcscpy(pData->FunName, L"NtAccessCheck");

				//wcscpy(pData->StrFormat, L"参数一:%016I64X 参数二:%08X 参数三:%08X 参数四:%016I64X 参数五:%016I64X 参数六:%016I64X 参数七:%016I64X 参数八:%016I64X");

				//设置参数一
				SetPragma(&pData->Paragma[0], SecurityDescriptor, Pragma_Type_Addr);
				//设置参数二
				SetPragma(&pData->Paragma[1], ClientToken, Pragma_Type_Dword);
				//设置参数三
				SetPragma(&pData->Paragma[2], DesiredAccess, Pragma_Type_Dword);
				//设置参数四
				SetPragma(&pData->Paragma[3], GenericMapping, Pragma_Type_Addr);
				//设置参数五
				SetPragma(&pData->Paragma[4], PrivilegeSet, Pragma_Type_Addr);
				//设置参数六
				SetPragma(&pData->Paragma[5], PrivilegeSetLength, Pragma_Type_Addr);
				//设置参数七
				SetPragma(&pData->Paragma[6], GrantedAccess, Pragma_Type_Addr);
				//设置参数八
				SetPragma(&pData->Paragma[7], AccessStatus, Pragma_Type_Addr);
			}

			//使用Apc异步发送消息
			{
				PRKAPC Kapc = MyExAllocMemOry(sizeof(KAPC), POOL_FLAG_NON_PAGED, KernelMode);
				if (Kapc)
				{
					KeInitializeApc(Kapc, KeGetCurrentThread(), OriginalApcEnvironment, ExFreePool, 0, ToUserSendMessgae, KernelMode, NULL);
					KeInsertQueueApc(Kapc, pData, NULL, NULL);
				}
				else
				{
					ExFreePool(pData);
				}
			}
		}
	}

	//此位置用于写后回调框架
	//后回调作用在于可以拦截返回值或修改返回值


	//执行完函数后引用计数减一
	InterlockedDecrement(&g_ApiCallNumber);
	return ntStatus;
}

ULONG64 MyNtWorkerFactoryWorkerReady(PVOID64 Paragma)
{
	//前回调



	//调用原本函数
	NTSTATUS ntStatus = ((PfnNtWorkerFactoryWorkerReady)g_MySSDTTableHookInfo[um_SSDT_Monitor_NtWorkerFactoryWorkerReady]->SrcAddr)(Paragma);

	//此处如果开启了监控就生成包
	if (g_MySSDTTableHookInfo[um_SSDT_Monitor_NtWorkerFactoryWorkerReady]->nIsMonitor)
	{
		//生成包
		ULONG32 PackSize = sizeof(CSSDTHookInfo);
		PCSSDTHookInfo pData = MyExAllocMemOry(PackSize, POOL_FLAG_NON_PAGED, KernelMode);
		if (pData != NULL)
		{
			{
				pData->Header.PackSize = PackSize;
				pData->Header.PackType = um_FilterMessageDataType_HookSSDT;
				pData->PID = PsGetCurrentProcessId();
				pData->TID = PsGetCurrentThreadId();
				pData->FunCallNumber = um_SSDT_Monitor_NtWorkerFactoryWorkerReady;
				pData->FunAddr = g_MySSDTTableHookInfo[um_SSDT_Monitor_NtWorkerFactoryWorkerReady]->SrcAddr;
				pData->RetValue = ntStatus;
				pData->ParagmaNumber = 1;
				wcscpy(pData->FunName, L"NtWorkerFactoryWorkerReady");

				//设置参数一
				SetPragma(&pData->Paragma[0], Paragma, Pragma_Type_Addr);
			}


			//使用Apc异步发送消息
			{
				PRKAPC Kapc = MyExAllocMemOry(sizeof(KAPC), POOL_FLAG_NON_PAGED, KernelMode);
				if (Kapc)
				{
					KeInitializeApc(Kapc, KeGetCurrentThread(), OriginalApcEnvironment, ExFreePool, 0, ToUserSendMessgae, KernelMode, NULL);
					KeInsertQueueApc(Kapc, pData, NULL, NULL);
				}
				else
				{
					ExFreePool(pData);
				}
			}
		}
	}

	//后回调

	//执行完函数后引用计数减一
	InterlockedDecrement(&g_ApiCallNumber);
	return ntStatus;
}

NTSTATUS
MyNtAcceptConnectPort(
	__out PHANDLE PortHandle,
	__in_opt PVOID PortContext,
	__in PPORT_MESSAGE ConnectionRequest,
	__in BOOLEAN AcceptConnection,
	__inout_opt PPORT_VIEW ServerView,
	__out_opt PREMOTE_PORT_VIEW ClientView
)
{
	//前回调



	//调用原本函数
	NTSTATUS ntStatus = ((PfnNtAcceptConnectPort)g_MySSDTTableHookInfo[um_SSDT_Monitor_NtAcceptConnectPort]->SrcAddr)(PortHandle, PortContext, ConnectionRequest, AcceptConnection, ServerView, ClientView);

	//此处如果开启了监控就生成包
	if (g_MySSDTTableHookInfo[um_SSDT_Monitor_NtAcceptConnectPort]->nIsMonitor)
	{
		//生成包
		ULONG32 PackSize = sizeof(CSSDTHookInfo);
		PCSSDTHookInfo pData = MyExAllocMemOry(PackSize, POOL_FLAG_NON_PAGED, KernelMode);
		if (pData != NULL)
		{
			{
				pData->Header.PackSize = PackSize;
				pData->Header.PackType = um_FilterMessageDataType_HookSSDT;
				pData->PID = PsGetCurrentProcessId();
				pData->TID = PsGetCurrentThreadId();
				pData->FunCallNumber = um_SSDT_Monitor_NtAcceptConnectPort;
				pData->FunAddr = g_MySSDTTableHookInfo[um_SSDT_Monitor_NtAcceptConnectPort]->SrcAddr;
				pData->RetValue = ntStatus;
				pData->ParagmaNumber = 1;
				wcscpy(pData->FunName, L"NtAcceptConnectPort");

				//设置参数一
				SetPragma(&pData->Paragma[0], PortHandle, Pragma_Type_Addr);
				//设置参数二
				SetPragma(&pData->Paragma[1], PortContext, Pragma_Type_Addr);
				//设置参数三
				SetPragma(&pData->Paragma[2], ConnectionRequest, Pragma_Type_Addr);
				//设置参数四
				SetPragma(&pData->Paragma[3], AcceptConnection, Pragma_Type_Char);
				//设置参数五
				SetPragma(&pData->Paragma[4], ServerView, Pragma_Type_Addr);
				//设置参数六
				SetPragma(&pData->Paragma[5], ClientView, Pragma_Type_Addr);

			}


			//使用Apc异步发送消息
			{
				PRKAPC Kapc = MyExAllocMemOry(sizeof(KAPC), POOL_FLAG_NON_PAGED, KernelMode);
				if (Kapc)
				{
					KeInitializeApc(Kapc, KeGetCurrentThread(), OriginalApcEnvironment, ExFreePool, 0, ToUserSendMessgae, KernelMode, NULL);
					KeInsertQueueApc(Kapc, pData, NULL, NULL);
				}
				else
				{
					ExFreePool(pData);
				}
			}
		}
	}

	//后回调


	//执行完函数后引用计数减一
	InterlockedDecrement(&g_ApiCallNumber);
	return ntStatus;
}


NTSTATUS
MyNtMapUserPhysicalPagesScatter(
	__in_ecount(NumberOfPages) PVOID* VirtualAddresses,
	__in ULONG_PTR NumberOfPages,
	__in_ecount_opt(NumberOfPages) PULONG_PTR UserPfnArray
)
{
	//前回调



	//调用原本函数
	NTSTATUS ntStatus = ((PfnNtMapUserPhysicalPagesScatter)g_MySSDTTableHookInfo[um_SSDT_Monitor_NtMapUserPhysicalPagesScatter]->SrcAddr)(VirtualAddresses, NumberOfPages, UserPfnArray);


	//此处如果开启了监控就生成包
	if (g_MySSDTTableHookInfo[um_SSDT_Monitor_NtMapUserPhysicalPagesScatter]->nIsMonitor)
	{
		//生成包
		ULONG32 PackSize = sizeof(CSSDTHookInfo);
		PCSSDTHookInfo pData = MyExAllocMemOry(PackSize, POOL_FLAG_NON_PAGED, KernelMode);
		if (pData != NULL)
		{
			{
				pData->Header.PackSize = PackSize;
				pData->Header.PackType = um_FilterMessageDataType_HookSSDT;
				pData->PID = PsGetCurrentProcessId();
				pData->TID = PsGetCurrentThreadId();
				pData->FunCallNumber = um_SSDT_Monitor_NtMapUserPhysicalPagesScatter;
				pData->FunAddr = g_MySSDTTableHookInfo[um_SSDT_Monitor_NtMapUserPhysicalPagesScatter]->SrcAddr;
				pData->RetValue = ntStatus;
				pData->ParagmaNumber = 3;
				wcscpy(pData->FunName, L"NtMapUserPhysicalPagesScatter");

				//设置参数一
				SetPragma(&pData->Paragma[0], VirtualAddresses, Pragma_Type_Addr);
				//设置参数二
				SetPragma(&pData->Paragma[1], NumberOfPages, Pragma_Type_Addr);
				//设置参数三
				SetPragma(&pData->Paragma[2], UserPfnArray, Pragma_Type_Addr);
			}


			//使用Apc异步发送消息
			{
				PRKAPC Kapc = MyExAllocMemOry(sizeof(KAPC), POOL_FLAG_NON_PAGED, KernelMode);
				if (Kapc)
				{
					KeInitializeApc(Kapc, KeGetCurrentThread(), OriginalApcEnvironment, ExFreePool, 0, ToUserSendMessgae, KernelMode, NULL);
					KeInsertQueueApc(Kapc, pData, NULL, NULL);
				}
				else
				{
					ExFreePool(pData);
				}
			}
		}
	}


	//后回调

	//执行完函数后引用计数减一
	InterlockedDecrement(&g_ApiCallNumber);
	return ntStatus;
}

NTSTATUS
MyNtWaitForSingleObject(
	__in HANDLE Handle,
	__in BOOLEAN Alertable,
	__in_opt PLARGE_INTEGER Timeout
)
{



	//调用原本函数
	NTSTATUS ntStatus = ((PfnNtWaitForSingleObject)g_MySSDTTableHookInfo[um_SSDT_Monitor_NtWaitForSingleObject]->SrcAddr)(Handle, Alertable, Timeout);


	//此处如果开启了监控就生成包
	if (g_MySSDTTableHookInfo[um_SSDT_Monitor_NtWaitForSingleObject]->nIsMonitor)
	{
		//生成包
		ULONG32 PackSize = sizeof(CSSDTHookInfo);
		PCSSDTHookInfo pData = MyExAllocMemOry(PackSize, POOL_FLAG_NON_PAGED, KernelMode);
		if (pData != NULL)
		{
			{
				pData->Header.PackSize = PackSize;
				pData->Header.PackType = um_FilterMessageDataType_HookSSDT;
				pData->PID = PsGetCurrentProcessId();
				pData->TID = PsGetCurrentThreadId();
				pData->FunCallNumber = um_SSDT_Monitor_NtWaitForSingleObject;
				pData->FunAddr = g_MySSDTTableHookInfo[um_SSDT_Monitor_NtWaitForSingleObject]->SrcAddr;
				pData->RetValue = ntStatus;
				pData->ParagmaNumber = 3;
				wcscpy(pData->FunName, L"NtWaitForSingleObject");

				//设置参数一
				SetPragma(&pData->Paragma[0], Handle, Pragma_Type_Addr);
				//设置参数二
				SetPragma(&pData->Paragma[1], Alertable, Pragma_Type_Char);
				//设置参数三
				SetPragma(&pData->Paragma[2], Timeout, Pragma_Type_Addr);
			}


			//使用Apc异步发送消息
			{
				PRKAPC Kapc = MyExAllocMemOry(sizeof(KAPC), POOL_FLAG_NON_PAGED, KernelMode);
				if (Kapc)
				{
					KeInitializeApc(Kapc, KeGetCurrentThread(), OriginalApcEnvironment, ExFreePool, 0, ToUserSendMessgae, KernelMode, NULL);
					KeInsertQueueApc(Kapc, pData, NULL, NULL);
				}
				else
				{
					ExFreePool(pData);
				}
			}
		}
	}


	//后回调

	//执行完函数后引用计数减一
	InterlockedDecrement(&g_ApiCallNumber);
	return ntStatus;
}



NTSTATUS
MyNtCallbackReturn(
	__in_bcount_opt(OutputLength) PVOID OutputBuffer,
	__in ULONG OutputLength,
	__in NTSTATUS Status
)
{

	NTSTATUS ntStatus = ((PfnNtCallbackReturn)g_MySSDTTableHookInfo[um_SSDT_Monitor_NtCallbackReturn]->SrcAddr)(OutputBuffer, OutputLength, Status);

	//此处如果开启了监控就生成包
	if (g_MySSDTTableHookInfo[um_SSDT_Monitor_NtCallbackReturn]->nIsMonitor)
	{
		//生成包
		ULONG32 PackSize = sizeof(CSSDTHookInfo);
		PCSSDTHookInfo pData = MyExAllocMemOry(PackSize, POOL_FLAG_NON_PAGED, KernelMode);
		if (pData != NULL)
		{
			{
				pData->Header.PackSize = PackSize;
				pData->Header.PackType = um_FilterMessageDataType_HookSSDT;
				pData->PID = PsGetCurrentProcessId();
				pData->TID = PsGetCurrentThreadId();
				pData->FunCallNumber = um_SSDT_Monitor_NtCallbackReturn;
				pData->FunAddr = g_MySSDTTableHookInfo[um_SSDT_Monitor_NtCallbackReturn]->SrcAddr;
				pData->RetValue = ntStatus;
				pData->ParagmaNumber = 3;
				wcscpy(pData->FunName, L"NtWaitForSingleObject");

				//设置参数一
				SetPragma(&pData->Paragma[0], OutputBuffer, Pragma_Type_Addr);
				//设置参数二
				SetPragma(&pData->Paragma[1], OutputLength, Pragma_Type_Dword);
				//设置参数三
				SetPragma(&pData->Paragma[2], Status, Pragma_Type_Addr);
			}

			//使用Apc异步发送消息
			{
				PRKAPC Kapc = MyExAllocMemOry(sizeof(KAPC), POOL_FLAG_NON_PAGED, KernelMode);
				if (Kapc)
				{
					KeInitializeApc(Kapc, KeGetCurrentThread(), OriginalApcEnvironment, ExFreePool, 0, ToUserSendMessgae, KernelMode, NULL);
					KeInsertQueueApc(Kapc, pData, NULL, NULL);
				}
				else
				{
					ExFreePool(pData);
				}
			}
		}
	}

	//执行完函数后引用计数减一
	InterlockedDecrement(&g_ApiCallNumber);
	return Status;
}


NTSTATUS
MyNtReadFile(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID Buffer,
	ULONG Length,
	PLARGE_INTEGER ByteOffset,
	PULONG Key
)
{

	NTSTATUS ntStatus = ((PfnNtReadFile)g_MySSDTTableHookInfo[um_SSDT_Monitor_NtReadFile]->SrcAddr)(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, Buffer, Length, ByteOffset, Key);

	//此处如果开启了监控就生成包
	if (g_MySSDTTableHookInfo[um_SSDT_Monitor_NtReadFile]->nIsMonitor)
	{
		//生成包
		ULONG32 PackSize = sizeof(CSSDTHookInfo);
		PCSSDTHookInfo pData = MyExAllocMemOry(PackSize, POOL_FLAG_NON_PAGED, KernelMode);
		if (pData != NULL)
		{
			{
				pData->Header.PackSize = PackSize;
				pData->Header.PackType = um_FilterMessageDataType_HookSSDT;
				pData->PID = PsGetCurrentProcessId();
				pData->TID = PsGetCurrentThreadId();
				pData->FunCallNumber = um_SSDT_Monitor_NtReadFile;
				pData->FunAddr = g_MySSDTTableHookInfo[um_SSDT_Monitor_NtReadFile]->SrcAddr;
				pData->RetValue = ntStatus;
				pData->ParagmaNumber = 9;
				wcscpy(pData->FunName, L"NtReadFile");

				//设置参数一
				SetPragma(&pData->Paragma[0], FileHandle, Pragma_Type_Addr);
				//设置参数二
				SetPragma(&pData->Paragma[1], Event, Pragma_Type_Dword);
				//设置参数三
				SetPragma(&pData->Paragma[2], ApcRoutine, Pragma_Type_Addr);
				//设置参数四
				SetPragma(&pData->Paragma[3], ApcContext, Pragma_Type_Addr);
				//设置参数五
				SetPragma(&pData->Paragma[4], IoStatusBlock, Pragma_Type_Dword);
				//设置参数六
				SetPragma(&pData->Paragma[5], Buffer, Pragma_Type_Addr);
				//设置参数七
				SetPragma(&pData->Paragma[6], Length, Pragma_Type_Dword);
				//设置参数八
				SetPragma(&pData->Paragma[7], ByteOffset, Pragma_Type_Dword);
				//设置参数九
				SetPragma(&pData->Paragma[8], Key, Pragma_Type_Addr);
			}

			//使用Apc异步发送消息
			{
				PRKAPC Kapc = MyExAllocMemOry(sizeof(KAPC), POOL_FLAG_NON_PAGED, KernelMode);
				if (Kapc)
				{
					KeInitializeApc(Kapc, KeGetCurrentThread(), OriginalApcEnvironment, ExFreePool, 0, ToUserSendMessgae, KernelMode, NULL);
					KeInsertQueueApc(Kapc, pData, NULL, NULL);
				}
				else
				{
					ExFreePool(pData);
				}
			}
		}
	}

	//执行完函数后引用计数减一
	InterlockedDecrement(&g_ApiCallNumber);
	return ntStatus;
}

NTSTATUS
MyNtDeviceIoControlFile(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG IoControlCode,
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength
)
{
	NTSTATUS ntStatus = ((PfnNtDeviceIoControlFile)g_MySSDTTableHookInfo[um_SSDT_Monitor_NtDeviceIoControlFile]->SrcAddr)(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, IoControlCode, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);

	//此处如果开启了监控就生成包
	if (g_MySSDTTableHookInfo[um_SSDT_Monitor_NtDeviceIoControlFile]->nIsMonitor)
	{
		//生成包
		ULONG32 PackSize = sizeof(CSSDTHookInfo);
		PCSSDTHookInfo pData = MyExAllocMemOry(PackSize, POOL_FLAG_NON_PAGED, KernelMode);
		if (pData != NULL)
		{
			{
				pData->Header.PackSize = PackSize;
				pData->Header.PackType = um_FilterMessageDataType_HookSSDT;
				pData->PID = PsGetCurrentProcessId();
				pData->TID = PsGetCurrentThreadId();
				pData->FunCallNumber = um_SSDT_Monitor_NtDeviceIoControlFile;
				pData->FunAddr = g_MySSDTTableHookInfo[um_SSDT_Monitor_NtDeviceIoControlFile]->SrcAddr;
				pData->RetValue = ntStatus;
				pData->ParagmaNumber = 10;
				wcscpy(pData->FunName, L"NtDeviceIoControlFile");

				//设置参数一
				SetPragma(&pData->Paragma[0], FileHandle, Pragma_Type_Addr);
				//设置参数二
				SetPragma(&pData->Paragma[1], Event, Pragma_Type_Dword);
				//设置参数三
				SetPragma(&pData->Paragma[2], ApcRoutine, Pragma_Type_Addr);
				//设置参数四
				SetPragma(&pData->Paragma[3], ApcContext, Pragma_Type_Addr);
				//设置参数五
				SetPragma(&pData->Paragma[4], IoStatusBlock, Pragma_Type_Dword);
				//设置参数六
				SetPragma(&pData->Paragma[5], IoControlCode, Pragma_Type_Addr);
				//设置参数七
				SetPragma(&pData->Paragma[6], InputBuffer, Pragma_Type_Dword);
				//设置参数八
				SetPragma(&pData->Paragma[7], InputBufferLength, Pragma_Type_Dword);
				//设置参数九
				SetPragma(&pData->Paragma[8], OutputBuffer, Pragma_Type_Addr);
				//设置参数十
				SetPragma(&pData->Paragma[9], OutputBufferLength, Pragma_Type_Addr);
			}

			//使用Apc异步发送消息
			{
				PRKAPC Kapc = MyExAllocMemOry(sizeof(KAPC), POOL_FLAG_NON_PAGED, KernelMode);
				if (Kapc)
				{
					KeInitializeApc(Kapc, KeGetCurrentThread(), OriginalApcEnvironment, ExFreePool, 0, ToUserSendMessgae, KernelMode, NULL);
					KeInsertQueueApc(Kapc, pData, NULL, NULL);
				}
				else
				{
					ExFreePool(pData);
				}
			}
		}
	}

	//执行完函数后引用计数减一
	InterlockedDecrement(&g_ApiCallNumber);
	return ntStatus;
}


NTSTATUS
MyNtWriteFile(
	__in HANDLE FileHandle,
	__in_opt HANDLE Event,
	__in_opt PIO_APC_ROUTINE ApcRoutine,
	__in_opt PVOID ApcContext,
	__out PIO_STATUS_BLOCK IoStatusBlock,
	__in_bcount(Length) PVOID Buffer,
	__in ULONG Length,
	__in_opt PLARGE_INTEGER ByteOffset,
	__in_opt PULONG Key
)
{

	NTSTATUS ntStatus = ((PfnNtWriteFile)g_MySSDTTableHookInfo[um_SSDT_Monitor_NtWriteFile]->SrcAddr)(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, Buffer, Length, ByteOffset, Key);

	//此处如果开启了监控就生成包
	if (g_MySSDTTableHookInfo[um_SSDT_Monitor_NtWriteFile]->nIsMonitor)
	{
		//生成包
		ULONG32 PackSize = sizeof(CSSDTHookInfo);
		PCSSDTHookInfo pData = MyExAllocMemOry(PackSize, POOL_FLAG_NON_PAGED, KernelMode);
		if (pData != NULL)
		{
			{
				pData->Header.PackSize = PackSize;
				pData->Header.PackType = um_FilterMessageDataType_HookSSDT;
				pData->PID = PsGetCurrentProcessId();
				pData->TID = PsGetCurrentThreadId();
				pData->FunCallNumber = um_SSDT_Monitor_NtWriteFile;
				pData->FunAddr = g_MySSDTTableHookInfo[um_SSDT_Monitor_NtWriteFile]->SrcAddr;
				pData->RetValue = ntStatus;
				pData->ParagmaNumber = 9;
				wcscpy(pData->FunName, L"NtWriteFile");

				//设置参数一
				SetPragma(&pData->Paragma[0], FileHandle, Pragma_Type_Addr);
				//设置参数二
				SetPragma(&pData->Paragma[1], Event, Pragma_Type_Dword);
				//设置参数三
				SetPragma(&pData->Paragma[2], ApcRoutine, Pragma_Type_Addr);
				//设置参数四
				SetPragma(&pData->Paragma[3], ApcContext, Pragma_Type_Addr);
				//设置参数五
				SetPragma(&pData->Paragma[4], IoStatusBlock, Pragma_Type_Dword);
				//设置参数六
				SetPragma(&pData->Paragma[5], Buffer, Pragma_Type_Addr);
				//设置参数七
				SetPragma(&pData->Paragma[6], Length, Pragma_Type_Dword);
				//设置参数八
				SetPragma(&pData->Paragma[7], ByteOffset, Pragma_Type_Dword);
				//设置参数九
				SetPragma(&pData->Paragma[8], Key, Pragma_Type_Addr);
			}

			//使用Apc异步发送消息
			{
				PRKAPC Kapc = MyExAllocMemOry(sizeof(KAPC), POOL_FLAG_NON_PAGED, KernelMode);
				if (Kapc)
				{
					KeInitializeApc(Kapc, KeGetCurrentThread(), OriginalApcEnvironment, ExFreePool, 0, ToUserSendMessgae, KernelMode, NULL);
					KeInsertQueueApc(Kapc, pData, NULL, NULL);
				}
				else
				{
					ExFreePool(pData);
				}
			}
		}
	}

	//执行完函数后引用计数减一
	InterlockedDecrement(&g_ApiCallNumber);
	return ntStatus;

}

NTSTATUS
MyNtRemoveIoCompletion(
	__in HANDLE IoCompletionHandle,
	__out PVOID* KeyContext,
	__out PVOID* ApcContext,
	__out PIO_STATUS_BLOCK IoStatusBlock,
	__in_opt PLARGE_INTEGER Timeout
)
{

}

NTSTATUS
MyNtReleaseSemaphore(
	__in HANDLE SemaphoreHandle,
	__in LONG ReleaseCount,
	__out_opt PLONG PreviousCount
)
{

}

NTSTATUS
MyNtReplyWaitReceivePort(
	__in HANDLE PortHandle,
	__out_opt PVOID* PortContext,
	__in_opt PPORT_MESSAGE ReplyMessage,
	__out PPORT_MESSAGE ReceiveMessage
)
{

}