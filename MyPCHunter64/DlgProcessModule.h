#pragma once
#include "afxdialogex.h"
#include "UserStruct.h"

// DlgProcessModule 对话框

class DlgProcessModule : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgProcessModule)
public:
	enum UMProcessModule
	{
		um_Process_Module_Name,
		um_Process_Module_BaseAddr,
		um_Process_Module_Size,
		um_Process_Module_Path,
		um_Process_Module_Signal,
		um_Process_Module_Company,
	};

public:
	DlgProcessModule(CString StrEprocess, CString StrProcessName, CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgProcessModule();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_PROCESS_MODULE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgProcessModule::InsertCtrlListControl(PCProcessModuleInfo pinfo);

public:
	afx_msg void OnProcessmoduleRefresh();
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	CListCtrl m_CListCtrl;
	CString m_StrEprocess;
	CString m_StrProcessName;
	afx_msg void OnNMRClickProcessModuleList(NMHDR* pNMHDR, LRESULT* pResult);
};
