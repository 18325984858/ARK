#pragma once
#include "afxdialogex.h"


// DlgProcessVad 对话框

class DlgProcessVad : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgProcessVad)
public:
	enum UMProcessVad
	{
		um_Process_Vad_NodeAddr,
		um_Process_Vad_StartAddr,
		um_Process_Vad_EndAddr,
		um_Process_Vad_Commit,
		um_Process_Vad_Mode,
		um_Process_Vad_Power,
		um_Process_Vad_Path
	};
public:
	DlgProcessVad(CString StrEprocess, CString StrProcessName, CString strArchitecture, CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgProcessVad();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_PROCESS_VAD };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

public:
	void DlgProcessVad::InsertCtrlListControl(PCProcessVadInfo pinfo);
public:
	CListCtrl m_CListCtrl;
	CString m_StrEprocess;
	CString m_StrProcessName;
	CString m_strArchitecture;

	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnProcessvadRefresh();
	afx_msg void OnProcessvadMemory();
	afx_msg void OnRclickProcessVadList(NMHDR* pNMHDR, LRESULT* pResult);
};
