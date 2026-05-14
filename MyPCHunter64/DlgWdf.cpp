// DlgWdf.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgWdf.h"
#include "Thread.h"
#include "MyPCHunter64Dlg.h"
#include "PdbResolver.h"
#include <unordered_map>
#include <string>

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
	ON_WM_CONTEXTMENU()
	ON_NOTIFY(NM_DBLCLK, ID_DLG_KERNEL_WDF_TREE, &DlgWdf::OnNMDblclkDlgKernelWdfTree)
	ON_NOTIFY(NM_RCLICK, ID_DLG_KERNEL_WDF_LIST, &DlgWdf::OnNMRClickDlgKernelWdfList)
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
	std::unordered_map<std::wstring, CString> companyCache;

	m_CListCtrl.SetRedraw(FALSE);

	if (pWdfInfo == NULL)
	{
		m_CListCtrl.SetRedraw(TRUE);
		m_CListCtrl.Invalidate();
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
		StrBuf.Format(L"%04X", (USHORT)pInfo->pFunOrder);
		m_CListCtrl.InsertItem(i, StrBuf);

		// 函数名称：统一走 PdbResolver；PDB 未就绪/无 PDB 时返回 "Module+0xRVA"
		StrBuf.Empty();
		if (pInfo->pFunAddr != 0 && pInfo->ModuleBase != 0 && pInfo->ModulePath[0] != 0)
		{
			WCHAR sym[256] = { 0 };
			PdbResolver_Resolve(pInfo->pFunAddr, pInfo->ModuleBase,
				pInfo->ModulePath, sym, _countof(sym));
			StrBuf = sym;
		}
		else
		{
			StrBuf.Format(L"WdfFunctions[%u]", (UINT)pInfo->pFunOrder);
		}
		m_CListCtrl.SetItemText(i, um_WdfDlgInfo_FunctionName, StrBuf);

		StrBuf.Format(L"%016I64X", pInfo->pFunAddr);
		m_CListCtrl.SetItemText(i, um_WdfDlgInfo_FunctionAddr, StrBuf);

		m_CListCtrl.SetItemText(i, um_WdfDlgInfo_Hook, pInfo->HookType ? L"已HOOK" : L"未HOOK");

		if (pInfo->pSrcFunAddr)
		{
			StrBuf.Format(L"%016I64X", pInfo->pSrcFunAddr);
			m_CListCtrl.SetItemText(i, um_WdfDlgInfo_SourceFunctionAddr, StrBuf);
		}
		else
		{
			m_CListCtrl.SetItemText(i, um_WdfDlgInfo_SourceFunctionAddr, L"--");
		}

		CString FilePath = pInfo->ModulePath[0] ? PathTransForm(pInfo->ModulePath) : CString(L"--");
		m_CListCtrl.SetItemText(i, um_WdfDlgInfo_Module, FilePath.GetBuffer());

		CString company = TEXT("--");
		if (pInfo->ModulePath[0])
		{
			auto it = companyCache.find(std::wstring(FilePath.GetString()));
			if (it != companyCache.end())
			{
				company = it->second;
			}
			else
			{
				CString szDstFileName;
				if (GetCompanyName(FilePath, szDstFileName))
				{
					company = szDstFileName;
				}
				companyCache.emplace(std::wstring(FilePath.GetString()), company);
			}
		}
		m_CListCtrl.SetItemText(i, um_WdfDlgInfo_FileVender, (LPWSTR)company.GetString());

		pCurList = pCurList->Blink;
		//释放资源
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pWdfInfo->List.List);

	m_CListCtrl.SetRedraw(TRUE);
	m_CListCtrl.Invalidate();
}

// 右键菜单：刷新 + 复制各列。动态构建，无需 .rc 资源。
void DlgWdf::OnNMRClickDlgKernelWdfList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	LPNMITEMACTIVATE pIA = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	LOGI("[DlgWdf] NM_RCLICK fired idFrom=%u iItem=%d", (unsigned)pNMHDR->idFrom, pIA ? pIA->iItem : -1);

	static const struct { UINT id; LPCWSTR text; int col; } kCopyItems[] = {
		{ 2001, L"序号",         um_WdfDlgInfo_Order },
		{ 2002, L"函数名称",     um_WdfDlgInfo_FunctionName },
		{ 2003, L"当前函数地址", um_WdfDlgInfo_FunctionAddr },
		{ 2004, L"HOOK",         um_WdfDlgInfo_Hook },
		{ 2005, L"原始函数地址", um_WdfDlgInfo_SourceFunctionAddr },
		{ 2006, L"模块路径",     um_WdfDlgInfo_Module },
		{ 2007, L"文件厂商",     um_WdfDlgInfo_FileVender },
	};
	const UINT kRefreshId = 2000;

	BOOL hasSel = (m_CListCtrl.GetFirstSelectedItemPosition() != NULL);

	CMenu copySub;
	copySub.CreatePopupMenu();
	for (auto& it : kCopyItems)
	{
		copySub.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), it.id, it.text);
	}

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenuW(MF_POPUP | (hasSel ? 0 : MF_GRAYED), (UINT_PTR)copySub.GetSafeHmenu(), L"复制");
	menu.AppendMenuW(MF_STRING, kRefreshId, L"刷新");
	copySub.Detach(); // 所有权已交给 menu，避免双重销毁

	POINT pt = { 0 };
	GetCursorPos(&pt);
	UINT cmd = menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, this);
	LOGI("[DlgWdf] TrackPopupMenu returned cmd=%u pt=(%ld,%ld) hasSel=%d", cmd, pt.x, pt.y, (int)hasSel);
	if (cmd == kRefreshId)
	{
		LOGI("[DlgWdf] -> Refresh");
		OnHaltableRefresh();
		return;
	}
	for (auto& it : kCopyItems)
	{
		if (cmd == it.id)
		{
			LOGI("[DlgWdf] -> Copy col=%d", it.col);
			CopyBufferToClipboard(&m_CListCtrl, it.col);
			return;
		}
	}
}

// 后备路径：WM_CONTEXTMENU。某些键盘菜单键或父窗口吞掉 NM_RCLICK 时使用。
void DlgWdf::OnContextMenu(CWnd* pWnd, CPoint point)
{
	LOGI("[DlgWdf] WM_CONTEXTMENU pWnd=%p (m_CListCtrl=%p m_CTreeCtrl=%p) pt=(%ld,%ld)",
		pWnd ? pWnd->GetSafeHwnd() : nullptr,
		m_CListCtrl.GetSafeHwnd(), m_CTreeCtrl.GetSafeHwnd(), point.x, point.y);
	if (pWnd && pWnd->GetSafeHwnd() == m_CListCtrl.GetSafeHwnd())
	{
		// 复用 NM_RCLICK 处理：构造一个空的 NMITEMACTIVATE
		NMITEMACTIVATE nm = { 0 };
		nm.hdr.hwndFrom = m_CListCtrl.GetSafeHwnd();
		nm.hdr.idFrom = ID_DLG_KERNEL_WDF_LIST;
		nm.hdr.code = NM_RCLICK;
		nm.iItem = -1;
		LRESULT r = 0;
		OnNMRClickDlgKernelWdfList((NMHDR*)&nm, &r);
		return;
	}
	CDialogEx::OnContextMenu(pWnd, point);
}