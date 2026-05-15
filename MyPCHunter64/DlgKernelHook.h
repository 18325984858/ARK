#pragma once
#include "afxdialogex.h"
#include "DlgSsdt.h"
#include "DlgSsdtShadow.h"
#include "DlgIdt.h"
#include "DlgObjectCallBack.h"
#include "DlgMajorfunction.h"
#include "DlgKernelHookList.h"
// DlgKernelHook 对话框

class DlgKernelHook : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgKernelHook)

public:
	enum KernelHookDlgType
	{
		KernelHookDlgType_SSDT,
		KernelHookDlgType_ShadowSSDT,
		KernelHookDlgType_FSD,
		KernelHookDlgType_KeyBoard,
		KernelHookDlgType_I8042Prt,
		KernelHookDlgType_Mouse,
		KernelHookDlgType_PartMgr,
		KernelHookDlgType_Disk,
		KernelHookDlgType_AtApi,
		KernelHookDlgType_Acpi,
		KernelHookDlgType_Scsi,
		KernelHookDlgType_KernelHook,
		KernelHookDlgType_ObjectHook,
		KernelHookDlgType_SystemInterrupt
	};

public:
	DlgKernelHook(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgKernelHook();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_KERNELHOOK };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CTabCtrl m_CTabCtrl;
	DlgSsdt m_DlgSsdt;
	DlgSsdtShadow m_DlgSsdtShadow;
	DlgIdt m_DlgIdt;
	DlgObjectCallBack m_DlgObjectCallBack;
	DlgMajorfunction m_DlgAcpi;
	DlgMajorfunction m_DlgAtapi;
	DlgMajorfunction m_DlgClassPnp;
	DlgMajorfunction m_DlgParTmgr;
	DlgMajorfunction m_DlgMouClass;
	DlgMajorfunction m_Dlgi8042prt;
	DlgMajorfunction m_Dlgkbdclass;
	DlgMajorfunction m_DlgNtfs;
	DlgMajorfunction m_Dlgstorport;
	DlgKernelHookList m_DlgKernelHook;

	afx_msg void OnNMClickKernelhookTab(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
};
