#pragma once
#include "afxdialogex.h"


// DlgIdt 对话框

class DlgIdt : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgIdt)
public:
	enum UMIDT									//SSDT表
	{
		um_Idt_CpuOrd = 0,
		um_Idt_IdtNumber,					//IDT编号
		um_Idt_IdtBase,
		um_Idt_Level,						//特权
		um_Idt_BaseAddr,
		um_Idt_Path,
		um_Idt_Company,
	};
public:
	DlgIdt(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgIdt();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_IDT };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgIdt::InsertCtrlListControl(PCIdtInfo pIdtInfo);
public:
	CListCtrl m_CListCtrl;
	afx_msg void OnNMRClickIdtList(NMHDR* pNMHDR, LRESULT* pResult);
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnIdtRefresh();
};
