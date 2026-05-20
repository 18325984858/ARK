// DlgSsdtShadow.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgSsdtShadow.h"
#include "LoadPe.h"
#include "Thread.h"

// DlgSsdtShadow 对话框

IMPLEMENT_DYNAMIC(DlgSsdtShadow, CDialogEx)

DlgSsdtShadow::DlgSsdtShadow(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_SSDTSHADOW, pParent)
{

}

DlgSsdtShadow::~DlgSsdtShadow()
{
}

void DlgSsdtShadow::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_SSDTSHADOW_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgSsdtShadow, CDialogEx)
	ON_NOTIFY(NM_RCLICK, ID_SSDTSHADOW_LIST, &DlgSsdtShadow::OnNMRClickSsdtshadowList)
	ON_COMMAND(ID_SSDTSHADOW_REFRESH, &DlgSsdtShadow::OnSsdtshadowRefresh)
	ON_WM_SIZE()
	ON_COMMAND(ID_SSDTSHADOW_RETURNHOOK, &DlgSsdtShadow::OnSsdtshadowReturnhook)
	ON_COMMAND(ID_SSDTSHADOW_ORIDER, &DlgSsdtShadow::OnSsdtshadowOrider)
	ON_COMMAND(ID_SSDTSHADOW_SERVICENUMBER, &DlgSsdtShadow::OnSsdtshadowServicenumber)
	ON_COMMAND(ID_SSDTSHADOW_FUNNAME, &DlgSsdtShadow::OnSsdtshadowFunname)
	ON_COMMAND(ID_SSDTSHADOW_CURKERNELADDR, &DlgSsdtShadow::OnSsdtshadowCurkerneladdr)
	ON_COMMAND(ID_SSDTSHADOW_SRCKERNELADDR, &DlgSsdtShadow::OnSsdtshadowSrckerneladdr)
	ON_COMMAND(ID_SSDTSHADOW_USERADDR, &DlgSsdtShadow::OnSsdtshadowUseraddr)
	ON_COMMAND(ID_SSDTSHADOW_MODULEPATH, &DlgSsdtShadow::OnSsdtshadowModulepath)
END_MESSAGE_MAP()

// DlgSsdtShadow 消息处理程序

void DlgSsdtShadow::OnNMRClickSsdtshadowList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	if (m_CListCtrl.GetItemCount() <= 0)
	{
		return;
	}

	CMenu menu;
	menu.LoadMenu(ID_MENU_SSDTSHADOW);
	CPoint point;
	GetCursorPos(&point);//获取当前的游标

	POSITION selPos = m_CListCtrl.GetFirstSelectedItemPosition();
	bool hasSel = (selPos != NULL);
	ULONG64 Index = hasSel ? ((int)selPos - 1) : 0;

	//还原
	// 注：ID_MENU_SSDTSHADOW 的 .rc 定义中并没有 “Hook” 项，只有 “恢复Hook”。
	if (!hasSel)
	{
		menu.EnableMenuItem(ID_SSDTSHADOW_RETURNHOOK, MF_GRAYED | MF_BYCOMMAND);
	}
	else
	{
		ULONG64 Data = m_CListCtrl.GetItemData((int)Index);
		if (Data == FALSE)
		{
			menu.EnableMenuItem(ID_SSDTSHADOW_RETURNHOOK, MF_GRAYED | MF_BYCOMMAND);
		}
	}

	if (this->m_ThreadFlags == TRUE)
	{
		menu.EnableMenuItem(ID_SSDTSHADOW_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	CMenu* pPopup = menu.GetSubMenu(0);
	CString explorerPath;
	UINT explorerCmd = AppendOpenInExplorerItem(*pPopup, &m_CListCtrl, explorerPath);

	UINT cmd = pPopup->TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
	if (HandleOpenInExplorerCmd(cmd, explorerCmd, explorerPath)) return;
	if (cmd != 0) PostMessage(WM_COMMAND, MAKEWPARAM(cmd, 0), 0);

}

void DlgSsdtShadow::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	CRect rect;
	GetClientRect(&rect);

	m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}

BOOL DlgSsdtShadow::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_SSDTShadow_Order, _T("序号"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_SSDTShadow_ServerNumber, _T("服务号"), LVCFMT_LEFT, 60);
	m_CListCtrl.InsertColumn(um_SSDTShadow_FunctionName, _T("函数名称"), LVCFMT_LEFT, 400);
	m_CListCtrl.InsertColumn(um_SSDTShadow_KernelAddr, _T("当前内核层地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_SSDTShadow_SrcKernelAddr, _T("原内核层地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_SSDTShadow_UserAddr, _T("用户层函数地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_SSDTShadow_Path, _T("函数所在模块位置"), LVCFMT_LEFT, 300);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	OnSsdtshadowRefresh();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgSsdtShadow::OnSsdtshadowRefresh()
{
	m_CListCtrl.DeleteAllItems();

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumSsdtShadowInfo, this });

}

void DlgSsdtShadow::InsertCtrlListControl(PCSsdtInfo pSsdtShadowInfo)
{
	LoadPEFile loadpe;									//加载Pe文件

	//遍历win32u.dll的导出表获取函数名称
	if (loadpe.ReadUserDefineDllToBuffer(L"C:\\Windows\\SysWOW64\\win32u.dll"))//加载32位ntdll.dll
	{
		loadpe.EnumWin32kPeExportTable((PLONG64)pSsdtShadowInfo, NULL);
	}
	else
	{
		if (loadpe.ReadUserDefineDllToBuffer(L"C:\\Windows\\System32\\win32u.dll"))//如加载32位失败,尝试加载64位
		{
			loadpe.EnumWin32kPeExportTable((PLONG64)pSsdtShadowInfo, NULL);
		}
		else
		{
			AfxMessageBox(_T("win32u.dll模块加载失败!"));
		}
	}

	PCLIST_ENTRY pCurList = &pSsdtShadowInfo->List.List;
	do
	{
		PCSsdtInfo pProcessVadInfo = (PCSsdtInfo)pCurList;
		if (pProcessVadInfo == NULL)
		{
			break;
		}

		int i = m_CListCtrl.GetItemCount();
		CString StrBuf;

		StrBuf.Format(L"%d", pProcessVadInfo->NumberOrder);
		m_CListCtrl.InsertItem(i, StrBuf);

		StrBuf.Format(L"%I64X", pProcessVadInfo->NtFunAddr);
		m_CListCtrl.SetItemText(i, um_SSDTShadow_KernelAddr, StrBuf);

		StrBuf.Format(L"%I64X", pProcessVadInfo->SrcNtFunAddr);
		m_CListCtrl.SetItemText(i, um_SSDTShadow_SrcKernelAddr, StrBuf);

		StrBuf.Format(L"0x%03X", pProcessVadInfo->ServiceNumber);
		m_CListCtrl.SetItemText(i, um_SSDTShadow_ServerNumber, StrBuf);

		m_CListCtrl.SetItemText(i, um_SSDTShadow_FunctionName, pProcessVadInfo->FunName);

		StrBuf.Format(L"%I64X", pProcessVadInfo->UsFunAddr);
		m_CListCtrl.SetItemText(i, um_SSDTShadow_UserAddr, StrBuf);

		CString FilePath = PathTransForm(pProcessVadInfo->Path);
		m_CListCtrl.SetItemText(i, um_SSDTShadow_Path, FilePath.GetBuffer());

		//判断原地址函数,是否和当前地址一样
		if (pProcessVadInfo->SrcNtFunAddr != pProcessVadInfo->NtFunAddr)
		{
			m_CListCtrl.SetItemData(i, TRUE);
		}
		else
		{
			m_CListCtrl.SetItemData(i, FALSE);
		}

		//指向下一个
		pCurList = pCurList->Blink;

		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pProcessVadInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}

	} while (pCurList != &pSsdtShadowInfo->List.List);

}

void DlgSsdtShadow::OnSsdtshadowReturnhook()
{
	//恢复钩子函数

}

void DlgSsdtShadow::OnSsdtshadowOrider()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDTShadow_Order);
}

void DlgSsdtShadow::OnSsdtshadowServicenumber()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDTShadow_ServerNumber);
}

void DlgSsdtShadow::OnSsdtshadowFunname()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDTShadow_FunctionName);
}

void DlgSsdtShadow::OnSsdtshadowCurkerneladdr()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDTShadow_KernelAddr);
}

void DlgSsdtShadow::OnSsdtshadowSrckerneladdr()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDTShadow_SrcKernelAddr);
}

void DlgSsdtShadow::OnSsdtshadowUseraddr()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDTShadow_UserAddr);
}

void DlgSsdtShadow::OnSsdtshadowModulepath()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDTShadow_Path);
}