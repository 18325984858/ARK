// DlgDpc.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgDpc.h"
#include "Thread.h"
#include "PdbResolver.h"
#include <unordered_map>
#include <string>

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
	*pResult = 0;

	static const int cols[] = {
		um_Dpc_DpcObject, um_Dpc_TimeObject, um_Dpc_TriggerCycle,
		um_Dpc_FunctionStartAddr, um_Dpc_FunctionName, um_Dpc_ModulePath, um_Dpc_CompanyName
	};
	static const wchar_t* const names[] = {
		L"Dpc对象", L"Time对象", L"触发周期", L"函数入口地址", L"函数名称", L"函数所在模块路径", L"文件厂商"
	};
	const int n = (int)_countof(cols);

	bool hasSel = (m_CListCtrl.GetFirstSelectedItemPosition() != NULL);
	int r = ShowListCopyRefreshMenu(cols, names, n, hasSel, this, &m_CListCtrl);
	if (r == 0) { if (this->m_ThreadFlags != 1) OnDpcRefresh(); }
	else if (r > 0) CopyBufferToClipboard(&m_CListCtrl, cols[r - 1]);
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
	m_CListCtrl.InsertColumn(um_Dpc_FunctionName, _T("函数名称"), LVCFMT_LEFT, 250);
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
	// 本次刷新的厂商名缓存，避免重复对同一路径的 GetFileVersionInfo 磁盘 I/O。
	std::unordered_map<std::wstring, CString> companyCache;

	// 批量插入期间关闭重绘，避免每行一次 WM_PAINT 风暴。
	m_CListCtrl.SetRedraw(FALSE);

	if (pInfo == NULL)
	{
		m_CListCtrl.SetRedraw(TRUE);
		m_CListCtrl.Invalidate();
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

		// 函数名称：走 PdbResolver（kModuleBase=0 让 resolver 从内核模块缓存里查）
		{
			WCHAR resolved[256] = { 0 };
			PdbResolver_Resolve(Info->FunCtionStartAddr, 0,
				Info->ModulePath, resolved, _countof(resolved));
			m_CListCtrl.SetItemText(i, um_Dpc_FunctionName, resolved);
		}

		CString FilePath = PathTransForm(Info->ModulePath);
		m_CListCtrl.SetItemText(i, um_Dpc_ModulePath, FilePath.GetBuffer());

		CString company = TEXT("--");
		auto it = companyCache.find(std::wstring(FilePath.GetString()));
		if (it != companyCache.end())
		{
			company = it->second;
		}
		else
		{
			CString szDstFileName;
			if (this->GetCompanyName(FilePath, szDstFileName))
			{
				company = szDstFileName;
			}
			companyCache.emplace(std::wstring(FilePath.GetString()), company);
		}
		m_CListCtrl.SetItemText(i, um_Dpc_CompanyName, (LPWSTR)company.GetString());

		pCurList = pCurList->Blink;
		//释放内存
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&Info, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}

	} while (pCurList != &pInfo->List.List);

	m_CListCtrl.SetRedraw(TRUE);
	m_CListCtrl.Invalidate();
}