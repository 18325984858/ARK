// DlgAcpi.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgMajorfunction.h"
#include "resource.h"
#include "Thread.h"
#include "PdbResolver.h"

// DlgAcpi 对话框

IMPLEMENT_DYNAMIC(DlgMajorfunction, CDialogEx)

DlgMajorfunction::DlgMajorfunction(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_MAJORFUNCTION, pParent)
{

}

DlgMajorfunction::~DlgMajorfunction()
{
}

void DlgMajorfunction::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_MAJORFUNCTION_LIST, m_CListCtrl);
}


BEGIN_MESSAGE_MAP(DlgMajorfunction, CDialogEx)
	ON_NOTIFY(LVN_ITEMCHANGED, ID_MAJORFUNCTION_LIST, &DlgMajorfunction::OnLvnItemchangedMajorFunctioniList)
	ON_WM_SIZE()
	ON_COMMAND(ID_DRIVER_MAJORFUNCTION_REFRESH, &DlgMajorfunction::OnDriverMajorFunctionRefresh)
	ON_NOTIFY(NM_RCLICK, ID_MAJORFUNCTION_LIST, &DlgMajorfunction::OnNMRClickMajorfunctionList)
END_MESSAGE_MAP()


// DlgAcpi 消息处理程序


void DlgMajorfunction::OnLvnItemchangedMajorFunctioniList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
}


void DlgMajorfunction::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	CRect rect;
	GetClientRect(&rect);

	m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}


BOOL DlgMajorfunction::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_MajorFuction_Ord, _T("序号"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_MajorFuction_FunName, _T("函数名称"), LVCFMT_LEFT, 300);
	m_CListCtrl.InsertColumn(um_MajorFuction_FunAddr, _T("函数地址"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_MajorFuction_Pos, _T("位置"), LVCFMT_LEFT, 250);
	m_CListCtrl.InsertColumn(um_MajorFuction_MoudlePath, _T("所在模块路径"), LVCFMT_LEFT, 250);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);


	m_DriverMajorFunctionInfo.m_Object = (ULONG64)this;
	m_DriverMajorFunctionInfo.m_ThreadFlags = FALSE;

	//OnDriverMajorFunctionRefresh();
	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}


void DlgMajorfunction::OnDriverMajorFunctionRefresh()
{
	m_CListCtrl.DeleteAllItems();
	//DWORD lpThreadId = 0;
	////HANDLE hThread = CreateThread(NULL, NULL, EnumDriverMajorFunctionThreadProc,/*变量参数地址*/(LPVOID)&m_DriverMajorFunctionInfo, 0, &lpThreadId);
	//
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserEnumDriverMajorFunctionInfo, &m_DriverMajorFunctionInfo };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//
	//CloseHandle(hThread);

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumDriverMajorFunctionInfo, &m_DriverMajorFunctionInfo });

}


void DlgMajorfunction::InsertCtrlListControl(PCSysMajorFunctionInfo pCSysMajorFunctionInfo)
{
	if (pCSysMajorFunctionInfo == NULL)
	{
		return;
	}

	PCLIST_ENTRY pCurList = &pCSysMajorFunctionInfo->List.List;
	do
	{
		if (pCurList == NULL)
		{
			break;
		}
		ULONG64 i = m_CListCtrl.GetItemCount();
		CString StrBuf;
		PCSysMajorFunctionInfo pInfo = (PCSysMajorFunctionInfo)pCurList;

		StrBuf.Format(L"%X", pInfo->Ord);
		m_CListCtrl.InsertItem(i, StrBuf);

		m_CListCtrl.SetItemText(i, um_MajorFuction_FunName, m_DriverMajorFunctionInfo.TypeName[pInfo->Type % IRP_MJ_MAXIMUM_FUNCTION]);

		StrBuf.Format(L"%016I64X", pInfo->FunAddr);
		m_CListCtrl.SetItemText(i, um_MajorFuction_FunAddr, StrBuf);

		// 位置列：PdbResolver 解析符号。优先用驱动传回的 ModuleBase，没则 resolver 查内核模块表
		{
			WCHAR resolved[256] = { 0 };
			PdbResolver_Resolve(pInfo->FunAddr, pInfo->ModuleBase,
				pInfo->ModulePath, resolved, _countof(resolved));
			m_CListCtrl.SetItemText(i, um_MajorFuction_Pos, resolved);
		}


		m_CListCtrl.SetItemText(i, um_MajorFuction_MoudlePath, pInfo->ModulePath);

		CString FilePath = PathTransForm(pInfo->ModulePath);
		m_CListCtrl.SetItemText(i, um_MajorFuction_MoudlePath, FilePath.GetBuffer());


		pCurList = pCurList->Blink;
		//清理资源
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pCSysMajorFunctionInfo->List.List);

}

void DlgMajorfunction::OnNMRClickMajorfunctionList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	int r = ShowListContextMenu(&m_CListCtrl, this);
	if (r == 0) { if (m_DriverMajorFunctionInfo.m_ThreadFlags != TRUE) OnDriverMajorFunctionRefresh(); }
	else if (r > 0) CopyBufferToClipboard(&m_CListCtrl, r - 1);
}
