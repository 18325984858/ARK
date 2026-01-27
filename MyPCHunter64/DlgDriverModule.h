#pragma once
#include "afxdialogex.h"
// DlgDriverModule 对话框

class DlgDriverModule : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgDriverModule)
public:
	enum UMDriver								//驱动枚举
	{
		um_Driver_Name = 0,
		um_Driver_BaseAddr,
		um_Driver_Size,
		um_Driver_LoadOrder,
		um_Driver_Object,
		um_Driver_ObjectName,
		um_Driver_ServerName,
		um_Driver_DigitalSignature,
		um_Driver_FilePath,
		um_Driver_FileName
	};
public:
	DlgDriverModule(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgDriverModule();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_DRIVERMODE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	DECLARE_MESSAGE_MAP()
public:
	void DlgDriverModule::InsertCtrlListControl(PCDriverInfo pInfo);
public:
	virtual BOOL OnInitDialog();
	CListCtrl m_CListCtrl;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDriverMenuCopyName();
	afx_msg void OnDriverMenuCopyBaseaddr();
	afx_msg void OnDriverMenuCopySize();
	afx_msg void OnDriverMenuCopyLoadorad();
	afx_msg void OnDriverMenuCopyObjeceaddr();
	afx_msg void OnDriverMenuCopyObjectname();
	afx_msg void OnDriverMenuCopyServername();
	afx_msg void OnDriverMenuCopyPath();
	afx_msg void OnDriverMenuCopyCompany();
	afx_msg void OnDriverRefresh();
	afx_msg void OnNMRClickControlDrivermoduleList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDriverMenuCopySing();
};
