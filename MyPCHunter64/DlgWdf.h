#pragma once
#include "afxdialogex.h"


// DlgWdf 对话框

class DlgWdf : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgWdf)

public:
	enum UmWdfDlgInfoType
	{
		um_WdfDlgInfoType_Wdf01000Maj,
		um_WdfDlgInfoType_WdfFunction
	};

	enum UMWdfInfo
	{
		um_WdfDlgInfo_Order,
		um_WdfDlgInfo_FunctionName,
		um_WdfDlgInfo_FunctionAddr,
		um_WdfDlgInfo_Hook,
		um_WdfDlgInfo_SourceFunctionAddr,
		um_WdfDlgInfo_Module,
		um_WdfDlgInfo_FileVender
	};

public:
	DlgWdf(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgWdf();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_WDF };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgWdf::OnHaltableRefresh();
public:
	void DlgWdf::InsertCtrlListControl(PCWdfInfo pInfo);
public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	CTreeCtrl m_CTreeCtrl;
	CListCtrl m_CListCtrl;
	int nPerSel = 0;
	afx_msg void OnNMDblclkDlgKernelWdfTree(NMHDR* pNMHDR, LRESULT* pResult);
};
