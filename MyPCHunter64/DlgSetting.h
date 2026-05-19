#pragma once
#include "afxdialogex.h"


// DlgSetting 对话框

class DlgSetting : public CDialogEx
{
	DECLARE_DYNAMIC(DlgSetting)

public:
	DlgSetting(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgSetting();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_SETTING };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	VOID UpkdDebuggerFlags();

public:
	VOID RefreshKdDebuggerFlags() { UpkdDebuggerFlags(); }
	ULONG64 SetKdDebuggerFlags(PCDebugFlagInfo pDebugFlagsInfo);

public:
	ULONG64 GetKdDebuggerFlags(PCDebugFlagInfo pDebugFlagsInfo);

public:
	virtual BOOL OnInitDialog();
	virtual BOOL DestroyWindow();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	BOOL m_kdDebuggerEnable;
	BOOL m_KdDebuggerNotPresent;
	BOOL m_pSharedDataKdDebuggerEnabled;
	afx_msg void OnBnClickedCheckKddebuggerenabled();
	afx_msg void OnBnClickedCheckKddebuggernotpresent();
	afx_msg void OnBnClickedCheckpshareddatakddebuggerenabled();
	BOOL m_KdPitchDebugger;
	afx_msg void OnBnClickedCheckKdpitchdebugger();
};
