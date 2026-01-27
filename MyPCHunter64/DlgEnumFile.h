#pragma once
#include "afxdialogex.h"


// DlgEnumFile 对话框

class DlgEnumFile : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgEnumFile)

public:
	DlgEnumFile(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgEnumFile();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum {
		IDD = ID_DLG_ENUMFILE
	};
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	//删除子节点
	void DlgEnumFile::DelTreeChild(HTREEITEM pNode);

	//清空List控件中所有绑定的数据
	void DlgEnumFile::DelListItemData();
	//枚举文件
	void DlgEnumFile::EnunFile();

	//删除文件
	void DlgEnumFile::DelteFile();

	//解除占用
	void DlgEnumFile::FileFiledeoccupy();

public:
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	CListCtrl m_CListCtrl;
	CTreeCtrl m_CTreeCtrl;
	CString m_CurPath;
	afx_msg void OnDblclkEnumfileTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnFileDelete();
	afx_msg void OnFileRefresh();
	afx_msg void OnNMRClickEnumfileList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnFileFiledeoccupy();
};
