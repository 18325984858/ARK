// DlgKernelHook.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgKernelHook.h"


// DlgKernelHook 对话框

IMPLEMENT_DYNAMIC(DlgKernelHook, CDialogEx)

DlgKernelHook::DlgKernelHook(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_KERNELHOOK, pParent)
{

}

DlgKernelHook::~DlgKernelHook()
{
}

void DlgKernelHook::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_KERNELHOOK_TAB, m_CTabCtrl);
}


BEGIN_MESSAGE_MAP(DlgKernelHook, CDialogEx)
	ON_NOTIFY(NM_CLICK, ID_KERNELHOOK_TAB, &DlgKernelHook::OnNMClickKernelhookTab)
	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// DlgKernelHook 消息处理程序


void DlgKernelHook::OnNMClickKernelhookTab(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	m_DlgSsdt.ShowWindow(FALSE);
	m_DlgSsdtShadow.ShowWindow(FALSE);
	m_DlgIdt.ShowWindow(FALSE);
	m_DlgObjectCallBack.ShowWindow(FALSE);
	m_DlgAcpi.ShowWindow(FALSE);
	m_DlgNtfs.ShowWindow(FALSE);
	m_Dlgkbdclass.ShowWindow(FALSE);
	m_Dlgi8042prt.ShowWindow(FALSE);
	m_DlgMouClass.ShowWindow(FALSE);
	m_DlgParTmgr.ShowWindow(FALSE);
	m_DlgClassPnp.ShowWindow(FALSE);
	m_DlgAtapi.ShowWindow(FALSE);
	m_Dlgstorport.ShowWindow(FALSE);

	switch (m_CTabCtrl.GetCurSel())
	{
	case DlgKernelHook::KernelHookDlgType_SSDT:
		m_DlgSsdt.OnSsdtRefresh();
		m_DlgSsdt.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_ShadowSSDT:
		m_DlgSsdtShadow.OnSsdtshadowRefresh();
		m_DlgSsdtShadow.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_FSD:
		m_DlgNtfs.OnDriverMajorFunctionRefresh();
		m_DlgNtfs.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_KeyBoard:
		m_Dlgkbdclass.OnDriverMajorFunctionRefresh();
		m_Dlgkbdclass.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_I8042Prt:
		m_Dlgi8042prt.OnDriverMajorFunctionRefresh();
		m_Dlgi8042prt.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_Mouse:
		m_DlgMouClass.OnDriverMajorFunctionRefresh();
		m_DlgMouClass.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_PartMgr:
		m_DlgParTmgr.OnDriverMajorFunctionRefresh();
		m_DlgParTmgr.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_Disk:
		m_DlgClassPnp.OnDriverMajorFunctionRefresh();
		m_DlgClassPnp.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_AtApi:
		m_DlgAtapi.OnDriverMajorFunctionRefresh();
		m_DlgAtapi.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_Acpi:
		m_DlgAcpi.OnDriverMajorFunctionRefresh();
		m_DlgAcpi.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_Scsi:
		m_Dlgstorport.OnDriverMajorFunctionRefresh();
		m_Dlgstorport.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_KernelHook:
		break;
	case DlgKernelHook::KernelHookDlgType_ObjectHook:
		m_DlgObjectCallBack.OnObjectcallbackRefresh();
		m_DlgObjectCallBack.ShowWindow(TRUE);
		break;
	case DlgKernelHook::KernelHookDlgType_SystemInterrupt:
		m_DlgIdt.OnIdtRefresh();
		m_DlgIdt.ShowWindow(TRUE);
		break;
	default:
		break;
	}

}


void DlgKernelHook::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	CRect rect;
	GetClientRect(&rect);

	m_CTabCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);


	RECT TabRect;
	m_CTabCtrl.GetClientRect(&TabRect);
	TabRect.top += 30;
	TabRect.left += 5;
	TabRect.right -= 7;

	m_DlgSsdt.MoveWindow(&TabRect, TRUE);
	m_DlgSsdtShadow.MoveWindow(&TabRect, TRUE);
	m_DlgIdt.MoveWindow(&TabRect, TRUE);
	m_DlgObjectCallBack.MoveWindow(&TabRect, TRUE);
	m_DlgAcpi.MoveWindow(&TabRect, TRUE);
	m_DlgNtfs.MoveWindow(&TabRect, TRUE);
	m_Dlgkbdclass.MoveWindow(&TabRect, TRUE);
	m_Dlgi8042prt.MoveWindow(&TabRect, TRUE);
	m_DlgMouClass.MoveWindow(&TabRect, TRUE);
	m_DlgParTmgr.MoveWindow(&TabRect, TRUE);
	m_DlgClassPnp.MoveWindow(&TabRect, TRUE);
	m_DlgAtapi.MoveWindow(&TabRect, TRUE);
	m_Dlgstorport.MoveWindow(&TabRect, TRUE);
}


BOOL DlgKernelHook::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	CString TableTitleStr[] = { L"SSDT",L"ShadowSSDT",L"FSD",L"键盘",L"I8042Prt",L"鼠标",L"PartMgr",L"Disk",L"AtApi",L"Acpi",L"Scsi",L"内核钩子",L"Object钩子",L"系统中断表" };

	m_CTabCtrl.InsertItem(KernelHookDlgType_SSDT, TableTitleStr[KernelHookDlgType_SSDT]);
	m_DlgSsdt.Create(ID_DLG_SSDT, &m_CTabCtrl);
	m_DlgSsdt.OnSsdtRefresh();

	m_CTabCtrl.InsertItem(KernelHookDlgType_ShadowSSDT, TableTitleStr[KernelHookDlgType_ShadowSSDT]);
	m_DlgSsdtShadow.Create(ID_DLG_SSDTSHADOW, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_FSD, TableTitleStr[KernelHookDlgType_FSD]);
	memcpy_s(m_DlgNtfs.m_DriverMajorFunctionInfo.MajorFunctionDriverName, MY_MAX_PATH, L"Ntfs.sys", wcslen(L"Ntfs.sys") * sizeof(WCHAR));
	m_DlgNtfs.Create(ID_DLG_MAJORFUNCTION, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_KeyBoard, TableTitleStr[KernelHookDlgType_KeyBoard]);
	memcpy_s(m_Dlgkbdclass.m_DriverMajorFunctionInfo.MajorFunctionDriverName, MY_MAX_PATH, L"kbdclass.sys", wcslen(L"kbdclass.sys") * sizeof(WCHAR));
	m_Dlgkbdclass.Create(ID_DLG_MAJORFUNCTION, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_I8042Prt, TableTitleStr[KernelHookDlgType_I8042Prt]);
	memcpy_s(m_Dlgi8042prt.m_DriverMajorFunctionInfo.MajorFunctionDriverName, MY_MAX_PATH, L"i8042prt.sys", wcslen(L"i8042prt.sys") * sizeof(WCHAR));
	m_Dlgi8042prt.Create(ID_DLG_MAJORFUNCTION, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_Mouse, TableTitleStr[KernelHookDlgType_Mouse]);
	memcpy_s(m_DlgMouClass.m_DriverMajorFunctionInfo.MajorFunctionDriverName, MY_MAX_PATH, L"MouClass.sys", wcslen(L"MouClass.sys") * sizeof(WCHAR));
	m_DlgMouClass.Create(ID_DLG_MAJORFUNCTION, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_PartMgr, TableTitleStr[KernelHookDlgType_PartMgr]);
	memcpy_s(m_DlgParTmgr.m_DriverMajorFunctionInfo.MajorFunctionDriverName, MY_MAX_PATH, L"ParTmgr.sys", wcslen(L"ParTmgr.sys") * sizeof(WCHAR));
	m_DlgParTmgr.Create(ID_DLG_MAJORFUNCTION, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_Disk, TableTitleStr[KernelHookDlgType_Disk]);
	memcpy_s(m_DlgClassPnp.m_DriverMajorFunctionInfo.MajorFunctionDriverName, MY_MAX_PATH, L"CLASSPNP.sys", wcslen(L"CLASSPNP.sys") * sizeof(WCHAR));
	m_DlgClassPnp.Create(ID_DLG_MAJORFUNCTION, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_AtApi, TableTitleStr[KernelHookDlgType_AtApi]);
	memcpy_s(m_DlgAtapi.m_DriverMajorFunctionInfo.MajorFunctionDriverName, MY_MAX_PATH, L"Atapi.sys", wcslen(L"Atapi.sys") * sizeof(WCHAR));
	m_DlgAtapi.Create(ID_DLG_MAJORFUNCTION, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_Acpi, TableTitleStr[KernelHookDlgType_Acpi]);
	memcpy_s(m_DlgAcpi.m_DriverMajorFunctionInfo.MajorFunctionDriverName, MY_MAX_PATH, L"Acpi.sys", wcslen(L"Acpi.sys") * sizeof(WCHAR));
	m_DlgAcpi.Create(ID_DLG_MAJORFUNCTION, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_Scsi, TableTitleStr[KernelHookDlgType_Scsi]);
	memcpy_s(m_Dlgstorport.m_DriverMajorFunctionInfo.MajorFunctionDriverName, MY_MAX_PATH, L"storport.sys", wcslen(L"storport.sys") * sizeof(WCHAR));
	m_Dlgstorport.Create(ID_DLG_MAJORFUNCTION, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_KernelHook, TableTitleStr[KernelHookDlgType_KernelHook]);

	m_CTabCtrl.InsertItem(KernelHookDlgType_ObjectHook, TableTitleStr[KernelHookDlgType_ObjectHook]);
	m_DlgObjectCallBack.Create(ID_DLG_KERNEL_OBJECTCALLBACK, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelHookDlgType_SystemInterrupt, TableTitleStr[KernelHookDlgType_SystemInterrupt]);
	m_DlgIdt.Create(ID_DLG_IDT, &m_CTabCtrl);



	RECT TabRect;
	m_CTabCtrl.GetClientRect(&TabRect);
	TabRect.top += 30;
	TabRect.left += 5;
	TabRect.right -= 7;

	m_DlgSsdt.MoveWindow(&TabRect);
	m_DlgSsdtShadow.MoveWindow(&TabRect);
	m_DlgIdt.MoveWindow(&TabRect);
	m_DlgObjectCallBack.MoveWindow(&TabRect);
	m_DlgAcpi.MoveWindow(&TabRect);
	m_DlgNtfs.MoveWindow(&TabRect);
	m_Dlgkbdclass.MoveWindow(&TabRect);
	m_Dlgi8042prt.MoveWindow(&TabRect);
	m_DlgMouClass.MoveWindow(&TabRect);
	m_DlgParTmgr.MoveWindow(&TabRect);
	m_DlgClassPnp.MoveWindow(&TabRect);
	m_DlgAtapi.MoveWindow(&TabRect);
	m_Dlgstorport.MoveWindow(&TabRect);

	m_DlgSsdt.ShowWindow(TRUE);


	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}


void DlgKernelHook::OnDestroy()
{
	CDialogEx::OnDestroy();



	m_DlgSsdt.DestroyWindow();
	m_DlgSsdtShadow.DestroyWindow();
	m_DlgIdt.DestroyWindow();
	m_DlgObjectCallBack.DestroyWindow();
	m_DlgAcpi.DestroyWindow();
	m_DlgNtfs.DestroyWindow();
	m_Dlgkbdclass.DestroyWindow();
	m_Dlgi8042prt.DestroyWindow();
	m_DlgMouClass.DestroyWindow();
	m_DlgParTmgr.DestroyWindow();
	m_DlgClassPnp.DestroyWindow();
	m_DlgAtapi.DestroyWindow();
	m_Dlgstorport.DestroyWindow();
}
