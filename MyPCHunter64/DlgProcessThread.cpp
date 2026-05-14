// DlgProcessThread.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgProcessThread.h"
#include "Thread.h"

// DlgProcessThread 对话框

IMPLEMENT_DYNAMIC(DlgProcessThread, CDialogEx)

DlgProcessThread::DlgProcessThread(CString StrEprocess, CString StrProcessName, CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_PROCESS_THREAD, pParent)
{
	m_StrEprocess = StrEprocess;
	m_StrProcessName = StrProcessName;
}

DlgProcessThread::~DlgProcessThread()
{
}

void DlgProcessThread::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_PROCESS_THREAD_LIST, m_CListCtrl);
}


BEGIN_MESSAGE_MAP(DlgProcessThread, CDialogEx)
	ON_NOTIFY(NM_RCLICK, ID_PROCESS_THREAD_LIST, &DlgProcessThread::OnRclickProcessThreadList)
	ON_WM_SIZE()
	ON_COMMAND(ID_PROCESSTHREAD_REFRESH, &DlgProcessThread::OnProcessthreadRefresh)
END_MESSAGE_MAP()


// DlgProcessThread 消息处理程序


void DlgProcessThread::OnRclickProcessThreadList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	int r = ShowListContextMenu(&m_CListCtrl, this);
	if (r == 0) { if (this->m_ThreadFlags != 1) OnProcessthreadRefresh(); }
	else if (r > 0) CopyBufferToClipboard(&m_CListCtrl, r - 1);
}


void DlgProcessThread::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	RECT rect = { 0 };
	rect.bottom = cy;
	rect.right = cx;
	m_CListCtrl.MoveWindow(&rect, TRUE);
}


BOOL DlgProcessThread::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(m_StrProcessName);

	CDialogEx::OnInitDialog();
	m_CListCtrl.InsertColumn(um_Thread_Module, _T("模块"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Thread_Id, _T("线程ID"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Thread_Object, _T("线程对象"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Thread_Teb, _T("TEB"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Thread_Type, _T("线程类型"), LVCFMT_LEFT, 80);
	m_CListCtrl.InsertColumn(um_Thread_StartAddr, _T("入口地址"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Thread_Priority, _T("优先级"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Thread_SwitchCount, _T("切换次数"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Thread_State, _T("状态"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Thread_CreateTime, _T("创建时间"), LVCFMT_LEFT, 350);
	m_CListCtrl.InsertColumn(um_Thread_CompanyName, _T("公司名"), LVCFMT_LEFT, 100);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	OnProcessthreadRefresh();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgProcessThread::OnProcessthreadRefresh()
{
	m_CListCtrl.DeleteAllItems();
	//DWORD lpThreadId = 0;
	////HANDLE hThread = CreateThread(NULL, NULL, EnumProcessThreadThreadProc,/*变量参数地址*/(LPVOID)this, 0, &lpThreadId);
	//
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserEnumProcessThreadInfo, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//CloseHandle(hThread);

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumProcessThreadInfo, this });
}

void DlgProcessThread::InsertCtrlListControl(PCProcessThreadInfo pInfo)
{
	if (pInfo == NULL)
	{
		return;
	}

	CString ThreadState[9] = { L"创建",L"就绪",L"运行",L"待命",L"结束",L"等待",L"过度",L"延迟",L"门等待" };//存储线程状态


	PCLIST_ENTRY pList = &pInfo->List.List;

	do
	{
		if (pList == NULL)
		{
			return;
		}

		PCProcessThreadInfo pProcessThreadInfo = (PCProcessThreadInfo)pList;
		{
			int i = m_CListCtrl.GetItemCount();

			CString szSrcFilePatch = pProcessThreadInfo->MoudleName;
			ULONG64 nFilePos = szSrcFilePatch.ReverseFind(L'\\');
			CString szMoudleName = szSrcFilePatch.Right(szSrcFilePatch.GetLength() - nFilePos - 1);
			m_CListCtrl.InsertItem(i, szMoudleName.GetString());

			CString StrBuf;

			StrBuf.Format(L"%d", pProcessThreadInfo->UniqueThread);
			m_CListCtrl.SetItemText(i, um_Thread_Id, StrBuf);

			StrBuf.Format(L"%016I64X", pProcessThreadInfo->Ethread);
			m_CListCtrl.SetItemText(i, um_Thread_Object, StrBuf);

			StrBuf.Format(L"%016I64X", pProcessThreadInfo->Teb);
			m_CListCtrl.SetItemText(i, um_Thread_Teb, StrBuf);

			UCHAR ThreadGuiFlag = pProcessThreadInfo->ThreadTypeFlag;

			if (ThreadGuiFlag == 0)
			{
				StrBuf = L"正常线程";
			}
			else
			{
				if (ThreadGuiFlag & 0x1)
				{
					StrBuf = L"GUI线程";
				}
				if (ThreadGuiFlag & 0x2)
				{
					StrBuf = L"受限GUI线程";
				}

			}
			m_CListCtrl.SetItemText(i, um_Thread_Type, StrBuf);


			StrBuf.Format(L"%016I64X", pProcessThreadInfo->StartAddress);
			m_CListCtrl.SetItemText(i, um_Thread_StartAddr, StrBuf);

			StrBuf.Format(L"%d", pProcessThreadInfo->Priority);
			m_CListCtrl.SetItemText(i, um_Thread_Priority, StrBuf);

			StrBuf.Format(L"%d", pProcessThreadInfo->ContextSwitches);
			m_CListCtrl.SetItemText(i, um_Thread_SwitchCount, StrBuf);

			m_CListCtrl.SetItemText(i, um_Thread_State, ThreadState[pProcessThreadInfo->State].GetBuffer());

			StrBuf.Format(L"%d/%d/%d--%d:%d:%d:%d",
				pProcessThreadInfo->CreateTime.Year,
				pProcessThreadInfo->CreateTime.Month,
				pProcessThreadInfo->CreateTime.Day,
				pProcessThreadInfo->CreateTime.Hour,
				pProcessThreadInfo->CreateTime.Minute,
				pProcessThreadInfo->CreateTime.Second,
				pProcessThreadInfo->CreateTime.Milliseconds);


			m_CListCtrl.SetItemText(i, um_Thread_CreateTime, StrBuf);

			CString szDstFilePatch;
			this->GetCompanyName(szSrcFilePatch, szDstFilePatch);
			m_CListCtrl.SetItemText(i, um_Thread_CompanyName, szDstFilePatch.GetString());
		}
		//获取下一个节点
		pList = pList->Blink;
		//释放当前空间
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pProcessThreadInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}

	} while (pList != &pInfo->List.List);
}