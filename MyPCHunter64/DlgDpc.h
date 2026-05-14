#pragma once
#include "afxdialogex.h"


// DlgDpc 对话框

class DlgDpc : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgDpc)
public:
	enum UmGdt
	{
		um_Dpc_DpcObject,
		um_Dpc_TimeObject,
		um_Dpc_TriggerCycle,
		um_Dpc_FunctionStartAddr,
		um_Dpc_FunctionName,
		um_Dpc_ModulePath,
		um_Dpc_CompanyName,
	};
public:
	DlgDpc(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgDpc();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_DPC };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgDpc::InsertCtrlListControl(PCDPcInfo pInfo);
public:
	afx_msg void OnNMRClickDpcList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	CListCtrl m_CListCtrl;
	afx_msg void OnDpcRefresh();
};
