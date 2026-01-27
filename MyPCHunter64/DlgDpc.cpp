// DlgDpc.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgDpc.h"
#include "Thread.h"

// DlgDpc 对话框

IMPLEMENT_DYNAMIC(DlgDpc, CDialogEx)

DlgDpc::DlgDpc(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_DPC, pParent)
{

}

DlgDpc::~DlgDpc()
{
}

void DlgDpc::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_DPC_LIST, m_CListCtrl);
}


BEGIN_MESSAGE_MAP(DlgDpc, CDialogEx)
	ON_NOTIFY(NM_RCLICK, ID_DPC_LIST, &DlgDpc::OnNMRClickDpcList)
	ON_WM_SIZE()
	ON_COMMAND(ID_DPC_REFRESH, &DlgDpc::OnDpcRefresh)
END_MESSAGE_MAP()


// DlgDpc 消息处理程序
void DlgDpc::OnNMRClickDpcList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	CMenu menu;
	POINT point = { 0 };

	GetCursorPos(&point);//获取当前的游标
	menu.LoadMenuW(ID_MENU_DPC);//加载菜单资源
	CMenu* pPopup = menu.GetSubMenu(0);//

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	if (this->m_ThreadFlags == 1)
	{
		menu.EnableMenuItem(ID_DPC_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	pPopup->TrackPopupMenu(TPM_LEFTBUTTON, point.x, point.y, this);//设置菜单栏出现的位置
}

void DlgDpc::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	CRect rect;
	GetClientRect(&rect);

	m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}

BOOL DlgDpc::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_Dpc_DpcObject, _T("Dpc对象"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Dpc_TimeObject, _T("Time对象"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Dpc_TriggerCycle, _T("触发周期"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Dpc_FunctionStartAddr, _T("函数入口地址"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Dpc_ModulePath, _T("函数所在模块路径"), LVCFMT_LEFT, 250);
	m_CListCtrl.InsertColumn(um_Dpc_CompanyName, _T("文件厂商"), LVCFMT_LEFT, 130);

	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);



	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgDpc::OnDpcRefresh()
{
	m_CListCtrl.DeleteAllItems();
	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumDpcInfo, this });
}

void DlgDpc::InsertCtrlListControl(PCDPcInfo pInfo)
{
	if (pInfo == NULL)
	{
		return;
	}
	PCLIST_ENTRY pCurList = &pInfo->List.List;
	do
	{
		if (pCurList == NULL)
		{
			break;
		}

		ULONG64 i = m_CListCtrl.GetItemCount();
		PCDPcInfo Info = (PCDPcInfo)pCurList;
		CString StrBuf;

		//插入数据
		StrBuf.Format(L"%016I64X", Info->DpcObject);
		m_CListCtrl.InsertItem(i, StrBuf);

		StrBuf.Format(L"%016I64X", Info->TimeObject);
		m_CListCtrl.SetItemText(i, um_Dpc_TimeObject, StrBuf);

		StrBuf.Format(L"%016I64X", Info->TriggerCycle);
		m_CListCtrl.SetItemText(i, um_Dpc_TriggerCycle, StrBuf);

		StrBuf.Format(L"%016I64X", Info->FunCtionStartAddr);
		m_CListCtrl.SetItemText(i, um_Dpc_FunctionStartAddr, StrBuf);

		CString FilePath = PathTransForm(Info->ModulePath);
		m_CListCtrl.SetItemText(i, um_Dpc_ModulePath, FilePath.GetBuffer());

		CString szDstFileName;
		m_CListCtrl.SetItemText(i, um_Dpc_CompanyName, TEXT("--"));
		if (this->GetCompanyName(FilePath, szDstFileName))
		{
			m_CListCtrl.SetItemText(i, um_Dpc_CompanyName, (LPWSTR)szDstFileName.GetString());
		}

		pCurList = pCurList->Blink;
		//释放内存
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&Info, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}

	} while (pCurList != &pInfo->List.List);
}