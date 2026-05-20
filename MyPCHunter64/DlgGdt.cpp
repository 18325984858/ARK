// DlgGdt.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgGdt.h"
#include "Thread.h"

// DlgGdt 对话框

IMPLEMENT_DYNAMIC(DlgGdt, CDialogEx)

DlgGdt::DlgGdt(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_GDT, pParent)
{

}

DlgGdt::~DlgGdt()
{
}

void DlgGdt::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_GDT_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgGdt, CDialogEx)
	ON_WM_SIZE()
	ON_COMMAND(ID_GDT_REFRESH, &DlgGdt::OnGdtRefresh)
	ON_NOTIFY(NM_RCLICK, ID_GDT_LIST, &DlgGdt::OnNMRClickGdtList)
END_MESSAGE_MAP()

// DlgGdt 消息处理程序

void DlgGdt::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	CRect rect;
	GetClientRect(&rect);

	m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}

BOOL DlgGdt::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_Gdt_CpuId, _T("CPU"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_Gdt_GdtBase, _T("GDTBase"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Gdt_GdtTlimit, _T("GDTTlimit"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Gdt_Index, _T("索引"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_Gdt_Base, _T("基址"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Gdt_Tlimit, _T("边界"), LVCFMT_LEFT, 120);
	m_CListCtrl.InsertColumn(um_Gdt_Granule, _T("颗粒"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Gdt_Level, _T("特权级"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_Gdt_Type, _T("类型"), LVCFMT_LEFT, 250);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	//OnGdtRefresh();

	return TRUE;
}

void DlgGdt::InsertCtrlListControl(PCGdtInfo pGdtInfo)
{
	if (pGdtInfo == NULL)
	{
		return;
	}

	//代码数据段
	CString SegmentDataCode[] = {
		L"Data - Read-Only",
		L"Data - Read-Only,accessed",
		L"Data - Read/Write",
		L"Data - Read/Write,accessed"
		L"Data - Read-Only, expand-down",
		L"Data - Read-Only, expand-down, accessed",
		L"Data - Read/Write, expand-down",
		L"Data - Read/Write, expand-down, accessed",
		L"Code - Execute-Only",
		L"Code - Execute-Only, accessed",
		L"Code - Execute/Read",
		L"Code - Execute/Read, accessed",
		L"Code - Execute-Only, conforming",
		L"Code - Execute-Only, conforming, accessed",
		L"Code - Execute/Read, conforming",
		L"Code - Execute/Read, conforming, accessed"
	};

	CString SystemSegment32[] = {
		L"Reserved",
		L"16-bit TSS (Available)",
		L"LDT",
		L"16-bit TSS (Busy)",
		L"16-bit Call Gate",
		L"Task Gate",
		L"16-bit Interrupt Gate",
		L"16-bit Trap Gate",
		L"Reserved",
		L"32-bit TSS (Available)",
		L"Reserved",
		L"32-bit TSS (Busy)",
		L"32-bit Call Gate",
		L"Reserved",
		L"32-bit Interrupt Gate",
		L"32-bit Trap Gate",
	};

	CString SystemSegment64[] = {
	L" Upper 8 bytes of an 16-byte descriptor",
	L"Reserved",
	L"LDT",
	L"Reserved",
	L"Reserved",
	L"Reserved",
	L"Reserved",
	L"Reserved",
	L"Reserved",
	L"64-bit TSS (Available)",
	L"Reserved",
	L"64-bit TSS (Busy)",
	L"64-bit Call Gate",
	L"Reserved",
	L"64-bit Interrupt Gate",
	L"64-bit Trap Gate",
	};

	ULONG64 nIndex = 0;

	SIZE_T FreeSize = sizeof(CGdtInfo);
	PCLIST_ENTRY pCurList = &pGdtInfo->List;
	do
	{
		if (pCurList == NULL)
		{
			return;
		}

		PCGdtInfo pCurGdtInfo = (PCGdtInfo)pCurList;

		int InsertIndex = m_CListCtrl.GetItemCount();

		CString StrBuf;
		//插入CPUID
		StrBuf.Format(L"%d", pCurGdtInfo->nCpuId);
		m_CListCtrl.InsertItem(InsertIndex, StrBuf);

		//插入GdtBase
		StrBuf.Format(L"%016I64X", pCurGdtInfo->GdtBase);
		m_CListCtrl.SetItemText(InsertIndex, um_Gdt_GdtBase, StrBuf);

		//插入GdtTlimit
		//StrBuf.Format(L"%d", pCurGdtInfo->);
		//m_CListCtrl.SetItemText(m_CListCtrl.GetItemCount(), um_Gdt_GdtTlimit, StrBuf);

		//插入索引
		StrBuf.Format(L"%d", pCurGdtInfo->nIndex);
		m_CListCtrl.SetItemText(InsertIndex, um_Gdt_Index, StrBuf);

		CString BaseAddr;
		CString Tlimit;
		CString Granule;

		//判断是否是64位段
		if (pCurGdtInfo->Is64Segment == TRUE)
		{
			ULONG64 dqBaseAddr = pCurGdtInfo->GdtData1.dwBaseAddr;
			dqBaseAddr = (((((dqBaseAddr << 8) | pCurGdtInfo->GdtData.BaseAddr2) << 8) | pCurGdtInfo->GdtData.BaseAddr1) << 16) | pCurGdtInfo->GdtData.BaseAddr0;
			BaseAddr.Format(L"%016I64X", dqBaseAddr);

			ULONG64 dqLimit = pCurGdtInfo->GdtData1.dwBaseLimit;
			dqLimit = (((dqLimit << 4) | pCurGdtInfo->GdtData.SegLimit1) << 16) | pCurGdtInfo->GdtData.SegLimit0;

			if (pCurGdtInfo->GdtData.G)
			{
				Tlimit.Format(L"%016I64X", (dqLimit * 0x1000) + 0xFFF);
				Granule = L"PAGE";
			}
			else
			{
				Tlimit.Format(L"%016I64X", dqLimit);
				Granule = L"BYTE";
			}
		}
		else
		{
			ULONG64 dwBaseAddr = pCurGdtInfo->GdtData.BaseAddr2;
			dwBaseAddr = (((dwBaseAddr << 8) | pCurGdtInfo->GdtData.BaseAddr1) << 8) | pCurGdtInfo->GdtData.BaseAddr0;
			BaseAddr.Format(L"%016I64X", dwBaseAddr);

			ULONG64 dqLimit = pCurGdtInfo->GdtData.SegLimit1;
			dqLimit = (dqLimit << 16) | pCurGdtInfo->GdtData.SegLimit0;

			if (pCurGdtInfo->GdtData.G)
			{
				Tlimit.Format(L"%016I64X", (dqLimit * 0x1000) + 0xFFF);
				Granule = L"PAGE";
			}
			else
			{
				Tlimit.Format(L"%016I64X", dqLimit);
				Granule = L"BYTE";
			}
		}
		//地址
		m_CListCtrl.SetItemText(InsertIndex, um_Gdt_Base, BaseAddr);

		//段边界
		m_CListCtrl.SetItemText(InsertIndex, um_Gdt_Tlimit, Tlimit);

		//颗粒
		m_CListCtrl.SetItemText(InsertIndex, um_Gdt_Granule, Granule);

		//插入特权级
		StrBuf.Format(L"%d", pCurGdtInfo->GdtData.Dpl);
		m_CListCtrl.SetItemText(InsertIndex, um_Gdt_Level, StrBuf);

		//类型
		if (pCurGdtInfo->GdtData.S)
		{
			//代码段 数据段
			m_CListCtrl.SetItemText(InsertIndex, um_Gdt_Type, SegmentDataCode[pCurGdtInfo->GdtData.Type]);
		}
		else
		{
			m_CListCtrl.SetItemText(InsertIndex, um_Gdt_Type, SystemSegment64[pCurGdtInfo->GdtData.Type]);

			////系统段
			//if (pCurGdtInfo->GdtData.L == 1)
			//{
			//	m_CListCtrl.SetItemText(InsertIndex, um_Gdt_Type, SystemSegment64[pCurGdtInfo->GdtData.Type]);
			//}
			//else
			//{
			//	m_CListCtrl.SetItemText(InsertIndex, um_Gdt_Type, SystemSegment32[pCurGdtInfo->GdtData.Type]);
			//}
		}

		pCurList = pCurList->Blink;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pCurGdtInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pGdtInfo->List);
}

void DlgGdt::OnGdtRefresh()
{
	m_CListCtrl.DeleteAllItems();

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumGdtInfo, this });

}

void DlgGdt::OnNMRClickGdtList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	int r = ShowListContextMenu(&m_CListCtrl, this);
	if (r == 0) { if (this->m_ThreadFlags != 1) OnGdtRefresh(); }
	else if (r > 0) CopyBufferToClipboard(&m_CListCtrl, r - 1);
}
