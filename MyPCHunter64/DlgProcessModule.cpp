// DlgProcessModule.cpp: 实现文件
//

#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgProcessModule.h"
#include "Thread.h"
#include "Dump/PeDump.h"
#include "Inject/DllInject.h"

// DlgProcessModule 对话框

IMPLEMENT_DYNAMIC(DlgProcessModule, CDialogEx)

DlgProcessModule::DlgProcessModule(CString StrEprocess, CString StrProcessName, CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_PROCESS_MODULE, pParent)
{
	m_StrEprocess = StrEprocess;
	m_StrProcessName = StrProcessName;
}

DlgProcessModule::~DlgProcessModule()
{
}

void DlgProcessModule::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_PROCESS_MODULE_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgProcessModule, CDialogEx)
	ON_COMMAND(ID_PROCESSMODULE_REFRESH, &DlgProcessModule::OnProcessmoduleRefresh)
	ON_WM_SIZE()
	ON_NOTIFY(NM_RCLICK, ID_PROCESS_MODULE_LIST, &DlgProcessModule::OnNMRClickProcessModuleList)
END_MESSAGE_MAP()

// DlgProcessModule 消息处理程序

void DlgProcessModule::OnProcessmoduleRefresh()
{
	m_CListCtrl.DeleteAllItems();

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumProcessModuleInfo, this });
}

BOOL DlgProcessModule::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(m_StrProcessName.GetBuffer());//设置窗口标题

	m_CListCtrl.InsertColumn(um_Process_Module_Name, _T("名称"), LVCFMT_LEFT, 200);
	m_CListCtrl.InsertColumn(um_Process_Module_BaseAddr, _T("模块基址"), LVCFMT_LEFT, 150);
	m_CListCtrl.InsertColumn(um_Process_Module_Size, _T("大小"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Process_Module_Path, _T("模块路径"), LVCFMT_LEFT, 300);
	m_CListCtrl.InsertColumn(um_Process_Module_Signal, _T("数字签名"), LVCFMT_LEFT, 150);
	m_CListCtrl.InsertColumn(um_Process_Module_Company, _T("公司名"), LVCFMT_LEFT, 150);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	OnProcessmoduleRefresh();
	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void DlgProcessModule::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	RECT rect = { 0 };
	rect.bottom = cy;
	rect.right = cx;
	m_CListCtrl.MoveWindow(&rect, TRUE);
}

void DlgProcessModule::InsertCtrlListControl(PCProcessModuleInfo pinfo)
{
	if (pinfo == NULL)
	{
		return;
	}

	SIZE_T FreeSize = sizeof(CProcessModuleInfo);
	PCLIST_ENTRY pCurList = &pinfo->List.List;

	do
	{
		if (pCurList == NULL)
		{
			break;
		}

		PCProcessModuleInfo pProcessModuleInfo = (PCProcessModuleInfo)pCurList;
		{
			int  i = m_CListCtrl.GetItemCount();

			CString StrBuf;

			//插入名称
			m_CListCtrl.InsertItem(i, pProcessModuleInfo->ModuleName);

			StrBuf.Format(L"0x%I64X", pProcessModuleInfo->ModuleBaseAddr);		
			m_CListCtrl.SetItemText(i, 1, StrBuf);

			StrBuf.Format(L"0x%08I64X", pProcessModuleInfo->ModuleSize);
			m_CListCtrl.SetItemText(i, 2, StrBuf);

			m_CListCtrl.SetItemText(i, 3, pProcessModuleInfo->ModuleFullPath);

			TCHAR szSoftSignBuf[MAXBYTE] = { 0 };
			if (GetSoftSign(pProcessModuleInfo->ModuleFullPath, szSoftSignBuf, MAXBYTE) == 0)
			{
				m_CListCtrl.SetItemText(i, 4, szSoftSignBuf);
			}
			else
			{
				m_CListCtrl.SetItemText(i, 4, TEXT("--"));
			}

			CString szDstFileName;
			m_CListCtrl.SetItemText(i, 5, this->GetCompanyName(pProcessModuleInfo->ModuleFullPath, szDstFileName) ? (LPWSTR)szDstFileName.GetString() : TEXT("--"));

		}
		//指向下一个
		pCurList = pCurList->Blink;

		//释放内存
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pProcessModuleInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pinfo->List.List);

}

void DlgProcessModule::OnNMRClickProcessModuleList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	if (m_CListCtrl.GetItemCount() <= 0) return;

	// 复用 ShowListContextMenu 的 "复制 / 刷新" 默认菜单，但因为我们要叠加
	// "Dump PE..." 这条业务项，所以这里走自定义菜单的写法。
	POSITION sp = m_CListCtrl.GetFirstSelectedItemPosition();
	bool hasSel = (sp != NULL);
	int selRow = hasSel ? ((int)sp - 1) : -1;

	enum { kCopyBase = 1000, kRefresh = 2000, kDumpPe = 2001, kHijack = 2004 };

	CMenu menu; menu.CreatePopupMenu();
	int nCols = AppendCopyColumnsSubmenu(menu, &m_CListCtrl, kCopyBase, hasSel);
	menu.AppendMenuW(MF_STRING, kRefresh, L"刷新");
	menu.AppendMenuW(MF_SEPARATOR, 0, L"");
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kDumpPe, L"Dump PE ...");
	menu.AppendMenuW(MF_STRING, kHijack, L"DLL 注入 (线程劫持) ...");

	CString explorerPath;
	UINT explorerCmd = AppendOpenInExplorerItem(menu, &m_CListCtrl, explorerPath);

	CPoint pt; GetCursorPos(&pt);
	int cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, this);
	if (cmd == 0) return;
	if (HandleOpenInExplorerCmd(cmd, explorerCmd, explorerPath)) return;

	if (TryHandleCopyColumnsCmd((UINT)cmd, kCopyBase, nCols, &m_CListCtrl)) return;
	if (cmd == kRefresh)
	{
		if (this->m_ThreadFlags != TRUE) OnProcessmoduleRefresh();
		return;
	}
	if (cmd == kDumpPe && hasSel)
	{
		// 取模块基址（列 1）、模块名（列 0）作为默认文件名
		CString baseStr = m_CListCtrl.GetItemText(selRow, um_Process_Module_BaseAddr);
		CString name = m_CListCtrl.GetItemText(selRow, um_Process_Module_Name);
		ULONG64 imageBase = _wcstoui64(
			baseStr.GetLength() > 2 && baseStr[0] == L'0' && (baseStr[1] == L'x' || baseStr[1] == L'X')
				? baseStr.GetBuffer() + 2 : baseStr.GetBuffer(),
			nullptr, 16);
		baseStr.ReleaseBuffer();
		if (imageBase == 0)
		{
			AfxMessageBox(L"无法解析模块基址");
			return;
		}

		// 默认文件名：<模块名>_<basehex>.dmp.<ext>
		CString defName;
		defName.Format(L"%s_0x%016I64X_dump", name.GetString(), imageBase);
		CFileDialog dlg(FALSE, L"bin", defName,
			OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
			L"PE Dump (*.exe;*.dll;*.bin)|*.exe;*.dll;*.bin|所有文件 (*.*)|*.*||", this);
		if (dlg.DoModal() != IDOK) return;

		// 把当前列表里所有模块收集起来，供 IAT 重建使用
		std::vector<ImportRebuilder::LoadedModule> modules;
		int total = m_CListCtrl.GetItemCount();
		modules.reserve(total);
		for (int i = 0; i < total; ++i)
		{
			ImportRebuilder::LoadedModule m;
			m.name     = m_CListCtrl.GetItemText(i, um_Process_Module_Name).GetString();
			m.fullPath = m_CListCtrl.GetItemText(i, um_Process_Module_Path).GetString();
			CString bStr  = m_CListCtrl.GetItemText(i, um_Process_Module_BaseAddr);
			CString szStr = m_CListCtrl.GetItemText(i, um_Process_Module_Size);
			const wchar_t* b = bStr.GetString();
			if (b[0] == L'0' && (b[1] == L'x' || b[1] == L'X')) b += 2;
			m.base = _wcstoui64(b, nullptr, 16);
			const wchar_t* s = szStr.GetString();
			if (s[0] == L'0' && (s[1] == L'x' || s[1] == L'X')) s += 2;
			m.size = _wcstoui64(s, nullptr, 16);
			if (m.base != 0 && m.size != 0 && !m.fullPath.empty())
				modules.push_back(std::move(m));
		}

		std::wstring err;
		PeDump::RebuildResult rr;
		bool ok = PeDump::DumpModuleFromProcess(
			m_StrEprocess.GetString(),
			(unsigned long long)imageBase,
			modules,
			dlg.GetPathName().GetString(),
			rr,
			err);

		if (!ok)
		{
			CString msg;
			msg.Format(L"Dump 失败：%s", err.c_str());
			AfxMessageBox(msg);
			return;
		}

		// Dump 成功，再展示 IAT 重建结果
		CString msg;
		if (!rr.attempted)
		{
			msg.Format(L"Dump 成功（未尝试 IAT 重建）：\n%s", dlg.GetPathName().GetString());
		}
		else if (rr.success)
		{
			msg.Format(
				L"Dump 成功，IAT 重建完成：\n%s\n\n"
				L"IAT 条目：%lu\n已解析：%lu  失败：%lu\n模块：%lu  区段：%lu%s\n\n"
				L"诊断日志：%s",
				dlg.GetPathName().GetString(),
				rr.stats.totalIatEntries, rr.stats.resolved, rr.stats.unresolved,
				rr.stats.modulesUsed, rr.stats.iatRegions,
				rr.stats.autoLocated ? L"（自动定位）" : L"",
				rr.iatLogPath.c_str());
		}
		else
		{
			msg.Format(
				L"Dump 成功，但 IAT 重建失败：\n%s\n\n"
				L"原因：%s\n\n"
				L"已写盘的是 raw memory snapshot，仍可用 IDA / x64dbg 静态分析。",
				dlg.GetPathName().GetString(),
				rr.errMsg.c_str());
		}
		AfxMessageBox(msg);
	}

	if (cmd == kHijack)
	{
		CFileDialog dlg(TRUE, L"dll", nullptr,
			OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
			L"动态链接库 (*.dll)|*.dll|所有文件 (*.*)|*.*||", this);
		if (dlg.DoModal() != IDOK) return;

		std::wstring err;
		DllInject::ManualResult rr;
		bool ok = DllInject::HijackInjectDll(
			m_StrEprocess.GetString(),
			dlg.GetPathName().GetString(),
			rr, err);

		CString msg;
		if (ok)
		{
			msg.Format(
				L"Force-APC 投递完成：\n%s\n\n"
				L"模块基址：0x%016llX\n入口点：0x%016llX\n大小：0x%lX\n"
				L"成功投递线程数：%lu\n枚举：%lu  系统线程：%lu  失败：%lu\n\n"
				L"注：使用 Blackbone 风格 force-APC（user APC + kernel APC 调 KeTestAlertThread(UserMode) 强制唤醒）。\n"
				L"提示成功只代表 APC 已入队；DllMain 是否真正执行需观察目标行为。\n"
				L"若目标进程沙箱限制 user APC，可换 notepad 等普通进程验证。",
				dlg.GetPathName().GetString(),
				rr.moduleBase, rr.entryPoint, rr.sizeOfImage,
				rr.queuedCount, rr.threadsSeen, rr.threadsSystem, rr.apcFails);
		}
		else
		{
			msg.Format(L"线程劫持失败：%s", err.c_str());
		}
		AfxMessageBox(msg);
		return;
	}
}
