#include "Thread.h"
#include "CLoadDriver.h"
#include "MyPCHunter64.h"
#include <unordered_set>
#include <mutex>
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
		LOGW("[task] cb=%llu this=%p REJECTED (Paragma==NULL or CallNumber out of range)",
			(unsigned long long)CallNumber, Paragma);
		return NULL;
	}

	// 防重入：用户快速重复点击刷新时，多个 worker 线程会并发对同一个 dialog
	// 调用 CListCtrl 接口（跨线程 SendMessage），UI 线程消息泵被淹没看起来就是"卡死"。
	// 注意：dialog 大多是 `public CDialogEx, public CFunction` 多重继承，
	// `(_MyCBaseDataObject*)Paragma` C 风格强转不会做基类偏移调整，
	// 直接读写 pInfo->m_ThreadFlags 会落到 CDialogEx 内部字段上 → 引发死锁/崩溃。
	// 所以这里用全局集合按 Paragma 指针看门，完全不去解引用它。
	static std::unordered_set<void*> s_busy;
	static std::mutex s_busyMtx;

	{
		std::lock_guard<std::mutex> lk(s_busyMtx);
		if (s_busy.count(Paragma))
		{
			LOGW("[task] cb=%llu this=%p SKIPPED (already busy in s_busy)",
				(unsigned long long)CallNumber, Paragma);
			return this;
		}
		s_busy.insert(Paragma);
	}

	struct Guard {
		void* p;
		std::mutex& m;
		std::unordered_set<void*>& s;
		~Guard() { std::lock_guard<std::mutex> lk(m); s.erase(p); }
	} guard{ Paragma, s_busyMtx, s_busy };

	DWORD t0 = GetTickCount();
	LOGI("[task] cb=%llu this=%p start", (unsigned long long)CallNumber, Paragma);
	g_LoadDriver.BaseInterfaceFun(CallNumber, (_MyCBaseDataObject*)Paragma);
	LOGI("[task] cb=%llu this=%p done in %lums",
		(unsigned long long)CallNumber, Paragma, GetTickCount() - t0);

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
