#pragma once
#include "afxdialogex.h"


// DlgObjectCallBack 对话框

class DlgObjectCallBack : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgObjectCallBack)
public:
	enum UMObjectCallBack				//对象类型枚举
	{
		um_ObjectCallBack_Type_Name = 0,
		um_ObjectCallBack_PreOperation,
		um_ObjectCallBack_PostOperation,
		um_ObjectCallBack_pHandle,
		um_ObjectCallBack_Altitude,
		um_ObjectCallBack_Moudle,
		um_ObjectCallBack_Firm,
	};

	enum UMObjectCallBackEx				//对象类型枚举
	{
		um_ObjectCallBackEx_Type_Type,
		um_ObjectCallBackEx_Type_ValidAccessMask,
		um_ObjectCallBackEx_Type_FunCallBack,
		um_ObjectCallBackEx_Type_FunName,
		um_ObjectCallBackEx_Type_Pos,
		um_ObjectCallBackEx_Type_Object,
		um_ObjectCallBackEx_Type_ModulePath,
		um_ObjectCallBackEx_Type_Firm,
	};


	enum ObjectCallBackTreeType
	{
		um_ObjectCallBack_Type = 1,
		um_ObjectCallBackInfo_Type
	};
public:
	DlgObjectCallBack(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgObjectCallBack();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_KERNEL_OBJECTCALLBACK };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void InsertCtrlListControl(PCObjectTypeCallBackInfo pCallBackInfo);
	void InsertCtrlListControl(PCObjectTypeCallBackExInfo pCallBackInfo);
	void SelectObjectTypeInfo();
	void SetRootTitle(LPCTSTR lpszRootTitle) { m_RootTitle = lpszRootTitle; }
public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	CListCtrl m_CListCtrl;
	int nPerSel = 0;	//存储先前选择的
	afx_msg void OnNMRClickDlgKernelObjectcallbackList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnObjectcallbackRefresh();
	CTreeCtrl m_CTreeCtrl;
	CString m_RootTitle;
	afx_msg void OnNMDblclkDlgKernelObjectcallbackTree(NMHDR* pNMHDR, LRESULT* pResult);
};
