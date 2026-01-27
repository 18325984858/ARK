#pragma once
#include "afxdialogex.h"
#include "UserStruct.h"

// DlgSsdtShadow 对话框

class DlgSsdtShadow : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgSsdtShadow)
public:
	enum UMSSDTShadow									//SSDT表
	{
		um_SSDTShadow_Order = 0,
		um_SSDTShadow_ServerNumber,
		um_SSDTShadow_FunctionName,
		um_SSDTShadow_KernelAddr,
		um_SSDTShadow_SrcKernelAddr,
		um_SSDTShadow_UserAddr,
		um_SSDTShadow_Path
	};
public:
	DlgSsdtShadow(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgSsdtShadow();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_SSDTSHADOW };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgSsdtShadow::InsertCtrlListControl(PCSsdtInfo pSsdtShadowInfo);
public:
	CListCtrl m_CListCtrl;
	afx_msg void OnNMRClickSsdtshadowList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSsdtshadowRefresh();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnSsdtshadowReturnhook();
	afx_msg void OnSsdtshadowOrider();
	afx_msg void OnSsdtshadowServicenumber();
	afx_msg void OnSsdtshadowFunname();
	afx_msg void OnSsdtshadowCurkerneladdr();
	afx_msg void OnSsdtshadowSrckerneladdr();
	afx_msg void OnSsdtshadowUseraddr();
	afx_msg void OnSsdtshadowModulepath();
};
