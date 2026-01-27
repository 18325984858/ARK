#include "CLoadDriver.h"
#include "../MyDriver64/Struct.h"
#include "pch.h"
#include <fltUser.h>
#include <winsvc.h>
#include "DlgMajorfunction.h"
#include "MyPCHunter64Dlg.h"
#include "CThreadPool.h"
#include "Thread.h"

_LoadDriver::_LoadDriver()
{
	m_Port = NULL;
	m_ThreadsRuning = FALSE;

	//初始化函数指针
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumProcessInfo] = &_LoadDriver::UserEnumProcessInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserKillProcess] = &_LoadDriver::UserKillProcess;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumDriverInfo] = &_LoadDriver::UserEnumDriverInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumProcessVadInfo] = &_LoadDriver::UserEnumProcessVadInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumProcessThreadInfo] = &_LoadDriver::UserEnumProcessThreadInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumProcessHandleInfo] = &_LoadDriver::UserEnumProcessHandleInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumProcessModuleInfo] = &_LoadDriver::UserEnumProcessModuleInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumFileInfo] = &_LoadDriver::UserEnumFileInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserFileDeoccupy] = &_LoadDriver::UserFileDeoccupy;
	m_pUserCallBackFun[Um_UserCallBackType_UserDelteFileInfo] = &_LoadDriver::UserDelteFileInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumGdtInfo] = &_LoadDriver::UserEnumGdtInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumSsdtInfo] = &_LoadDriver::UserEnumSsdtInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumSsdtShadowInfo] = &_LoadDriver::UserEnumSsdtShadowInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumRegistryInfo] = &_LoadDriver::UserEnumRegistryInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumKernelCallBackInfo] = &_LoadDriver::UserEnumKernelCallBackInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumMiniFilterCallBackInfo] = &_LoadDriver::UserEnumMiniFilterCallBackInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumObjectCallBackInfo] = &_LoadDriver::UserEnumObjectCallBackInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserRWMemOryInfo] = &_LoadDriver::UserRWMemOryInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumHalTableInfo] = &_LoadDriver::UserEnumHalTableInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumWdfInfo] = &_LoadDriver::UserEnumWdfInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumDpcInfo] = &_LoadDriver::UserEnumDpcInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumDriverMajorFunctionInfo] = &_LoadDriver::UserEnumDriverMajorFunctionInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumIdtInfo] = &_LoadDriver::UserEnumIdtInfo;
	m_pUserCallBackFun[Um_UserCallBackType_UserHookSsdtTable] = &_LoadDriver::UserHookSsdtTable;
	m_pUserCallBackFun[Um_UserCallBackType_UserInsertMonitorDlg] = &_LoadDriver::UserInsertMonitorDlg;
	m_pUserCallBackFun[Um_UserCallBackType_InitData] = &_LoadDriver::InitData;
	m_pUserCallBackFun[Um_UserCallBackType_UserEnumFilterDriver] = &_LoadDriver::UserEnumFilterDriver;
	m_pUserCallBackFun[Um_UserCallBackType_UserGetDebugFlags] = &_LoadDriver::UserDebugFlags;
	m_pUserCallBackFun[Um_UserCallBackType_UserSetDebugFlags] = &_LoadDriver::UserDebugFlags1;


	m_pUserCallBackFun[Um_UserCallBackType_Test] = &_LoadDriver::UserTestFun;


	m_pUserCallBackFun[Um_UserCallBackType_NULL] = NULL;
}
_LoadDriver::~_LoadDriver()
{

	//释放资源

	//首先断开连接
	if (m_Port != NULL)
	{
		CloseHandle(m_Port);
		m_Port = NULL;
	}

	//设置线程运行标志
	m_ThreadsRuning = FALSE;

	//其次释放掉线程
	if (!m_Threads.empty())
	{
		for (auto Lst : m_Threads)
		{
			//等待线程结束
			Lst->join();
			//MessageBox(NULL, TEXT("释放线程!"), TEXT("测试"), MB_OK);

			//释放线程资源
			delete Lst;
		}
	}

	//遍历链表 查找是否还有未被取走的数据 释放
	if (!m_FilterMessageQueue.empty())
	{
		for (auto Lst : m_FilterMessageQueue)
		{
			//直接释放资源
			free(Lst);
		}
	}

}
ULONG64 _LoadDriver::ConnectDriver(CString Name)
{
	if (m_Port != NULL)
	{
		return FALSE;
	}

	//初始化线程
	//参数二:FLT_PORT_FLAG_SYNC_HANDLE 同步模式  0异步模式
	if (FilterConnectCommunicationPort(Name.GetString(), 0, NULL, 0, NULL, &m_Port) == S_OK)
	{
		//初始化线程
		ULONG64 ThreadNumber = CreateGetMessageThread(1);
		if (ThreadNumber == NULL)
		{
			MessageBox(NULL, TEXT("创建线程池失败!"), TEXT("警告"), MB_OK);
		}

		return TRUE;
	}

	return FALSE;
}
VOID _LoadDriver::SetFileNameAndPath(PWCHAR Driver_Name, PWCHAR Driver_Path)
{
	if (Driver_Name != NULL && Driver_Path != NULL)
	{
		wcscpy_s(DriverName, MAX_PATH, Driver_Name);
		GetFullPathNameW(Driver_Path, MAX_PATH, DriverFullNamePath, NULL); //获取当前路径
	}
}
BOOL _LoadDriver::LoadDriverFun() //加载驱动
{

	//先调用卸载
	UnLoadDriverFun();

	SC_HANDLE hServiceMgr = NULL; // SCM管理器句柄	
	hServiceMgr = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		ErrorMessage(GetLastError(), TEXT("OpenSCManagerW错误信息:"));
		return FALSE;
	}
	SC_HANDLE hServiceDDK = NULL; // NT驱动程序服务句柄
	hServiceDDK = CreateServiceW(
		hServiceMgr,
		DriverName,
		DriverName,
		SERVICE_ALL_ACCESS,
		SERVICE_KERNEL_DRIVER,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_IGNORE,
		DriverFullNamePath,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL);
	if (NULL == hServiceDDK)
	{
		DWORD dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING && dwErr != ERROR_SERVICE_EXISTS)
		{
			ErrorMessage(GetLastError(), TEXT("OpenSCManagerW错误信息:"));
			return FALSE;
		}
	}

	// 驱动服务已经创建，打开服务
	hServiceDDK = OpenServiceW(hServiceMgr, DriverName, SERVICE_ALL_ACCESS);
	if (!StartService(hServiceDDK, NULL, NULL))
	{
		DWORD dwErr = GetLastError();
		if (dwErr != ERROR_SERVICE_ALREADY_RUNNING)
		{
			ErrorMessage(GetLastError(), TEXT("运行驱动服务失败! 错误信息:"));
			return FALSE;
		}
	}

	if (hServiceDDK)
	{
		CloseServiceHandle(hServiceDDK);
	}
	if (hServiceMgr)
	{
		CloseServiceHandle(hServiceMgr);
	}
	return TRUE;
}
BOOL _LoadDriver::UnLoadDriverFun()//卸载驱动函数
{
	//先关闭通信句柄
	if (m_Port != NULL)
	{
		CloseHandle(m_Port);
		m_Port = NULL;
	}


	SC_HANDLE hServiceMgr = OpenSCManagerW(0, 0, SC_MANAGER_ALL_ACCESS);
	SC_HANDLE hServiceDDK = OpenServiceW(hServiceMgr, DriverName, SERVICE_ALL_ACCESS);
	SERVICE_STATUS SvrStatus;
	ControlService(hServiceDDK, SERVICE_CONTROL_STOP, &SvrStatus);
	DeleteService(hServiceDDK);
	if (hServiceDDK)
	{
		CloseServiceHandle(hServiceDDK);
	}
	if (hServiceMgr)
	{
		CloseServiceHandle(hServiceMgr);
	}
	return TRUE;
}
ULONG64 _LoadDriver::SendMsg(IN ULONG64 dqCmd, IN LPVOID ilpBuffer, OUT LPVOID* OlpBuffer, OUT PDWORD nNumberOfBytesToWrite, IN OUT PVOID Param)
{
	if (m_Port == NULL)
	{
		return FALSE;
	}

	ULONG64 MsgRet = -1;
	CCommunicationInfo CmdInfo = { 0 };
	CmdInfo.m_Cmd = dqCmd;
	CmdInfo.m_pIndata = (ULONG64)ilpBuffer;
	CmdInfo.m_pOutData = OlpBuffer;
	CmdInfo.m_nRet = &MsgRet;
	CmdInfo.m_pParam = Param;

	DWORD lpBytesReturned = 0;

	//向驱动发送消息
	ULONG64 nRet = FilterSendMessage(m_Port, &CmdInfo, sizeof(CCommunicationInfo), NULL, NULL, (LPDWORD)&lpBytesReturned);
	if (S_OK != nRet)
	{
		CString ErrorStr;
		ErrorStr.Format(L"发送消息失败!命令:[%I64X]\n", dqCmd);
		ErrorMessage(GetLastError(), ErrorStr);
		return MsgRet;
	}

	//接收返回的字节大小
	if (NULL != nNumberOfBytesToWrite)
	{
		*nNumberOfBytesToWrite = lpBytesReturned;
	}

	return MsgRet;
}
ULONG64 _LoadDriver::BaseInterfaceFun(ULONG64 Index/*功能号*/, PVOID Pragma)
{
	if (Index < MAX_USER_CALL_BACK_COUNT && m_pUserCallBackFun[Index] != NULL)
	{
		return (this->*m_pUserCallBackFun[Index])(Pragma);
	}
	return -1;
}
ULONG64 _LoadDriver::InitData(PVOID pInfo)
{
	ULONG64 nRet = 0;

	nRet = SendMsg(um_Cmd_Init_Data, NULL, (LPVOID*)&nRet);

	return nRet;
}
ULONG64 _LoadDriver::UserEnumProcessInfo(PVOID pDlgProcessInfo)
{

	if (pDlgProcessInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCProcessInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_Process_info, NULL, (LPVOID*)&pInfo);


	//插入到控件中
	((DlgProcess*)pDlgProcessInfo)->InsertCtrlListControl(pInfo);


	return TRUE;
}
ULONG64 _LoadDriver::UserKillProcess(PVOID pDlgProcessInfo)
{

	if (pDlgProcessInfo == NULL)
	{
		return FALSE;
	}

	//插入到控件中
	((DlgProcess*)pDlgProcessInfo)->ProcessKillprocess();


	return TRUE;
}
ULONG64 _LoadDriver::UserEnumDriverInfo(PVOID pDlgDriverInfo)
{

	if (pDlgDriverInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCDriverInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_Driver_info, NULL, (LPVOID*)&pInfo);

	//插入到控件中
	((DlgDriverModule*)pDlgDriverInfo)->InsertCtrlListControl(pInfo);


	return TRUE;
}
ULONG64 _LoadDriver::UserEnumProcessVadInfo(PVOID pDlgProcessVadInfo)
{
	if (pDlgProcessVadInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCProcessVadInfo pInfo = NULL;
	//__debugbreak();
	SendMsg(um_Cmd_Enum_ProcessVad_info, (PVOID)_wcstoui64(((DlgProcessVad*)pDlgProcessVadInfo)->m_StrEprocess.GetBuffer(), 0, 16), (LPVOID*)&pInfo);
	//插入到控件中
	((DlgProcessVad*)pDlgProcessVadInfo)->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumProcessThreadInfo(PVOID pDlgProcessThreadInfo)
{
	if (pDlgProcessThreadInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCProcessThreadInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_ProcessThread_info, (PVOID)_wcstoui64(((DlgProcessThread*)pDlgProcessThreadInfo)->m_StrEprocess.GetBuffer(), 0, 16), (LPVOID*)&pInfo);


	//插入到控件中
	((DlgProcessThread*)pDlgProcessThreadInfo)->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumProcessHandleInfo(PVOID pDlgProcessHandleInfo)
{
	if (pDlgProcessHandleInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCProcessHandleInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_ProcessHandle_info, (PVOID)_wcstoui64(((DlgProcessHandle*)pDlgProcessHandleInfo)->m_StrEprocess.GetBuffer(), 0, 16), (LPVOID*)&pInfo);

	//插入到控件中
	((DlgProcessHandle*)pDlgProcessHandleInfo)->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumProcessModuleInfo(PVOID pDlgProcessModuleInfo)
{
	if (pDlgProcessModuleInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCProcessModuleInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_ProcessModule_info, (PVOID)_wcstoui64(((DlgProcessModule*)pDlgProcessModuleInfo)->m_StrEprocess.GetBuffer(), 0, 16), (LPVOID*)&pInfo);

	//插入到控件中
	((DlgProcessModule*)pDlgProcessModuleInfo)->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumFileInfo(PVOID pDlgFileInfo)
{

	if (pDlgFileInfo == NULL)
	{
		return FALSE;
	}

	((DlgEnumFile*)pDlgFileInfo)->EnunFile();

	return TRUE;
}
ULONG64 _LoadDriver::UserFileDeoccupy(PVOID pDlgFileInfo)
{

	if (pDlgFileInfo == NULL)
	{
		return FALSE;
	}

	((DlgEnumFile*)pDlgFileInfo)->FileFiledeoccupy();

	return TRUE;
}
ULONG64 _LoadDriver::UserDelteFileInfo(PVOID pDlgFileInfo)
{

	if (pDlgFileInfo == NULL)
	{
		return FALSE;
	}


	((DlgEnumFile*)pDlgFileInfo)->DelteFile();


	return TRUE;
}
ULONG64 _LoadDriver::UserEnumRegistryInfo(PVOID pDlgRegistryInfo)
{

	if (pDlgRegistryInfo == NULL)
	{
		return FALSE;
	}

	((DlgEnumRegistry*)pDlgRegistryInfo)->InsertCtrlListControl();

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumGdtInfo(PVOID pDlgGdtInfo)
{

	if (pDlgGdtInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCGdtInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_Gdt_info, NULL, (LPVOID*)&pInfo);

	((DlgGdt*)pDlgGdtInfo)->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumIdtInfo(PVOID pDlgIdtInfo)
{

	if (pDlgIdtInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCIdtInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_Idt_info, NULL, (LPVOID*)&pInfo);

	((DlgIdt*)pDlgIdtInfo)->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumSsdtInfo(PVOID pDlgSsdtInfo)
{

	if (pDlgSsdtInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCSsdtInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_SSDT_info, NULL, (LPVOID*)&pInfo);

	((DlgSsdt*)pDlgSsdtInfo)->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumSsdtShadowInfo(PVOID pDlgSsdtShadowInfo)
{

	if (pDlgSsdtShadowInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCSsdtInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_SSDTShadow_info, NULL, (LPVOID*)&pInfo);

	((DlgSsdtShadow*)pDlgSsdtShadowInfo)->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumKernelCallBackInfo(PVOID pDlgKernelCallBackInfo)
{

	if (pDlgKernelCallBackInfo == NULL)
	{
		return FALSE;
	}

	//发送消息货期进程信息
	PCKernelCallBackInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_KernelCallBack_info, NULL, (LPVOID*)&pInfo);

	((DlgKernelCallBack*)pDlgKernelCallBackInfo)->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumMiniFilterCallBackInfo(PVOID pDlgMiniFilterCallBackInfo)
{

	if (pDlgMiniFilterCallBackInfo == NULL)
	{
		return FALSE;
	}

	//按当前选择的去刷新
	switch (((DlgMiniFilterCallBack*)pDlgMiniFilterCallBackInfo)->nPerSel)
	{
	case DlgMiniFilterCallBack::um_FileSystemType_MiniPortFilter:
	{
		PCMiniFilterCallBackInfo pInfo = NULL;
		SendMsg(um_Cmd_Enum_MiniFilterCallBack_info, NULL, (LPVOID*)&pInfo);
		((DlgMiniFilterCallBack*)pDlgMiniFilterCallBackInfo)->InsertCtrlListControl(pInfo);
		break;
	}
	case DlgMiniFilterCallBack::um_FileSystemType_FileSystem:
	{
		PCFileSystemDeviceInfo pInfo = NULL;
		SendMsg(um_Cmd_Enum_SystemDevice_info, NULL, (LPVOID*)&pInfo);
		((DlgMiniFilterCallBack*)pDlgMiniFilterCallBackInfo)->InsertCtrlListControl(pInfo);
		break;
	}
	case DlgMiniFilterCallBack::um_FileSystemType_SfilterCallBack:
		break;
	case DlgMiniFilterCallBack::um_FileSystemType_ClassInitDataClass:
		break;
	case DlgMiniFilterCallBack::um_FileSystemType_NpfsMajorFunction:
	{

		CString DriverName = L"npfs.sys";
		PCSysMajorFunctionInfo pInfo = NULL;
		SendMsg(um_Cmd_Enum_ObjectMajorFunction_info, DriverName.GetBuffer(), (LPVOID*)&pInfo);
		((DlgMiniFilterCallBack*)pDlgMiniFilterCallBackInfo)->InsertCtrlListControl(pInfo);
		break;
	}
	case DlgMiniFilterCallBack::um_FileSystemType_MsfsMajorFunction:
	{
		CString DriverName = L"msfs.sys";
		PCSysMajorFunctionInfo pInfo = NULL;
		SendMsg(um_Cmd_Enum_ObjectMajorFunction_info, DriverName.GetBuffer(), (LPVOID*)&pInfo);
		((DlgMiniFilterCallBack*)pDlgMiniFilterCallBackInfo)->InsertCtrlListControl(pInfo);
		break;
	}
	case DlgMiniFilterCallBack::um_FileSystemType_UsbPortMajorFunction:
	{
		CString DriverName = L"usbport.sys";
		PCSysMajorFunctionInfo pInfo = NULL;
		SendMsg(um_Cmd_Enum_ObjectMajorFunction_info, DriverName.GetBuffer(), (LPVOID*)&pInfo);
		((DlgMiniFilterCallBack*)pDlgMiniFilterCallBackInfo)->InsertCtrlListControl(pInfo);
		break;
	}
	default:
		break;
	}
	return TRUE;
}
ULONG64 _LoadDriver::UserEnumObjectCallBackInfo(PVOID pDlgObjectCallBackInfo)
{
	if (pDlgObjectCallBackInfo == NULL)
	{
		return FALSE;
	}


	switch (((DlgObjectCallBack*)pDlgObjectCallBackInfo)->nPerSel)
	{
	case DlgObjectCallBack::um_ObjectCallBack_Type:
	{
		//发送消息货期进程信息
		PCObjectTypeCallBackInfo pInfo = NULL;
		SendMsg(um_Cmd_Enum_ObjectCallBack_info, NULL, (LPVOID*)&pInfo);

		((DlgObjectCallBack*)pDlgObjectCallBackInfo)->InsertCtrlListControl(pInfo);
	}
	break;

	case DlgObjectCallBack::um_ObjectCallBackInfo_Type:
	{
		PCObjectTypeCallBackExInfo pInfo = NULL;
		SendMsg(um_Cmd_Enum_ObjectCallBackEx_info, NULL, (LPVOID*)&pInfo);

		((DlgObjectCallBack*)pDlgObjectCallBackInfo)->InsertCtrlListControl(pInfo);
	}
	break;
	}

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumDriverMajorFunctionInfo(PVOID pDriverMajorFunctionInfo)
{
	if (pDriverMajorFunctionInfo == NULL)
	{
		return FALSE;
	}

	PWCHAR pDriverName = ((DriverMajorFunctionInfo*)pDriverMajorFunctionInfo)->MajorFunctionDriverName;

	PCSysMajorFunctionInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_ObjectMajorFunction_info, pDriverName, (LPVOID*)&pInfo);


	DlgMajorfunction* pDlg = (DlgMajorfunction*)((DriverMajorFunctionInfo*)pDriverMajorFunctionInfo)->m_Object;
	pDlg->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumDpcInfo(PVOID pDlgDpc)
{
	if (pDlgDpc == NULL)
	{
		return FALSE;
	}

	PCDPcInfo pInfo = NULL;
	SendMsg(um_Cmd_Enum_Dpc_info, NULL, (LPVOID*)&pInfo);

	((DlgDpc*)pDlgDpc)->InsertCtrlListControl(pInfo);

	return TRUE;
}
ULONG64 _LoadDriver::UserRWMemOryInfo(PVOID pDlgRWMemory)
{
	if (pDlgRWMemory == NULL)
	{
		return FALSE;
	}

	((DlgRWMemory*)pDlgRWMemory)->ReadWriteMemOry();


	return TRUE;
}
ULONG64 _LoadDriver::UserEnumHalTableInfo(PVOID pDlgHalTable)
{
	if (pDlgHalTable == NULL)
	{
		return FALSE;
	}

	((DlgHalTable*)pDlgHalTable)->InsertCtrlListControl();

	return TRUE;
}
ULONG64 _LoadDriver::UserEnumWdfInfo(PVOID pDlgWdf)
{
	if (pDlgWdf == NULL)
	{
		return FALSE;
	}

	switch (((DlgWdf*)pDlgWdf)->nPerSel)
	{
	case DlgWdf::um_WdfDlgInfoType_Wdf01000Maj:
	{
		//发送消息货期进程信息
		PCWdfInfo pInfo = NULL;
		SendMsg(um_Cmd_Enum_Wdf01000_info, NULL, (LPVOID*)&pInfo);

		((DlgWdf*)pDlgWdf)->InsertCtrlListControl(pInfo);
	}
	break;

	case DlgWdf::um_WdfDlgInfoType_WdfFunction:
	{
		PCWdfInfo pInfo = NULL;
		SendMsg(um_Cmd_Enum_WdfFunction_info, NULL, (LPVOID*)&pInfo);

		((DlgWdf*)pDlgWdf)->InsertCtrlListControl(pInfo);
	}
	break;
	}
	return TRUE;
}
ULONG64 _LoadDriver::UserHookSsdtTable(PVOID pHookInfo)
{
	if (!pHookInfo)
	{
		return FALSE;
	}

	SendMsg(um_Cmd_Hook_Ssdt, (LPVOID*)pHookInfo);


	delete pHookInfo;

	return TRUE;
}
ULONG64 _LoadDriver::UserInsertMonitorDlg(PVOID pInfo)
{
	if (g_CreateFlagsDlgProcessMonitor)
	{
		g_DlgProcessMonitor.PostMessage(wm_User_DlgProcessMonitor_Insert, (WPARAM)pInfo, NULL);
		return TRUE;
	}
	return FALSE;
}

ULONG64 _LoadDriver::UserEnumFilterDriver(PVOID pInfo)
{

	if (!pInfo)
	{
		return FALSE;
	}

	PCFilterDeviceInfo pFilterDeviceInfo = NULL;
	int nRet = SendMsg(um_Cmd_Enum_FilterDriver_info, NULL, (LPVOID*)&pFilterDeviceInfo);

	((DlgFilterDriver*)pInfo)->InsertCtrlListControl(pFilterDeviceInfo);

	return TRUE;
}

ULONG64 _LoadDriver::UserDebugFlags(PVOID pInfo)
{
	DlgSetting* pDlgSetting = (DlgSetting*)pInfo;
	if (pDlgSetting)
	{
		CDebugFlagInfo pDebugFlagsInfo = { 0 };

		pDebugFlagsInfo.UserOperate = USER_GET_DEBUG_FLAG; //设置为获取调试标志

		if (SendMsg(um_Cmd_DebugFlags_info, NULL, (LPVOID*)&pDebugFlagsInfo))
		{
			pDlgSetting->SetKdDebuggerFlags(&pDebugFlagsInfo);
		}

		return TRUE;
	}
	return FALSE;
}


ULONG64 _LoadDriver::UserDebugFlags1(PVOID pInfo)
{
	if (pInfo)
	{
		return SendMsg(um_Cmd_DebugFlags_info, pInfo);
	}
	return FALSE;
}


ULONG64 _LoadDriver::UserTestFun(PVOID pInfo)
{
	SendMsg(um_Cmd_Test, pInfo);
	return TRUE;
}

typedef struct _SetProcessPortectionInfo
{
	ULONG64 eProcess;
	UCHAR PortectionValue;
}CSetProcessPortectionInfo, * PCSetProcessPortectionInfo;


ULONG64 _LoadDriver::CreateGetMessageThread(ULONG64 ThreadNumbers)
{
	//判断端口是否已连接
	if (m_Port == NULL)
	{
		return FALSE;
	}

	//验证线程数量
	if (ThreadNumbers == 0)
	{
		ThreadNumbers = std::thread::hardware_concurrency();
	}

	//设置标志为可运行状态
	m_ThreadsRuning = TRUE;

	int i = 0;
	//创建线程
	for (i = 0; i < ThreadNumbers; i++) {

		auto pThread = new std::thread(&_LoadDriver::WorkThread, this, i);

		//当有一个创建失败时就返回有一个是一个
		if (pThread == NULL)
		{
			return i;
		}

		//插入到线程链表中
		m_Threads.push_back(pThread);
	}
	return i;
}
PVOID64 _LoadDriver::AllocPack(PCFilterUserGetMessageHeadInfo pinfo)
{
	PVOID64 pAddr = NULL;

	if (pinfo != NULL && pinfo->PackSize != 0)
	{
		pAddr = new char[pinfo->PackSize];
		if (pAddr != NULL)
		{
			memset(pAddr, 0, pinfo->PackSize);

			memcpy_s(pAddr, pinfo->PackSize, pinfo, pinfo->PackSize);
		}
	}
	return pAddr;
}
ULONG64 BuildSendKernelPack(FILTER_REPLY_HEADER* PackHeader, PVOID64 pData, DWORD Type, DWORD PackSize)
{
	static int g_PackIndex = 0;
#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)								// ntsubauth

	PCFilterUserGetMessageHeadInfo PackHead = (PCFilterUserGetMessageHeadInfo)pData;			//将包头转换为对应的包头类型

	//包的ID必须与收到包的ID一致	否则无法回复包
	PackHeader->MessageId = PackHead->Header.MessageId;
	PackHeader->Status = STATUS_SUCCESS;														//设置状态为成功

	switch (Type)
	{
	case um_FilterMessageDataType_GetStructOffset:
	{
		PCFilterUserSendtMessageStructInfo pPackHeader = (PCFilterUserSendtMessageStructInfo)PackHeader;
		PCFilterUserGetMessageStructInfo pStructInfo = (PCFilterUserGetMessageStructInfo)pData;

		pPackHeader->Header.PackSize = PackSize;														//设置包大小
		pPackHeader->Header.PackType = Type;															//设置包类型

		memcpy(&pPackHeader->StructInfo, &pStructInfo->StructInfo, sizeof(CStructInfo));				//拷贝数据包

		return 1;
	}

	case um_FilterMessageDataType_GetStructSize:
	{
		PCFilterUserSendtMessageStructSize pPackHeader = (PCFilterUserSendtMessageStructSize)PackHeader;
		PCFilterUserGetMessageStructSize pStructInfo = (PCFilterUserGetMessageStructSize)pData;

		pPackHeader->Header.PackSize = PackSize;														//设置包大小
		pPackHeader->Header.PackType = Type;															//设置包类型

		memcpy(&pPackHeader->StructInfo, &pStructInfo->StructInfo, sizeof(CStructSize));				//拷贝数据包

		return 1;
	}

	case um_FilterMessageDataType_GetGlobalVariables:
	{
		PCFilterUserSendMessageGlobalVariables pPackHeader = (PCFilterUserSendMessageGlobalVariables)PackHeader;
		PCFilterUserGetMessageGlobalVariables pStructInfo = (PCFilterUserGetMessageGlobalVariables)pData;

		pPackHeader->Header.PackSize = PackSize;														//设置包大小
		pPackHeader->Header.PackType = Type;															//设置包类型

		memcpy(&pPackHeader->VarInfo, &pStructInfo->VarInfo, sizeof(CGlobalVariables));					//拷贝数据包
		return 1;
	}

	default:
		break;
	}
	return 0;
}

PVOID _LoadDriver::WorkThread(_LoadDriver* pThis, uint32_t nIndex)								/*工作线程*/
{
#define FLT_MAX_BUFFER_SIZE (0x1000)

	//OVERLAPPED overlapped = { 0 };
	//__debugbreak();
	while (pThis->m_ThreadsRuning)
	{
		//获取消息
		UCHAR pInfo[FLT_MAX_BUFFER_SIZE] = { 0 };
		//获取消息
		HRESULT hr = FilterGetMessage(pThis->m_Port,
			(PFILTER_MESSAGE_HEADER)pInfo,
			FLT_MAX_BUFFER_SIZE,
			NULL  /*直到有消息才返回让其一直等待*/);

		if (hr == S_OK)
		{
			if (((PCFilterUserGetMessageHeadInfo)pInfo)->PackSize == NULL)
			{
				continue;
			}

			PVOID64 pNewinfo = pThis->AllocPack((PCFilterUserGetMessageHeadInfo)pInfo);				//申请一个包,并将数据拷贝到新申请的空间中
			{
				switch (((PCFilterUserGetMessageHeadInfo)pInfo)->PackType)
				{
				case um_FilterMessageDataType_HookSSDT:
				{
					//直接将收到的包添加到任务中
					//g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserInsertMonitorDlg, pNewinfo });

					g_DlgProcessMonitor.PostMessage(wm_User_DlgProcessMonitor_Insert, (WPARAM)pNewinfo, NULL);
					break;
				}
				case um_FilterMessageDataType_GetStructOffset:
				{
					PCFilterUserGetMessageStructInfo pStructInfo = (PCFilterUserGetMessageStructInfo)pNewinfo;

					for (int i = 0; i < MAX_MODULE_NAME_NUMBER; i++)
					{
						if (g_ModuleCall[i].szModuleName == NULL)
						{
							break;
						}

						if (wcscmp(pStructInfo->StructInfo.szModuleName, g_ModuleCall[i].szModuleName) == 0)
						{
							//获取指定结构体中指定成员的偏移量
							pStructInfo->StructInfo.nOffset = g_ModuleCall[i].m_PdbInfo->GetMemberOffset(pStructInfo->StructInfo.szClassType, pStructInfo->StructInfo.szmemberName);

							CFilterUserSendtMessageStructInfo replyHeader = { 0 };

							//生成包
							BuildSendKernelPack((FILTER_REPLY_HEADER*)&replyHeader, pStructInfo, um_FilterMessageDataType_GetStructOffset, sizeof(CFilterUserSendtMessageStructInfo));

							//回复kernel层 不需要去掉FILTER_REPLY_HEADER头
							FilterReplyMessage(pThis->m_Port, (FILTER_REPLY_HEADER*)&replyHeader, replyHeader.Header.PackSize);
							break;
						}
					}
					break;
				}

				case um_FilterMessageDataType_GetStructSize:
				{

					PCFilterUserGetMessageStructSize pStructInfo = (PCFilterUserGetMessageStructSize)pNewinfo;

					for (int i = 0; i < MAX_MODULE_NAME_NUMBER; i++)
					{
						if (g_ModuleCall[i].szModuleName == NULL)
						{
							break;
						}

						if (wcscmp(pStructInfo->StructInfo.szModuleName, g_ModuleCall[i].szModuleName) == 0)
						{
							pStructInfo->StructInfo.nSize = g_ModuleCall[i].m_PdbInfo->GetStructSize(pStructInfo->StructInfo.szClassName);

							CFilterUserSendtMessageStructSize replyHeader = { 0 };

							//生成包
							BuildSendKernelPack((FILTER_REPLY_HEADER*)&replyHeader, pStructInfo, um_FilterMessageDataType_GetStructSize, sizeof(CFilterUserSendtMessageStructSize));

							//回复kernel层 不需要去掉FILTER_REPLY_HEADER头
							FilterReplyMessage(pThis->m_Port, (FILTER_REPLY_HEADER*)&replyHeader, replyHeader.Header.PackSize);
							break;
						}
					}
					break;
				}

				case um_FilterMessageDataType_GetGlobalVariables:
				{
					PCFilterUserGetMessageGlobalVariables pGlobalVariables = (PCFilterUserGetMessageGlobalVariables)pNewinfo;

					for (int i = 0; i < MAX_MODULE_NAME_NUMBER; i++)
					{
						if (g_ModuleCall[i].szModuleName == NULL)
						{
							break;
						}

						if (wcscmp(pGlobalVariables->VarInfo.szModuleName, g_ModuleCall[i].szModuleName) == 0)
						{
							pGlobalVariables->VarInfo.nOffset = g_ModuleCall[i].m_PdbInfo->GetGlobalVariablesOffset(pGlobalVariables->VarInfo.szVarName);

							CFilterUserGetMessageGlobalVariables replyHeader = { 0 };

							//生成包
							BuildSendKernelPack((FILTER_REPLY_HEADER*)&replyHeader, pGlobalVariables, um_FilterMessageDataType_GetGlobalVariables, sizeof(CFilterUserGetMessageGlobalVariables));

							//回复kernel层 不需要去掉FILTER_REPLY_HEADER头
							FilterReplyMessage(pThis->m_Port, (FILTER_REPLY_HEADER*)&replyHeader, replyHeader.Header.PackSize);
							break;
						}
					}
					break;
				}
				default:
					break;
				}
			}
		}
	}
	return (PVOID)pThis->m_ThreadsRuning;
}

