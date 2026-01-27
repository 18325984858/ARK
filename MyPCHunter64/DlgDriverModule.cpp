// DlgDriverModule.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgDriverModule.h"
#include "Thread.h"

// DlgDriverModule 对话框

IMPLEMENT_DYNAMIC(DlgDriverModule, CDialogEx)

DlgDriverModule::DlgDriverModule(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_DRIVERMODE, pParent)
{

}

DlgDriverModule::~DlgDriverModule()
{
}

void DlgDriverModule::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_CONTROL_DRIVERMODULE_LIST, m_CListCtrl);
}


BEGIN_MESSAGE_MAP(DlgDriverModule, CDialogEx)
	ON_WM_SIZE()
	ON_COMMAND(ID_DRIVER_MENU_COPY_NAME, &DlgDriverModule::OnDriverMenuCopyName)
	ON_COMMAND(ID_DRIVER_MENU_COPY_BASEADDR, &DlgDriverModule::OnDriverMenuCopyBaseaddr)
	ON_COMMAND(ID_DRIVER_MENU_COPY_SIZE, &DlgDriverModule::OnDriverMenuCopySize)
	ON_COMMAND(ID_DRIVER_MENU_COPY_LOADORAD, &DlgDriverModule::OnDriverMenuCopyLoadorad)
	ON_COMMAND(ID_DRIVER_MENU_COPY_OBJECEADDR, &DlgDriverModule::OnDriverMenuCopyObjeceaddr)
	ON_COMMAND(ID_DRIVER_MENU_COPY_OBJECTNAME, &DlgDriverModule::OnDriverMenuCopyObjectname)
	ON_COMMAND(ID_DRIVER_MENU_COPY_SERVERNAME, &DlgDriverModule::OnDriverMenuCopyServername)
	ON_COMMAND(ID_DRIVER_MENU_COPY_PATH, &DlgDriverModule::OnDriverMenuCopyPath)
	ON_COMMAND(ID_DRIVER_MENU_COPY_COMPANY, &DlgDriverModule::OnDriverMenuCopyCompany)
	ON_COMMAND(ID_DRIVER_REFRESH, &DlgDriverModule::OnDriverRefresh)
	ON_NOTIFY(NM_RCLICK, ID_CONTROL_DRIVERMODULE_LIST, &DlgDriverModule::OnNMRClickControlDrivermoduleList)
	ON_COMMAND(ID_DRIVER_MENU_COPY_SING, &DlgDriverModule::OnDriverMenuCopySing)
END_MESSAGE_MAP()


// DlgDriverModule 消息处理程序


BOOL DlgDriverModule::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_Driver_Name, _T("驱动名"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Driver_BaseAddr, _T("基地址"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Driver_Size, _T("大小"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Driver_LoadOrder, _T("加载顺序"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Driver_Object, _T("驱动对象"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Driver_ObjectName, _T("对象名称"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Driver_ServerName, _T("服务名称"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Driver_DigitalSignature, _T("数字签名"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Driver_FilePath, _T("路径"), LVCFMT_LEFT, 500);
	m_CListCtrl.InsertColumn(um_Driver_FileName, _T("公司名"), LVCFMT_LEFT, 250);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;
}

void DlgDriverModule::InsertCtrlListControl(PCDriverInfo pInfo)
{
	if (pInfo == NULL)
	{
		return;
	}


	PCLIST_ENTRY pList = &pInfo->List.List;
	do
	{
		int i = m_CListCtrl.GetItemCount();

		PCDriverInfo pDriverInfo = (PCDriverInfo)pList;

		m_CListCtrl.InsertItem(i, pDriverInfo->ImageBaseName);

		WCHAR str_DllBase[256] = { 0 };
		wsprintf(str_DllBase, L"%I64X", pDriverInfo->ImageBaseAddr);
		m_CListCtrl.SetItemText(i, um_Driver_BaseAddr, str_DllBase);

		WCHAR str_Size[256] = { 0 };
		wsprintf(str_Size, L"0x%08X", pDriverInfo->Size);
		m_CListCtrl.SetItemText(i, um_Driver_Size, str_Size);

		WCHAR str_LoadOrder[256] = { 0 };
		wsprintf(str_LoadOrder, L"%d", i);
		m_CListCtrl.SetItemText(i, um_Driver_LoadOrder, str_LoadOrder);

		WCHAR str_DriverObject[256] = { 0 };
		wsprintf(str_DriverObject, L"%I64X", pDriverInfo->DriverObject);
		m_CListCtrl.SetItemText(i, um_Driver_Object, pDriverInfo->DriverObject ? str_DriverObject : TEXT("--"));

		m_CListCtrl.SetItemText(i, um_Driver_ObjectName, pDriverInfo->DriverObject ? pDriverInfo->ServerName : TEXT("--"));

		m_CListCtrl.SetItemText(i, um_Driver_ServerName, pDriverInfo->DriverObject ? pDriverInfo->DriverName : TEXT("--"));

		CString FilePath = PathTransForm(pDriverInfo->ImageFullBaseName);

		m_CListCtrl.SetItemText(i, um_Driver_FilePath, FilePath.GetBuffer());

		TCHAR szSoftSignBuf[MAXBYTE] = { 0 };
		if (GetSoftSign(FilePath.GetBuffer(), szSoftSignBuf, MAXBYTE) == 0)
		{
			m_CListCtrl.SetItemText(i, um_Driver_DigitalSignature, szSoftSignBuf);
		}
		else
		{
			m_CListCtrl.SetItemText(i, um_Driver_DigitalSignature, TEXT("--"));
		}


		CString szDstFileName;
		m_CListCtrl.SetItemText(i, um_Driver_FileName, TEXT("--"));
		if (this->GetCompanyName(FilePath, szDstFileName))
		{
			m_CListCtrl.SetItemText(i, um_Driver_FileName, (LPWSTR)szDstFileName.GetString());
		}

		//获取下一个节点
		pList = pList->Blink;

		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pDriverInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}

	} while (pList != &pInfo->List.List);

}

void DlgDriverModule::OnNMRClickControlDrivermoduleList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;


	CMenu menu;
	POINT point = { 0 };

	GetCursorPos(&point);//获取当前的游标
	menu.LoadMenuW(ID_MENU_DRIVER);//加载菜单资源
	CMenu* pPopup = menu.GetSubMenu(0);//

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	if (this->m_ThreadFlags == 1)
	{
		menu.EnableMenuItem(ID_DRIVER_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	pPopup->TrackPopupMenu(TPM_LEFTBUTTON, point.x, point.y, this);//设置菜单栏出现的位置
}

void DlgDriverModule::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	RECT rect = { 0 };
	rect.bottom = cy;
	rect.right = cx;
	m_CListCtrl.MoveWindow(&rect, TRUE);
}

void DlgDriverModule::OnDriverRefresh()
{
	// TODO: 在此添加命令处理程序代码
	m_CListCtrl.DeleteAllItems();
	//DWORD lpThreadId = 0;
	//
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserEnumDriverInfo, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//CloseHandle(hThread);

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumDriverInfo, this });

}

void DlgDriverModule::OnDriverMenuCopyName()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_Name);
}

void DlgDriverModule::OnDriverMenuCopyBaseaddr()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_BaseAddr);
}

void DlgDriverModule::OnDriverMenuCopySize()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_Size);
}

void DlgDriverModule::OnDriverMenuCopyLoadorad()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_LoadOrder);
}

void DlgDriverModule::OnDriverMenuCopyObjeceaddr()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_Object);
}

void DlgDriverModule::OnDriverMenuCopyObjectname()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_ObjectName);
}

void DlgDriverModule::OnDriverMenuCopyServername()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_ServerName);
}

void DlgDriverModule::OnDriverMenuCopySing()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_DigitalSignature);
}

void DlgDriverModule::OnDriverMenuCopyPath()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_FilePath);
}

void DlgDriverModule::OnDriverMenuCopyCompany()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Driver_FileName);
}
