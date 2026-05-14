#pragma once
#include "afxdialogex.h"


// DlgHalTable 对话框

class DlgHalTable : public CDialogEx,public CFunction
{
	DECLARE_DYNAMIC(DlgHalTable)

	enum HalTableDlgType
	{
		um_HalTableDlgType_HalDispatchTable,
		um_HalTableDlgType_HalPrivateDispatchTable,
		um_HalTableDlgType_HalAcpiDispatchTable,
	};

	enum HalTableDlgInfo
	{
		um_HalTableDlgInfo_Order,
		um_HalTableDlgInfo_FunName,
		um_HalTableDlgInfo_CurFunAddr,
		um_HalTableDlgInfo_Hook,
		um_HalTableDlgInfo_SrcFunAddr,
		um_HalTableDlgInfo_Pos,
		um_HalTableDlgInfo_CurModule,
		um_HalTableDlgInfo_FileVender
	};

public:
	DlgHalTable(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgHalTable();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_HALTABLE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	LONG64 nPerSel = -1;

	void DlgHalTable::InsertCtrlListControl( );
public:
	CTreeCtrl m_CTreeCtrl;
	CListCtrl m_CListCtrl;
	afx_msg void OnNMDblclkDlgKernelMinifiltercallbackTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnHaltableRefresh();
	afx_msg void OnNMRClickDlgKernelMinifiltercallbackList(NMHDR* pNMHDR, LRESULT* pResult);
};
