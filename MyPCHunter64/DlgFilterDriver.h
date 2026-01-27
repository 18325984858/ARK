#pragma once
#include "afxdialogex.h"


// DlgFilterDriver 对话框

class DlgFilterDriver : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgFilterDriver)

public:
	DlgFilterDriver(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgFilterDriver();
	
	enum UMFilterDriver								//驱动枚举
	{
		um_FilterDriver_Type = 0,
		um_FilterDriver_Driver,
		um_FilterDriver_DriverPath,
		um_FilterDriver_DeviceObject,
		um_FilterDriver_DeviceName,
		um_FilterDriver_SrcDriverObjectName,
		um_FilterDriver_FileName
	};
	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_FILTERDRIVER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgFilterDriver::InsertCtrlListControl(PCFilterDeviceInfo pInfo);
public:
	CListCtrl m_CListCtrl;
	BOOL m_ThreadFlags = 0;

	virtual BOOL OnInitDialog();
	virtual BOOL DestroyWindow();

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnFilterdriverRefresh();
	afx_msg void OnRclickListFilterdriver(NMHDR* pNMHDR, LRESULT* pResult);
};
