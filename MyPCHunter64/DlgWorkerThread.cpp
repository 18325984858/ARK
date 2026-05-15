// DlgWorkerThread.cpp: 工作线程队列对话框
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgWorkerThread.h"
#include "Thread.h"
#include "PdbResolver.h"


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
	ON_NOTIFY(NM_RCLICK, ID_DPC_LIST, &DlgWorkerThread::OnNMRClickList)
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
	m_CListCtrl.InsertColumn(um_Worker_FunctionName, _T("函数名称"), LVCFMT_LEFT, 250);
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

		// 函数名：PdbResolver
		{
			WCHAR resolved[256] = { 0 };
			PdbResolver_Resolve(Info->StartAddress, 0, Info->MoudleName, resolved, _countof(resolved));
			m_CListCtrl.SetItemText(i, um_Worker_FunctionName, resolved);
		}

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

// 右键菜单：刷新 + 复制各列。动态构建，避免改 .rc 资源。
void DlgWorkerThread::OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	static const struct { UINT id; LPCWSTR text; int col; } kCopyItems[] = {
		{ 2101, L"序号",     um_Worker_Index },
		{ 2102, L"ETHREAD",  um_Worker_Ethread },
		{ 2103, L"线程ID",   um_Worker_Tid },
		{ 2104, L"优先级",   um_Worker_Priority },
		{ 2105, L"入口地址", um_Worker_StartAddress },
		{ 2106, L"函数名称", um_Worker_FunctionName },
		{ 2107, L"所在模块", um_Worker_Module },
	};
	const UINT kRefreshId = 2100;

	BOOL hasSel = (m_CListCtrl.GetFirstSelectedItemPosition() != NULL);

	CMenu copySub;
	copySub.CreatePopupMenu();
	for (auto& it : kCopyItems)
	{
		copySub.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), it.id, it.text);
	}

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenuW(MF_POPUP | (hasSel ? 0 : MF_GRAYED), (UINT_PTR)copySub.GetSafeHmenu(), L"复制");
	menu.AppendMenuW(MF_STRING, kRefreshId, L"刷新");
	copySub.Detach(); // 所有权已交给 menu，避免双重销毁

	POINT pt = { 0 };
	GetCursorPos(&pt);
	UINT cmd = menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, this);
	if (cmd == kRefreshId)
	{
		OnWorkerThreadRefresh();
		return;
	}
	for (auto& it : kCopyItems)
	{
		if (cmd == it.id)
		{
			CopyBufferToClipboard(&m_CListCtrl, it.col);
			return;
		}
	}
}
