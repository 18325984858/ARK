#include "CThreadPool.h"


CThreadPool::CThreadPool(uint32_t nMaxThreadCount) :m_ActiveThreads(0)
{
	if (nMaxThreadCount == 0)
	{
		return;
	}

	m_ThreadsRuning = TRUE;
	for (int i = 0; i < nMaxThreadCount; i++)
	{
		//创建线程对象
		auto pThread = new std::thread(&CThreadPool::WorkThread, this, i);

		//插入到线程链表中
		m_Threads.push_back(pThread);
	}
}

CThreadPool::~CThreadPool()
{
	//等待线程退出
	Wait();
}

PVOID CThreadPool::WorkThread(CThreadPool* pThis, uint32_t nIndex)
{
	CTask* pTask = nullptr;

	while (TRUE)
	{
		{
			std::unique_lock<std::mutex> Lock(pThis->m_TaskLock);		//任务锁

			//条件锁,等待任务信号,且任务队列不为空和线程运行状态为真,否则线程挂起
			pThis->m_CountLock.wait(Lock, [&] {return !pThis->m_TaskQueue.empty() || !pThis->m_ThreadsRuning; });

			if (!pThis->m_ThreadsRuning)
			{
				break;
			}

			/*获取任务*/
			if (!pThis->m_TaskQueue.empty())
			{
				pTask = pThis->m_TaskQueue.front();
				pThis->m_TaskQueue.pop_front();
			}
		}

		//正在运行的线程数量
		pThis->m_ActiveThreads++;

		if (pTask != NULL)
		{
			//执行任务
			pTask->DoTask();

			//删除资源
			delete pTask;

			pTask = NULL;
		}

		pThis->m_ActiveThreads--;

	}

	//投递任务信号
	pThis->m_CountLock.notify_one();

	return (LPVOID)TRUE;
}

int32_t CThreadPool::AddTask(CTask* pTask)
{

	if (pTask == nullptr)
	{
		return -1;
	}

	{
		std::unique_lock<std::mutex> Lock(m_TaskLock);
		/*添加任务*/
		m_TaskQueue.push_back(pTask);

		m_CountLock.notify_one();	//投递任务信号
	}

	return TRUE;
}

int32_t CThreadPool::Wait()
{
	//线程状态设置为假
	m_ThreadsRuning = FALSE;
	//投递信号
	m_CountLock.notify_all();


	//等待线程退出
	for (auto lst : m_Threads)
	{
		lst->join();
	}

	//释放所有任务
	for (auto lst : m_TaskQueue)
	{
		lst->DoTask();
		delete lst;
	}
	return TRUE;
}

