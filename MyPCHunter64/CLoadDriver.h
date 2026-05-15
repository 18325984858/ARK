#pragma once
#include "framework.h"
#include "DlgProcess.h"
#include "DlgEnumFile.h"
#include "DlgEnumRegistry.h"
#include "DlgGdt.h"
#include "DlgProcessVad.h"
#include "DlgProcessThread.h"
#include "DlgProcessHandle.h"
#include "DlgProcessModule.h"
#include "DlgDriverModule.h"
#include "DlgSsdt.h"
#include "DlgSsdtShadow.h"
#include "DlgIdt.h"
#include "DlgKernelCallBack.h"
#include "DlgMiniFilterCallBack.h"
#include "DlgObjectCallBack.h"
#include "DlgDpc.h"
#include "DlgRWMemory.h"
#include "DlgHalTable.h"
#include "DlgWdf.h"
#include "DlgSetting.h"
#include <thread>
#include <list>
#include <mutex>

#define IRP_MJ_MAXIMUM_FUNCTION			(0x1b+1)
#define IRP_MJ_MAXIMUM_FUNCTION_EX		(IRP_MJ_MAXIMUM_FUNCTION+27)

enum UMDriverMajorFunctionType
{
	Acpi = 0,
};

class _DriverMajorFunctionInfo :public _MyCBaseDataObject
{
public:
	_DriverMajorFunctionInfo()
	{
		TypeName[0] = L"IRP_MJ_CREATE";
		TypeName[1] = L"IRP_MJ_CREATE_NAMED_PIPE";
		TypeName[2] = L"IRP_MJ_CLOSE";
		TypeName[3] = L"IRP_MJ_READ";
		TypeName[4] = L"IRP_MJ_WRITE";
		TypeName[5] = L"IRP_MJ_QUERY_INFORMATION";
		TypeName[6] = L"IRP_MJ_SET_INFORMATION";
		TypeName[7] = L"IRP_MJ_QUERY_EA";
		TypeName[8] = L"IRP_MJ_SET_EA";
		TypeName[9] = L"IRP_MJ_FLUSH_BUFFERS";
		TypeName[10] = L"IRP_MJ_QUERY_VOLUME_INFORMATION";
		TypeName[11] = L"IRP_MJ_SET_VOLUME_INFORMATION";
		TypeName[12] = L"IRP_MJ_DIRECTORY_CONTROL";
		TypeName[13] = L"IRP_MJ_FILE_SYSTEM_CONTROL";
		TypeName[14] = L"IRP_MJ_DEVICE_CONTROL";
		TypeName[15] = L"IRP_MJ_INTERNAL_DEVICE_CONTROL";
		TypeName[16] = L"IRP_MJ_SHUTDOWN";
		TypeName[17] = L"IRP_MJ_LOCK_CONTROL";
		TypeName[18] = L"IRP_MJ_CLEANUP";
		TypeName[19] = L"IRP_MJ_CREATE_MAILSLOT";
		TypeName[20] = L"IRP_MJ_QUERY_SECURITY";
		TypeName[21] = L"IRP_MJ_SET_SECURITY";
		TypeName[22] = L"IRP_MJ_POWER";
		TypeName[23] = L"IRP_MJ_SYSTEM_CONTROL";
		TypeName[24] = L"IRP_MJ_DEVICE_CHANGE";
		TypeName[25] = L"IRP_MJ_QUERY_QUOTA";
		TypeName[26] = L"IRP_MJ_SET_QUOTA";
		TypeName[27] = L"IRP_MJ_PNP";

		//快速IO
		TypeName[28] = L"FastIoCheckIfPossible";
		TypeName[29] = L"FastIoRead";
		TypeName[30] = L"FastIoWrite";
		TypeName[31] = L"FastIoQueryBasicInfo";
		TypeName[32] = L"FastIoQueryStandardInfo";
		TypeName[33] = L"FastIoLock";
		TypeName[34] = L"FastIoUnlockSingle";
		TypeName[35] = L"FastIoUnlockAll";
		TypeName[36] = L"FastIoUnlockAllByKey";
		TypeName[37] = L"FastIoDeviceControl";
		TypeName[38] = L"AcquireFileForNtCreateSection";
		TypeName[39] = L"ReleaseFileForNtCreateSection";
		TypeName[40] = L"FastIoDetachDevice";
		TypeName[41] = L"FastIoQueryNetworkOpenInfo";
		TypeName[42] = L"AcquireForModWrite";
		TypeName[43] = L"MdlRead";
		TypeName[44] = L"MdlReadComplete";
		TypeName[45] = L"PrepareMdlWrite";
		TypeName[46] = L"MdlWriteComplete";
		TypeName[47] = L"FastIoReadCompressed";
		TypeName[48] = L"FastIoWriteCompressed";
		TypeName[49] = L"MdlReadCompleteCompressed";
		TypeName[50] = L"MdlWriteCompleteCompressed";
		TypeName[51] = L"FastIoQueryOpen";
		TypeName[52] = L"ReleaseForModWrite";
		TypeName[53] = L"AcquireForCcFlush";
		TypeName[54] = L"ReleaseForCcFlush";
	}
public:
	WCHAR MajorFunctionDriverName[MY_MAX_PATH] = { 0 };					//存放驱动名称
	CString TypeName[IRP_MJ_MAXIMUM_FUNCTION_EX];						//存储IRP类型名
	ULONG64 m_ObjectType;												//对象类型
	ULONG64	m_Object;													//存放对象类型
};

using DriverMajorFunctionInfo = _DriverMajorFunctionInfo;
using PDriverMajorFunctionInfo = _DriverMajorFunctionInfo*;

class _LoadDriver
{
public:
	_LoadDriver();
	~_LoadDriver();

public:
	//设置驱动名称和驱动路径
	//参数一:驱动文件名
	//参数二:驱动全路径
	VOID _LoadDriver::SetFileNameAndPath(PWCHAR Driver_Name, PWCHAR Driver_Path);
	//功能 :停止 卸载 驱动
	BOOL _LoadDriver::UnLoadDriverFun();
	//功能 :注册 加载 驱动
	BOOL _LoadDriver::LoadDriverFun();
public:
	//功能:与驱动建立通信
	//参数一:名称
	ULONG64 _LoadDriver::ConnectDriver(CString Name);

	//功能:向驱动发送信息
	//参数一:要发送的数据缓冲区
	//参数二:要发送的大小
	//参数三:输出缓冲区
	//参数四:返回输出的缓冲区大小
	//参数五:预留参数
	//返回值:返回驱动返回的信息,则返回缓冲区大小
	ULONG64 _LoadDriver::SendMsg(IN ULONG64 dqCmd, IN LPVOID ilpBuffer = NULL, OUT LPVOID* OlpBuffer = NULL, OUT PDWORD nNumberOfBytesToWrite = NULL, IN OUT PVOID Param = NULL);
public:
#define MAX_USER_CALL_BACK_COUNT 0x50											/*存储函数指针最大数量*/

	/*此类型为功能号*/
	enum UserCallBackType
	{
		Um_UserCallBackType_UserEnumProcessInfo,
		Um_UserCallBackType_UserKillProcess,
		Um_UserCallBackType_UserEnumDriverInfo,
		Um_UserCallBackType_UserEnumProcessVadInfo,
		Um_UserCallBackType_UserEnumProcessThreadInfo,
		Um_UserCallBackType_UserEnumProcessHandleInfo,
		Um_UserCallBackType_UserEnumProcessModuleInfo,
		Um_UserCallBackType_UserEnumFileInfo,
		Um_UserCallBackType_UserFileDeoccupy,
		Um_UserCallBackType_UserDelteFileInfo,
		Um_UserCallBackType_UserEnumRegistryInfo,
		Um_UserCallBackType_UserEnumGdtInfo,
		Um_UserCallBackType_UserEnumIdtInfo,
		Um_UserCallBackType_UserEnumSsdtInfo,
		Um_UserCallBackType_UserEnumSsdtShadowInfo,
		Um_UserCallBackType_UserEnumKernelCallBackInfo,
		Um_UserCallBackType_UserEnumMiniFilterCallBackInfo,
		Um_UserCallBackType_UserEnumObjectCallBackInfo,
		Um_UserCallBackType_UserRWMemOryInfo,
		Um_UserCallBackType_UserEnumHalTableInfo,
		Um_UserCallBackType_UserEnumWdfInfo,
		Um_UserCallBackType_UserEnumDpcInfo,
		Um_UserCallBackType_UserEnumDriverMajorFunctionInfo,
		Um_UserCallBackType_UserHookSsdtTable,
		Um_UserCallBackType_UserInsertMonitorDlg,
		Um_UserCallBackType_InitData,
		Um_UserCallBackType_UserEnumFilterDriver,
		Um_UserCallBackType_UserGetDebugFlags,
		Um_UserCallBackType_UserSetDebugFlags,
		Um_UserCallBackType_UserEnumWorkerThreadInfo,										//枚举工作线程队列

		Um_UserCallBackType_Test,
		Um_UserCallBackType_UserEnumKernelHookInfo,										//内核 inline 钩子扫描
		Um_UserCallBackType_NULL,
	};
	typedef ULONG64(__thiscall _LoadDriver::* PUSERCALLBAKC)(PVOID Pragma);					//函数指针
	PUSERCALLBAKC m_pUserCallBackFun[MAX_USER_CALL_BACK_COUNT] = { NULL };					//存储回调指针
public:
	//接口函数
	ULONG64 _LoadDriver::BaseInterfaceFun(ULONG64 Index/*功能号*/, PVOID Pragma/*参数*/);
public:
	//功能:向驱动发送消息 用于初始化数据
	ULONG64 _LoadDriver::InitData(PVOID pInfo);
	//功能:向驱动发消息 枚举进程信息 并写到ListControl中
	ULONG64 _LoadDriver::UserEnumProcessInfo(PVOID pDlgProcessInfo);
	//功能:向驱动发送信息 强制结束进程
	ULONG64 _LoadDriver::UserKillProcess(PVOID pDlgProcessInfo);
	//功能:向驱动发消息 枚举驱动信息 并写到ListControl中
	ULONG64 _LoadDriver::UserEnumDriverInfo(PVOID pDlgDriverInfo);
	//功能:向驱动发送消息 枚举进程Vad内存信息,写入到ListControl中
	ULONG64  _LoadDriver::UserEnumProcessVadInfo(PVOID pDlgProcessVadInfo);
	//功能:向驱动发送消息 枚举进程Thread信息,写入到ListControl中
	ULONG64  _LoadDriver::UserEnumProcessThreadInfo(PVOID pDlgProcessThreadInfo);
	//功能:向驱动发送消息 枚举进程私有句柄表信息,写入到ListControl中
	ULONG64  _LoadDriver::UserEnumProcessHandleInfo(PVOID pDlgProcessHandleInfo);
	//功能:向驱动发送消息 枚举进程模块信息,写入到ListControl中
	ULONG64  _LoadDriver::UserEnumProcessModuleInfo(PVOID pDlgProcessHandleInfo);
	//功能:向驱动发消息 枚举文件信息 并写到ListControl和Tree中
	ULONG64 _LoadDriver::UserEnumFileInfo(PVOID pDlgFileInfo);
	//功能:向驱动发送消息 解除当前选择的文件占用情况
	ULONG64 _LoadDriver::UserFileDeoccupy(PVOID pDlgFileInfo);
	//功能:向驱动发消息 强制删除指定的文件路径
	ULONG64 _LoadDriver::UserDelteFileInfo(PVOID pDlgFileInfo);
	//功能:向驱动发消息 枚举注册表信息 并写到ListControl和Tree中
	ULONG64 _LoadDriver::UserEnumRegistryInfo(PVOID pDlgRegistryInfo);
	//功能:向驱动发消息 枚举GDT表信息 并写入到ListControl中
	ULONG64 _LoadDriver::UserEnumGdtInfo(PVOID pDlgGdtInfo);
	//功能:向驱动发消息 枚举IDT表信息 并写入到ListControl中
	ULONG64 _LoadDriver::UserEnumIdtInfo(PVOID pDlgIdtInfo);
	//功能:向驱动发消息 枚举SSDT表信息 并写入到ListControl中
	ULONG64 _LoadDriver::UserEnumSsdtInfo(PVOID pDlgSsdtInfo);
	//功能:向驱动发消息 枚举SSDTShadow表信息 并写入到ListControl中
	ULONG64 _LoadDriver::UserEnumSsdtShadowInfo(PVOID pDlgSsdtShadowInfo);
	//功能:向驱动发消息 枚举KernelCallBack表信息 并写入到ListControl中
	ULONG64 _LoadDriver::UserEnumKernelCallBackInfo(PVOID pDlgKernelCallBackInfo);
	//功能:向驱动发消息 枚举MiniFilterCallBack表信息 并写入到ListControl中
	ULONG64 _LoadDriver::UserEnumMiniFilterCallBackInfo(PVOID pDlgMiniFilterCallBackInfo);
	//功能:向驱动发消息 枚举对象类型回调信息 并写入到ListControl中
	ULONG64 _LoadDriver::UserEnumObjectCallBackInfo(PVOID pDlgObjectCallBackInfo);
	//功能:向驱动发消息 枚举驱动对象Majorfunction信息 并写入到ListControl中
	ULONG64 _LoadDriver::UserEnumDriverMajorFunctionInfo(PVOID pDriverMajorFunctionInfo);
	//功能:向驱动发消息 枚举Dpc回调信息 并写入到ListControl中
	ULONG64 _LoadDriver::UserEnumDpcInfo(PVOID pDlgDpc);
	//功能:向驱动发送消息 读取指定用户地址的数据,默认0x1000大小一个页
	ULONG64 _LoadDriver::UserRWMemOryInfo(PVOID pDlgRWMemory);
	//功能:向驱动发送消息 读取对应Hal表的数据
	ULONG64 _LoadDriver::UserEnumHalTableInfo(PVOID pDlgHalTable);
	//功能:向驱动发送消息 读取对应WDf驱动的数据
	ULONG64 _LoadDriver::UserEnumWdfInfo(PVOID pDlgWdf);
	//功能:向驱动发送消息 HookSsdt表中的函数
	ULONG64 _LoadDriver::UserHookSsdtTable(PVOID pHookInfo);
	//功能:向窗口插入数据
	ULONG64 _LoadDriver::UserInsertMonitorDlg(PVOID pInfo);
	//功能:枚举过滤驱动
	ULONG64 _LoadDriver::UserEnumFilterDriver(PVOID pInfo);
	//功能:调试标志
	ULONG64 _LoadDriver::UserDebugFlags(PVOID pInfo);
	ULONG64 _LoadDriver::UserDebugFlags1(PVOID pInfo);
	//功能:向驱动发消息 枚举内核工作线程队列 并写入到ListControl中
	ULONG64 _LoadDriver::UserEnumWorkerThreadInfo(PVOID pDlgWorkerThread);

	//功能:内核 inline 钩子扫描，在后台线程跑，扫完后回 UI 插入到 ListControl
	ULONG64 _LoadDriver::UserEnumKernelHookInfo(PVOID pDlgKernelHookList);



	//功能:测试功能结口
	ULONG64 _LoadDriver::UserTestFun(PVOID pInfo);

public:
	HANDLE m_Port;																								//存储通信的端口
	WCHAR DriverName[MAX_PATH];																					//存储驱动名字
	WCHAR DriverFullNamePath[MAX_PATH];																			//存储驱动路径
public:
	//功能创建线程接收
	ULONG64	CreateGetMessageThread(ULONG64 ThreadNumbers = std::thread::hardware_concurrency()/*获取CPU线程数*/);
	//根据包类型申请包空间 使用浅拷贝
	PVOID64 AllocPack(PCFilterUserGetMessageHeadInfo pinfo);
	//接收0环发送的数据
	static PVOID _LoadDriver::WorkThread(_LoadDriver* pThis, uint32_t nIndex);									//工作线程
public:
	ULONG64										m_ThreadsRuning;												//线程运行标志
	HANDLE										m_Completion;													//存储完成端口
	std::list<std::thread*>						m_Threads;														//存储线程
	std::list<PCFilterGetMessageHeadInfo>		m_FilterMessageQueue;											//任务队列
	std::mutex									m_TaskLock;														//同步队列
};

using LoadDriver = _LoadDriver;
using PLoadDriver = _LoadDriver*;

EXTERN_C LoadDriver g_LoadDriver;