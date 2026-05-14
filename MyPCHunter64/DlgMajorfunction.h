#pragma once
#include "afxdialogex.h"


// DlgAcpi 对话框

class DlgMajorfunction : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgMajorfunction)
public:
	enum UmAcpi
	{
		um_MajorFuction_Ord,
		um_MajorFuction_FunName,
		um_MajorFuction_FunAddr,
		um_MajorFuction_Pos,
		um_MajorFuction_MoudlePath,
	};

public:
	DlgMajorfunction(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgMajorfunction();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_MAJORFUNCTION };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void InsertCtrlListControl(PCSysMajorFunctionInfo pCSysMajorFunctionInfo);
public:
	CListCtrl m_CListCtrl;
	DriverMajorFunctionInfo m_DriverMajorFunctionInfo;
	afx_msg void OnLvnItemchangedMajorFunctioniList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnDriverMajorFunctionRefresh();
	afx_msg void OnNMRClickMajorfunctionList(NMHDR* pNMHDR, LRESULT* pResult);
};
