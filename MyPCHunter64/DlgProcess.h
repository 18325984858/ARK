#pragma once
#include "afxdialogex.h"
#include "../MyDriver64/Struct.h"
#include "UserStruct.h"
// DlgProcess 对话框

class DlgProcess : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgProcess)
public:
	enum UMProcess								//进程枚举
	{
		um_Process_Name = 0,
		um_Process_Id,
		um_Process_ParentId,
		um_Process_SessionId,
		um_Process_UserName,
		um_Process_FilePath,
		um_Process_Object,
		um_Process_VisitState,
		um_Process_FileFirm,
		um_Process_DebugState,
		um_Process_Architecture,
		um_Process_RunTime,
		um_Process_Param,
	};

public:
	DlgProcess(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgProcess();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_PROCESS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

public:
	//将获取到的进程信息插入到
	void DlgProcess::InsertCtrlListControl(PCProcessInfo Processinfo);


	//强制结束进程
	void DlgProcess::ProcessKillprocess();

	//
public:
	virtual BOOL OnInitDialog();
	CListCtrl m_CListCtrl;

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnNMRClickControlProcessList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDriverRefresh();
	afx_msg void OnProcessVad();
	afx_msg void OnProcessHandle();
	afx_msg void OnProcessThread();
	afx_msg void OnProcessModule();
	afx_msg void OnProcessMenuCopyName();
	afx_msg void OnProcessMenuCopyId();
	afx_msg void OnProcessMenuCopyParentid();
	afx_msg void OnProcessMenuCopySessionid();
	afx_msg void OnProcessMenuCopyUsername();
	afx_msg void OnProcessMenuCopyFilepath();
	afx_msg void OnProcessMenuCopyEprocess();
	afx_msg void OnProcessMenuCopyVisitstate();
	afx_msg void OnProcessMenuCopyFilefirm();
	afx_msg void OnProcessMenuCopyDebugstate();
	afx_msg void OnProcessMenuCopyRuntime();
	afx_msg void OnProcessMenuCopyParam();
	afx_msg void OnProcessMenuCopyOpenfile();
	afx_msg void OnProcessMenuCopyAttribute();
	afx_msg void OnProcessKillprocess();
	afx_msg void OnProcessClearProtection();
	afx_msg void OnProcessPpl();
	afx_msg void OnProcessPp();
	afx_msg void OnProcessNp();
};
