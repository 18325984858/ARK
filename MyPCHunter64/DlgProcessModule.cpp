// DlgProcessModule.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgProcessModule.h"
#include "Thread.h"

// DlgProcessModule 对话框

IMPLEMENT_DYNAMIC(DlgProcessModule, CDialogEx)

DlgProcessModule::DlgProcessModule(CString StrEprocess, CString StrProcessName, CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_PROCESS_MODULE, pParent)
{
	m_StrEprocess = StrEprocess;
	m_StrProcessName = StrProcessName;
}

DlgProcessModule::~DlgProcessModule()
{
}

void DlgProcessModule::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_PROCESS_MODULE_LIST, m_CListCtrl);
}


BEGIN_MESSAGE_MAP(DlgProcessModule, CDialogEx)
	ON_COMMAND(ID_PROCESSMODULE_REFRESH, &DlgProcessModule::OnProcessmoduleRefresh)
	ON_WM_SIZE()
	ON_NOTIFY(NM_RCLICK, ID_PROCESS_MODULE_LIST, &DlgProcessModule::OnNMRClickProcessModuleList)
END_MESSAGE_MAP()


// DlgProcessModule 消息处理程序


void DlgProcessModule::OnProcessmoduleRefresh()
{
	m_CListCtrl.DeleteAllItems();
	//DWORD lpThreadId = 0;
	////HANDLE hThread = CreateThread(NULL, NULL, EnumProcessModuleThreadProc,/*变量参数地址*/(LPVOID)this, 0, &lpThreadId);
	//
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserEnumProcessModuleInfo, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//CloseHandle(hThread);

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumProcessModuleInfo, this });
}


BOOL DlgProcessModule::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(m_StrProcessName.GetBuffer());//设置窗口标题


	m_CListCtrl.InsertColumn(um_Process_Module_Name, _T("名称"), LVCFMT_LEFT, 200);
	m_CListCtrl.InsertColumn(um_Process_Module_BaseAddr, _T("模块基址"), LVCFMT_LEFT, 150);
	m_CListCtrl.InsertColumn(um_Process_Module_Size, _T("大小"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Process_Module_Path, _T("模块路径"), LVCFMT_LEFT, 300);
	m_CListCtrl.InsertColumn(um_Process_Module_Signal, _T("数字签名"), LVCFMT_LEFT, 150);
	m_CListCtrl.InsertColumn(um_Process_Module_Company, _T("公司名"), LVCFMT_LEFT, 150);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	OnProcessmoduleRefresh();
	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}


void DlgProcessModule::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);


	RECT rect = { 0 };
	rect.bottom = cy;
	rect.right = cx;
	m_CListCtrl.MoveWindow(&rect, TRUE);
}

void DlgProcessModule::InsertCtrlListControl(PCProcessModuleInfo pinfo)
{
	if (pinfo == NULL)
	{
		return;
	}

	SIZE_T FreeSize = sizeof(CProcessModuleInfo);
	PCLIST_ENTRY pCurList = &pinfo->List.List;

	do
	{
		if (pCurList == NULL)
		{
			break;
		}

		PCProcessModuleInfo pProcessModuleInfo = (PCProcessModuleInfo)pCurList;
		{
			int  i = m_CListCtrl.GetItemCount();

			CString StrBuf;

			//插入名称
			m_CListCtrl.InsertItem(i, pProcessModuleInfo->ModuleName);

			StrBuf.Format(L"0x%I64X", pProcessModuleInfo->ModuleBaseAddr);		
			m_CListCtrl.SetItemText(i, 1, StrBuf);

			StrBuf.Format(L"0x%08I64X", pProcessModuleInfo->ModuleSize);
			m_CListCtrl.SetItemText(i, 2, StrBuf);

			m_CListCtrl.SetItemText(i, 3, pProcessModuleInfo->ModuleFullPath);

			TCHAR szSoftSignBuf[MAXBYTE] = { 0 };
			if (GetSoftSign(pProcessModuleInfo->ModuleFullPath, szSoftSignBuf, MAXBYTE) == 0)
			{
				m_CListCtrl.SetItemText(i, 4, szSoftSignBuf);
			}
			else
			{
				m_CListCtrl.SetItemText(i, 4, TEXT("--"));
			}

			CString szDstFileName;
			m_CListCtrl.SetItemText(i, 5, this->GetCompanyName(pProcessModuleInfo->ModuleFullPath, szDstFileName) ? (LPWSTR)szDstFileName.GetString() : TEXT("--"));

		}
		//指向下一个
		pCurList = pCurList->Blink;

		//释放内存
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pProcessModuleInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pinfo->List.List);


}

void DlgProcessModule::OnNMRClickProcessModuleList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	if (m_CListCtrl.GetItemCount() < 0)
	{
		return;
	}

	CMenu menu;
	menu.LoadMenu(ID_MENU_PROCESS_MODULE);
	CPoint point;
	GetCursorPos(&point);//获取当前的游标


	if (this->m_ThreadFlags == TRUE)
	{
		menu.EnableMenuItem(ID_PROCESSMODULE_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	(menu.GetSubMenu(0))->TrackPopupMenu(TPM_LEFTBUTTON, point.x, point.y, this);
}
