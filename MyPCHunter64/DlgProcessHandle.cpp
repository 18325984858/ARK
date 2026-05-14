// DlgProcessHandle.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgProcessHandle.h"
#include "Thread.h"

// DlgProcessHandle 对话框

IMPLEMENT_DYNAMIC(DlgProcessHandle, CDialogEx)

DlgProcessHandle::DlgProcessHandle(CString StrEprocess, CString StrProcessName, CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_PROCESS_HANDLE, pParent)
{
	m_StrEprocess = StrEprocess;
	m_StrProcessName = StrProcessName;
}

DlgProcessHandle::~DlgProcessHandle()
{

}

void DlgProcessHandle::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_PROCESS_HANDLE_LIST, m_CListCtrl);
}


BEGIN_MESSAGE_MAP(DlgProcessHandle, CDialogEx)
	ON_WM_SIZE()
	ON_NOTIFY(NM_RCLICK, ID_PROCESS_HANDLE_LIST, &DlgProcessHandle::OnRclickProcessHandleList)
	ON_COMMAND(ID_PROCESSHANDLE_REFRESH, &DlgProcessHandle::OnProcesshandleRefresh)
END_MESSAGE_MAP()


// DlgProcessHandle 消息处理程序


void DlgProcessHandle::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	RECT rect = { 0 };
	rect.bottom = cy;
	rect.right = cx;
	m_CListCtrl.MoveWindow(&rect, TRUE);
}


BOOL DlgProcessHandle::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(m_StrProcessName);

	m_CListCtrl.InsertColumn(um_Process_Handle_Type, _T("句柄类型"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Process_Handle_Name, _T("句柄名"), LVCFMT_LEFT, 200);
	m_CListCtrl.InsertColumn(um_Process_Handle_Handle, _T("句柄"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Process_Handle_Object, _T("句柄对象"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Process_Handle_Power, _T("权限"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Process_Handle_Index, _T("索引"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Process_Handle_Reference, _T("引用"), LVCFMT_LEFT, 75);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);


	OnProcesshandleRefresh();
	return TRUE;
}


void DlgProcessHandle::OnRclickProcessHandleList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	if (m_CListCtrl.GetItemCount() < 0) return;
	int r = ShowListContextMenu(&m_CListCtrl, this);
	if (r == 0) { if (this->m_ThreadFlags != TRUE) OnProcesshandleRefresh(); }
	else if (r > 0) CopyBufferToClipboard(&m_CListCtrl, r - 1);
}


void DlgProcessHandle::OnProcesshandleRefresh()
{
	m_CListCtrl.DeleteAllItems();
	//DWORD lpThreadId = 0;
	////HANDLE hThread = CreateThread(NULL, NULL, EnumProcessHandleThreadProc,/*变量参数地址*/(LPVOID)this, 0, &lpThreadId);
	//
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserEnumProcessHandleInfo, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//
	//CloseHandle(hThread);

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumProcessHandleInfo, this });

}

void DlgProcessHandle::InsertCtrlListControl(PCProcessHandleInfo pinfo)
{

	if (pinfo == NULL)
	{
		return;
	}
	SIZE_T FreeSize = sizeof(CProcessHandleInfo);
	PCLIST_ENTRY pCurList = &pinfo->List.List;

	do
	{
		if (pCurList == NULL)
		{
			break;
		}

		PCProcessHandleInfo pProcessHandleInfo = (PCProcessHandleInfo)pCurList;
		{
			CString StrBuf;
			int i = m_CListCtrl.GetItemCount();

			m_CListCtrl.InsertItem(i, pProcessHandleInfo->HandleType);

			m_CListCtrl.SetItemText(i, um_Process_Handle_Name, pProcessHandleInfo->HandleName);

			StrBuf.Format(L"%08I64X", pProcessHandleInfo->Handle);
			m_CListCtrl.SetItemText(i, um_Process_Handle_Handle, StrBuf);

			StrBuf.Format(L"%016I64X", pProcessHandleInfo->HandleObject);
			m_CListCtrl.SetItemText(i, um_Process_Handle_Object, StrBuf);

			StrBuf.Format(L"%08I64X", pProcessHandleInfo->Power);
			m_CListCtrl.SetItemText(i, um_Process_Handle_Power, StrBuf);

			StrBuf.Format(L"%08I64X", pProcessHandleInfo->Index);
			m_CListCtrl.SetItemText(i, um_Process_Handle_Index, StrBuf);

			StrBuf.Format(L"%08I64X", pProcessHandleInfo->Quote);
			m_CListCtrl.SetItemText(i, um_Process_Handle_Reference, StrBuf);
		}
		//指向下一个
		pCurList = pCurList->Blink;

		//释放内存
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pProcessHandleInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pinfo->List.List);


}