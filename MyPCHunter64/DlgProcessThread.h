#pragma once
#include "afxdialogex.h"


// DlgProcessThread 对话框

class DlgProcessThread : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgProcessThread)

public:
	enum UMThread								//线程枚举
	{
		um_Thread_Module = 0,
		um_Thread_Id,
		um_Thread_Object,
		um_Thread_Teb,
		um_Thread_Type,
		um_Thread_StartAddr,
		um_Thread_Priority,
		um_Thread_SwitchCount,
		um_Thread_State,
		um_Thread_CreateTime,
		um_Thread_CompanyName,
	};

public:
	DlgProcessThread(CString StrEprocess, CString StrProcessName, CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgProcessThread();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_PROCESS_THREAD };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

public:
	void InsertCtrlListControl(PCProcessThreadInfo pInfo);
public:
	afx_msg void OnRclickProcessThreadList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	CListCtrl m_CListCtrl;
	CString m_StrEprocess;
	CString m_StrProcessName;
	afx_msg void OnProcessthreadRefresh();
};
