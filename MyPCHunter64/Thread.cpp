#include "Thread.h"
#include "CLoadDriver.h"
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
#include "DlgHalTable.h"
#include "DlgWdf.h"

//全局互斥体 
//
// 功能:用于同步应用层进内核的操作
// 
HANDLE g_hThreadMutex = CreateMutex(NULL, FALSE, NULL);

#define MUTEX_WAIT_TIME (-1)/*((64*100)*10)*/

DWORD WINAPI UniversalThreadFunction(PVOID lpThreadParameter)
{
	//先验证参数
	CThreadInfo* pThreadInfo = (CThreadInfo*)lpThreadParameter;
	//判断数据 参数 调用号是否有效
	if (pThreadInfo == NULL || pThreadInfo->Paragma == NULL || pThreadInfo->CallNumber >= MAX_USER_CALL_BACK_COUNT)
	{
		return FALSE;
	}

	//同步线程
	WaitForSingleObject(g_hThreadMutex, MUTEX_WAIT_TIME);

	//__debugbreak();

	_MyCBaseDataObject* pInfo = (_MyCBaseDataObject*)pThreadInfo->Paragma;
	pInfo->m_ThreadFlags = TRUE;

	//调用接口
	g_LoadDriver.BaseInterfaceFun(pThreadInfo->CallNumber, pInfo);

	pInfo->m_ThreadFlags = FALSE;

	//释放调传进来的参数
	delete lpThreadParameter;

	return ReleaseMutex(g_hThreadMutex) /*重置信号*/;
}


LPVOID _CThreadPack::DoTask()
{
	//判断数据 参数 调用号是否有效
	if (Paragma == NULL ||
		CallNumber >= MAX_USER_CALL_BACK_COUNT ||
		CallNumber == _LoadDriver::UserCallBackType::Um_UserCallBackType_NULL)
	{
		return NULL;
	}

	_MyCBaseDataObject* pInfo = (_MyCBaseDataObject*)Paragma;
	pInfo->m_ThreadFlags = TRUE;

	//调用接口
	g_LoadDriver.BaseInterfaceFun(CallNumber, pInfo);

	pInfo->m_ThreadFlags = FALSE;

	//释放调传进来的参数
	return this;
}

/*
LPVOID _CThreadPack::DoTask()
{
	//先验证参数
	CThreadInfo* pThreadInfo = (CThreadInfo*)Paragma;
	//判断数据 参数 调用号是否有效
	if (pThreadInfo == NULL ||
		pThreadInfo->Paragma == NULL ||
		pThreadInfo->CallNumber >= MAX_USER_CALL_BACK_COUNT ||
		pThreadInfo->CallNumber == _LoadDriver::UserCallBackType::Um_UserCallBackType_NULL)
	{
		return NULL;
	}

	//同步线程
	WaitForSingleObject(g_hThreadMutex, MUTEX_WAIT_TIME);

	//__debugbreak();

	_MyCBaseDataObject* pInfo = (_MyCBaseDataObject*)pThreadInfo->Paragma;
	pInfo->m_ThreadFlags = TRUE;

	//调用接口
	g_LoadDriver.BaseInterfaceFun(pThreadInfo->CallNumber, pInfo);

	pInfo->m_ThreadFlags = FALSE;

	//释放调传进来的参数
	//delete lpThreadParameter;
	ReleaseMutex(g_hThreadMutex);
	return this;
}
*/


_CThreadPack::_CThreadPack()
{
	CallNumber = _LoadDriver::UserCallBackType::Um_UserCallBackType_NULL;
	Paragma = NULL;
}

_CThreadPack::_CThreadPack(_LoadDriver::UserCallBackType dwCallNumber, PVOID pParagma)
{
	CallNumber = dwCallNumber;
	Paragma = pParagma;
}

_CThreadPack::~_CThreadPack()
{

}
