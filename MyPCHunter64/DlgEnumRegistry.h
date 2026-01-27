#pragma once
#include "afxdialogex.h"


// DlgEnumRegistry 对话框

class DlgEnumRegistry : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgEnumRegistry)

public:
	DlgEnumRegistry(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgEnumRegistry();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_ENUMREGISTRY };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgEnumRegistry::DelTreeChild(HTREEITEM pNode);
	void InsertCtrlListControl();
public:
	CTreeCtrl m_CTreeCtrl;
	CListCtrl m_CListCtrl;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	ULONG64 InitControl();
	afx_msg void OnNMDblclkEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult);
};
