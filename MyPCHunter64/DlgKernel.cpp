// DlgKernel.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgKernel.h"
#include "Thread.h"
#include "DlgSsdt.h"
#include "DlgSsdtShadow.h"


// DlgKernel 对话框

IMPLEMENT_DYNAMIC(DlgKernel, CDialogEx)

DlgKernel::DlgKernel(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_KERNEL, pParent)
{

}

DlgKernel::~DlgKernel()
{
}

void DlgKernel::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_KERNEL_TAB, m_CTabCtrl);
}


BEGIN_MESSAGE_MAP(DlgKernel, CDialogEx)
	ON_WM_SIZE()
	ON_NOTIFY(NM_CLICK, ID_KERNEL_TAB, &DlgKernel::OnNMClickKernelTab)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// DlgKernel 消息处理程序


void DlgKernel::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);


	CRect rect;
	GetClientRect(&rect);

	m_CTabCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);

	m_CTabCtrl.GetClientRect(&rect);
	rect.top += 25;
	rect.left += 5;
	rect.right -= 7;
	rect.bottom -= 5;
	m_DlgGdt.MoveWindow(rect, TRUE);
	m_DlgKernelCallBack.MoveWindow(rect, TRUE);
	m_DlgMiniFilterCallBack.MoveWindow(rect, TRUE);
	m_DlgDpc.MoveWindow(rect, TRUE);
	m_DlgHalTable.MoveWindow(rect, TRUE);
	m_DlgWdf.MoveWindow(rect, TRUE);
	m_FilterDriver.MoveWindow(rect, TRUE);
}


BOOL DlgKernel::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	CString TableTitleStr[] = { L"系统回调",L"过滤驱动",L"DPC定时器",L"工作线程队列",L"Hal",L"Wdf",L"文件系统",L"系统调试",L"对象劫持",L"直接IO",L"GDT" };

	m_CTabCtrl.InsertItem(KernelDlgType_SystemCallBack, TableTitleStr[KernelDlgType_SystemCallBack]);
	m_DlgKernelCallBack.OnKernelcallbackRefresh();
	m_DlgKernelCallBack.Create(ID_DLG_KERNEL_KERNELCALLBACK, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelDlgType_FilterDriver, TableTitleStr[KernelDlgType_FilterDriver]);
	m_FilterDriver.Create(ID_DLG_FILTERDRIVER, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelDlgType_DpcTimer, TableTitleStr[KernelDlgType_DpcTimer]);
	m_DlgDpc.Create(ID_DLG_DPC, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelDlgType_WorkThread, TableTitleStr[KernelDlgType_WorkThread]);

	m_CTabCtrl.InsertItem(KernelDlgType_Hal, TableTitleStr[KernelDlgType_Hal]);
	m_DlgHalTable.Create(ID_DLG_HALTABLE, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelDlgType_Wdf, TableTitleStr[KernelDlgType_Wdf]);
	m_DlgWdf.Create(ID_DLG_WDF, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelDlgType_FileSystem, TableTitleStr[KernelDlgType_FileSystem]);
	m_DlgMiniFilterCallBack.Create(ID_DLG_KERNEL_MINIFILTERCALLBACK, &m_CTabCtrl);

	m_CTabCtrl.InsertItem(KernelDlgType_SystemDbg, TableTitleStr[KernelDlgType_SystemDbg]);
	m_CTabCtrl.InsertItem(KernelDlgType_ObjectHijack, TableTitleStr[KernelDlgType_ObjectHijack]);
	m_CTabCtrl.InsertItem(KernelDlgType_IO, TableTitleStr[KernelDlgType_IO]);
	m_CTabCtrl.InsertItem(KernelDlgType_GDT, TableTitleStr[KernelDlgType_GDT]);
	m_DlgGdt.Create(ID_DLG_GDT, &m_CTabCtrl);



	//加载窗口
	RECT TabRect;
	m_CTabCtrl.GetClientRect(&TabRect);
	TabRect.top += 25;
	TabRect.left += 5;
	TabRect.right -= 7;
	TabRect.bottom -= 5;
	m_DlgGdt.MoveWindow(&TabRect);
	m_DlgKernelCallBack.MoveWindow(&TabRect);
	m_FilterDriver.MoveWindow(&TabRect);
	m_DlgMiniFilterCallBack.MoveWindow(&TabRect);
	m_DlgDpc.MoveWindow(&TabRect);
	m_DlgHalTable.MoveWindow(&TabRect);
	m_DlgWdf.MoveWindow(&TabRect);

	m_DlgKernelCallBack.ShowWindow(TRUE);
	return TRUE;
}


void DlgKernel::OnNMClickKernelTab(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	m_DlgGdt.ShowWindow(FALSE);
	m_DlgKernelCallBack.ShowWindow(FALSE);
	m_DlgMiniFilterCallBack.ShowWindow(FALSE);
	m_DlgDpc.ShowWindow(FALSE);
	m_DlgHalTable.ShowWindow(FALSE);
	m_DlgWdf.ShowWindow(FALSE);
	m_FilterDriver.ShowWindow(FALSE);


	switch (m_CTabCtrl.GetCurSel())
	{
	case DlgKernel::KernelDlgType_SystemCallBack:
		m_DlgKernelCallBack.ShowWindow(TRUE);
		m_DlgKernelCallBack.OnKernelcallbackRefresh();
		break;
	case DlgKernel::KernelDlgType_FilterDriver:
		m_FilterDriver.ShowWindow(TRUE);
		m_FilterDriver.OnFilterdriverRefresh();
		break;
	case DlgKernel::KernelDlgType_DpcTimer:
		m_DlgDpc.ShowWindow(TRUE);
		m_DlgDpc.OnDpcRefresh();
		break;
	case DlgKernel::KernelDlgType_WorkThread:
		break;
	case DlgKernel::KernelDlgType_Hal:
		m_DlgHalTable.ShowWindow(TRUE);
		m_DlgHalTable.OnHaltableRefresh();
		break;
	case DlgKernel::KernelDlgType_Wdf:
		m_DlgWdf.ShowWindow(TRUE);
		m_DlgWdf.OnHaltableRefresh();
		break;
	case DlgKernel::KernelDlgType_FileSystem:
		m_DlgMiniFilterCallBack.ShowWindow(TRUE);
		m_DlgMiniFilterCallBack.OnMinifiltercallbackRefresh();
		break;
	case DlgKernel::KernelDlgType_SystemDbg:
		break;
	case DlgKernel::KernelDlgType_ObjectHijack:
		break;
	case DlgKernel::KernelDlgType_IO:
		break;
	case DlgKernel::KernelDlgType_GDT:
		m_DlgGdt.ShowWindow(TRUE);
		m_DlgGdt.OnGdtRefresh();
		break;
	default:
		//AfxMessageBox(L"控件出现错误!");
		break;
	}
}


void DlgKernel::OnDestroy()
{
	CDialogEx::OnDestroy();

	m_FilterDriver.DestroyWindow();
	m_DlgGdt.DestroyWindow();
	m_DlgKernelCallBack.DestroyWindow();
	m_DlgMiniFilterCallBack.DestroyWindow();
	m_DlgDpc.DestroyWindow();
	m_DlgHalTable.DestroyWindow();
	m_DlgWdf.DestroyWindow();

	// TODO: 在此处添加消息处理程序代码
}
