#pragma once
#include "afxdialogex.h"


// DlgRWMemory 对话框

class DlgRWMemory : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgRWMemory)

	enum UMRWMemory
	{
		um_RWMemory_Addr,
		um_RWMemory_Opcode,
		um_RWMemory_Asm,
	};
	enum UMRWMemorOpcode
	{
		um_RWMemory_Opcode_Addr,
		um_RWMemory_Opcode_1,
		um_RWMemory_Opcode_2,
		um_RWMemory_Opcode_3,
		um_RWMemory_Opcode_4,
		um_RWMemory_Opcode_5,
		um_RWMemory_Opcode_6,
		um_RWMemory_Opcode_7,
		um_RWMemory_Opcode_8,
		um_RWMemory_Opcode_9,
		um_RWMemory_Opcode_10,
		um_RWMemory_Opcode_11,
		um_RWMemory_Opcode_12,
		um_RWMemory_Opcode_13,
		um_RWMemory_Opcode_14,
		um_RWMemory_Opcode_15,
		um_RWMemory_Opcode_16,
	};

public:
	DlgRWMemory(ULONG64 Eprocess, ULONG64 Addr = 0, UCHAR IsX64 = TRUE, CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgRWMemory();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_RW_MEMORY };
#endif
#define COL_NUM 16 

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

public:
	ULONG64 m_DstEprocess;					//目标进程EPROCESS
	ULONG64 m_DstAddr;						//存储了要读取和写入的地址
	UCHAR m_szBuf[MAX_READ_SIZE];			//存储数据缓冲区
	UCHAR m_IsX64;							//存储是否是x64程序

public:
	void DlgRWMemory::ReadWriteMemOry();
public:
	CEdit m_CEdit;
	CEdit m_CEdit1;

	PCRWMemoryInfo  m_MemInfo = NULL;

	int m_EditItem = 0;
	int m_EditSubItem = 0;

	afx_msg void OnBnClickedRwMemoryButton();
	CEdit m_AddrEdit;
	virtual BOOL OnInitDialog();
	CListCtrl m_CListCtrl;
	CListCtrl m_CListCtrlOpcode;
	afx_msg void OnNMDblclkListOpcode(NMHDR* pNMHDR, LRESULT* pResult);
	virtual void OnOK();
	virtual BOOL DestroyWindow();
	CButton m_CButton_Save;
	afx_msg void OnBnClickedRwMemorySave();
};
