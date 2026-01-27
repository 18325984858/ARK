#pragma once
#include "afxdialogex.h"


// DlgGdt 对话框

class DlgGdt : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgGdt)
public:
	enum UmGdt
	{
		um_Gdt_CpuId,
		um_Gdt_GdtBase,
		um_Gdt_GdtTlimit,
		um_Gdt_Index,
		um_Gdt_Base,
		um_Gdt_Tlimit,
		um_Gdt_Granule,
		um_Gdt_Level,
		um_Gdt_Type
	};
public:
	DlgGdt(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgGdt();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_GDT };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

public:
	void InsertCtrlListControl(PCGdtInfo pGdtInfo);
public:
	CListCtrl m_CListCtrl;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnGdtRefresh();
	afx_msg void OnNMRClickGdtList(NMHDR* pNMHDR, LRESULT* pResult);
};
