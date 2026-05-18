#pragma once
#include "Interface.h"
#include "CommunCation.h"
#include "Struct.h"
#include "Head.h"
#include "Ssdt.h"

CCmd g_CmdFun[MAX_FUNCALL_INDEX] = {
	{um_Cmd_Enum_Process_info								,EnumProcessInfo},					//获取进程信息
	{um_Cmd_Enum_ProcessVad_info							,EnumProcessVadInfo},				//获取进程中VAD内存信息
	{um_Cmd_Enum_ProcessThread_info							,EnumProcessThreadInfo},			//获取进程中线程信息
	{um_Cmd_Enum_ProcessHandle_info							,EnumProcessHandleInfo},			//获取进程中线程信息
	{um_Cmd_Enum_ProcessModule_info							,EnumProcessModuleInfo},			//获取进程中的模块信息
	{um_Cmd_Enum_Driver_info								,EnumDriverInfo},					//获取驱动对象信息
	{um_Cmd_Enum_File_info									,EnumFileInfo },					//枚举磁盘文件信息
	{um_Cmd_Enum_Registry_info								,EnumRegistryInfo},					//枚举注册表信息
	{um_Cmd_Enum_Gdt_info									,EnumGdtInfo },						//枚举GDT表信息
	{um_Cmd_Enum_Idt_info									,EnumIdtInfo},						//枚举IDT表信息
	{um_Cmd_Enum_SSDT_info									,EnumSsdtInfo},						//枚举SSDT表信息
	{um_Cmd_Enum_SSDTShadow_info							,EnumSsdtShadowInfo},				//枚举SSDTShadow信息
	{um_Cmd_Enum_KernelCallBack_info						,EnumKernelCallBackInfo},			//枚举内核回调信息
	{um_Cmd_Enum_MiniFilterCallBack_info					,EnumMiniFilterCallBackInfo},		//枚举MiniFilter回调信息
	{um_Cmd_Enum_ObjectCallBack_info						,EnumObjectTypeCallBackInfo},		//枚举对象回调信息
	{um_Cmd_Enum_ObjectCallBackEx_info						,EnumObjectTypeCallBackExInfo},		//枚举对象回调信息
	{um_Cmd_Enum_ObjectMajorFunction_info					,EnumDriverMajorFunctionInfo},		//枚举驱动IRP回调信息
	{um_Cmd_Enum_Dpc_info									,EnumDpcInfo},						//枚举DPC回调信息
	{um_Cmd_DeleteFile_info									,MyDeleteFile},						//强制删除正在运行的文件
	{um_Cmd_ReturnSsdtAndSsdtShadow_info					,MyReturnSsdtAndSsdtShadow},		//恢复SSDT表和SSDTShadow表钩子
	{um_Cmd_FileDeoccupy_info								,MyFileDeoccupy},					//解除文件占用
	{um_Cmd_KillProcess_info								,MyKillProcess},					//强制结束进程
	{um_Cmd_RWProcessMemOry_info							,MyRWMemory},						//读取或写入内存
	{um_Cmd_HookSystemServiceTable_info						,HookSystemServiceTable},			//hook系统服务表	SSDT 或者 SSDTShadow 表
	{um_Cmd_Enum_HalTable_info								,EnumHalTableInfo},					//枚举Hanl表
	{um_Cmd_Enum_SystemDevice_info							,EnumSystemDeviceInfo},				//枚举FileSystemDevice表
	{um_Cmd_Hook_Ssdt										,HookSsdtTable},					//HookSSdt表中的函数
	{um_Cmd_Init_Data										,InitData},							//初始化数据		
	{um_Cmd_Set_ProcessPortection	                        ,SetProcessPortection },			//清除进程保护
	{um_Cmd_Get_ProcessPortection							,GetProcessPortection},				//获取进程保护值
	{um_Cmd_Enum_FilterDriver_info							,EnumFilterDriverInfo},				//获取过滤驱动信息
	{um_Cmd_DebugFlags_info									,DebugFlagsInfo},					//获取或设置调试标志信息
	{um_Cmd_Test											,MyTest},							//获取进程保护值
	// 工作线程队列：用 designated initializer 显式落在与 cmd 值匹配的槽位，
	// 这样 CommuniCation.c 里的硬化校验 g_CmdFun[m_Cmd].m_Cmd == m_Cmd 才能命中。
	[um_Cmd_Enum_WorkerThread_info]							= {um_Cmd_Enum_WorkerThread_info, EnumWorkerThreadInfo},
	// WDF 枚举同样需要 designated initializer，避免跳号造成表错位。
	[um_Cmd_Enum_Wdf01000_info]								= {um_Cmd_Enum_Wdf01000_info,     EnumWdf01000Info},
	[um_Cmd_Enum_WdfFunction_info]							= {um_Cmd_Enum_WdfFunction_info,  EnumWdfFunctionInfo},
	[um_Cmd_Enum_SfilterCallBack_info]						= {um_Cmd_Enum_SfilterCallBack_info, EnumSfilterCallbackInfo},
	[um_Cmd_Enum_ClassInitDataCallBack_info]				= {um_Cmd_Enum_ClassInitDataCallBack_info, EnumClassInitDataCallbackInfo},
	[um_Cmd_Probe_KernelMemory_info]						= {um_Cmd_Probe_KernelMemory_info, ProbeKernelMemoryInfo},
	[um_Cmd_Read_KernelRange_info]							= {um_Cmd_Read_KernelRange_info, ReadKernelRangeInfo},
	[um_Cmd_Dump_ProcessPE_info]							= {um_Cmd_Dump_ProcessPE_info, DumpProcessPEInfo},
};

VOID MyThreadRoutine(PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);

	UNICODE_STRING eventName = { 0 };
	OBJECT_ATTRIBUTES objAttr = { 0 };
	NTSTATUS status = NULL;
	HANDLE hEvent = NULL;

	// 初始化事件名称，必须加上命名空间前缀
	RtlInitUnicodeString(&eventName, L"\\BaseNamedObjects\\Global\\7028001A-27A3-4C83-B359-0CFC333A50BB");

	// 初始化对象属性
	InitializeObjectAttributes(&objAttr,
		&eventName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	// 打开事件对象
	status = ZwOpenEvent(&hEvent, EVENT_ALL_ACCESS, &objAttr);
	if (NT_SUCCESS(status))
	{

		ULONG64 nRet = 0;
		//获取偏移
		nRet = GetOffset();
		if (!nRet)
		{
			MyDbgPrintfEx("%s GetOffset 函数失败！\n", __FUNCTION__);
		}

		ZwSetEvent(hEvent, NULL); // 设置事件为已触发状态
		ZwClose(hEvent); // 关闭句柄
	}
	else {
		DbgPrint("Failed to open event: 0x%X\n", status);
	}

	//结束线程
	PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID __vectorcall InitData(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	HANDLE threadHandle;
	NTSTATUS status = PsCreateSystemThread(
		&threadHandle,
		THREAD_ALL_ACCESS,
		NULL,
		NULL,
		NULL,
		MyThreadRoutine,
		NULL
	);

	if (threadHandle)
	{
		ZwClose(threadHandle);
	}
}

VOID __vectorcall EnumProcessInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	//验证参数是否有效
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = NULL;

	UNICODE_STRING ProcessName = { 0 };
	RtlInitUnicodeString(&ProcessName, L"Process");

	PCProcessInfo pProcessInfo = NULL;
	do
	{
		dqRet = EnumGlobalHandleTable(ProcessName, &pProcessInfo, sizeof(CProcessInfo));
		if (dqRet <= 0 || !MmIsAddressValid(pProcessInfo))
		{
			break;
		}

		PCLIST_ENTRY pCurList = &pProcessInfo->HandleInfo.List.List;

		do
		{
			if (!MmIsAddressValid(pCurList))
			{
				break;
			}

			//从EPROCESS中获取进程信息
			WriteBufferToProcessStructEx(pCurList, ((PCProcessInfo)pCurList)->HandleInfo.Object);

			pCurList = pCurList->Blink;
		} while (pCurList != &pProcessInfo->HandleInfo.List.List);


		//赋值,传给三环
		*(PULONG64)pOutData = pProcessInfo;
	} while (0);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumProcessVadInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pIndata) || !MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = EnumProcessVad(pIndata, pOutData);


	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumProcessThreadInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{

	//__debugbreak();
	//验证参数
	if (!MmIsAddressValid(pIndata) || !MmIsAddressValid(pOutData))
	{
		return;
	}

	//枚举线程
	ULONG64 dqRet = EnumThread(pIndata, pOutData);

	//返回值
	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumProcessHandleInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	//验证参数
	if (!MmIsAddressValid(pIndata) || !MmIsAddressValid(pOutData))
	{
		return;
	}


	//枚举线程
	ULONG64 dqRet = EnumProcessHandleTable(pIndata, pOutData);

	//返回值
	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumProcessModuleInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	//验证参数
	if (!MmIsAddressValid(pIndata) || !MmIsAddressValid(pOutData))
	{
		return;
	}


	//枚举线程
	ULONG64 dqRet = EnumProcessModule(pIndata, pOutData);

	//返回值
	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumDriverInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = 0;


	ULONG64 dqCount1 = EnumSysModule(g_CurDriverObject, pOutData);	//遍历当前驱动对象中的链表
	if (dqCount1 <= 0)
	{
		//当前驱动搞对象链表中不存在直接返回
		return;
	}

	PCDriverInfo pDriverObjectTmp = NULL;
	ULONG64 dqCount = EnumDriverObject(&pDriverObjectTmp);				//遍历对象管理器中的驱动对象获取对象
	if (dqCount <= 0)
	{
		//当为NULL时直接走人
		return;
	}

	PCDriverInfo pDriverObject = (PCDriverInfo) * (PULONG64)pOutData;

	PCLIST_ENTRY pCurList = &pDriverObject->List.List;

	do
	{
		if (pCurList == NULL)
		{
			break;
		}

		PCDriverInfo pDriverObjectInfo = (PCDriverInfo)pCurList;

		PCLIST_ENTRY pCurListTmp = &pDriverObjectTmp->List.List;
		do
		{
			if (pCurListTmp == NULL)
			{
				break;
			}


			PCDriverInfo pDriverObjectInfoTmp = (PCDriverInfo)pCurListTmp;
			//比较是否是同一个
			if ((pDriverObjectInfoTmp->DriverStart == pDriverObjectInfo->ImageBaseAddr) && pDriverObjectInfoTmp->DriverStart && pDriverObjectInfo->ImageBaseAddr)
			{
				pDriverObjectInfo->DriverObject = pDriverObjectInfoTmp->DriverObject;
				memcpy_s(pDriverObjectInfo->ServerName, MAX_PATH, pDriverObjectInfoTmp->ServerName, MY_MAX_PATH);
				memcpy_s(pDriverObjectInfo->DriverName, MAX_PATH, pDriverObjectInfoTmp->DriverName, MY_MAX_PATH);
			}

			//指向下一个
			pCurListTmp = pCurListTmp->Blink;

		} while (pCurListTmp != &pDriverObjectTmp->List.List);

		//指向下一个
		pCurList = pCurList->Blink;
	} while (pCurList != &pDriverObject->List.List);



	SIZE_T AllocFreeSize = 0;
	//释放掉pDriverObjectTmp空间
	PCLIST_ENTRY pCurListTmp = &pDriverObjectTmp->List.List;
	do
	{
		if (pCurListTmp == NULL)
		{
			break;
		}
		PCDriverInfo pDriverObjectInfoTmp = (PCDriverInfo)pCurListTmp;
		//指向下一个
		pCurListTmp = pCurListTmp->Blink;

		ZwFreeVirtualMemory(NtCurrentProcess(), &pDriverObjectInfoTmp, &AllocFreeSize, MEM_RELEASE);
	} while (pCurListTmp != &pDriverObjectTmp->List.List);


	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumFileInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	//验证参数
	if (!MmIsAddressValid(pIndata) || !MmIsAddressValid(pOutData))
	{
		return;
	}

	WCHAR szBuf[MY_MAX_PATH] = { 0 };
	swprintf(szBuf, L"\\??\\%ws", (PWCHAR)pIndata);
	UNICODE_STRING ustrQueryFile;
	RtlInitUnicodeString(&ustrQueryFile, szBuf);


	ULONG64 dpRet = MyQueryFileAndFileFolder(ustrQueryFile, pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dpRet;
	}
}

VOID __vectorcall EnumRegistryInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	//验证参数
	if (!MmIsAddressValid(pIndata) || !MmIsAddressValid(pOutData))
	{
		return;
	}

	UNICODE_STRING ustrRegistryPath;
	RtlInitUnicodeString(&ustrRegistryPath, pIndata);

	EnumRegistryKey(ustrRegistryPath, pOutData);
	EnumRegistryValue(ustrRegistryPath, pOutData);



	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = TRUE;
	}
}

VOID __vectorcall EnumGdtInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = 0;


	EnumGdtTable(pOutData);


	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumIdtInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = 0;


	EnumIdtTable(pOutData);


	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumSsdtInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = EnumSsdtTable(pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumSsdtShadowInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = EnumSsdtShadowTable(pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumKernelCallBackInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{

	if (!MmIsAddressValid(pOutData))
	{
		return;
	}
	ULONG64 dqRet = 0;


	//枚举创建进程回调
	dqRet += EnumCreateProcessCallBack(pOutData);
	//枚举创建线程回调
	dqRet += EnumCreateThreadCallBack(pOutData);
	//枚举注册表回调
	dqRet += EnumRegistryCallBack(pOutData);
	//枚举加载模块回调
	dqRet += EnumLoadImageCallBack(pOutData);
	//枚举关机回调
	dqRet += EnumShutdownCallBack(pOutData);
	//枚举即插即用回调
	dqRet += EnumPnpCallBack(pOutData);
	//枚举错误信息回调
	dqRet += EnumBugCheckCallback(pOutData);
	//枚举定时器信息回调
	dqRet += EnumIoTimer(pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumMiniFilterCallBackInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	//验证参数
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = EnumMiniFilter(g_pFilter, pOutData, NULL);


	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumObjectTypeCallBackInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}
	ULONG64 dqRet = 0;


	if (MmIsAddressValid(ObTypeIndexTable))
	{
		ULONG64 i = 2; //从第三项开始遍历

		ULONG64 Object = NULL;
		do
		{
			Object = ObTypeIndexTable[i++];
			if (!MmIsAddressValid(Object))
			{
				break;
			}
			dqRet += EnumObjectTypeCallBack(Object, pOutData);
		} while (Object != NULL);
	}

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumObjectTypeCallBackExInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}
	ULONG64 dqRet = 0;


	if (MmIsAddressValid(ObTypeIndexTable))
	{
		ULONG64 i = 2; //从第三项开始遍历

		ULONG64 Object = NULL;
		do
		{
			Object = ObTypeIndexTable[i++];
			if (!MmIsAddressValid(Object))
			{
				break;
			}
			dqRet += EnumObjectTypeCallBackEx(Object, pOutData);
		} while (Object != NULL);
	}

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumDriverMajorFunctionInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{

	//验证参数是否合法
	if (!MmIsAddressValid(pIndata) || !MmIsAddressValid(pOutData))
	{
		return;
	}

	//根据名称获取驱动对象地址
	UNICODE_STRING DriverName = { 0 };
	RtlInitUnicodeString(&DriverName, pIndata);
	ULONG64 dqRet = LookUpDriverObjectByName(&DriverName, NULL);
	if (MmIsAddressValid(dqRet))
	{
		//获取驱动对象MajorFunction信息
		dqRet = EnumSysObjectMajorFunction(dqRet, pOutData);
	}

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumDpcInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = EnumDpcTimer(pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumWorkerThreadInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	UNREFERENCED_PARAMETER(nCmd);
	UNREFERENCED_PARAMETER(pIndata);
	UNREFERENCED_PARAMETER(pParam);

	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = EnumWorkerThread(pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

// WdfFunctions 函数表枚举入口
VOID __vectorcall EnumWdfFunctionInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	UNREFERENCED_PARAMETER(nCmd);
	UNREFERENCED_PARAMETER(pIndata);
	UNREFERENCED_PARAMETER(pParam);

	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = EnumWdfFunction(pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

// Wdf01000 主要函数枚举：DRIVER_OBJECT.MajorFunction[28]
VOID __vectorcall EnumWdf01000Info(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	UNREFERENCED_PARAMETER(nCmd);
	UNREFERENCED_PARAMETER(pIndata);
	UNREFERENCED_PARAMETER(pParam);

	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = EnumWdf01000Maj(pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

// Sfilter 回调枚举：IopFsNotifyChangeQueueHead / MountAware
VOID __vectorcall EnumSfilterCallbackInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	UNREFERENCED_PARAMETER(nCmd);
	UNREFERENCED_PARAMETER(pIndata);
	UNREFERENCED_PARAMETER(pParam);

	if (!MmIsAddressValid(pOutData)) return;

	ULONG64 dqRet = EnumSfilterCallback(pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

// ClassInitData 回调枚举
VOID __vectorcall EnumClassInitDataCallbackInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	UNREFERENCED_PARAMETER(nCmd);
	UNREFERENCED_PARAMETER(pIndata);
	UNREFERENCED_PARAMETER(pParam);

	if (!MmIsAddressValid(pOutData)) return;

	ULONG64 dqRet = EnumClassInitDataCallback(pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall MyDeleteFile(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{

	if (!MmIsAddressValid(pIndata))
	{
		return;
	}
	//__debugbreak();
	UNICODE_STRING FilePath = { 0 };
	WCHAR szFilePath[MY_MAX_PATH] = { 0 };
	swprintf(szFilePath, L"\\??\\%ws", pIndata);
	RtlInitUnicodeString(&FilePath, szFilePath);
	ULONG64 dqRet = MyDeleteRunFile(&FilePath);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall MyReturnSsdtAndSsdtShadow(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pIndata))
	{
		return;
	}

	ULONG32 TableIndex = pIndata & 0x8000000000000000;	//判断是修改 0为:SSDT 表还是 1:为SSDTShadow表 
	ULONG64 DataIndex = pIndata & 0x7FFFFFFFFFFFFFFF;	//获取索引
	ULONG64 dqRet = NULL;

	if (!TableIndex)
	{
		dqRet = ReturnSsdt(DataIndex);
	}
	else
	{
		dqRet = ReturnSsdtShadow(DataIndex);
	}


	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall MyFileDeoccupy(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	//验证参数,文件路径是否有效
	if (!MmIsAddressValid(pIndata))
	{
		return;
	}
	UNICODE_STRING inputString = { 0 };
	UNICODE_STRING outputString = { 0 };

	WCHAR szBuf[MY_MAX_PATH] = { 0 };
	swprintf(szBuf, L"*%ws", &((PWCHAR)pIndata)[2]);
	RtlInitUnicodeString(&inputString, szBuf);

	outputString.Length = 0;
	outputString.MaximumLength = inputString.MaximumLength;
	outputString.Buffer = (PWCH)ExAllocatePoolWithTag(NonPagedPool, outputString.MaximumLength, 'tag');
	if (outputString.Buffer == NULL)
	{
		return;
	}

	//小写转换大写
	NTSTATUS status = RtlUpcaseUnicodeString(&outputString, &inputString, FALSE);

	//调用解除文件占用的函数
	ULONG64 dqRet = UnlockFile(&outputString);


	//释放资源
	if (outputString.Buffer)
	{
		ExFreePoolWithTag(outputString.Buffer, 'tag');
	}

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}

}

VOID __vectorcall MyKillProcess(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	//验证参数是否正确 ,要结束的进程的EPROCESS
	if (!MmIsAddressValid(pIndata))
	{
		return;
	}

	if (!MmIsAddressValid(MyPspTerminateProcess))
	{
		return;
	}

	ULONG64 dqRet = MyPspTerminateProcess(pIndata, KeGetCurrentThread(), 0, 1);		/*结束进程*/

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall MyRWMemory(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	//验证参数是否有效,PCRWMemoryInfo结构体
	if (!MmIsAddressValid(pIndata))
	{
		return;
	}

	//__debugbreak();

	PCRWMemoryInfo pMemInfo = pIndata;

	//MyDbgPrintfEx("Buf:%I64X\n", pMemInfo->Buf);

	ULONG64 dqRet = 0;


	if (pMemInfo->Mode == RWMEMORY_WRITE)
	{
		dqRet = MiWriteVirtualMemory(pMemInfo->Eprocess, pMemInfo->DstAddr, pMemInfo->dqSize, pMemInfo->Buf);
	}
	else if (pMemInfo->Mode == RWMEMORY_READ)
	{
		PUCHAR pBuf = NULL;
		dqRet = MiReadVirtualMemory(pMemInfo->Eprocess, pMemInfo->DstAddr, pMemInfo->dqSize, &pBuf);
		if (dqRet > 0)
		{
			RtlMoveMemory(pMemInfo->Buf, pBuf, pMemInfo->dqSize);
			ExFreePool(pBuf);
		}
	}
	else
	{
		dqRet = 0;
	}

	//__debugbreak();

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall HookSystemServiceTable(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pIndata))
	{
		return;
	}

	ULONG64 dqRet = FALSE;





	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall HookSsdtTable(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pIndata))
	{
		return;
	}

	//__debugbreak();

	ULONG64 FunNumber = ((PCHookSsdtTableInfo)pIndata)->FunNumber;
	ULONG64 State = ((PCHookSsdtTableInfo)pIndata)->State;

	g_MySSDTTableHookInfo[FunNumber]->nIsMonitor = State;
}

VOID __vectorcall EnumHalTableInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	//__debugbreak();

	ULONG64 dqRet = 0;

	if (HalDispatchTable_Type == pIndata)
	{
		dqRet = EnumHalDispatchTable(HalDispatchTable, sizeof(HAL_DISPATCH) / sizeof(ULONG64), pOutData);
	}
	else if (HalPrivateDispatchTable_Type == pIndata)
	{
		dqRet = EnumHalDispatchTable(HalPrivateDispatchTable, sizeof(HAL_PRIVATE_DISPATCH) / sizeof(ULONG64), pOutData);
	}
	else
	{
		dqRet = 0;
	}

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall EnumSystemDeviceInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	if (!MmIsAddressValid(pOutData))
	{
		return;
	}

	ULONG64 dqRet = 0;
	dqRet += EnumFileSystemDevice(IopNetworkFileSystemQueueHead, um_FileSystemDeviceType_Network, pOutData);
	dqRet += EnumFileSystemDevice(IopCdRomFileSystemQueueHead, um_FileSystemDeviceType_CdRom, pOutData);
	dqRet += EnumFileSystemDevice(IopDiskFileSystemQueueHead, um_FileSystemDeviceType_Disk, pOutData);
	dqRet += EnumFileSystemDevice(IopTapeFileSystemQueueHead, um_FileSystemDeviceType_Tape, pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall SetProcessPortection(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	ULONG64 dqRet = FALSE;
	if (MmIsAddressValid(pIndata))
	{
		dqRet = PsSetProcessProtection(pIndata, pParam);
	}

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}

VOID __vectorcall GetProcessPortection(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	ULONG64 dqRet = FALSE;
	if (MmIsAddressValid(pIndata))
	{
		dqRet = PsGetProcessProtection(pIndata);
	}

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = dqRet;
	}
}
VOID __vectorcall EnumFilterDriverInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	int nRet = 0;


	if (MmIsAddressValid(pOutData))
	{
		nRet = EnumFilterDriver(pOutData);
	}


	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = nRet;
	}
}

VOID __vectorcall DebugFlagsInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	int nRet = 0;

	//__debugbreak();

	if (pIndata)
		nRet = kdDebugFlags(pIndata);

	if (pOutData)
		nRet = kdDebugFlags(pOutData);

	if (MmIsAddressValid(pRet))
	{
		*(PULONG64)pRet = nRet;
	}
}

VOID __vectorcall MyTest(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	//EnumFilterDriver();
	//GetDriverLoadOrd(NULL);

	BOOL nret = IsDebug();
	MyDbgPrintfEx("IsDebug:%d\n", nret);
}

// 内核钩子检测：批量探测内核虚拟地址前若干字节。
// pIndata 指向 R3 进程地址空间里的 CKernelProbeHeader。
// 我们就地填 Items[i].Bytes / Valid。
VOID __vectorcall ProbeKernelMemoryInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	UNREFERENCED_PARAMETER(nCmd);
	UNREFERENCED_PARAMETER(pOutData);
	UNREFERENCED_PARAMETER(pParam);

	if (!MmIsAddressValid((PVOID)pIndata)) return;

	PCKernelProbeHeader p = (PCKernelProbeHeader)pIndata;
	ULONG cnt = p->Count;
	if (cnt > MAX_KHK_PROBE) cnt = MAX_KHK_PROBE;

	for (ULONG i = 0; i < cnt; ++i)
	{
		PCKernelProbeEntry e = &p->Items[i];
		e->Valid = 0;
		RtlZeroMemory(e->Bytes, KHK_BYTES_PER);

		PVOID src = (PVOID)e->Addr;
		if (src == NULL) continue;

		// 先粗筛：MmIsAddressValid 只检查 PTE 中的 P 位，足够避免明显的无效地址。
		// 真正的 SEH 兜底捕获换出页 / 页错误。
		if (!MmIsAddressValid(src)) continue;
		// 仅读取，避免覆盖到分页边界后半段不可读；只要前 16 字节起始页可读即可。
		if (!MmIsAddressValid((PUCHAR)src + KHK_BYTES_PER - 1)) continue;

		__try
		{
			RtlCopyMemory(e->Bytes, src, KHK_BYTES_PER);
			e->Valid = 1;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			e->Valid = 0;
			RtlZeroMemory(e->Bytes, KHK_BYTES_PER);
		}
	}

	if (MmIsAddressValid((PVOID)pRet))
	{
		*(PULONG64)pRet = (ULONG64)cnt;
	}
}

// 读连续内核VA区间到 R3 进程地址空间里的用户缓冲区。
// 用于"全 ntoskrnl .text 段扫描"——一次最多 MAX_KRD_PER_CALL 字节，
// R3 端按块循环调即可。MmIsAddressValid 起始/结束页 + SEH 安全拷贝。
// 因为 IPC 是 minifilter port 同步派发，运行在请求进程上下文，可以直接写 R3 user buffer。
VOID __vectorcall ReadKernelRangeInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	UNREFERENCED_PARAMETER(nCmd);
	UNREFERENCED_PARAMETER(pOutData);
	UNREFERENCED_PARAMETER(pParam);

	if (!MmIsAddressValid((PVOID)pIndata)) return;

	PCKernelRangeReadInfo p = (PCKernelRangeReadInfo)pIndata;
	p->BytesRead = 0;

	if (p->Length == 0 || p->Length > MAX_KRD_PER_CALL) return;
	if (p->UserBuf == NULL) return;

	PUCHAR src = (PUCHAR)p->KernelAddr;
	// 粗筛：起始页 + 末尾页都得有 PTE
	if (!MmIsAddressValid(src)) return;
	if (!MmIsAddressValid(src + p->Length - 1)) return;

	__try
	{
		ProbeForWrite(p->UserBuf, p->Length, 1);				//校验 R3 缓冲区可写
		RtlCopyMemory(p->UserBuf, src, p->Length);
		p->BytesRead = p->Length;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		p->BytesRead = 0;
	}

	if (MmIsAddressValid((PVOID)pRet))
	{
		*(PULONG64)pRet = (ULONG64)p->BytesRead;
	}
}