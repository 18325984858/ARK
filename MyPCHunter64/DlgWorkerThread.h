#pragma once
#include "afxdialogex.h"
#include "BaseClass.h"
#include "../MyDriver64/Struct.h"


// DlgWorkerThread 对话框
// 工作线程队列：枚举 System 进程(PID=4)中的所有内核线程。
// 没有在 .rc 中新增对话框模板，复用 ID_DLG_DPC（同样是"整张对话框只有一个 ListCtrl"的布局），
// 列在 OnInitDialog 里运行时配置，避免触碰脆弱的 UTF-16 资源脚本。

class DlgWorkerThread : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgWorkerThread)
public:
	enum WorkerCol
	{
		um_Worker_Index,
		um_Worker_Ethread,
		um_Worker_Tid,
		um_Worker_Priority,
		um_Worker_StartAddress,
		um_Worker_FunctionName,
		um_Worker_Module,
	};

public:
	DlgWorkerThread(CWnd* pParent = nullptr);
	virtual ~DlgWorkerThread();

#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_DPC };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()

public:
	void InsertCtrlListControl(PCProcessThreadInfo pInfo);

public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnWorkerThreadRefresh();
	afx_msg void OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	CListCtrl m_CListCtrl;
};
