// DlgFilterDriver.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgFilterDriver.h"
#include "Thread.h"

// DlgFilterDriver 对话框

IMPLEMENT_DYNAMIC(DlgFilterDriver, CDialogEx)

DlgFilterDriver::DlgFilterDriver(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_FILTERDRIVER, pParent)
{

}

DlgFilterDriver::~DlgFilterDriver()
{
}

void DlgFilterDriver::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_FILTERDRIVER, m_CListCtrl);
}

void DlgFilterDriver::InsertCtrlListControl(PCFilterDeviceInfo pInfo)
{
	if (pInfo == NULL)
	{
		return;
	}

	PCLIST_ENTRY pList = &pInfo->List.List;
	do
	{
		int i = m_CListCtrl.GetItemCount();
		WCHAR StrBuf[256] = { 0 };

		PCFilterDeviceInfo pFilterDriverInfo = (PCFilterDeviceInfo)pList;

		m_CListCtrl.InsertItem(i, pFilterDriverInfo->TypeName);
		CString FilePath = PathTransForm(pFilterDriverInfo->FilterDriverPath);

		m_CListCtrl.SetItemText(i, um_FilterDriver_Driver, pFilterDriverInfo->FilterDriverName);
		m_CListCtrl.SetItemText(i, um_FilterDriver_DriverPath, FilePath);

		wsprintf(StrBuf, L"%I64X", pFilterDriverInfo->FilterDeviceObject);
		m_CListCtrl.SetItemText(i, um_FilterDriver_DeviceObject, StrBuf);

		m_CListCtrl.SetItemText(i, um_FilterDriver_DeviceName, pFilterDriverInfo->FilterDeviceName);
		m_CListCtrl.SetItemText(i, um_FilterDriver_SrcDriverObjectName, pFilterDriverInfo->SrcDriverName);

		CString szDstFileName;
		m_CListCtrl.SetItemText(i, um_FilterDriver_FileName, TEXT("--"));
		if (this->GetCompanyName(FilePath, szDstFileName))
		{
			m_CListCtrl.SetItemText(i, um_FilterDriver_FileName, (LPWSTR)szDstFileName.GetString());
		}

		//获取下一个节点
		pList = pList->Blink;
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pFilterDriverInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}

	} while (pList != &pInfo->List.List);
}


BEGIN_MESSAGE_MAP(DlgFilterDriver, CDialogEx)
	ON_WM_SIZE()
	ON_COMMAND(ID_FILTERDRIVER_REFRESH, &DlgFilterDriver::OnFilterdriverRefresh)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_FILTERDRIVER, &DlgFilterDriver::OnRclickListFilterdriver)
END_MESSAGE_MAP()


// DlgFilterDriver 消息处理程序

BOOL DlgFilterDriver::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_FilterDriver_Type, _T("类型"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_FilterDriver_Driver, _T("驱动对象名"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_FilterDriver_DriverPath, _T("过滤驱动路径"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_FilterDriver_DeviceObject, _T("过滤设备对象"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_FilterDriver_DeviceName, _T("过滤设备名"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_FilterDriver_SrcDriverObjectName, _T("宿主驱动对象名"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_FilterDriver_FileName, _T("文件厂商"), LVCFMT_LEFT, 100);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

BOOL DlgFilterDriver::DestroyWindow()
{
	// TODO: 在此添加专用代码和/或调用基类

	return CDialogEx::DestroyWindow();
}

void DlgFilterDriver::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	RECT rect = { 0 };
	rect.bottom = cy;
	rect.right = cx;
	m_CListCtrl.MoveWindow(&rect, TRUE);
}

void DlgFilterDriver::OnFilterdriverRefresh()
{
	m_CListCtrl.DeleteAllItems();
	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumFilterDriver, this });
}

void DlgFilterDriver::OnRclickListFilterdriver(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	CMenu menu;
	POINT point = { 0 };

	GetCursorPos(&point);//获取当前的游标
	menu.LoadMenuW(ID_MENU_FILTERDRIVER);//加载菜单资源
	CMenu* pPopup = menu.GetSubMenu(0);//

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	if (this->m_ThreadFlags == 1)
	{
		menu.EnableMenuItem(ID_FILTERDRIVER_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	pPopup->TrackPopupMenu(TPM_LEFTBUTTON, point.x, point.y, this);//设置菜单栏出现的位置

}
