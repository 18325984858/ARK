#pragma once
#include "afxdialogex.h"


// DlgProcessHandle 对话框

class DlgProcessHandle : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgProcessHandle)
public:
	enum UMProcessHandle
	{
		um_Process_Handle_Type,
		um_Process_Handle_Name,
		um_Process_Handle_Handle,
		um_Process_Handle_Object,
		um_Process_Handle_Power,
		um_Process_Handle_Index,
		um_Process_Handle_Reference
	};
public:
	DlgProcessHandle(CString StrEprocess, CString StrProcessName, CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgProcessHandle();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_PROCESS_HANDLE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgProcessHandle::InsertCtrlListControl(PCProcessHandleInfo pinfo);
public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnRclickProcessHandleList(NMHDR* pNMHDR, LRESULT* pResult);
	CListCtrl m_CListCtrl;
	CString m_StrEprocess;
	CString m_StrProcessName;
	afx_msg void OnProcesshandleRefresh();
};
