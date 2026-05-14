// DlgWorkerThread.cpp: 工作线程队列对话框
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgWorkerThread.h"
#include "Thread.h"


IMPLEMENT_DYNAMIC(DlgWorkerThread, CDialogEx)

DlgWorkerThread::DlgWorkerThread(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_DPC, pParent)
{
}

DlgWorkerThread::~DlgWorkerThread()
{
}

void DlgWorkerThread::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_DPC_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgWorkerThread, CDialogEx)
	ON_WM_SIZE()
END_MESSAGE_MAP()


void DlgWorkerThread::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	CRect rect;
	GetClientRect(&rect);
	m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}

BOOL DlgWorkerThread::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_Worker_Index, _T("序号"), LVCFMT_LEFT, 60);
	m_CListCtrl.InsertColumn(um_Worker_Ethread, _T("ETHREAD"), LVCFMT_LEFT, 140);
	m_CListCtrl.InsertColumn(um_Worker_Tid, _T("线程ID"), LVCFMT_LEFT, 80);
	m_CListCtrl.InsertColumn(um_Worker_Priority, _T("优先级"), LVCFMT_LEFT, 60);
	m_CListCtrl.InsertColumn(um_Worker_StartAddress, _T("入口地址"), LVCFMT_LEFT, 140);
	m_CListCtrl.InsertColumn(um_Worker_Module, _T("所在模块"), LVCFMT_LEFT, 260);

	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;
}

void DlgWorkerThread::OnWorkerThreadRefresh()
{
	m_CListCtrl.DeleteAllItems();
	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumWorkerThreadInfo, this });
}

void DlgWorkerThread::InsertCtrlListControl(PCProcessThreadInfo pInfo)
{
	if (pInfo == NULL)
	{
		return;
	}

	PCLIST_ENTRY pCurList = &pInfo->List.List;
	do
	{
		if (pCurList == NULL)
		{
			break;
		}

		PCProcessThreadInfo Info = (PCProcessThreadInfo)pCurList;
		int i = m_CListCtrl.GetItemCount();
		CString StrBuf;

		StrBuf.Format(L"%d", i + 1);
		m_CListCtrl.InsertItem(i, StrBuf);

		StrBuf.Format(L"%016I64X", Info->Ethread);
		m_CListCtrl.SetItemText(i, um_Worker_Ethread, StrBuf);

		StrBuf.Format(L"%I64u", Info->UniqueThread);
		m_CListCtrl.SetItemText(i, um_Worker_Tid, StrBuf);

		StrBuf.Format(L"%d", (int)Info->Priority);
		m_CListCtrl.SetItemText(i, um_Worker_Priority, StrBuf);

		StrBuf.Format(L"%016I64X", Info->StartAddress);
		m_CListCtrl.SetItemText(i, um_Worker_StartAddress, StrBuf);

		CString FilePath = PathTransForm(Info->MoudleName);
		m_CListCtrl.SetItemText(i, um_Worker_Module, FilePath.GetBuffer());

		pCurList = pCurList->Blink;

		// 释放在 R3 进程地址空间分配的节点
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&Info, &FreeSize, MEM_RELEASE) != 0)
		{
			// 释放失败保持沉默：列表本身已经填好，避免连环弹框
		}

	} while (pCurList != &pInfo->List.List);
}
