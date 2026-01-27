#pragma once
#include "DefineArea.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//最大接口限制																												
#define MAX_FUNCALL_INDEX 0xFF
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//通信接口函数指针																											
typedef VOID(__vectorcall* PCMDFUN)(ULONG64 nCmd, ULONG64 pIndata, ULONG64 pOutData, ULONG64 pRet, ULONG64 pParam);
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//通信接口结构体																												
typedef struct _Cmd
{
	IN ULONG64 m_Cmd;			//命令
	PCMDFUN m_pfn;				//函数指针
}CCmd, * PCCmd;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//全局变量存储 接口函数指针,和索引 (m_Cmd 索引可有可无,不使用,方便阅读)															
EXTERN_C CCmd g_CmdFun[MAX_FUNCALL_INDEX];
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


VOID MyThreadRoutine(PVOID Context);


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	接口 用于与应用层通信时调用
// 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VOID __vectorcall InitData(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumProcessInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumProcessVadInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumProcessThreadInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumProcessHandleInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumProcessModuleInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumDriverInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumFileInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumRegistryInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumGdtInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumIdtInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumSsdtInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumSsdtShadowInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumKernelCallBackInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumMiniFilterCallBackInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumObjectTypeCallBackInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumObjectTypeCallBackExInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumDriverMajorFunctionInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumDpcInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall MyDeleteFile(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall MyReturnSsdtAndSsdtShadow(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall MyFileDeoccupy(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall MyKillProcess(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall MyRWMemory(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall HookSystemServiceTable(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall HookSsdtTable(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumHalTableInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumSystemDeviceInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall SetProcessPortection(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall GetProcessPortection(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall EnumFilterDriverInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall DebugFlagsInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);
VOID __vectorcall MyTest(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam);