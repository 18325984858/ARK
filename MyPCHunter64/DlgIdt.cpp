// DlgIdt.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgIdt.h"
#include "resource.h"
#include "Thread.h"
// DlgIdt 对话框

IMPLEMENT_DYNAMIC(DlgIdt, CDialogEx)

DlgIdt::DlgIdt(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_IDT, pParent)
{

}

DlgIdt::~DlgIdt()
{
}

void DlgIdt::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_IDT_LIST, m_CListCtrl);
}


BEGIN_MESSAGE_MAP(DlgIdt, CDialogEx)
	ON_NOTIFY(NM_RCLICK, ID_IDT_LIST, &DlgIdt::OnNMRClickIdtList)
	ON_WM_SIZE()
	ON_COMMAND(ID_IDT_REFRESH, &DlgIdt::OnIdtRefresh)
END_MESSAGE_MAP()


// DlgIdt 消息处理程序


void DlgIdt::OnNMRClickIdtList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	CMenu menu;
	POINT point = { 0 };

	GetCursorPos(&point);//获取当前的游标
	menu.LoadMenuW(ID_MENU_IDT);//加载菜单资源
	CMenu* pPopup = menu.GetSubMenu(0);//

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	if (this->m_ThreadFlags == 1)
	{
		menu.EnableMenuItem(ID_IDT_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	pPopup->TrackPopupMenu(TPM_LEFTBUTTON, point.x, point.y, this);//设置菜单栏出现的位置
}


BOOL DlgIdt::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_Idt_CpuOrd, _T("CPU"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_Idt_IdtNumber, _T("中断号"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_Idt_IdtBase, _T("IDTBase"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Idt_Level, _T("特权级"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_Idt_BaseAddr, _T("Offset"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Idt_Path, _T("所在模块路径"), LVCFMT_LEFT, 250);
	m_CListCtrl.InsertColumn(um_Idt_Company, _T("公司名"), LVCFMT_LEFT, 250);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	//OnIdtRefresh();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}


void DlgIdt::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);


	CRect rect;
	GetClientRect(&rect);

	m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}


void DlgIdt::OnIdtRefresh()
{
	m_CListCtrl.DeleteAllItems();
	//DWORD lpThreadId = 0;
	//
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserEnumIdtInfo, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	////HANDLE hThread = CreateThread(NULL, NULL, EnumIdtThreadProc,/*变量参数地址*/(LPVOID)this, 0, &lpThreadId);
	//CloseHandle(hThread);

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumIdtInfo, this });

}


void DlgIdt::InsertCtrlListControl(PCIdtInfo pIdtInfo)
{

	if (pIdtInfo == NULL)
	{
		return;
	}

	PCLIST_ENTRY pCurList = &pIdtInfo->List.List;
	do
	{
		PCIdtInfo pCurIdtInfo = (PCIdtInfo)pCurList;

		int i = m_CListCtrl.GetItemCount();

		CString StrBuf;

		StrBuf.Format(L"%d", pCurIdtInfo->nCpuId);
		m_CListCtrl.InsertItem(i, StrBuf);

		StrBuf.Format(L"%02X", pCurIdtInfo->nIndex);
		m_CListCtrl.SetItemText(i, um_Idt_IdtNumber, StrBuf);

		StrBuf.Format(L"%016I64X", pCurIdtInfo->IdtBase);
		m_CListCtrl.SetItemText(i, um_Idt_IdtBase, StrBuf);

		StrBuf.Format(L"%d", pCurIdtInfo->IdtData.Dpl);
		m_CListCtrl.SetItemText(i, um_Idt_Level, StrBuf);

		ULONG64 FunAddr = pCurIdtInfo->IdtData.Offset2;
		FunAddr = (((FunAddr << 16) | pCurIdtInfo->IdtData.Offset1) << 16) | pCurIdtInfo->IdtData.Offset0;
		StrBuf.Format(L"%016I64X", FunAddr);
		m_CListCtrl.SetItemText(i, um_Idt_BaseAddr, StrBuf);

		CString Path = PathTransForm(pCurIdtInfo->szPath);
		m_CListCtrl.SetItemText(i, um_Idt_Path, Path);

		GetCompanyName(Path, StrBuf);
		m_CListCtrl.SetItemText(i, um_Idt_Company, StrBuf);


		//指向下一个
		pCurList = pCurList->Blink;

		//释放资源
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pCurIdtInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pIdtInfo->List.List);



}