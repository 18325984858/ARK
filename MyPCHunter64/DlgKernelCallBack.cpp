// DlgKernelCallBack.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgKernelCallBack.h"
#include "resource.h"
#include "Thread.h"
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
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;


	CMenu menu;
	POINT point = { 0 };

	GetCursorPos(&point);//获取当前的游标
	menu.LoadMenuW(ID_MENU_KERNELCALLBACK);//加载菜单资源
	CMenu* pPopup = menu.GetSubMenu(0);//

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	if (this->m_ThreadFlags == 1)
	{
		menu.EnableMenuItem(ID_KERNELCALLBACK_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	pPopup->TrackPopupMenu(TPM_LEFTBUTTON, point.x, point.y, this);//设置菜单栏出现的位置

}


void DlgKernelCallBack::OnKernelcallbackRefresh()
{
	m_CListCtrl.DeleteAllItems();
	//DWORD lpThreadId = 0;
	////HANDLE hThread = CreateThread(NULL, NULL, EnumKernelCallBackThreadProc,/*变量参数地址*/(LPVOID)this, 0, &lpThreadId);
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserEnumKernelCallBackInfo, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//
	//CloseHandle(hThread);

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

