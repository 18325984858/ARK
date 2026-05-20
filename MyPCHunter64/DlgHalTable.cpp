// DlgHalTable.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgHalTable.h"
#include "Thread.h"
#include "PdbResolver.h"

// DlgHalTable 对话框

IMPLEMENT_DYNAMIC(DlgHalTable, CDialogEx)

DlgHalTable::DlgHalTable(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_HALTABLE, pParent)
{

}

DlgHalTable::~DlgHalTable()
{
}

void DlgHalTable::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_DLG_KERNEL_MINIFILTERCALLBACK_TREE, m_CTreeCtrl);
	DDX_Control(pDX, ID_DLG_KERNEL_MINIFILTERCALLBACK_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgHalTable, CDialogEx)
	ON_NOTIFY(NM_DBLCLK, ID_DLG_KERNEL_MINIFILTERCALLBACK_TREE, &DlgHalTable::OnNMDblclkDlgKernelMinifiltercallbackTree)
	ON_WM_SIZE()
	ON_COMMAND(ID_HALTABLE_REFRESH, &DlgHalTable::OnHaltableRefresh)
	ON_NOTIFY(NM_RCLICK, ID_DLG_KERNEL_MINIFILTERCALLBACK_LIST, &DlgHalTable::OnNMRClickDlgKernelMinifiltercallbackList)
END_MESSAGE_MAP()

// DlgHalTable 消息处理程序

void DlgHalTable::OnNMDblclkDlgKernelMinifiltercallbackTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	//获取选择的子集
	auto hSelectItem = m_CTreeCtrl.GetSelectedItem();

	//获取绑定的数据
	int SelType = m_CTreeCtrl.GetItemData(hSelectItem);
	switch (SelType)
	{
	case um_HalTableDlgType_HalDispatchTable:
	{
		if (nPerSel == um_HalTableDlgType_HalDispatchTable)
		{
			return;
		}

		//刷新显示的数据
		OnHaltableRefresh();

		//重新设置选择的地方
		nPerSel = um_HalTableDlgType_HalDispatchTable;
	}
	break;
	case um_HalTableDlgType_HalPrivateDispatchTable:
	{

		if (nPerSel == um_HalTableDlgType_HalPrivateDispatchTable)
		{
			return;

		}

		//刷新显示的数据
		OnHaltableRefresh();

		//重新设置选择的地方
		nPerSel = um_HalTableDlgType_HalPrivateDispatchTable;
	}
	break;
	case um_HalTableDlgType_HalAcpiDispatchTable:
	{

		if (nPerSel == um_HalTableDlgType_HalAcpiDispatchTable)
		{
			return;
		}

		//刷新显示的数据
		OnHaltableRefresh();

		//重新设置选择的地方
		nPerSel = um_HalTableDlgType_HalAcpiDispatchTable;
	}
	break;
	default:
		nPerSel = 0;
		break;
	}
}

void DlgHalTable::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	CRect rect;
	GetClientRect(&rect);

	float fwidth = rect.Width() / 4;

	m_CTreeCtrl.SetWindowPos(NULL, 0, 0, fwidth, rect.Height(), SWP_NOZORDER);
	m_CListCtrl.SetWindowPos(NULL, fwidth, 0, rect.Width() - fwidth, rect.Height(), SWP_NOZORDER);
}

BOOL DlgHalTable::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	//初始化树控件
	m_CTreeCtrl.SetExtendedStyle(m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
		m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS);

	//创建根节点
	auto RootNode = m_CTreeCtrl.InsertItem(L"Hal回调");
	//创建子节点1
	auto ChildNode0 = m_CTreeCtrl.InsertItem(L"HalDispatchTable", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode0, um_HalTableDlgType_HalDispatchTable);
	//创建子节点2
	auto ChildNode1 = m_CTreeCtrl.InsertItem(L"HalPrivateDispatchTable", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode1, um_HalTableDlgType_HalPrivateDispatchTable);
	//创建子节点2
	auto ChildNode3 = m_CTreeCtrl.InsertItem(L"HalAcpiDispatchTable", RootNode);
	m_CTreeCtrl.SetItemData(ChildNode3, um_HalTableDlgType_HalAcpiDispatchTable);

	//初始化List控件
	m_CListCtrl.InsertColumn(um_HalTableDlgInfo_Order, _T("序号"), LVCFMT_LEFT, 70);
	m_CListCtrl.InsertColumn(um_HalTableDlgInfo_FunName, _T("函数名称"), LVCFMT_LEFT, 150);
	m_CListCtrl.InsertColumn(um_HalTableDlgInfo_CurFunAddr, _T("当前函数地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_HalTableDlgInfo_Hook, _T("HOOK"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_HalTableDlgInfo_SrcFunAddr, _T("原始函数地址"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_HalTableDlgInfo_Pos, _T("位置"), LVCFMT_LEFT, 250);
	m_CListCtrl.InsertColumn(um_HalTableDlgInfo_CurModule, _T("当前函数所在模块路径"), LVCFMT_LEFT, 300);
	m_CListCtrl.InsertColumn(um_HalTableDlgInfo_FileVender, _T("文件厂商"), LVCFMT_LEFT, 125);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgHalTable::OnHaltableRefresh()
{
	m_CListCtrl.DeleteAllItems();

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumHalTableInfo, this });

}

void DlgHalTable::OnNMRClickDlgKernelMinifiltercallbackList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	int r = ShowListContextMenu(&m_CListCtrl, this);
	if (r == 0) { if (this->m_ThreadFlags != 1) OnHaltableRefresh(); }
	else if (r > 0) CopyBufferToClipboard(&m_CListCtrl, r - 1);
}

void DlgHalTable::InsertCtrlListControl()
{
	//验证当前选择的项
	if (nPerSel < 0 || nPerSel > 3)
	{
		return;
	}

	//发送包
	PCHalFunTableInfo pCHalTableInfo = NULL;
	if (!g_LoadDriver.SendMsg(um_Cmd_Enum_HalTable_info, (LPVOID)((nPerSel + 1) % 0x4), (LPVOID*)&pCHalTableInfo))
	{
		return;
	}

	WCHAR* HalPrivateDispatchTableFunName[] = {
		L"HalHandlerForBus",
		L"HalHandlerForConfigSpace",
		L"HalLocateHiberRanges",
		L"HalRegisterBusHandler",
		L"HalSetWakeEnable",
		L"HalSetWakeAlarm",
		L"HalPciTranslateBusAddress",
		L"HalPciAssignSlotResources",
		L"HalHaltSystem",
		L"HalFindBusAddressTranslation",
		L"HalResetDisplay",
		L"HalAllocateMapRegisters",
		L"KdSetupPciDeviceForDebugging",
		L"KdReleasePciDeviceForDebugging",
		L"KdGetAcpiTablePhase0" ,
		L"KdCheckPowerButton" ,
		L"HalVectorToIDTEntry" ,
		L"KdMapPhysicalMemory64" ,
		L"KdUnmapVirtualAddress" ,
		L"KdGetPciDataByOffset" ,
		L"KdSetPciDataByOffset" ,
		L"HalGetInterruptVectorOverride" ,
		L"HalGetVectorInputOverride",
		L"HalLoadMicrocode",
		L"HalUnloadMicrocode",
		L"HalPostMicrocodeUpdate",
		L"HalAllocateMessageTargetOverride",
		L"HalFreeMessageTargetOverride",
		L"HalDpReplaceBegin",
		L"HalDpReplaceTarget",
		L"HalDpReplaceControl",
		L"HalDpReplaceEnd",
		L"HalPrepareForBugcheck",
		L"HalQueryWakeTime",
		L"HalReportIdleStateUsage",
		L"HalTscSynchronization",
		L"HalWheaInitProcessorGenericSection",
		L"HalStopLegacyUsbInterrupts",
		L"HalReadWheaPhysicalMemory",
		L"HalWriteWheaPhysicalMemory",
		L"HalDpMaskLevelTriggeredInterrupts",
		L"HalDpUnmaskLevelTriggeredInterrupts",
		L"HalDpGetInterruptReplayState",
		L"HalDpReplayInterrupts",
		L"HalQueryIoPortAccessSupported",
		L"KdSetupIntegratedDeviceForDebugging",
		L"KdReleaseIntegratedDeviceForDebugging",
		L"HalGetEnlightenmentInformation",
		L"HalAllocateEarlyPages",
		L"HalMapEarlyPages",
		L"Dummy1",
		L"Dummy2",
		L"HalNotifyProcessorFreeze",
		L"HalPrepareProcessorForIdle",
		L"HalRegisterLogRoutine",
		L"HalResumeProcessorFromIdle",
		L"Dummy",
		L"HalVectorToIDTEntryEx",
		L"HalSecondaryInterruptQueryPrimaryInformation",
		L"HalMaskInterrupt",
		L"HalUnmaskInterrupt",
		L"HalIsInterruptTypeSecondary",
		L"HalAllocateGsivForSecondaryInterrupt",
		L"HalAddInterruptRemapping",
		L"HalRemoveInterruptRemapping",
		L"HalSaveAndDisableHvEnlightenment",
		L"HalRestoreHvEnlightenment",
		L"HalFlushIoBuffersExternalCache",
		L"HalFlushExternalCache",
		L"HalPciEarlyRestore",
		L"HalGetProcessorId",
		L"HalAllocatePmcCounterSet",
		L"HalCollectPmcCounters",
		L"HalFreePmcCounterSet",
		L"HalProcessorHalt",
		L"HalTimerQueryCycleCounter",
		L"Dummy3",
		L"HalPciMarkHiberPhase",
		L"HalQueryProcessorRestartEntryPoint",
		L"HalRequestInterrupt",
		L"HalEnumerateUnmaskedInterrupts",
		L"HalFlushAndInvalidatePageExternalCache",
		L"KdEnumerateDebuggingDevices",
		L"HalFlushIoRectangleExternalCache",
		L"HalPowerEarlyRestore",
		L"HalQueryCapsuleCapabilities",
		L"HalUpdateCapsule",
		L"HalPciMultiStageResumeCapable",
		L"HalDmaFreeCrashDumpRegisters",
		L"HalAcpiAoacCapable",
		L"HalInterruptSetDestination",
		L"HalGetClockConfiguration",
		L"HalClockTimerActivate",
		L"HalClockTimerInitialize",
		L"HalClockTimerStop",
		L"HalClockTimerArm",
		L"HalTimerOnlyClockInterruptPending",
		L"HalAcpiGetMultiNode",
		L"HalPowerSetRebootHandler",
		L"HalIommuRegisterDispatchTable",
		L"HalTimerWatchdogStart",
		L"HalTimerWatchdogResetCountdown",
		L"HalTimerWatchdogStop",
		L"HalTimerWatchdogGeneratedLastReset",
		L"HalTimerWatchdogTriggerSystemReset",
		L"HalInterruptVectorDataToGsiv" ,
		L"HalInterruptGetHighestPriorityInterrupt" ,
		L"HalProcessorOn",
		L"HalProcessorOff",
		L"HalProcessorFreeze",
		L"HalDmaLinkDeviceObjectByToken",
		L"HalDmaCheckAdapterToken" ,
		L"Dummy4" ,
		L"HalTimerConvertPerformanceCounterToAuxiliaryCounter" ,
		L"HalTimerConvertAuxiliaryCounterToPerformanceCounter" ,
		L"HalTimerQueryAuxiliaryCounterFrequency" ,
		L"HalConnectThermalInterrupt" ,
		L"HalIsEFIRuntimeActive" ,
		L"HalTimerQueryAndResetRtcErrors" ,
		L"HalAcpiLateRestore" ,
		L"KdWatchdogDelayExpiration" ,
		L"HalGetProcessorStats" ,
		L"HalTimerWatchdogQueryDueTime" ,
		L"HalConnectSyntheticInterrupt" ,
		L"HalPreprocessNmi" ,
		L"HalEnumerateEnvironmentVariablesWithFilter" ,
		L"HalCaptureLastBranchRecordStack" ,
		L"HalClearLastBranchRecordStack" ,
		L"HalConfigureLastBranchRecord" ,
		L"HalGetLastBranchInformation" ,
		L"HalResumeLastBranchRecord" ,
		L"HalStartLastBranchRecord" ,
		L"HalStopLastBranchRecord" ,
		L"HalIommuBlockDevice" ,
		L"HalIommuUnblockDevice" ,
		L"HalGetIommuInterface" ,
		L"HalRequestGenericErrorRecovery" ,
		L"HalTimerQueryHostPerformanceCounter" ,
		L"HalTopologyQueryProcessorRelationships" ,
		L"HalInitPlatformDebugTriggers" ,
		L"HalRunPlatformDebugTriggers" ,
		L"HalTimerGetReferencePage" ,
		L"HalGetHiddenProcessorPowerInterface" ,
		L"HalGetHiddenProcessorPackageId" ,
		L"HalGetHiddenPackageProcessorCount" ,
		L"HalGetHiddenProcessorApicIdByIndex" ,
		L"HalRegisterHiddenProcessorIdleState" ,
		L"HalIommuReportIommuFault" ,
		L"HalIommuDmaRemappingCapable" };
	WCHAR* HalDispatchTableFunName[] = {
		L"HalQuerySystemInformation",
		L"HalSetSystemInformation",
		L"HalQueryBusSlots",
		L"Spare1",
		L"HalExamineMBR",
		L"HalIoReadPartitionTable",
		L"HalIoSetPartitionInformation",
		L"HalIoWritePartitionTable",
		L"HalReferenceHandlerForBus",
		L"HalReferenceBusHandler",
		L"HalDereferenceBusHandler",
		L"HalInitPnpDriver",
		L"HalInitPowerManagement",
		L"HalGetDmaAdapter",
		L"HalGetInterruptTranslator",
		L"HalStartMirroring",
		L"HalEndMirroring",
		L"HalMirrorPhysicalMemory",
		L"HalEndOfBoot",
		L"HalMirrorVerify",
		L"HalGetCachedAcpiTable",
		L"HalSetPciErrorHandlerCallback",
		L"HalGetPrmCache" };

	WCHAR** NameTable[] = { HalDispatchTableFunName, HalPrivateDispatchTableFunName,NULL };
	ULONG64 LengthTable[] = { (sizeof(HalDispatchTableFunName) / sizeof(WCHAR*)) + 1,(sizeof(HalPrivateDispatchTableFunName) / sizeof(WCHAR*)) + 1 };

	PCLIST_ENTRY pCurList = &pCHalTableInfo->List.List;
	do
	{
		if (pCurList == NULL)
		{
			break;
		}
		ULONG64 i = m_CListCtrl.GetItemCount();
		PCHalFunTableInfo pInfo = (PCHalFunTableInfo)pCurList;
		CString StrBuf;
		StrBuf.Format(L"%04X", pInfo->pFunOrder);
		m_CListCtrl.InsertItem(i, StrBuf);

		if (NameTable[nPerSel] != NULL)
		{
			m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_FunName, (NameTable[nPerSel])[i % LengthTable[nPerSel]]);
		}

		StrBuf.Format(L"%016I64X", pInfo->pFunAddr);
		m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_CurFunAddr, StrBuf);

		// 位置：PdbResolver 解析符号名
		{
			WCHAR resolved[256] = { 0 };
			PdbResolver_Resolve(pInfo->pFunAddr, 0, pInfo->ModulePath, resolved, _countof(resolved));
			m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_Pos, resolved);
		}

		CString FilePath = PathTransForm(pInfo->ModulePath);
		m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_CurModule, pInfo->pFunAddr == NULL ? L"--" : FilePath.GetBuffer());

		CString szDstFileName;
		m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_FileVender, TEXT("--"));
		if (GetCompanyName(FilePath, szDstFileName))
		{
			m_CListCtrl.SetItemText(i, um_HalTableDlgInfo_FileVender, (LPWSTR)szDstFileName.GetString());
		}

		//获取下一个
		pCurList = pCurList->Blink;
		//释放资源
		SIZE_T FreeSize = 0;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}

	} while (pCurList != &pCHalTableInfo->List.List);
}