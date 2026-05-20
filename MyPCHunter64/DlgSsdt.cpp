// DlgSsdt.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgSsdt.h"
#include "Thread.h"
#include "LoadPe.h"
#include "MyPCHunter64Dlg.h"
// DlgSsdt 对话框
UCHAR g_HookStateFlags[SSDT_MAX_NUMBER] = { 0 };

IMPLEMENT_DYNAMIC(DlgSsdt, CDialogEx)

DlgSsdt::DlgSsdt(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_SSDT, pParent)
{

}

DlgSsdt::~DlgSsdt()
{
}

void DlgSsdt::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_SSDT_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgSsdt, CDialogEx)
	ON_NOTIFY(NM_RCLICK, ID_SSDT_LIST, &DlgSsdt::OnNMRClickSsdtList)
	ON_WM_SIZE()
	ON_COMMAND(ID_SSDT_REFRESH, &DlgSsdt::OnSsdtRefresh)
	ON_COMMAND(ID_SSDT_HOOK, &DlgSsdt::OnSsdtHook)
	ON_COMMAND(ID_SSDT_RETURNHOOK, &DlgSsdt::OnSsdtReturnhook)
	ON_COMMAND(ID_SSDT_COPY_ORIDER, &DlgSsdt::OnSsdtCopyOrider)
	ON_COMMAND(ID_SSDT_COPY_SERVICENUMBER, &DlgSsdt::OnSsdtCopyServicenumber)
	ON_COMMAND(ID_SSDT_COPY_FUNNAME, &DlgSsdt::OnSsdtCopyFunname)
	ON_COMMAND(ID_SSDT_COPY_CURKERNELADDR, &DlgSsdt::OnSsdtCopyCurkerneladdr)
	ON_COMMAND(ID_SSDT_COPY_SRCKERNELADDR, &DlgSsdt::OnSsdtCopySrckerneladdr)
	ON_COMMAND(ID_SSDT_COPY_CURUSERADDR, &DlgSsdt::OnSsdtCopyCuruseraddr)
	ON_COMMAND(ID_SSDT_COPY_MOUDLEPATH, &DlgSsdt::OnSsdtCopyMoudlepath)
END_MESSAGE_MAP()

// DlgSsdt 消息处理程序

void DlgSsdt::OnNMRClickSsdtList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	if (m_CListCtrl.GetItemCount() <= 0)
	{
		return;
	}

	CMenu menu;
	menu.LoadMenu(ID_MENU_SSDT);
	CPoint point;
	GetCursorPos(&point);//获取当前的游标

	POSITION selPos = m_CListCtrl.GetFirstSelectedItemPosition();
	bool hasSel = (selPos != NULL);
	ULONG64 Index = hasSel ? ((int)selPos - 1) : 0;

	if (hasSel)
	{
		//获取函数名称
		CString FunName = m_CListCtrl.GetItemText((int)Index, um_SSDT_FunctionName);
		CString FunName1;
		FunName1.Format(L"Hook %ws", FunName.GetBuffer());
		menu.ModifyMenu(ID_SSDT_HOOK, MF_BYCOMMAND | MF_STRING, ID_SSDT_HOOK, FunName1);
	}

	//还原
	if (!hasSel)
	{
		// 未选中任何行：Hook / 还原 Hook 都不能操作
		menu.EnableMenuItem(ID_SSDT_HOOK,       MF_GRAYED | MF_BYCOMMAND);
		menu.EnableMenuItem(ID_SSDT_RETURNHOOK, MF_GRAYED | MF_BYCOMMAND);
	}
	else
	{
		UCHAR Data = g_HookStateFlags[Index];
		if (!Data)
		{
			menu.EnableMenuItem(ID_SSDT_RETURNHOOK, MF_GRAYED | MF_BYCOMMAND);
		}
	}

	if (this->m_ThreadFlags == TRUE)
	{
		menu.EnableMenuItem(ID_SSDT_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	CMenu* pPopup = menu.GetSubMenu(0);
	CString explorerPath;
	UINT explorerCmd = AppendOpenInExplorerItem(*pPopup, &m_CListCtrl, explorerPath);

	UINT cmd = pPopup->TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
	if (HandleOpenInExplorerCmd(cmd, explorerCmd, explorerPath)) return;
	if (cmd != 0) PostMessage(WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
}

BOOL DlgSsdt::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_SSDT_Order, _T("序号"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_SSDT_ServerNumber, _T("服务号"), LVCFMT_LEFT, 60);
	m_CListCtrl.InsertColumn(um_SSDT_FunctionName, _T("函数名称"), LVCFMT_LEFT, 400);
	m_CListCtrl.InsertColumn(um_SSDT_KernelAddr, _T("当前内核层地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_SSDT_SrcKernelAddr, _T("原内核层地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_SSDT_UserAddr, _T("用户层函数地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_SSDT_Path, _T("函数所在模块位置"), LVCFMT_LEFT, 300);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	OnSsdtRefresh();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgSsdt::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	CRect rect;
	GetClientRect(&rect);
	m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}

void DlgSsdt::OnSsdtRefresh()
{
	m_CListCtrl.DeleteAllItems();

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumSsdtInfo, this });

}

void DlgSsdt::InsertCtrlListControl(PCSsdtInfo pSsdtInfo)
{
	LoadPEFile loadpe;

	//加载PE 获取函数名称
	if (loadpe.ReadUserDefineDllToBuffer(L"C:\\Windows\\SysWOW64\\ntdll.dll"))//加载32位ntdll.dll
	{
		loadpe.EnumNtdllPeExportTable((PLONG64)pSsdtInfo, NULL);
	}
	else
	{
		if (loadpe.ReadUserDefineDllToBuffer(L"C:\\Windows\\System32\\ntdll.dll"))//如加载32位失败,尝试加载64位
		{
			loadpe.EnumNtdllPeExportTable((PLONG64)pSsdtInfo, NULL);
		}
		else
		{
			AfxMessageBox(_T("ntdll.dll模块加载失败!"));
		}
	}

	PCLIST_ENTRY pCurList = &pSsdtInfo->List.List;
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

		StrBuf.Format(L"%016I64X", pProcessVadInfo->NtFunAddr);
		m_CListCtrl.SetItemText(i, um_SSDT_KernelAddr, StrBuf);

		StrBuf.Format(L"%016I64X", pProcessVadInfo->SrcNtFunAddr);
		m_CListCtrl.SetItemText(i, um_SSDT_SrcKernelAddr, StrBuf);

		StrBuf.Format(L"0x%03X", pProcessVadInfo->ServiceNumber);
		m_CListCtrl.SetItemText(i, um_SSDT_ServerNumber, StrBuf);

		m_CListCtrl.SetItemText(i, um_SSDT_FunctionName, pProcessVadInfo->FunName);

		StrBuf.Format(L"%016I64X", pProcessVadInfo->UsFunAddr);
		m_CListCtrl.SetItemText(i, um_SSDT_UserAddr, StrBuf);

		CString FilePath = PathTransForm(pProcessVadInfo->Path);
		m_CListCtrl.SetItemText(i, um_SSDT_Path, FilePath.GetBuffer());

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

	} while (pCurList != &pSsdtInfo->List.List);
}

void DlgSsdt::OnSsdtHook()
{
	//获取索引
	ULONG64 Index = (int)m_CListCtrl.GetFirstSelectedItemPosition() - 1;
	if (Index < 0)
	{
		return;
	}

	// TODO: 在此添加命令处理程序代码
	if (!g_CreateFlagsDlgProcessMonitor)
	{
		g_DlgProcessMonitor.Create(ID_DIALOG_SSDT_MONITOR);
		g_CreateFlagsDlgProcessMonitor = TRUE;
	}
	else
	{
		g_DlgProcessMonitor.ShowWindow(1);
	}

	//获取当前是否被Hook了
	UCHAR Flags = g_HookStateFlags[Index];
	if (!Flags)
	{
		PCHookSsdtTableInfo pInfo = new CHookSsdtTableInfo{ (ULONG32)Index ,1 };
		if (!pInfo)
		{
			return;
		}

		g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserHookSsdtTable,(PVOID64)pInfo });
		//标志当前为真
		m_CListCtrl.SetItemData(Index, (DWORD_PTR)1);

		if (g_CreateFlagsDlgProcessMonitor)
		{

			PCHookSsdtInfo pInfo = new CHookSsdtInfo;
			if (!pInfo)
			{
				return;
			}
			CString FunName = m_CListCtrl.GetItemText(Index, um_SSDT_FunctionName);
			wcscpy(pInfo->FunName, FunName.GetBuffer());
			pInfo->State = TRUE;
			g_DlgProcessMonitor.PostMessage(wm_User_DlgProcessMonitor_InsertApi, (WPARAM)pInfo, NULL);
		}
		g_HookStateFlags[Index] = TRUE;
	}
}

void DlgSsdt::OnSsdtReturnhook()
{
	if (!g_CreateFlagsDlgProcessMonitor)
	{
		return;
	}

	ULONG64 Index = (int)m_CListCtrl.GetFirstSelectedItemPosition() - 1;
	if (Index < 0)
	{
		return;
	}

	if (g_HookStateFlags[Index])
	{
		//还原Hook

		PCAlterHookSsdtInfo pInfo = new CAlterHookSsdtInfo;
		if (!pInfo)
		{
			return;
		}

		CString FunName = m_CListCtrl.GetItemText(Index, um_SSDT_FunctionName);
		wcscpy(pInfo->BaseSsdtInfo.FunName, FunName.GetBuffer());
		pInfo->BaseSsdtInfo.State = FALSE;

		g_DlgProcessMonitor.PostMessage(wm_User_DlgProcessMonitor_AlterApi, (WPARAM)pInfo, NULL);

		g_HookStateFlags[Index] = FALSE;
	}
}

void DlgSsdt::OnSsdtCopyOrider()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDT_Order);
}

void DlgSsdt::OnSsdtCopyServicenumber()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDT_ServerNumber);
}

void DlgSsdt::OnSsdtCopyFunname()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDT_FunctionName);
}

void DlgSsdt::OnSsdtCopyCurkerneladdr()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDT_KernelAddr);
}

void DlgSsdt::OnSsdtCopySrckerneladdr()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDT_SrcKernelAddr);
}

void DlgSsdt::OnSsdtCopyCuruseraddr()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDT_UserAddr);
}

void DlgSsdt::OnSsdtCopyMoudlepath()
{
	CopyBufferToClipboard(&m_CListCtrl, um_SSDT_Path);
}