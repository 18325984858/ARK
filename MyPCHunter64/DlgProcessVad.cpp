// DlgProcessVad.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgProcessVad.h"
#include "Thread.h"
#include "DlgRWMemory.h"
// DlgProcessVad 对话框

IMPLEMENT_DYNAMIC(DlgProcessVad, CDialogEx)

DlgProcessVad::DlgProcessVad(CString StrEprocess, CString StrProcessName, CString strArchitecture, CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_PROCESS_VAD, pParent)
{
	m_StrEprocess = StrEprocess;
	m_StrProcessName = StrProcessName;
	m_strArchitecture = strArchitecture;
}

DlgProcessVad::~DlgProcessVad()
{
}

void DlgProcessVad::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_PROCESS_VAD_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgProcessVad, CDialogEx)
	ON_WM_SIZE()
	ON_COMMAND(ID_PROCESSVAD_REFRESH, &DlgProcessVad::OnProcessvadRefresh)
	ON_COMMAND(ID_PROCESSVAD_MEMORY, &DlgProcessVad::OnProcessvadMemory)
	ON_NOTIFY(NM_RCLICK, ID_PROCESS_VAD_LIST, &DlgProcessVad::OnRclickProcessVadList)
END_MESSAGE_MAP()

// DlgProcessVad 消息处理程序

void DlgProcessVad::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	CRect rect;
	GetClientRect(&rect);

	m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}

BOOL DlgProcessVad::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(m_StrProcessName);

	m_CListCtrl.InsertColumn(um_Process_Vad_NodeAddr, _T("节点地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_Process_Vad_StartAddr, _T("开始地址"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Process_Vad_EndAddr, _T("结束地址"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Process_Vad_Commit, _T("Commit"), LVCFMT_LEFT, 85);
	m_CListCtrl.InsertColumn(um_Process_Vad_Mode, _T("模式"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Process_Vad_Power, _T("权限"), LVCFMT_LEFT, 100);
	m_CListCtrl.InsertColumn(um_Process_Vad_Path, _T("路径"), LVCFMT_LEFT, 800);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	OnProcessvadRefresh();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgProcessVad::OnProcessvadRefresh()
{
	m_CListCtrl.DeleteAllItems();
	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumProcessVadInfo, this });
}

void DlgProcessVad::OnProcessvadMemory()
{
	// TODO: 在此添加命令处理程序代码
	//获取要读取或写入的节点地址
	POSITION pos = m_CListCtrl.GetFirstSelectedItemPosition() - 1;//获取选中行的行数  pos = 行数 - 1

	CString VadStartAddr = m_CListCtrl.GetItemText((int)pos, um_Process_Vad_StartAddr);
	CString VadEndAddr = m_CListCtrl.GetItemText((int)pos, um_Process_Vad_EndAddr);

	//读写内存
	DlgRWMemory WDlgRwMemory((ULONG64)_wcstoui64(m_StrEprocess.GetBuffer(), 0, 16), (ULONG64)_wcstoui64(VadStartAddr.GetBuffer(), 0, 16), m_strArchitecture == TEXT("x64"));
	WDlgRwMemory.DoModal();
}

void DlgProcessVad::InsertCtrlListControl(PCProcessVadInfo pinfo)
{
	if (pinfo == NULL)
	{
		return;
	}

	PWCHAR MemoryMode[2] = { L"Mapped",L"Private" };
	PWCHAR MemoryPower[8] = { L"NO_ACCESS",L"PAGE_NOACCESS",L"PAGE_READONLY",L"PAGE_READWRITE",
		L"PAGE_WRITECOPY",L"PAGE_EXECUTE",L"PAGE_EXECUTE_READ",L"PAGE_EXECUTE_READWRITE" };

	SIZE_T FreeSize = sizeof(CProcessVadInfo);
	PCLIST_ENTRY pCurList = &pinfo->List.List;


	// 插入前关闭重绘
	m_CListCtrl.SetRedraw(FALSE);
	do
	{
		PCProcessVadInfo pProcessVadInfo = (PCProcessVadInfo)pCurList;
		if (pProcessVadInfo == NULL)
		{
			break;
		}

		int i = m_CListCtrl.GetItemCount();

		CString strBuf;
		strBuf.Format(L"%I64X", pProcessVadInfo->VadNode);
		m_CListCtrl.InsertItem(i, strBuf);

		strBuf.Format(L"%I64X", pProcessVadInfo->StartingVpn);
		m_CListCtrl.SetItemText(i, um_Process_Vad_StartAddr, strBuf);

		strBuf.Format(L"%I64X", pProcessVadInfo->EndingVpn);
		m_CListCtrl.SetItemText(i, um_Process_Vad_EndAddr, strBuf);

		strBuf.Format(L"%08X", pProcessVadInfo->CommitCharge);
		m_CListCtrl.SetItemText(i, um_Process_Vad_Commit, strBuf);

		m_CListCtrl.SetItemText(i, um_Process_Vad_Mode, MemoryMode[pProcessVadInfo->PrivateMemory]);

		CString Protection;
		if (pProcessVadInfo->Protection < 8)
		{
			Protection = MemoryPower[pProcessVadInfo->Protection];
		}
		else
		{
			Protection = MemoryPower[0];
		}
		m_CListCtrl.SetItemText(i, um_Process_Vad_Power, Protection);


		m_CListCtrl.SetItemText(i, um_Process_Vad_Path, pProcessVadInfo->ExeFilePath);

		//指向下一个节点
		pCurList = pCurList->Blink;

		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pProcessVadInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pinfo->List.List);

	// 插入后开启重绘并刷新
	m_CListCtrl.SetRedraw(TRUE);
	m_CListCtrl.Invalidate();
	m_CListCtrl.UpdateWindow();

}

void DlgProcessVad::OnRclickProcessVadList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	if (m_CListCtrl.GetItemCount() < 0)
	{
		return;
	}

	CMenu menu;
	menu.LoadMenu(ID_MENU_PROCESS_VAD);
	CPoint point;
	GetCursorPos(&point);//获取当前的游标


	if (this->m_ThreadFlags == TRUE)
	{
		menu.EnableMenuItem(ID_PROCESSVAD_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	(menu.GetSubMenu(0))->TrackPopupMenu(TPM_LEFTBUTTON, point.x, point.y, this);
}
