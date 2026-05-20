// DlgMiniFilterCallBack.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgMiniFilterCallBack.h"
#include "Thread.h"
#include "PdbResolver.h"
#include <unordered_map>
#include <string>

// DlgMiniFilterCallBack 对话框

IMPLEMENT_DYNAMIC(DlgMiniFilterCallBack, CDialogEx)

DlgMiniFilterCallBack::DlgMiniFilterCallBack(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_KERNEL_MINIFILTERCALLBACK, pParent)
{

}

DlgMiniFilterCallBack::~DlgMiniFilterCallBack()
{
}

void DlgMiniFilterCallBack::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_DLG_KERNEL_MINIFILTERCALLBACK_LIST, m_CListCtrl);
	DDX_Control(pDX, ID_DLG_KERNEL_MINIFILTERCALLBACK_TREE, m_CTreeCtrl);
}

BEGIN_MESSAGE_MAP(DlgMiniFilterCallBack, CDialogEx)
	ON_WM_SIZE()
	ON_COMMAND(ID_MINIFILTERCALLBACK_REFRESH, &DlgMiniFilterCallBack::OnMinifiltercallbackRefresh)
	ON_NOTIFY(NM_RCLICK, ID_DLG_KERNEL_MINIFILTERCALLBACK_LIST, &DlgMiniFilterCallBack::OnNMRClickDlgKernelMinifiltercallbackList)
	ON_NOTIFY(NM_DBLCLK, ID_DLG_KERNEL_MINIFILTERCALLBACK_TREE, &DlgMiniFilterCallBack::OnNMDblclkDlgKernelMinifiltercallbackTree)
END_MESSAGE_MAP()

// DlgMiniFilterCallBack 消息处理程序

void DlgMiniFilterCallBack::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	CRect rect;
	GetClientRect(&rect);

	float fwidth = rect.Width() / 4;

	m_CTreeCtrl.SetWindowPos(NULL, 0, 0, fwidth, rect.Height(), SWP_NOZORDER);
	m_CListCtrl.SetWindowPos(NULL, fwidth, 0, rect.Width() - fwidth, rect.Height(), SWP_NOZORDER);
}

BOOL DlgMiniFilterCallBack::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	//初始化树控件
	m_CTreeCtrl.SetExtendedStyle(m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
		m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS);

	//创建根节点
	auto RootNode = m_CTreeCtrl.InsertItem(L"对象类型回调");
	//创建子节点1
	auto ChildNode0 = m_CTreeCtrl.InsertItem(L"微端口过滤器", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode0, um_FileSystemType_MiniPortFilter);
	//创建子节点2
	auto ChildNode1 = m_CTreeCtrl.InsertItem(L"文件系统", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode1, um_FileSystemType_FileSystem);
	//创建子节点3
	auto ChildNode2 = m_CTreeCtrl.InsertItem(L"Sfilter回调", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode2, um_FileSystemType_SfilterCallBack);
	//创建子节点4
	auto ChildNode3 = m_CTreeCtrl.InsertItem(L"ClassInitData回调", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode3, um_FileSystemType_ClassInitDataClass);
	//创建子节点5
	auto ChildNode4 = m_CTreeCtrl.InsertItem(L"Npfs派发函数", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode4, um_FileSystemType_NpfsMajorFunction);
	//创建子节点5
	auto ChildNode5 = m_CTreeCtrl.InsertItem(L"Msfs派发函数", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode5, um_FileSystemType_MsfsMajorFunction);
	//创建子节点5
	auto ChildNode6 = m_CTreeCtrl.InsertItem(L"UsbPort派发函数", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode6, um_FileSystemType_UsbPortMajorFunction);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE`
}

void DlgMiniFilterCallBack::OnMinifiltercallbackRefresh()
{
	m_CListCtrl.DeleteAllItems();

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumMiniFilterCallBackInfo, this });

}

void DlgMiniFilterCallBack::InsertCtrlListControl(PCMiniFilterCallBackInfo pMiniFilterCallBackInfo)
{
	if (pMiniFilterCallBackInfo == NULL)
	{
		return;
	}

	PCLIST_ENTRY pCurList = &pMiniFilterCallBackInfo->List.List;
	do
	{
		if (pCurList == NULL)
		{
			break;
		}
		ULONG64 i = m_CListCtrl.GetItemCount();
		PCMiniFilterCallBackInfo pInfo = (PCMiniFilterCallBackInfo)pCurList;

		CString StrBuf;
		m_CListCtrl.InsertItem(i, pInfo->szFilterTypeName);

		StrBuf.Format(L"%016I64X", pInfo->PreOperation);
		m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_PreOperation, StrBuf);

		StrBuf.Format(L"%016I64X", pInfo->PostOperation);
		m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_PostOperation, StrBuf);

		StrBuf.Format(L"%016I64X", pInfo->pFilterAddr);
		m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_pFilterAddr, StrBuf);

		m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_Altitude, pInfo->Altitude);

		CString FilePath = PathTransForm(pInfo->ModulePath);
		m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_Moudle, FilePath.GetBuffer());

		CString szDstFileName;
		m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_Firm, TEXT("--"));
		if (GetCompanyName(FilePath, szDstFileName))
		{
			m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_Firm, (LPWSTR)szDstFileName.GetString());
		}

		//获取下一个
		pCurList = pCurList->Blink;
		//释放资源
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}

	} while (pCurList != &pMiniFilterCallBackInfo->List.List);

}

void DlgMiniFilterCallBack::InsertCtrlListControl(PCFileSystemDeviceInfo pFileSystemDeviceInfo)
{
	if (pFileSystemDeviceInfo == NULL)
	{
		return;
	}

	PCLIST_ENTRY pCurList = &pFileSystemDeviceInfo->List.List;

	CString TypeName[] = { L"Disk",L"CdRom", L"Network", L"Tape" };

	do
	{
		if (pCurList == NULL)
		{
			break;
		}
		ULONG64 i = m_CListCtrl.GetItemCount();
		PCFileSystemDeviceInfo pInfo = (PCFileSystemDeviceInfo)pCurList;

		CString StrBuf;
		m_CListCtrl.InsertItem(i, TypeName[pInfo->nType % ((sizeof(TypeName) / sizeof(CString)) + 1)]);

		StrBuf.Format(L"%016I64X", pInfo->DeviceObject);
		m_CListCtrl.SetItemText(i, um_FileSystem_Deivce, StrBuf);

		StrBuf.Format(L"%016I64X", pInfo->DriverObject);
		m_CListCtrl.SetItemText(i, um_FileSystem_Driver, StrBuf);

		m_CListCtrl.SetItemText(i, um_FileSystem_DriverName, pInfo->DriverName);
		//StrBuf.Format(L"%016I64X", pInfo->PostOperation);
		//m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_PostOperation, StrBuf);

		//StrBuf.Format(L"%016I64X", pInfo->pFilterAddr);
		//m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_pFilterAddr, StrBuf);
		//
		//m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_Altitude, pInfo->Altitude);
		//
		//CString FilePath = PathTransForm(pInfo->ModulePath);
		//m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_Moudle, FilePath.GetBuffer());
		//
		//CString szDstFileName;
		//m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_Firm, TEXT("--"));
		//if (GetCompanyName(FilePath, szDstFileName))
		//{
		//	m_CListCtrl.SetItemText(i, um_MiniFilterCallBack_Firm, (LPWSTR)szDstFileName.GetString());
		//}

		//获取下一个
		pCurList = pCurList->Blink;
		//释放资源
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}

	} while (pCurList != &pFileSystemDeviceInfo->List.List);

}

// Sfilter 回调列表（来自 IopFsNotifyChangeQueueHead / MountAware）
void DlgMiniFilterCallBack::InsertCtrlListControlSfilter(PCKernelCallBackInfo pSfilterInfo)
{
	if (pSfilterInfo == NULL) return;

	std::unordered_map<std::wstring, CString> companyCache;
	PCLIST_ENTRY pCurList = &pSfilterInfo->List.List;
	do
	{
		if (pCurList == NULL) break;
		int i = m_CListCtrl.GetItemCount();
		PCKernelCallBackInfo pInfo = (PCKernelCallBackInfo)pCurList;
		CString StrBuf;

		// 类型
		const wchar_t* typeStr = (pInfo->CallBackType == 1) ? L"Sfilter (MountAware)" : L"Sfilter";
		m_CListCtrl.InsertItem(i, typeStr);

		// 回调地址
		StrBuf.Format(L"%016I64X", pInfo->CallBackAddr);
		m_CListCtrl.SetItemText(i, um_Sfilter_CallBackAddr, StrBuf);

		// 位置：走 PdbResolver
		{
			ULONG64 modBase = (pInfo->CallBackAddr >= pInfo->ModuleOffset && pInfo->ModuleOffset != 0)
				? (pInfo->CallBackAddr - pInfo->ModuleOffset) : 0;
			WCHAR resolved[256] = { 0 };
			PdbResolver_Resolve(pInfo->CallBackAddr, modBase,
				pInfo->ModulePath, resolved, _countof(resolved));
			if (resolved[0]) m_CListCtrl.SetItemText(i, um_Sfilter_Pos, resolved);
		}

		// 驱动对象
		StrBuf.Format(L"%016I64X", pInfo->Descr);
		m_CListCtrl.SetItemText(i, um_Sfilter_DriverObject, StrBuf);

		// 路径
		CString filePath = pInfo->ModulePath[0] ? PathTransForm(pInfo->ModulePath) : CString(L"--");
		m_CListCtrl.SetItemText(i, um_Sfilter_ModulePath, filePath.GetBuffer());

		// 厂商
		CString company = L"--";
		if (pInfo->ModulePath[0])
		{
			auto it = companyCache.find(std::wstring(filePath.GetString()));
			if (it != companyCache.end()) company = it->second;
			else
			{
				CString tmp;
				if (GetCompanyName(filePath, tmp)) company = tmp;
				companyCache.emplace(std::wstring(filePath.GetString()), company);
			}
		}
		m_CListCtrl.SetItemText(i, um_Sfilter_Company, (LPWSTR)company.GetString());

		pCurList = pCurList->Blink;
		SIZE_T FreeSize = 0;
		MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE);
	} while (pCurList != &pSfilterInfo->List.List);
}

// ClassInitData 槽位名（与驱动端 EnumClassInitDataCallback 里的 slotOffs 索引一一对应）
static const wchar_t* const g_ClassInitSlotNames[] = {
	L"ClassInitDevice",
	L"ClassStartDevice",
	L"ClassPowerDevice",
	L"ClassStopDevice",
	L"ClassRemoveDevice",
	L"ClassQueryPnpCapabilities",
	L"ClassQueryId",
	L"ClassReadWriteVerification",
	L"ClassDeviceControl",
	L"ClassShutdownFlush",
	L"ClassCreateClose",
	L"ClassError",
};

void DlgMiniFilterCallBack::InsertCtrlListControlClassInit(PCKernelCallBackInfo pClassInitInfo)
{
	if (pClassInitInfo == NULL) return;

	std::unordered_map<std::wstring, CString> companyCache;
	PCLIST_ENTRY pCurList = &pClassInitInfo->List.List;
	do
	{
		if (pCurList == NULL) break;
		int i = m_CListCtrl.GetItemCount();
		PCKernelCallBackInfo pInfo = (PCKernelCallBackInfo)pCurList;
		CString StrBuf;

		// 槽位名
		const wchar_t* slotName = L"?";
		if (pInfo->CallBackType < _countof(g_ClassInitSlotNames))
			slotName = g_ClassInitSlotNames[pInfo->CallBackType];
		m_CListCtrl.InsertItem(i, slotName);

		// 回调地址
		StrBuf.Format(L"%016I64X", pInfo->CallBackAddr);
		m_CListCtrl.SetItemText(i, um_Sfilter_CallBackAddr, StrBuf);

		// 位置 → PdbResolver
		{
			ULONG64 modBase = (pInfo->CallBackAddr >= pInfo->ModuleOffset && pInfo->ModuleOffset != 0)
				? (pInfo->CallBackAddr - pInfo->ModuleOffset) : 0;
			WCHAR resolved[256] = { 0 };
			PdbResolver_Resolve(pInfo->CallBackAddr, modBase,
				pInfo->ModulePath, resolved, _countof(resolved));
			if (resolved[0]) m_CListCtrl.SetItemText(i, um_Sfilter_Pos, resolved);
		}

		// 客户驱动对象
		StrBuf.Format(L"%016I64X", pInfo->Descr);
		m_CListCtrl.SetItemText(i, um_Sfilter_DriverObject, StrBuf);

		// 路径
		CString filePath = pInfo->ModulePath[0] ? PathTransForm(pInfo->ModulePath) : CString(L"--");
		m_CListCtrl.SetItemText(i, um_Sfilter_ModulePath, filePath.GetBuffer());

		// 厂商
		CString company = L"--";
		if (pInfo->ModulePath[0])
		{
			auto it = companyCache.find(std::wstring(filePath.GetString()));
			if (it != companyCache.end()) company = it->second;
			else
			{
				CString tmp;
				if (GetCompanyName(filePath, tmp)) company = tmp;
				companyCache.emplace(std::wstring(filePath.GetString()), company);
			}
		}
		m_CListCtrl.SetItemText(i, um_Sfilter_Company, (LPWSTR)company.GetString());

		pCurList = pCurList->Blink;
		SIZE_T FreeSize = 0;
		MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE);
	} while (pCurList != &pClassInitInfo->List.List);
}

void DlgMiniFilterCallBack::InsertCtrlListControl(PCSysMajorFunctionInfo pCSysMajorFunctionInfo)
{
	if (pCSysMajorFunctionInfo == NULL)
	{
		return;
	}
	DriverMajorFunctionInfo m_DriverMajorFunctionInfo;
	PCLIST_ENTRY pCurList = &pCSysMajorFunctionInfo->List.List;
	do
	{
		if (pCurList == NULL)
		{
			break;
		}
		ULONG64 i = m_CListCtrl.GetItemCount();
		CString StrBuf;
		PCSysMajorFunctionInfo pInfo = (PCSysMajorFunctionInfo)pCurList;

		StrBuf.Format(L"%X", pInfo->Ord);
		m_CListCtrl.InsertItem(i, StrBuf);

		// “函数名称”优先用 PDB 解符号；PDB 未就绪时回落 module+0xRVA
		{
			WCHAR resolved[256] = { 0 };
			PdbResolver_Resolve(pInfo->FunAddr, pInfo->ModuleBase,
				pInfo->ModulePath, resolved, _countof(resolved));
			if (resolved[0])
				m_CListCtrl.SetItemText(i, um_FileSystemMajorFunction_FunName, resolved);
			else
				m_CListCtrl.SetItemText(i, um_FileSystemMajorFunction_FunName,
					m_DriverMajorFunctionInfo.TypeName[pInfo->Type % IRP_MJ_MAXIMUM_FUNCTION]);
		}

		StrBuf.Format(L"%016I64X", pInfo->FunAddr);
		m_CListCtrl.SetItemText(i, um_FileSystemMajorFunction_FunAddr, StrBuf);

		m_CListCtrl.SetItemText(i, um_FileSystemMajorFunction_MoudlePath, pInfo->ModulePath);

		CString FilePath = PathTransForm(pInfo->ModulePath);
		m_CListCtrl.SetItemText(i, um_FileSystemMajorFunction_MoudlePath, FilePath.GetBuffer());

		pCurList = pCurList->Blink;
		//清理资源
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pCSysMajorFunctionInfo->List.List);
}

void DlgMiniFilterCallBack::OnNMRClickDlgKernelMinifiltercallbackList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	int r = ShowListContextMenu(&m_CListCtrl, this);
	if (r == 0) { if (this->m_ThreadFlags != 1) OnMinifiltercallbackRefresh(); }
	else if (r > 0) CopyBufferToClipboard(&m_CListCtrl, r - 1);
}

void DlgMiniFilterCallBack::OnNMDblclkDlgKernelMinifiltercallbackTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	//获取选择的子集
	auto hSelectItem = m_CTreeCtrl.GetSelectedItem();

	//获取绑定的数据
	int SelType = m_CTreeCtrl.GetItemData(hSelectItem);
	switch (SelType)
	{
	case DlgMiniFilterCallBack::um_FileSystemType_MiniPortFilter:
	{
		//当重复选择时不刷新当前页面
		if (nPerSel != um_FileSystemType_MiniPortFilter)
		{
			//删除所有Column
			while (m_CListCtrl.DeleteColumn(0)) {}
			m_CListCtrl.InsertColumn(um_MiniFilterCallBack_FilterType_Name, _T("回调名"), LVCFMT_LEFT, 150);
			m_CListCtrl.InsertColumn(um_MiniFilterCallBack_PreOperation, _T("PreOperation"), LVCFMT_LEFT, 125);
			m_CListCtrl.InsertColumn(um_MiniFilterCallBack_PostOperation, _T("PostOperation"), LVCFMT_LEFT, 125);
			m_CListCtrl.InsertColumn(um_MiniFilterCallBack_pFilterAddr, _T("句柄"), LVCFMT_LEFT, 125);
			m_CListCtrl.InsertColumn(um_MiniFilterCallBack_Altitude, _T("Altitude"), LVCFMT_LEFT, 125);
			m_CListCtrl.InsertColumn(um_MiniFilterCallBack_Moudle, _T("所在模块路径"), LVCFMT_LEFT, 300);
			m_CListCtrl.InsertColumn(um_MiniFilterCallBack_Firm, _T("文件厂商"), LVCFMT_LEFT, 150);
			m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		}
		else
		{
			//不刷新
			return;
		}

		//重新设置选择的地方
		nPerSel = um_FileSystemType_MiniPortFilter;

		OnMinifiltercallbackRefresh();
	}
	break;
	case DlgMiniFilterCallBack::um_FileSystemType_FileSystem:
		if (nPerSel != um_FileSystemType_FileSystem)
		{
			while (m_CListCtrl.DeleteColumn(0)) {}
			m_CListCtrl.InsertColumn(um_FileSystem_Type, _T("类型"), LVCFMT_LEFT, 150);
			m_CListCtrl.InsertColumn(um_FileSystem_Deivce, _T("设备对象"), LVCFMT_LEFT, 125);
			m_CListCtrl.InsertColumn(um_FileSystem_DeviceObjectName, _T("设备对象名"), LVCFMT_LEFT, 125);
			m_CListCtrl.InsertColumn(um_FileSystem_Driver, _T("驱动对象"), LVCFMT_LEFT, 125);
			m_CListCtrl.InsertColumn(um_FileSystem_DriverName, _T("驱动对象名称"), LVCFMT_LEFT, 125);
			m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		}
		else
		{
			//不刷新
			return;
		}
		//重新设置选择的地方
		nPerSel = um_FileSystemType_FileSystem;

		OnMinifiltercallbackRefresh();
		break;
	case DlgMiniFilterCallBack::um_FileSystemType_SfilterCallBack:
		if (nPerSel != um_FileSystemType_SfilterCallBack)
		{
			while (m_CListCtrl.DeleteColumn(0)) {}
			m_CListCtrl.InsertColumn(um_Sfilter_Type,         _T("类型"),         LVCFMT_LEFT, 140);
			m_CListCtrl.InsertColumn(um_Sfilter_CallBackAddr, _T("回调地址"),     LVCFMT_LEFT, 140);
			m_CListCtrl.InsertColumn(um_Sfilter_Pos,          _T("位置"),         LVCFMT_LEFT, 200);
			m_CListCtrl.InsertColumn(um_Sfilter_DriverObject, _T("驱动对象"),     LVCFMT_LEFT, 140);
			m_CListCtrl.InsertColumn(um_Sfilter_ModulePath,   _T("所在模块路径"), LVCFMT_LEFT, 280);
			m_CListCtrl.InsertColumn(um_Sfilter_Company,      _T("文件厂商"),     LVCFMT_LEFT, 150);
			m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		}
		else
		{
			return;
		}
		nPerSel = um_FileSystemType_SfilterCallBack;
		OnMinifiltercallbackRefresh();
		break;

	case DlgMiniFilterCallBack::um_FileSystemType_ClassInitDataClass:
		if (nPerSel != um_FileSystemType_ClassInitDataClass)
		{
			while (m_CListCtrl.DeleteColumn(0)) {}
			m_CListCtrl.InsertColumn(um_Sfilter_Type,         _T("槽位"),         LVCFMT_LEFT, 200);
			m_CListCtrl.InsertColumn(um_Sfilter_CallBackAddr, _T("回调地址"),     LVCFMT_LEFT, 140);
			m_CListCtrl.InsertColumn(um_Sfilter_Pos,          _T("位置"),         LVCFMT_LEFT, 240);
			m_CListCtrl.InsertColumn(um_Sfilter_DriverObject, _T("客户驱动对象"), LVCFMT_LEFT, 140);
			m_CListCtrl.InsertColumn(um_Sfilter_ModulePath,   _T("所在模块路径"), LVCFMT_LEFT, 280);
			m_CListCtrl.InsertColumn(um_Sfilter_Company,      _T("文件厂商"),     LVCFMT_LEFT, 150);
			m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		}
		else
		{
			return;
		}
		nPerSel = um_FileSystemType_ClassInitDataClass;
		OnMinifiltercallbackRefresh();
		break;
	case DlgMiniFilterCallBack::um_FileSystemType_NpfsMajorFunction:
	case DlgMiniFilterCallBack::um_FileSystemType_MsfsMajorFunction:
	case DlgMiniFilterCallBack::um_FileSystemType_UsbPortMajorFunction:
	{
		if (nPerSel != um_FileSystemType_NpfsMajorFunction || 
			nPerSel != um_FileSystemType_MsfsMajorFunction ||
			nPerSel != um_FileSystemType_UsbPortMajorFunction)
		{
			while (m_CListCtrl.DeleteColumn(0)) {}
			m_CListCtrl.InsertColumn(um_FileSystemCallBack_Order, _T("序号"), LVCFMT_LEFT, 50);
			m_CListCtrl.InsertColumn(um_FileSystemCallBack_FunName, _T("函数名称"), LVCFMT_LEFT, 125);
			m_CListCtrl.InsertColumn(um_FileSystemCallBack_CurFunAddr, _T("当前函数地址"), LVCFMT_LEFT, 125);
			//m_CListCtrl.InsertColumn(um_FileSystemCallBack_Hook, _T("Hook"), LVCFMT_LEFT, 125);
			//m_CListCtrl.InsertColumn(um_FileSystemCallBack_SrcFunAddr, _T("原函数地址"), LVCFMT_LEFT, 125);
			m_CListCtrl.InsertColumn(um_FileSystemCallBack_ModulePath, _T("函数所在模块路径"), LVCFMT_LEFT, 300);
			m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		}
		else
		{
			//不刷新
			return;
		}
		nPerSel = SelType;
		OnMinifiltercallbackRefresh();
		break;
	}
	default:
		break;
	}
}
