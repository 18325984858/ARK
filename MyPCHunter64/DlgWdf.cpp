// DlgWdf.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgWdf.h"
#include "Thread.h"

// DlgWdf 对话框

IMPLEMENT_DYNAMIC(DlgWdf, CDialogEx)

DlgWdf::DlgWdf(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_WDF, pParent)
{

}

DlgWdf::~DlgWdf()
{
}

void DlgWdf::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_DLG_KERNEL_WDF_TREE, m_CTreeCtrl);
	DDX_Control(pDX, ID_DLG_KERNEL_WDF_LIST, m_CListCtrl);
}


BEGIN_MESSAGE_MAP(DlgWdf, CDialogEx)
	ON_WM_SIZE()
	ON_NOTIFY(NM_DBLCLK, ID_DLG_KERNEL_WDF_TREE, &DlgWdf::OnNMDblclkDlgKernelWdfTree)
END_MESSAGE_MAP()


// DlgWdf 消息处理程序


void DlgWdf::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	CRect rect;
	GetClientRect(&rect);

	float fwidth = rect.Width() / 4;

	m_CTreeCtrl.SetWindowPos(NULL, 0, 0, fwidth, rect.Height(), SWP_NOZORDER);
	m_CListCtrl.SetWindowPos(NULL, fwidth, 0, rect.Width() - fwidth, rect.Height(), SWP_NOZORDER);
	// TODO: 在此处添加消息处理程序代码
}


BOOL DlgWdf::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	//初始化树控件

	//初始化树控件
	m_CTreeCtrl.SetExtendedStyle(m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
		m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS);

	//创建根节点
	auto RootNode = m_CTreeCtrl.InsertItem(L"Wdf项目");
	//创建子节点1
	auto ChildNode0 = m_CTreeCtrl.InsertItem(L"Wdf01000派发函数", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode0, um_WdfDlgInfoType_Wdf01000Maj);
	//创建子节点2
	auto ChildNode1 = m_CTreeCtrl.InsertItem(L"WdfFunction", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode1, um_WdfDlgInfoType_WdfFunction);


	//初始化List控件
	m_CListCtrl.InsertColumn(um_WdfDlgInfo_Order, _T("序号"), LVCFMT_LEFT, 70);
	m_CListCtrl.InsertColumn(um_WdfDlgInfo_FunctionName, _T("函数名称"), LVCFMT_LEFT, 150);
	m_CListCtrl.InsertColumn(um_WdfDlgInfo_FunctionAddr, _T("当前函数地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_WdfDlgInfo_Hook, _T("HOOK"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_WdfDlgInfo_SourceFunctionAddr, _T("原始函数地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_WdfDlgInfo_Module, _T("当前函数所在模块路径"), LVCFMT_LEFT, 300);
	m_CListCtrl.InsertColumn(um_WdfDlgInfo_FileVender, _T("文件厂商"), LVCFMT_LEFT, 125);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}


void DlgWdf::OnNMDblclkDlgKernelWdfTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	auto hSelectItem = m_CTreeCtrl.GetSelectedItem();

	//获取绑定的数据
	int SelType = m_CTreeCtrl.GetItemData(hSelectItem);
	switch (SelType)
	{
	case um_WdfDlgInfoType_Wdf01000Maj:
	{
		if (nPerSel == um_WdfDlgInfoType_Wdf01000Maj)
		{
			return;
		}

		//刷新显示的数据
		OnHaltableRefresh();

		//重新设置选择的地方
		nPerSel = um_WdfDlgInfoType_Wdf01000Maj;
	}
	break;
	case um_WdfDlgInfoType_WdfFunction:
	{

		if (nPerSel == um_WdfDlgInfoType_WdfFunction)
		{
			return;
		}

		//刷新显示的数据
		OnHaltableRefresh();


		//重新设置选择的地方
		nPerSel = um_WdfDlgInfoType_WdfFunction;
	}
	break;
	default:
		nPerSel = 0;
		break;
	}
}

void DlgWdf::OnHaltableRefresh()
{
	m_CListCtrl.DeleteAllItems();
	//DWORD lpThreadId = 0;
	////HANDLE hThread = CreateThread(NULL, NULL, EnumWdf,/*变量参数地址*/(LPVOID)this, 0, &lpThreadId);
	//
	//
	//CThreadInfo* pThread = new CThreadInfo{ _LoadDriver::Um_UserCallBackType_UserEnumWdfInfo, this };
	//HANDLE hThread = CreateThread(NULL, NULL, UniversalThreadFunction,/*变量参数地址*/(LPVOID)pThread, 0, &lpThreadId);
	//
	//CloseHandle(hThread);

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumWdfInfo, this });

}

void DlgWdf::InsertCtrlListControl(PCWdfInfo pWdfInfo)
{
	if (pWdfInfo == NULL)
	{
		return;
	}

	PCLIST_ENTRY pCurList = &pWdfInfo->List.List;

	do 
	{
		if (pCurList == NULL)
		{
			break;
		}
		ULONG64 i = m_CListCtrl.GetItemCount();
		PCWdfInfo pInfo = (PCWdfInfo)pCurList;


		CString StrBuf;
		StrBuf.Format(L"%04X", pInfo->pFunOrder);
		m_CListCtrl.InsertItem(i, StrBuf);
// 
// 		if (NameTable[nPerSel] != NULL)
// 		{
// 			m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_FunName, (NameTable[nPerSel])[i % LengthTable[nPerSel]]);
// 		}
// 
// 		StrBuf.Format(L"%016I64X", pInfo->pFunAddr);
// 		m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_CurFunAddr, StrBuf);
// 
// 
// 		CString FilePath = PathTransForm(pInfo->ModulePath);
// 		m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_CurModule, pInfo->pFunAddr == NULL ? L"--" : FilePath.GetBuffer());
// 
// 		CString szDstFileName;
// 		m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_FileVender, TEXT("--"));
// 		if (GetCompanyName(FilePath, szDstFileName))
// 		{
// 			m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_FileVender, (LPWSTR)szDstFileName.GetString());
// 		}


		pCurList = pCurList->Blink;
		//释放资源
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pWdfInfo->List.List);

}