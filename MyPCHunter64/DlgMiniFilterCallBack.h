#pragma once
#include "afxdialogex.h"

// DlgMiniFilterCallBack 对话框

class DlgMiniFilterCallBack : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgMiniFilterCallBack)
public:
	enum UMMiniFilterCallBack				//MiniFilter类型枚举
	{
		um_MiniFilterCallBack_FilterType_Name = 0,
		um_MiniFilterCallBack_PreOperation,
		um_MiniFilterCallBack_PostOperation,
		um_MiniFilterCallBack_pFilterAddr,
		um_MiniFilterCallBack_Altitude,
		um_MiniFilterCallBack_Moudle,
		um_MiniFilterCallBack_Firm,
	};

	enum UMFileSystemType					//Tree选项
	{
		um_FileSystemType_MiniPortFilter = 1,
		um_FileSystemType_FileSystem,
		um_FileSystemType_SfilterCallBack,
		um_FileSystemType_ClassInitDataClass,
		um_FileSystemType_NpfsMajorFunction,
		um_FileSystemType_MsfsMajorFunction,
		um_FileSystemType_UsbPortMajorFunction,
	};

	enum UMFileSystemInfo
	{
		um_FileSystem_Type,
		um_FileSystem_Deivce,
		um_FileSystem_DeviceObjectName,
		um_FileSystem_Driver,
		um_FileSystem_DriverName
	};

	enum UMFileSystemCallBackInfo
	{
		um_FileSystemCallBack_Order,
		um_FileSystemCallBack_FunName,
		um_FileSystemCallBack_CurFunAddr,
		//um_FileSystemCallBack_Hook,
		//um_FileSystemCallBack_SrcFunAddr,
		um_FileSystemCallBack_ModulePath
	};

	enum UmFileSystemMajorFunction
	{
		um_FileSystemMajorFunction_Ord,
		um_FileSystemMajorFunction_FunName,
		um_FileSystemMajorFunction_FunAddr,
		um_FileSystemMajorFunction_MoudlePath,
	};

public:
	DlgMiniFilterCallBack(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgMiniFilterCallBack();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_KERNEL_MINIFILTERCALLBACK };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgMiniFilterCallBack::InsertCtrlListControl(PCMiniFilterCallBackInfo pMiniFilterCallBackInfo);
	void DlgMiniFilterCallBack::InsertCtrlListControl(PCFileSystemDeviceInfo pFileSystemDeviceInfo);
	void DlgMiniFilterCallBack::InsertCtrlListControl(PCSysMajorFunctionInfo pCSysMajorFunctionInfo);
public:
	CListCtrl m_CListCtrl;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	afx_msg void OnMinifiltercallbackRefresh();
	afx_msg void OnNMRClickDlgKernelMinifiltercallbackList(NMHDR* pNMHDR, LRESULT* pResult);
	CTreeCtrl m_CTreeCtrl;
	int nPerSel = 0;	//存储先前选择的
	afx_msg void OnNMDblclkDlgKernelMinifiltercallbackTree(NMHDR* pNMHDR, LRESULT* pResult);
};
