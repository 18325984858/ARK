// DlgSetting.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgSetting.h"
#include "Thread.h"
#include "LoadPe.h"
#include "MyPCHunter64Dlg.h"

// DlgSetting 对话框

IMPLEMENT_DYNAMIC(DlgSetting, CDialogEx)

DlgSetting::DlgSetting(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_SETTING, pParent)
	, m_kdDebuggerEnable(FALSE)
	, m_KdDebuggerNotPresent(FALSE)
	, m_pSharedDataKdDebuggerEnabled(FALSE)
	, m_KdPitchDebugger(FALSE)
{

}

DlgSetting::~DlgSetting()
{
}

void DlgSetting::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK_KdDebuggerEnabled, m_kdDebuggerEnable);
	DDX_Check(pDX, IDC_CHECK_KdDebuggerNotPresent, m_KdDebuggerNotPresent);
	DDX_Check(pDX, IDC_CHECK_pSharedDataKdDebuggerEnabled, m_pSharedDataKdDebuggerEnabled);
	DDX_Check(pDX, IDC_CHECK_KdPitchDebugger, m_KdPitchDebugger);
}

VOID DlgSetting::UpkdDebuggerFlags()
{
	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserGetDebugFlags, this });
}

ULONG64 DlgSetting::SetKdDebuggerFlags(PCDebugFlagInfo pDebugFlagsInfo)
{
	if (!pDebugFlagsInfo)
	{
		return FALSE;
	}


	UpdateData(TRUE); //更新对话框数据

	//判断是否是获取调试标志
	if (pDebugFlagsInfo->UserOperate == USER_GET_DEBUG_FLAG)
	{
		m_KdPitchDebugger = pDebugFlagsInfo->KdPitchDebugger; 
		m_kdDebuggerEnable = pDebugFlagsInfo->kdDebuggerEnable;
		m_KdDebuggerNotPresent = pDebugFlagsInfo->KdDebuggerNotPresent;
		m_pSharedDataKdDebuggerEnabled = pDebugFlagsInfo->SharedDataKdDebuggerEnabled;
	}

	UpdateData(FALSE); //更新对话框数据


	return TRUE;
}

ULONG64 DlgSetting::GetKdDebuggerFlags(PCDebugFlagInfo pDebugFlagsInfo)
{
	if (!pDebugFlagsInfo)
	{
		return FALSE;
	}

	UpdateData(TRUE); //更新对话框数据
	// TODO: 在此添加控件通知处理程序代码

	pDebugFlagsInfo->DebugFlag = USER_SET_DEBUG_FLAG; //设置为设置调试标志
	pDebugFlagsInfo->KdPitchDebugger = m_KdPitchDebugger; //获取对话框数据
	pDebugFlagsInfo->kdDebuggerEnable = m_kdDebuggerEnable; //获取对话框数据
	pDebugFlagsInfo->KdDebuggerNotPresent = m_KdDebuggerNotPresent; //获取对话框数据
	pDebugFlagsInfo->SharedDataKdDebuggerEnabled = m_pSharedDataKdDebuggerEnabled;

	UpdateData(FALSE); //更新对话框数据

	return TRUE;
}

BEGIN_MESSAGE_MAP(DlgSetting, CDialogEx)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_CHECK_KdDebuggerEnabled, &DlgSetting::OnBnClickedCheckKddebuggerenabled)
	ON_BN_CLICKED(IDC_CHECK_KdDebuggerNotPresent, &DlgSetting::OnBnClickedCheckKddebuggernotpresent)
	ON_BN_CLICKED(IDC_CHECK_pSharedDataKdDebuggerEnabled, &DlgSetting::OnBnClickedCheckpshareddatakddebuggerenabled)
	ON_BN_CLICKED(IDC_CHECK_KdPitchDebugger, &DlgSetting::OnBnClickedCheckKdpitchdebugger)
END_MESSAGE_MAP()


// DlgSetting 消息处理程序

BOOL DlgSetting::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  在此添加额外的初始化

	UpkdDebuggerFlags();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

BOOL DlgSetting::DestroyWindow()
{
	// TODO: 在此添加专用代码和/或调用基类

	return CDialogEx::DestroyWindow();
}

void DlgSetting::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	// TODO: 在此处添加消息处理程序代码
}

void DlgSetting::OnBnClickedCheckKddebuggerenabled()
{
	CDebugFlagInfo DebugFlagsInfo = { 0 };
	GetKdDebuggerFlags(&DebugFlagsInfo);

	DebugFlagsInfo.kdDebuggerEnable = m_kdDebuggerEnable ? TRUE : FALSE; //获取对话框数据

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserSetDebugFlags, &DebugFlagsInfo });
}

void DlgSetting::OnBnClickedCheckKddebuggernotpresent()
{
	CDebugFlagInfo DebugFlagsInfo = { 0 };
	GetKdDebuggerFlags(&DebugFlagsInfo);

	DebugFlagsInfo.KdDebuggerNotPresent = m_KdDebuggerNotPresent ? TRUE : FALSE; //获取对话框数据

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserSetDebugFlags, &DebugFlagsInfo });
}

void DlgSetting::OnBnClickedCheckpshareddatakddebuggerenabled()
{

	CDebugFlagInfo DebugFlagsInfo = { 0 };
	GetKdDebuggerFlags(&DebugFlagsInfo);

	DebugFlagsInfo.SharedDataKdDebuggerEnabled = m_pSharedDataKdDebuggerEnabled ? TRUE : FALSE; //获取对话框数据

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserSetDebugFlags, &DebugFlagsInfo });
}

void DlgSetting::OnBnClickedCheckKdpitchdebugger()
{
	CDebugFlagInfo DebugFlagsInfo = { 0 };
	GetKdDebuggerFlags(&DebugFlagsInfo);

	DebugFlagsInfo.KdPitchDebugger = m_KdPitchDebugger ? TRUE : FALSE; //获取对话框数据

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserSetDebugFlags, &DebugFlagsInfo });
}
