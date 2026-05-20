// DlgProcess.cpp: 实现文件
//
#define _CRT_NON_CONFORMING_SWPRINTFS
#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgProcess.h"
#include "../MyDriver64/Struct.h"
#include "Thread.h"
#include "DlgProcessVad.h"
#include "DlgProcessThread.h"
#include "DlgProcessModule.h"
// DlgProcess 对话框

IMPLEMENT_DYNAMIC(DlgProcess, CDialogEx)

DlgProcess::DlgProcess(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_PROCESS, pParent)
{

}

DlgProcess::~DlgProcess()
{
}

void DlgProcess::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_CONTROL_PROCESS_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgProcess, CDialogEx)
	ON_WM_SIZE()
	ON_NOTIFY(NM_RCLICK, ID_CONTROL_PROCESS_LIST, &DlgProcess::OnNMRClickControlProcessList)
	ON_COMMAND(ID_PROCESS_REFRESH, &DlgProcess::OnDriverRefresh)
	ON_COMMAND(ID_PROCESS_VAD, &DlgProcess::OnProcessVad)
	ON_COMMAND(ID_PROCESS_HANDLE, &DlgProcess::OnProcessHandle)
	ON_COMMAND(ID_PROCESS_THREAD, &DlgProcess::OnProcessThread)
	ON_COMMAND(ID_PROCESS_MODULE, &DlgProcess::OnProcessModule)
	ON_COMMAND(ID_PROCESS_MENU_COPY_NAME, &DlgProcess::OnProcessMenuCopyName)
	ON_COMMAND(ID_PROCESS_MENU_COPY_ID, &DlgProcess::OnProcessMenuCopyId)
	ON_COMMAND(ID_PROCESS_MENU_COPY_PARENTID, &DlgProcess::OnProcessMenuCopyParentid)
	ON_COMMAND(ID_PROCESS_MENU_COPY_SESSIONID, &DlgProcess::OnProcessMenuCopySessionid)
	ON_COMMAND(ID_PROCESS_MENU_COPY_USERNAME, &DlgProcess::OnProcessMenuCopyUsername)
	ON_COMMAND(ID_PROCESS_MENU_COPY_FILEPATH, &DlgProcess::OnProcessMenuCopyFilepath)
	ON_COMMAND(ID_PROCESS_MENU_COPY_EPROCESS, &DlgProcess::OnProcessMenuCopyEprocess)
	ON_COMMAND(ID_PROCESS_MENU_COPY_VISITSTATE, &DlgProcess::OnProcessMenuCopyVisitstate)
	ON_COMMAND(ID_PROCESS_MENU_COPY_FILEFIRM, &DlgProcess::OnProcessMenuCopyFilefirm)
	ON_COMMAND(ID_PROCESS_MENU_COPY_DEBUGSTATE, &DlgProcess::OnProcessMenuCopyDebugstate)
	ON_COMMAND(ID_PROCESS_MENU_COPY_RUNTIME, &DlgProcess::OnProcessMenuCopyRuntime)
	ON_COMMAND(ID_PROCESS_MENU_COPY_PARAM, &DlgProcess::OnProcessMenuCopyParam)
	ON_COMMAND(ID_PROCESS_MENU_COPY_OPENFILE, &DlgProcess::OnProcessMenuCopyOpenfile)
	ON_COMMAND(ID_PROCESS_MENU_COPY_ATTRIBUTE, &DlgProcess::OnProcessMenuCopyAttribute)
	ON_COMMAND(ID_PROCESS_KILLPROCESS, &DlgProcess::OnProcessKillprocess)
	ON_COMMAND(ID_PROCESS_CLEAR_PROTECTION, &DlgProcess::OnProcessClearProtection)
	ON_COMMAND(ID_PROCESS_PPL, &DlgProcess::OnProcessPpl)
	ON_COMMAND(ID_PROCESS_PP, &DlgProcess::OnProcessPp)
	ON_COMMAND(ID_PROCESS_NP, &DlgProcess::OnProcessNp)
END_MESSAGE_MAP()

// DlgProcess 消息处理程序

BOOL DlgProcess::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_CListCtrl.InsertColumn(um_Process_Name, _T("映像名称"), LVCFMT_LEFT, 130);
	m_CListCtrl.InsertColumn(um_Process_Id, _T("进程ID"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_Process_ParentId, _T("父进程ID"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_Process_SessionId, _T("会话ID"), LVCFMT_LEFT, 50);
	m_CListCtrl.InsertColumn(um_Process_UserName, _T("用户名"), LVCFMT_LEFT, 75);
	m_CListCtrl.InsertColumn(um_Process_FilePath, _T("映像路径"), LVCFMT_LEFT, 350);
	m_CListCtrl.InsertColumn(um_Process_Object, _T("EPROCESS"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_Process_VisitState, _T("应用层访问状态"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_Process_FileFirm, _T("文件厂商"), LVCFMT_LEFT, 125);
	m_CListCtrl.InsertColumn(um_Process_DebugState, _T("调试状态"), LVCFMT_LEFT, 70);
	m_CListCtrl.InsertColumn(um_Process_Architecture, _T("架构"), LVCFMT_LEFT, 70);
	m_CListCtrl.InsertColumn(um_Process_RunTime, _T("开始运行时间"), LVCFMT_LEFT, 165);
	m_CListCtrl.InsertColumn(um_Process_Param, _T("参数"), LVCFMT_LEFT, 2000);
	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	return TRUE;
	// 异常: OCX 属性页应返回 FALSE
}

void DlgProcess::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	RECT rect = { 0 };
	rect.bottom = cy;
	rect.right = cx;
	m_CListCtrl.MoveWindow(&rect, TRUE);
}

void DlgProcess::OnNMRClickControlProcessList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	CMenu menu;
	POINT point = { 0 };

	GetCursorPos(&point);//获取当前的游标
	menu.LoadMenuW(ID_MENU_PROCESS);//加载菜单资源
	CMenu* pPopup = menu.GetSubMenu(0);//

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	if (this->m_ThreadFlags == 1)
	{
		menu.EnableMenuItem(ID_PROCESS_REFRESH, MF_GRAYED | MF_BYCOMMAND);
	}

	if (FristIndex <= 0)
	{
		menu.EnableMenuItem(ID_PROCESS_KILLPROCESS, MF_GRAYED | MF_BYCOMMAND);
		menu.EnableMenuItem(ID_PROCESS_MODULE, MF_GRAYED | MF_BYCOMMAND);
		menu.EnableMenuItem(ID_PROCESS_HANDLE, MF_GRAYED | MF_BYCOMMAND);
		menu.EnableMenuItem(ID_PROCESS_THREAD, MF_GRAYED | MF_BYCOMMAND);
		menu.EnableMenuItem(ID_PROCESS_VAD, MF_GRAYED | MF_BYCOMMAND);
	}

	CString explorerPath;
	UINT explorerCmd = AppendOpenInExplorerItem(*pPopup, &m_CListCtrl, explorerPath);

	// --- 动态追加：暂停/恢复进程 / 修改优先级 / CPU亲和性 / 哈希 / 签名 ---
	DWORD pid = 0;
	{
		CString sPid = m_CListCtrl.GetItemText((int)FristIndex - 1, um_Process_Id);
		pid = (DWORD)_wtoi(sPid);
	}
	CString procPath = m_CListCtrl.GetItemText((int)FristIndex - 1, um_Process_FilePath);
	BOOL hasPid = (FristIndex > 0 && pid > 4);  // 不允许操作 System(4)/Idle
	BOOL hasPath = (!procPath.IsEmpty() && procPath != L"--");

	const UINT kProcSuspend  = 9200;
	const UINT kProcResume   = 9201;
	const UINT kProcHash     = 9202;
	const UINT kProcVerify   = 9203;
	const UINT kPrioBase     = 9210;  // 6 项
	const UINT kPrioIdle     = kPrioBase + 0;
	const UINT kPrioBelow    = kPrioBase + 1;
	const UINT kPrioNormal   = kPrioBase + 2;
	const UINT kPrioAbove    = kPrioBase + 3;
	const UINT kPrioHigh     = kPrioBase + 4;
	const UINT kPrioRealTime = kPrioBase + 5;
	const UINT kAffinityBase = 9220;  // 9220=全部, 9221=仅0号, 9222=仅1号 ... 最多 8 个核

	CMenu prioSub;
	prioSub.CreatePopupMenu();
	prioSub.AppendMenuW(MF_STRING | (hasPid ? 0 : MF_GRAYED), kPrioIdle,     L"Idle");
	prioSub.AppendMenuW(MF_STRING | (hasPid ? 0 : MF_GRAYED), kPrioBelow,    L"BelowNormal");
	prioSub.AppendMenuW(MF_STRING | (hasPid ? 0 : MF_GRAYED), kPrioNormal,   L"Normal");
	prioSub.AppendMenuW(MF_STRING | (hasPid ? 0 : MF_GRAYED), kPrioAbove,    L"AboveNormal");
	prioSub.AppendMenuW(MF_STRING | (hasPid ? 0 : MF_GRAYED), kPrioHigh,     L"High");
	prioSub.AppendMenuW(MF_STRING | (hasPid ? 0 : MF_GRAYED), kPrioRealTime, L"RealTime");

	SYSTEM_INFO si = { 0 };
	GetSystemInfo(&si);
	DWORD cpuCount = si.dwNumberOfProcessors;
	if (cpuCount > 8) cpuCount = 8; // 仅生成 8 项以保持菜单简洁

	CMenu affSub;
	affSub.CreatePopupMenu();
	affSub.AppendMenuW(MF_STRING | (hasPid ? 0 : MF_GRAYED), kAffinityBase, L"所有处理器");
	for (DWORD c = 0; c < cpuCount; ++c)
	{
		CString label; label.Format(L"仅 CPU %u", c);
		affSub.AppendMenuW(MF_STRING | (hasPid ? 0 : MF_GRAYED), kAffinityBase + 1 + c, label);
	}

	pPopup->AppendMenuW(MF_SEPARATOR, 0, (LPCTSTR)NULL);
	pPopup->AppendMenuW(MF_STRING | (hasPid ? 0 : MF_GRAYED), kProcSuspend, L"暂停进程");
	pPopup->AppendMenuW(MF_STRING | (hasPid ? 0 : MF_GRAYED), kProcResume,  L"恢复进程");
	pPopup->AppendMenuW(MF_POPUP  | (hasPid ? 0 : MF_GRAYED), (UINT_PTR)prioSub.GetSafeHmenu(), L"修改优先级");
	pPopup->AppendMenuW(MF_POPUP  | (hasPid ? 0 : MF_GRAYED), (UINT_PTR)affSub.GetSafeHmenu(),  L"设置 CPU 亲和性");
	prioSub.Detach();
	affSub.Detach();
	pPopup->AppendMenuW(MF_SEPARATOR, 0, (LPCTSTR)NULL);
	pPopup->AppendMenuW(MF_STRING | (hasPath ? 0 : MF_GRAYED), kProcHash,   L"计算 MD5 / SHA1 / SHA256");
	pPopup->AppendMenuW(MF_STRING | (hasPath ? 0 : MF_GRAYED), kProcVerify, L"检查数字签名");

	UINT cmd = pPopup->TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, this);
	if (HandleOpenInExplorerCmd(cmd, explorerCmd, explorerPath))
	{
		return;
	}

	auto OpenProc = [&](DWORD access) -> HANDLE {
		return OpenProcess(access, FALSE, pid);
	};
	if (cmd == kProcSuspend && hasPid)
	{
		typedef NTSTATUS (NTAPI *PFN_NtSuspendProcess)(HANDLE);
		static PFN_NtSuspendProcess pfn = (PFN_NtSuspendProcess)
			GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtSuspendProcess");
		HANDLE h = OpenProc(PROCESS_SUSPEND_RESUME);
		if (pfn && h) { pfn(h); CloseHandle(h); }
		else ::MessageBoxW(GetSafeHwnd(), L"无法打开进程或获取 NtSuspendProcess。", L"提示", MB_OK | MB_ICONWARNING);
		return;
	}
	if (cmd == kProcResume && hasPid)
	{
		typedef NTSTATUS (NTAPI *PFN_NtResumeProcess)(HANDLE);
		static PFN_NtResumeProcess pfn = (PFN_NtResumeProcess)
			GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtResumeProcess");
		HANDLE h = OpenProc(PROCESS_SUSPEND_RESUME);
		if (pfn && h) { pfn(h); CloseHandle(h); }
		else ::MessageBoxW(GetSafeHwnd(), L"无法打开进程或获取 NtResumeProcess。", L"提示", MB_OK | MB_ICONWARNING);
		return;
	}
	if (cmd >= kPrioBase && cmd <= kPrioRealTime && hasPid)
	{
		static const DWORD prioMap[] = {
			IDLE_PRIORITY_CLASS,
			BELOW_NORMAL_PRIORITY_CLASS,
			NORMAL_PRIORITY_CLASS,
			ABOVE_NORMAL_PRIORITY_CLASS,
			HIGH_PRIORITY_CLASS,
			REALTIME_PRIORITY_CLASS
		};
		HANDLE h = OpenProc(PROCESS_SET_INFORMATION);
		if (h)
		{
			BOOL ok = SetPriorityClass(h, prioMap[cmd - kPrioBase]);
			CloseHandle(h);
			if (!ok) ::MessageBoxW(GetSafeHwnd(), L"设置优先级失败。", L"提示", MB_OK | MB_ICONWARNING);
		}
		else ::MessageBoxW(GetSafeHwnd(), L"无法以 PROCESS_SET_INFORMATION 打开进程。", L"提示", MB_OK | MB_ICONWARNING);
		return;
	}
	if (cmd >= kAffinityBase && cmd <= kAffinityBase + 8 && hasPid)
	{
		DWORD_PTR mask;
		if (cmd == kAffinityBase)
		{
			DWORD_PTR procMask = 0, sysMask = 0;
			HANDLE hh = OpenProc(PROCESS_QUERY_INFORMATION);
			if (hh) { GetProcessAffinityMask(hh, &procMask, &sysMask); CloseHandle(hh); }
			mask = sysMask ? sysMask : (DWORD_PTR)-1;
		}
		else
		{
			DWORD cpu = cmd - kAffinityBase - 1;
			mask = (DWORD_PTR)1 << cpu;
		}
		HANDLE h = OpenProc(PROCESS_SET_INFORMATION);
		if (h)
		{
			BOOL ok = SetProcessAffinityMask(h, mask);
			CloseHandle(h);
			if (!ok) ::MessageBoxW(GetSafeHwnd(), L"设置亲和性失败。", L"提示", MB_OK | MB_ICONWARNING);
		}
		else ::MessageBoxW(GetSafeHwnd(), L"无法以 PROCESS_SET_INFORMATION 打开进程。", L"提示", MB_OK | MB_ICONWARNING);
		return;
	}
	if (cmd == kProcHash && hasPath)   { ShowFileHashesDialog(GetSafeHwnd(), procPath); return; }
	if (cmd == kProcVerify && hasPath) { VerifyFileSignatureDialog(GetSafeHwnd(), procPath); return; }

	if (cmd != 0)
	{
		PostMessage(WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
	}
}

void DlgProcess::InsertCtrlListControl(PCProcessInfo Processinfo)
{
	CString StrFilePath;
	WCHAR tempFullFileNameDrvice[MAX_PATH] = { 0 };
	int i = 0;
	if (Processinfo != NULL)
	{
		PCLIST_ENTRY pList = &Processinfo->HandleInfo.List.List;

		do
		{
			PCProcessInfo pProcessInfo = (PCProcessInfo)pList;
			{
				////////////////////////////////////////////////////////
				//获取当前系统用户名
				PWCHAR szwHostName = nullptr;
				DWORD  nHoustLength = 0;

				GetComputerNameW(NULL, &nHoustLength);
				if (nHoustLength != 0)
				{
					szwHostName = new WCHAR[nHoustLength + 2];
					memset(szwHostName, 0, sizeof(WCHAR) * (nHoustLength + 2));
					GetComputerNameW(szwHostName, &nHoustLength);
					wcscat(szwHostName, L"$");
				}

				///////////////////////////////////////////////////////////////////////////////
				//插入进程名字
				WCHAR WImageBaseName[MAX_PATH] = { 0 };
				PWCHAR CurrentPathName = NULL;
				PWCHAR CurrentPos = NULL;

				if (pProcessInfo->ImageBaseName[0] != 0 && pProcessInfo->ImageBaseName[1] != 0)
				{
					memset(WImageBaseName, 0, MAX_PATH);
					AsciitoUniCodeString(pProcessInfo->ImageBaseName, WImageBaseName);

					if (pProcessInfo->FullFileName[0] != 0 && pProcessInfo->FullFileName[1] != 0)
					{
						CurrentPathName = pProcessInfo->FullFileName;
						CurrentPos = CurrentPathName;
						for (ULONG64 j = 0; j < pProcessInfo->FullFileNameLength; j++)
						{
							if (_wcsnicmp(L"\\", &CurrentPathName[j], wcslen(L"\\")) == 0)
							{
								CurrentPos = &CurrentPathName[j];
							}
							if (_wcsnicmp(L".exe", &CurrentPathName[j], wcslen(L".exe")) == 0)
							{
								memset(WImageBaseName, 0, MAX_PATH);
								memcpy_s(WImageBaseName, MAX_PATH, CurrentPos + 1, pProcessInfo->FullFileNameLength);
								goto insertdata;
							}
						}
					}
					else if (pProcessInfo->FullFileNameDrvice[0] != 0 && pProcessInfo->FullFileNameDrvice[1] != 0)
					{
						CurrentPathName = pProcessInfo->FullFileNameDrvice;
						CurrentPos = CurrentPathName;
						for (ULONG64 j = 0; j < pProcessInfo->FullFileNameDrviceLength; j++)
						{
							if (_wcsnicmp(L"\\", &CurrentPathName[j], wcslen(L"\\")) == 0)
							{
								CurrentPos = &CurrentPathName[j];
							}
							if (_wcsnicmp(TEXT(".exe"), &CurrentPathName[j], wcslen(TEXT(".exe"))) == 0)
							{
								memset(WImageBaseName, 0, MAX_PATH);
								memcpy_s(WImageBaseName, MAX_PATH, CurrentPos + 1, pProcessInfo->FullFileNameDrviceLength);
								goto insertdata;
							}
						}
					}
					else
					{
						goto insertdata;
					}
				}
				///////////////////////////////////////////////////////////////////////////////
			insertdata:
				m_CListCtrl.InsertItem(i, WImageBaseName);
				///////////////////////////////////////////////////////////////////////////////
				//插入进程ID
				WCHAR str_ID[256] = { 0 };
				wsprintf(str_ID, L"%d", pProcessInfo->ProcessId);
				m_CListCtrl.SetItemText(i, um_Process_Id, str_ID);
				///////////////////////////////////////////////////////////////////////////////

				///////////////////////////////////////////////////////////////////////////////
				 //插入父进程ID
				WCHAR str_PID[256] = { 0 };
				wsprintf(str_PID, L"%d", pProcessInfo->ParentPId);
				m_CListCtrl.SetItemText(i, um_Process_ParentId, str_PID);
				///////////////////////////////////////////////////////////////////////////////

				///////////////////////////////////////////////////////////////////////////////
				 //插入会话ID
				WCHAR str_Session[256] = { 0 };
				wsprintf(str_Session, L"%d", pProcessInfo->Session);
				m_CListCtrl.SetItemText(i, um_Process_SessionId, str_Session);
				///////////////////////////////////////////////////////////////////////////////

				///////////////////////////////////////////////////////////////////////////////
				 //插入进程用户名
				if (wmemcmp(szwHostName, pProcessInfo->UserName, nHoustLength + 2) == 0)
				{
					m_CListCtrl.SetItemText(i, um_Process_UserName, TEXT("SYSTEM"));
				}
				else
				{
					m_CListCtrl.SetItemText(i, um_Process_UserName, pProcessInfo->UserName);
				}
				///////////////////////////////////////////////////////////////////////////////

				///////////////////////////////////////////////////////////////////////////////
				//插入进程所在路径
				if (pProcessInfo->FullFileName[0] != 0 && pProcessInfo->FullFileName[1] != 0)
				{
					m_CListCtrl.SetItemText(i, um_Process_FilePath, pProcessInfo->FullFileName);
					StrFilePath = pProcessInfo->FullFileName;
				}
				else if (pProcessInfo->FullFileNameDrvice[0] != 0 &&
					pProcessInfo->FullFileNameDrvice[1] != 0)
				{
					DeviceDosPathToNtPath(pProcessInfo->FullFileNameDrvice, tempFullFileNameDrvice);
					m_CListCtrl.SetItemText(i, um_Process_FilePath, tempFullFileNameDrvice);

					StrFilePath = tempFullFileNameDrvice;
				}
				else
				{
					m_CListCtrl.SetItemText(i, um_Process_FilePath, _T("--"));
				}
				///////////////////////////////////////////////////////////////////////////////

				///////////////////////////////////////////////////////////////////////////////
				//插入进程EPROCESS
				WCHAR str_Eprocess[256] = { 0 };
				wsprintf(str_Eprocess, L"%I64X", (ULONG64)pProcessInfo->Eprocess);
				m_CListCtrl.SetItemText(i, um_Process_Object, str_Eprocess);
				///////////////////////////////////////////////////////////////////////////////

				///////////////////////////////////////////////////////////////////////////////
				//判断是否是受保护的进程
				m_CListCtrl.SetItemText(i, um_Process_VisitState, (ULONG64)pProcessInfo->IsUserVisit ? TEXT("拒绝") : TEXT("--"));
				///////////////////////////////////////////////////////////////////////////////

				///////////////////////////////////////////////////////////////////////////////
				//插入进程文件厂商
				CString szDstFileName;
				m_CListCtrl.SetItemText(i, um_Process_FileFirm, this->GetCompanyName(StrFilePath, szDstFileName) ? (LPWSTR)szDstFileName.GetString() : TEXT("--"));
				///////////////////////////////////////////////////////////////////////////////

				///////////////////////////////////////////////////////////////////////////////
				//判断是否是调试状态
				m_CListCtrl.SetItemText(i, um_Process_DebugState, pProcessInfo->DebugPort ? TEXT("调试中") : TEXT("--"));
				///////////////////////////////////////////////////////////////////////////////

				//插入架构
				m_CListCtrl.SetItemText(i, um_Process_Architecture, pProcessInfo->Is64Process ? TEXT("x64") : TEXT("x32"));

				///////////////////////////////////////////////////////////////////////////////
				//插入进程创建时间
				WCHAR str_CreateTime[256] = { 0 };
				wsprintf(str_CreateTime, L"%d/%d/%d--%d:%d:%d:%d",
					(USHORT)pProcessInfo->CreateTime.Year,
					(USHORT)pProcessInfo->CreateTime.Month,
					(USHORT)pProcessInfo->CreateTime.Day,
					(USHORT)pProcessInfo->CreateTime.Hour,
					(USHORT)pProcessInfo->CreateTime.Minute,
					(USHORT)pProcessInfo->CreateTime.Second,
					(USHORT)pProcessInfo->CreateTime.Milliseconds);
				m_CListCtrl.SetItemText(i, um_Process_RunTime, str_CreateTime);
				///////////////////////////////////////////////////////////////////////////////

				///////////////////////////////////////////////////////////////////////////////
				//插入进程命令行参数
				m_CListCtrl.SetItemText(i, um_Process_Param, pProcessInfo->CommandLine);
				///////////////////////////////////////////////////////////////////////////////
			}

			//获取下一个节点
			pList = pList->Blink;
			i++;
			//释放当前空间
			SIZE_T FreeSize = 0;
			if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pProcessInfo, &FreeSize, MEM_RELEASE) != 0)
			{
				AfxMessageBox(L"释放空间失败!");
			}

		} while (pList != NULL && pList != &Processinfo->HandleInfo.List.List);
	}
}

void DlgProcess::OnDriverRefresh()
{
	// TODO: 在此添加命令处理程序代码
	m_CListCtrl.DeleteAllItems();

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumProcessInfo, this });
}

void DlgProcess::OnProcessVad()
{
	POSITION pos = m_CListCtrl.GetFirstSelectedItemPosition() - 1;//获取选中行的行数  pos = 行数 - 1

	DlgProcessVad Dlg(m_CListCtrl.GetItemText((int)pos, um_Process_Object),
		m_CListCtrl.GetItemText((int)pos, um_Process_Name),
		m_CListCtrl.GetItemText((int)pos, um_Process_Architecture));//设置EPROCESSS和进程名还有架构

	Dlg.DoModal();
}

void DlgProcess::OnProcessHandle()
{
	POSITION pos = m_CListCtrl.GetFirstSelectedItemPosition() - 1;//获取选中行的行数  pos = 行数 - 1
	DlgProcessHandle Dlg(m_CListCtrl.GetItemText((int)pos, um_Process_Object), m_CListCtrl.GetItemText((int)pos, um_Process_Name));//设置EPROCESSS和进程名
	Dlg.DoModal();
}

void DlgProcess::OnProcessThread()
{
	POSITION pos = m_CListCtrl.GetFirstSelectedItemPosition() - 1;//获取选中行的行数  pos = 行数 - 1
	DlgProcessThread Dlg(m_CListCtrl.GetItemText((int)pos, um_Process_Object), m_CListCtrl.GetItemText((int)pos, um_Process_Name));//设置EPROCESSS和进程名
	Dlg.DoModal();

}

void DlgProcess::OnProcessModule()
{
	POSITION pos = m_CListCtrl.GetFirstSelectedItemPosition() - 1;//获取选中行的行数  pos = 行数 - 1
	DlgProcessModule Dlg(m_CListCtrl.GetItemText((int)pos, um_Process_Object), m_CListCtrl.GetItemText((int)pos, um_Process_Name));//设置EPROCESSS和进程名
	Dlg.DoModal();
}

void DlgProcess::OnProcessMenuCopyName()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_Name);
}

void DlgProcess::OnProcessMenuCopyId()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_Id);
}

void DlgProcess::OnProcessMenuCopyParentid()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_ParentId);
}

void DlgProcess::OnProcessMenuCopySessionid()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_SessionId);
}

void DlgProcess::OnProcessMenuCopyUsername()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_UserName);
}

void DlgProcess::OnProcessMenuCopyFilepath()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_FilePath);
}

void DlgProcess::OnProcessMenuCopyEprocess()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_Object);
}

void DlgProcess::OnProcessMenuCopyVisitstate()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_VisitState);
}

void DlgProcess::OnProcessMenuCopyFilefirm()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_FileFirm);
}

void DlgProcess::OnProcessMenuCopyDebugstate()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_DebugState);
}

void DlgProcess::OnProcessMenuCopyRuntime()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_RunTime);
}

void DlgProcess::OnProcessMenuCopyParam()
{
	CopyBufferToClipboard(&m_CListCtrl, um_Process_Param);
}

void DlgProcess::OnProcessMenuCopyOpenfile()
{
	POSITION pos = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int index = (int)pos - 1;
	do
	{
		CString w_str = m_CListCtrl.GetItemText(index, 5);
		WCHAR FilePath[MAX_PATH] = { 0 };
		memcpy_s(FilePath, MAX_PATH, _T("/select,"), (strlen("/select,") + 1) * 2);
		wcscat_s(FilePath, MAX_PATH, w_str.GetBuffer());
		if ((int)ShellExecute(NULL, L"open", L"explorer.exe", FilePath, NULL, SW_SHOWNORMAL) < 32)
		{
			MessageBoxW(_T("文件不存在或已被删除!"), MB_OK);
		}
		w_str.ReleaseBuffer();//释放内存

		index = m_CListCtrl.GetNextSelectedItem(pos) + 1;
	} while (pos);
}

void DlgProcess::OnProcessMenuCopyAttribute()
{
	// TODO: 在此添加命令处理程序代码
	//WCHAR filePath[MAX_PATH] = { 0 };

	POSITION pos = m_CListCtrl.GetFirstSelectedItemPosition() - 1;//获取选中行的行数  pos = 行数 - 1
	CString FilePath = m_CListCtrl.GetItemText((int)pos, 5);
	SHELLEXECUTEINFOW info = { 0 };
	info.cbSize = sizeof info;
	info.lpFile = FilePath.GetBuffer();
	info.nShow = SW_SHOW;
	info.fMask = SEE_MASK_INVOKEIDLIST;
	info.lpVerb = TEXT("properties");

	CString ShowData;
	switch (ShellExecuteExW(&info))
	{
	case SE_ERR_FNF:
		MessageBox(TEXT("找不到文件"), NULL, MB_OK | MB_ICONERROR);
		break;
	case SE_ERR_PNF:
		MessageBox(TEXT("找不到路径"), NULL, MB_OK | MB_ICONERROR);
		break;
	}
}

void DlgProcess::OnProcessKillprocess()
{
	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserKillProcess, this });

}

void DlgProcess::ProcessKillprocess()
{
	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	CString Eprocess = m_CListCtrl.GetItemText(TempIndex, um_Process_Object);

	//::MessageBoxW(NULL, Eprocess, L"提示", MB_OK);
	ULONG64 dqRet = g_LoadDriver.SendMsg(um_Cmd_KillProcess_info, (PVOID)_wcstoui64(Eprocess.GetBuffer(), 0, 16), NULL);
	if (dqRet == 0)
	{
		::MessageBoxW(NULL, L"强制结束进程成功!", L"提示", MB_OK);
	}
	else
	{
		::MessageBoxW(NULL, L"强制结束进程失败!", L"提示", MB_OK);
	}
}

void DlgProcess::OnProcessClearProtection()
{
	// TODO: 在此添加命令处理程序代码

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	CString Eprocess = m_CListCtrl.GetItemText(TempIndex, um_Process_Object);

	ULONG64 dqRet = g_LoadDriver.SendMsg(um_Cmd_Set_ProcessPortection, (PVOID)_wcstoui64(Eprocess.GetBuffer(), 0, 16), NULL, NULL, 0);
	if (dqRet)
	{

	}
}

void DlgProcess::OnProcessPpl()
{
	// TODO: 在此添加命令处理程序代码

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	CString Eprocess = m_CListCtrl.GetItemText(TempIndex, um_Process_Object);

	ULONG64 dqRet = g_LoadDriver.SendMsg(um_Cmd_Get_ProcessPortection, (PVOID)_wcstoui64(Eprocess.GetBuffer(), 0, 16), NULL, NULL, 0);

	dqRet = g_LoadDriver.SendMsg(um_Cmd_Set_ProcessPortection, (PVOID)_wcstoui64(Eprocess.GetBuffer(), 0, 16), NULL, NULL, (PVOID)(dqRet | PsProtectedTypeProtectedLight));
	if (dqRet)
	{

	}
}

void DlgProcess::OnProcessPp()
{

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	CString Eprocess = m_CListCtrl.GetItemText(TempIndex, um_Process_Object);

	ULONG64 dqRet = g_LoadDriver.SendMsg(um_Cmd_Get_ProcessPortection, (PVOID)_wcstoui64(Eprocess.GetBuffer(), 0, 16), NULL, NULL, 0);

	dqRet = g_LoadDriver.SendMsg(um_Cmd_Set_ProcessPortection, (PVOID)_wcstoui64(Eprocess.GetBuffer(), 0, 16), NULL, NULL, (PVOID)(dqRet | PsProtectedTypeProtected));
	if (dqRet)
	{

	}
}

void DlgProcess::OnProcessNp()
{

	POSITION FristIndex = m_CListCtrl.GetFirstSelectedItemPosition();//获取选中行的行数  pos = 行数 - 1
	int TempIndex = (int)FristIndex - 1;//存储第一次的索引位置

	CString Eprocess = m_CListCtrl.GetItemText(TempIndex, um_Process_Object);

	ULONG64 dqRet = g_LoadDriver.SendMsg(um_Cmd_Set_ProcessPortection, (PVOID)_wcstoui64(Eprocess.GetBuffer(), 0, 16), NULL, NULL, (PVOID)0);
	if (dqRet)
	{

	}
}
