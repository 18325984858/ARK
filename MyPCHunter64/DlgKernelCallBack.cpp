// DlgKernelCallBack.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgKernelCallBack.h"
#include "resource.h"
#include "Thread.h"
#include "PdbResolver.h"
// DlgKernelCallBack 对话框

IMPLEMENT_DYNAMIC(DlgKernelCallBack, CDialogEx)

DlgKernelCallBack::DlgKernelCallBack(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_KERNEL_KERNELCALLBACK, pParent)
{

}

DlgKernelCallBack::~DlgKernelCallBack()
{
}

void DlgKernelCallBack::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_DLG_KERNEL_KERNELCALLBACK_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgKernelCallBack, CDialogEx)
	ON_NOTIFY(NM_RCLICK, ID_DLG_KERNEL_KERNELCALLBACK_LIST, &DlgKernelCallBack::OnNMRClickDlgKernelKernelcallbackList)
	ON_COMMAND(ID_KERNELCALLBACK_REFRESH, &DlgKernelCallBack::OnKernelcallbackRefresh)
	ON_WM_SIZE()
END_MESSAGE_MAP()

// DlgKernelCallBack 消息处理程序

void DlgKernelCallBack::OnNMRClickDlgKernelKernelcallbackList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	int r = ShowListContextMenu(&m_CListCtrl, this);
	if (r == 0) { if (this->m_ThreadFlags != 1) OnKernelcallbackRefresh(); }
	else if (r > 0) CopyBufferToClipboard(&m_CListCtrl, r - 1);
}

void DlgKernelCallBack::OnKernelcallbackRefresh()
{
	m_CListCtrl.DeleteAllItems();

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumKernelCallBackInfo, this });

}

void DlgKernelCallBack::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	CRect rect;
	GetClientRect(&rect);

	m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}

BOOL DlgKernelCallBack::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_KernelCallBack_Type, _T("类型"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_KernelCallBack_Addr, _T("地址"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_KernelCallBack_Pos, _T("位置"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_KernelCallBack_Path, _T("路径"), LVCFMT_LEFT, 250);
	m_CListCtrl.InsertColumn(um_KernelCallBack_Company, _T("公司名"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_KernelCallBack_Descr, _T("备注"), LVCFMT_LEFT, 120);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgKernelCallBack::InsertCtrlListControl(PCKernelCallBackInfo pKernelCallBackInfo)
{
	if (pKernelCallBackInfo == NULL)
	{
		return;
	}

	//类型
	CString CallBackType[] = { L"ShutDown",L"DbgCheck",L"PlugPlay",L"LoadImage",L"CreateProcess",L"CreateThread",L"Registry" ,L"IoTime" };

	PCLIST_ENTRY pCurList = &pKernelCallBackInfo->List.List;

	do
	{
		PCKernelCallBackInfo pInfo = (PCKernelCallBackInfo)pCurList;
		if (pInfo == NULL)
		{
			break;
		}

		int InsertIndex = m_CListCtrl.GetItemCount();

		CString StrBuf;

		m_CListCtrl.InsertItem(InsertIndex, CallBackType[pInfo->CallBackType % (sizeof(CallBackType) / sizeof(CString))]);

		StrBuf.Format(L"%016I64X", pInfo->CallBackAddr);
		m_CListCtrl.SetItemText(InsertIndex, um_KernelCallBack_Addr, StrBuf);

		int n = 0;
		int n1 = 0;

		if (pInfo->ModulePath[0] != L'\0')
		{
			for (int i = 0; i < wcslen(pInfo->ModulePath); i++)
			{

				if (pInfo->ModulePath[i] == L'\\')
				{
					n = i;
				}

				if (pInfo->ModulePath[i] == L'.')
				{
					n1 = i;
				}
			}
		}

		StrBuf.Format(L"%ws+%I64X", &pInfo->ModulePath[n + 1], pInfo->ModuleOffset);
		// “位置”列用 PdbResolver 尝试解符号；PDB 未就绪时仍推出下载并托底为 module+offset
		{
			ULONG64 modBase = (pInfo->CallBackAddr >= pInfo->ModuleOffset) ?
				(pInfo->CallBackAddr - pInfo->ModuleOffset) : 0;
			WCHAR resolved[256] = { 0 };
			PdbResolver_Resolve(pInfo->CallBackAddr, modBase,
				pInfo->ModulePath, resolved, _countof(resolved));
			if (resolved[0]) StrBuf = resolved;
		}
		m_CListCtrl.SetItemText(InsertIndex, um_KernelCallBack_Pos, StrBuf);

		CString Path = PathTransForm(pInfo->ModulePath);
		m_CListCtrl.SetItemText(InsertIndex, um_KernelCallBack_Path, Path.GetBuffer());

		GetCompanyName(Path, StrBuf);
		m_CListCtrl.SetItemText(InsertIndex, um_KernelCallBack_Company, StrBuf);

		StrBuf.Format(L"%016I64X", pInfo->Descr);
		m_CListCtrl.SetItemText(InsertIndex, um_KernelCallBack_Descr, StrBuf);

		pCurList = pCurList->Blink;

		//释放空间
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pKernelCallBackInfo->List.List);
}

