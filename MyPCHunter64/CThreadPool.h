#pragma once
#include <list>
#include <mutex>
#include <thread>
#include <Windows.h>
#include <condition_variable>  //条件变量 


class CTask
{
public:
	virtual LPVOID DoTask() = 0;
	virtual ~CTask() {}
};

class CThreadPool
{
public:
	CThreadPool(uint32_t nMaxThreadCount = std::thread::hardware_concurrency()	/*获取当前CPU核数*/);
	virtual ~CThreadPool();
public:
	static PVOID WorkThread(CThreadPool* pThis, uint32_t nIndex);				/*工作线程*/
	int32_t AddTask(CTask* pTask);												/*添加任务*/
	int32_t Wait();																/*等待线程*/
private:
	uint32_t					m_ThreadsRuning;								//线程运行状态
	std::atomic_int32_t			m_ActiveThreads;								//正在运行的线程数
	std::mutex					m_TaskLock;										//同步任务
	std::condition_variable		m_CountLock;									//通知任务
	std::list<CTask*>			m_TaskQueue;									//任务队列
	std::list<std::thread*>		m_Threads;										//存储线程
};

EXTERN_C CThreadPool g_ThreadPool;