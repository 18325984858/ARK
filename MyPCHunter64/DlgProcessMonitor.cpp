// DlgProcessMonitor.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgProcessMonitor.h"
#include "../MyDriver64/Struct.h"

// DlgProcessMonitor 对话框

IMPLEMENT_DYNAMIC(DlgProcessMonitor, CDialogEx)

DlgProcessMonitor::DlgProcessMonitor(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DIALOG_SSDT_MONITOR, pParent)
{

}

DlgProcessMonitor::~DlgProcessMonitor()
{
}

void DlgProcessMonitor::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_LIST_MONITOR, m_CListCtrlMonitor);
	DDX_Control(pDX, ID_LIST_API, m_CListCtrlApi);
}


BEGIN_MESSAGE_MAP(DlgProcessMonitor, CDialogEx)
	ON_WM_SIZE()
END_MESSAGE_MAP()


// DlgProcessMonitor 消息处理程序
#define REFRESH_MONITOR_CONTROL_TIMER_ID  0x1

BOOL DlgProcessMonitor::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  在此添加额外的初始化

	m_CListCtrlApi.InsertColumn(um_ApiDlg_FunName, _T("函数名称"), LVCFMT_LEFT, 130);
	m_CListCtrlApi.InsertColumn(um_ApiDlg_Level, _T("层级"), LVCFMT_LEFT, 50);
	m_CListCtrlApi.InsertColumn(um_ApiDlg_State, _T("状态"), LVCFMT_LEFT, 50);
	m_CListCtrlApi.SetExtendedStyle(m_CListCtrlApi.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	m_CListCtrlMonitor.InsertColumn(um_ApiMonitorDlg_ProcessName, _T("进程名"), LVCFMT_LEFT, 130);
	m_CListCtrlMonitor.InsertColumn(um_ApiMonitorDlg_PID, _T("进程ID"), LVCFMT_LEFT, 50);
	m_CListCtrlMonitor.InsertColumn(um_ApiMonitorDlg_TID, _T("线程ID"), LVCFMT_LEFT, 50);
	m_CListCtrlMonitor.InsertColumn(um_ApiMonitorDlg_ApiName, _T("函数名称"), LVCFMT_LEFT, 130);
	m_CListCtrlMonitor.InsertColumn(um_ApiMonitorDlg_Parameter, _T("参数"), LVCFMT_LEFT, 200);
	m_CListCtrlMonitor.InsertColumn(um_ApiMonitorDlg_RetNumber, _T("返回值"), LVCFMT_LEFT, 80);
	m_CListCtrlMonitor.SetExtendedStyle(m_CListCtrlMonitor.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}


BOOL DlgProcessMonitor::DestroyWindow()
{
	// TODO: 在此添加专用代码和/或调用基类

	return CDialogEx::DestroyWindow();
}


void DlgProcessMonitor::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialogEx::OnOK();
}


void DlgProcessMonitor::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	// TODO: 在此处添加消息处理程序代码

	CDialogEx::OnSize(nType, cx, cy);
	CRect rect;
	GetClientRect(&rect);

	float fwidth = rect.Width() / 4;

	m_CListCtrlApi.SetWindowPos(NULL, 0, 0, fwidth, rect.Height(), SWP_NOZORDER);
	m_CListCtrlMonitor.SetWindowPos(NULL, fwidth, 0, rect.Width() - fwidth, rect.Height(), SWP_NOZORDER);
}

BOOL DlgProcessMonitor::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	// TODO: 在此添加专用代码和/或调用基类

	return CDialogEx::OnNotify(wParam, lParam, pResult);
}


LRESULT DlgProcessMonitor::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	// TODO: 在此添加专用代码和/或调用基类

	switch (message)
	{
	case wm_User_DlgProcessMonitor_Insert:

		//插入数据
		InsertHookData((PCSSDTHookInfo)wParam);

		break;

	case wm_User_DlgProcessMonitor_InsertApi:

		InsertApi((PCHookSsdtInfo)wParam);

		break;

	case wm_User_DlgProcessMonitor_AlterApi:

		AlterApi((PCAlterHookSsdtInfo)wParam);

		break;

	default:
		break;
	}
	return CDialogEx::WindowProc(message, wParam, lParam);
}

VOID DlgProcessMonitor::InsertHookData(PCSSDTHookInfo pSsdtHookInfo)
{
	if (!pSsdtHookInfo)
	{
		return;
	}

	//获取控件的数量
	ULONG Index = m_CListCtrlMonitor.GetItemCount();
	if (Index < 0)
	{
		return;
	}

	CString StrBuf;

	//m_CListCtrlMonitor.InsertItem(Index, pSsdtHookInfo->ProcessName);


	StrBuf.Format(L"%08X", pSsdtHookInfo->PID);
	m_CListCtrlMonitor.SetItemText(Index, um_ApiMonitorDlg_PID, StrBuf);

	StrBuf.Format(L"%08X", pSsdtHookInfo->TID);
	m_CListCtrlMonitor.SetItemText(Index, um_ApiMonitorDlg_TID, StrBuf);

	m_CListCtrlMonitor.SetItemText(Index, um_ApiMonitorDlg_ApiName, pSsdtHookInfo->FunName);

	StrBuf.Format(L"%08X", pSsdtHookInfo->RetValue);
	m_CListCtrlMonitor.SetItemText(Index, um_ApiMonitorDlg_RetNumber, StrBuf);

	CString Pragma;
	for (int i = 0; i < pSsdtHookInfo->ParagmaNumber; i++)
	{
		CString TmpStr;
		switch (pSsdtHookInfo->Paragma[i].Type)
		{
		case Pragma_Type_Char:
			TmpStr.Format(L"参数%d:%02X  ", i, pSsdtHookInfo->Paragma[i].pPragma);
			break;
		case Pragma_Type_Short:
			TmpStr.Format(L"参数%d:%04X  ", i, pSsdtHookInfo->Paragma[i].pPragma);
			break;
		case Pragma_Type_Int:
			TmpStr.Format(L"参数%d:%d  ", i, pSsdtHookInfo->Paragma[i].pPragma);
			break;
		case Pragma_Type_Dword:
			TmpStr.Format(L"参数%d:%08X  ", i, pSsdtHookInfo->Paragma[i].pPragma);
			break;
		case Pragma_Type_Addr:
			TmpStr.Format(L"参数%d:%p  ", i, pSsdtHookInfo->Paragma[i].pPragma);
			break;
		case Pragma_Type_String:
			TmpStr.Format(L"参数%d:%s  ", i, pSsdtHookInfo->Paragma[i].pPragma);
			break;
		case Pragma_Type_WString:
			TmpStr.Format(L"参数%d:%ws  ", i, pSsdtHookInfo->Paragma[i].pPragma);
			break;
		case Pragma_Type_Float:
			TmpStr.Format(L"参数%d:%f  ", i, pSsdtHookInfo->Paragma[i].pPragma);
			break;
		case Pragma_Type_Double:
			TmpStr.Format(L"参数%d:%llf  ", i, pSsdtHookInfo->Paragma[i].pPragma);
			break;
		default:
			break;
		}
		Pragma += TmpStr;
	}

	m_CListCtrlMonitor.SetItemText(Index, um_ApiMonitorDlg_Parameter, Pragma);


	//释放资源
	delete pSsdtHookInfo;
}

VOID DlgProcessMonitor::InsertApi(PCHookSsdtInfo pInfo)
{
	if (!pInfo)
	{
		return;
	}

	//获取数量
	ULONG Index = m_CListCtrlApi.GetItemCount();
	if (Index < 0)
	{
		return;
	}


	m_CListCtrlApi.InsertItem(Index, pInfo->FunName);

	CString StrBuf;

	StrBuf.Format(L"X:%04X Y:%04X", MYHIWORD(pInfo->Level), MYLOWORD(pInfo->Level));
	m_CListCtrlApi.SetItemText(Index, um_ApiDlg_Level, StrBuf);

	m_CListCtrlApi.SetItemText(Index, um_ApiDlg_State, pInfo->State ? L"Enable" : L"Disable");

	delete pInfo;
	return;
}

VOID DlgProcessMonitor::AlterApi(PCAlterHookSsdtInfo pInfo)
{
	if (!pInfo)
	{
		return;
	}

	CString StrBuf;

	StrBuf.Format(L"X:%04X Y:%04X", MYHIWORD(pInfo->BaseSsdtInfo.Level), MYLOWORD(pInfo->BaseSsdtInfo.Level));
	m_CListCtrlApi.SetItemText(pInfo->Index, um_ApiDlg_Level, StrBuf);
	m_CListCtrlApi.SetItemText(pInfo->Index, um_ApiDlg_State, pInfo->BaseSsdtInfo.State ? L"Enable" : L"Disable");

	delete pInfo;
	return;
}