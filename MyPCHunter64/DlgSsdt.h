#pragma once
#include "afxdialogex.h"


// DlgSsdt 对话框

class DlgSsdt : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgSsdt)
public:
	enum UMSSDT									//SSDT表
	{
		um_SSDT_Order = 0,
		um_SSDT_ServerNumber,
		um_SSDT_FunctionName,
		um_SSDT_KernelAddr,
		um_SSDT_SrcKernelAddr,
		um_SSDT_UserAddr,
		um_SSDT_Path
	};
public:
	DlgSsdt(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgSsdt();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_SSDT };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgSsdt::InsertCtrlListControl(PCSsdtInfo pSsdtInfo);



public:
	CListCtrl m_CListCtrl;
	afx_msg void OnNMRClickSsdtList(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSsdtRefresh();
	afx_msg void OnSsdtHook();
	afx_msg void OnSsdtReturnhook();
	afx_msg void OnSsdtCopyOrider();
	afx_msg void OnSsdtCopyServicenumber();
	afx_msg void OnSsdtCopyFunname();
	afx_msg void OnSsdtCopyCurkerneladdr();
	afx_msg void OnSsdtCopySrckerneladdr();
	afx_msg void OnSsdtCopyCuruseraddr();
	afx_msg void OnSsdtCopyMoudlepath();
};
