#pragma once
#include "afxdialogex.h"


// DlgProcessMonitor 对话框

class DlgProcessMonitor : public CDialogEx
{
	DECLARE_DYNAMIC(DlgProcessMonitor)
public:
	enum UMApiMonitorType
	{
		um_ApiMonitorDlg_ProcessName,
		um_ApiMonitorDlg_PID,
		um_ApiMonitorDlg_TID,
		um_ApiMonitorDlg_ApiName,
		um_ApiMonitorDlg_Parameter,
		um_ApiMonitorDlg_RetNumber
	};

	enum UMApiType
	{
		um_ApiDlg_FunName,
		um_ApiDlg_Level,
		um_ApiDlg_State,
	};
public:
	DlgProcessMonitor(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgProcessMonitor();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DIALOG_SSDT_MONITOR };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:

	VOID DlgProcessMonitor::InsertHookData(PCSSDTHookInfo pSsdtHookInfo);
	VOID DlgProcessMonitor::InsertApi(PCHookSsdtInfo pInfo);
	VOID DlgProcessMonitor::AlterApi(PCAlterHookSsdtInfo pInfo);
public:
	CListCtrl m_CListCtrlMonitor;
	virtual BOOL OnInitDialog();
	virtual BOOL DestroyWindow();
	virtual void OnOK();
	CListCtrl m_CListCtrlApi;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
};
