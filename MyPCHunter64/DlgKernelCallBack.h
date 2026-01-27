#pragma once
#include "afxdialogex.h"

// DlgKernelCallBack 对话框

class DlgKernelCallBack : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgKernelCallBack)
public:
	enum UmKernelCallBack
	{
		um_KernelCallBack_Type,
		um_KernelCallBack_Addr,
		um_KernelCallBack_Pos,
		um_KernelCallBack_Path,
		um_KernelCallBack_Company,
		um_KernelCallBack_Descr
	};
public:
	DlgKernelCallBack(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgKernelCallBack();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_KERNEL_KERNELCALLBACK };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void InsertCtrlListControl(PCKernelCallBackInfo pGdtInfo);
public:
	CListCtrl m_CListCtrl;
	afx_msg void OnNMRClickDlgKernelKernelcallbackList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKernelcallbackRefresh();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
};
