#pragma once
#include "afxdialogex.h"
#include "DlgGdt.h"
#include "DlgKernelCallBack.h"
#include "DlgMiniFilterCallBack.h"
#include "DlgDpc.h"
#include "DlgHalTable.h"
#include "DlgWdf.h"
#include "DlgFilterDriver.h"
#include "DlgWorkerThread.h"
#include "DlgSetting.h"
#include "DlgObjectCallBack.h"
#include "DlgDirectIo.h"
// DlgKernel 对话框

class DlgKernel : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgKernel)
public:
	enum KernelDlgType
	{
		KernelDlgType_SystemCallBack,
		KernelDlgType_FilterDriver,
		KernelDlgType_DpcTimer,
		KernelDlgType_WorkThread,
		KernelDlgType_Hal,
		KernelDlgType_Wdf,
		KernelDlgType_FileSystem,
		KernelDlgType_SystemDbg,
		KernelDlgType_ObjectHijack,
		KernelDlgType_IO,
		KernelDlgType_GDT,
	};


public:
	DlgKernel(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgKernel();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_KERNEL };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持


	DECLARE_MESSAGE_MAP()
public:
	DlgGdt m_DlgGdt;
public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	CTabCtrl m_CTabCtrl;
	DlgKernelCallBack m_DlgKernelCallBack;
	DlgMiniFilterCallBack m_DlgMiniFilterCallBack;
	DlgDpc m_DlgDpc;
	DlgHalTable m_DlgHalTable;
	DlgWdf m_DlgWdf;
	DlgFilterDriver m_FilterDriver;
	DlgWorkerThread m_DlgWorkerThread;
	DlgSetting m_DlgSystemDbg;
	DlgObjectCallBack m_DlgObjectHijack;
	DlgDirectIo m_DlgDirectIo;

	afx_msg void OnNMClickKernelTab(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDestroy();
};
