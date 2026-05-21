
// MyPCHunter64Dlg.h: 头文件
//

#pragma once
#include "DlgProcess.h"
#include "DlgDriverModule.h"
#include "DlgEnumFile.h"
#include "DlgEnumRegistry.h"
#include "DlgKernel.h"
#include "DlgKernelHook.h"
#include "DlgProcessMonitor.h"
#include "DlgSetting.h"
#include "Pdb/Pdb.h"
#include "Pdb/SymLoader.h"

#include "../include/capstone-5.0-Release/include/capstone/capstone.h"
#pragma comment(lib,"include/capstone-5.0-Release/capstone.lib")

enum DlgType //窗口类型
{
	DlgType_Process,
	DlgType_DriverModule,
	DlgType_Kernel,
	DlgType_KernelHook,
	DlgType_EnumFile,
	DlgType_EnumRegistry,
	DlgType_Setting,
};

EXTERN_C DlgProcessMonitor g_DlgProcessMonitor;
EXTERN_C UCHAR g_CreateFlagsDlgProcessMonitor;
EXTERN_C UCHAR g_HookStateFlags[SSDT_MAX_NUMBER];
EXTERN_C MyPdb g_NtPdb;
EXTERN_C MyPdb g_fltmgrPDB;
EXTERN_C MyPdb g_WdfPdb;


typedef struct MyModuleCall
{
	WCHAR* szModuleName;
	MyPdb* m_PdbInfo;		//存储PDB信息
}CMyModuleCall, * PCMyModuleCall;

#define MAX_MODULE_NAME_NUMBER 0x64 
EXTERN_C CMyModuleCall g_ModuleCall[MAX_MODULE_NAME_NUMBER];


// CMyPCHunter64Dlg 对话框
class CMyPCHunter64Dlg : public CDialogEx
{
	// 构造
public:
	CMyPCHunter64Dlg(CWnd* pParent = nullptr);	// 标准构造函数

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum {
		IDD = ID_DLG_MAIN
	};
#endif
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持
	// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	VOID InitTableControl();
	VOID RegHostKey();
	VOID UnHostKey();
	// 切换主 Tab 到“文件”页并在 DlgEnumFile 里定位高亮指定文件路径。
	VOID ShowFileTabAndNavigate(const CString& fullPath);
public:
	DlgProcess m_DlgProcess;
	DlgDriverModule m_DlgDriverModule;
	DlgKernel m_DlgKernel;
	DlgKernelHook m_DlgKernelHook;

	DlgEnumFile m_DlgEnumFile;
	DlgEnumRegistry m_DlgEnumRegistry;

	DlgSetting m_DlgSetting;
public:
	CTabCtrl m_Control_Tab;
	afx_msg void OnClickControlMainTab(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL DestroyWindow();
	virtual void OnOK();
	afx_msg void OnClose();
	afx_msg void OnMenuMainDlgMonitordlg();
	afx_msg void OnMenuLoadOphion();
	afx_msg void OnMenuUnloadOphion();
	afx_msg void OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2);
	afx_msg LRESULT OnPdbProgress(WPARAM wParam, LPARAM lParam);

private:
	CStatusBar m_StatusBar;
};

// 供其它对话框友好调用：指令主窗口切到文件页并定位文件。实现在 MyPCHunter64Dlg.cpp。
void OpenFileInMainFileTab(const CString& fullPath);
