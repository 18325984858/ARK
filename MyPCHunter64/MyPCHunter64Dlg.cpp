
// MyPCHunter64Dlg.cpp: 实现文件
//
#define _CRT_NON_CONFORMING_SWPRINTFS
#include "pch.h"
#include "framework.h"
#include "MyPCHunter64.h"
#include "MyPCHunter64Dlg.h"
#include "afxdialogex.h"
#include "CThreadPool.h"
#include "../MyDriver64/Struct.h"
#include "Thread.h"
#include "PdbResolver.h"
#include <thread>



#ifdef _DEBUG
#define new DEBUG_NEW
#endif

LoadDriver g_LoadDriver;
CThreadPool g_ThreadPool{ std::thread::hardware_concurrency() - 1 };
DlgProcessMonitor g_DlgProcessMonitor = { 0 };
UCHAR g_CreateFlagsDlgProcessMonitor = FALSE;
MyPdb g_NtPdb;
MyPdb g_fltmgrPDB;
MyPdb g_WdfPdb;
pfunNtFreeVirtualMemory MyNtFreeVirtualMemory = NULL;
// CMyPCHunter64Dlg 对话框



CMyPCHunter64Dlg::CMyPCHunter64Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_MAIN, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMyPCHunter64Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_CONTROL_MAIN_TAB, m_Control_Tab);
}

BEGIN_MESSAGE_MAP(CMyPCHunter64Dlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(NM_CLICK, ID_CONTROL_MAIN_TAB, &CMyPCHunter64Dlg::OnClickControlMainTab)
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_COMMAND(ID_MENU_MAIN_DLG_MONITORDLG, &CMyPCHunter64Dlg::OnMenuMainDlgMonitordlg)
	ON_WM_HOTKEY()
	ON_MESSAGE(WM_USER_PDB_PROGRESS, &CMyPCHunter64Dlg::OnPdbProgress)
END_MESSAGE_MAP()


CMyModuleCall g_ModuleCall[MAX_MODULE_NAME_NUMBER] = {
	{L"ntoskrnel.exe",&g_NtPdb},
	{L"fltmgr.sys",&g_fltmgrPDB},
	{L"Wdf01000.sys",&g_WdfPdb},
	0
};

// CMyPCHunter64Dlg 消息处理程序

BOOL CMyPCHunter64Dlg::OnInitDialog()
{
	LOGI("[OnInitDialog] BEGIN");
	CDialogEx::OnInitDialog();
	LOGI("[OnInitDialog] CDialogEx::OnInitDialog done");

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
	LOGI("[OnInitDialog] icons set");


	//初始化PDB库
	{
		LOGI("[OnInitDialog] g_NtPdb.InitPDB(ntoskrnl.exe) start");
		if (!g_NtPdb.InitPDB(L"C:\\Windows\\System32\\ntoskrnl.exe"))
		{
			LOGE("[OnInitDialog] g_NtPdb.InitPDB FAILED");
			AfxMessageBox(L"初始化ntoskrnl PDB失败!");
			exit(1);
		}
		LOGI("[OnInitDialog] g_NtPdb.InitPDB OK");


		LOGI("[OnInitDialog] g_fltmgrPDB.InitPDB(fltmgr.sys) start");
		if (!g_fltmgrPDB.InitPDB(L"C:\\Windows\\System32\\drivers\\fltmgr.sys"))
		{
			LOGE("[OnInitDialog] g_fltmgrPDB.InitPDB FAILED");
			AfxMessageBox(L"初始化fltmgr PDB失败!");
			exit(1);
		}
		LOGI("[OnInitDialog] g_fltmgrPDB.InitPDB OK");

		// Wdf01000 的 PDB 由 PdbResolver 统一异步下载（参见 OnInitDialog 下方 PdbResolver_Init 后的 Request）
	}

	{
		// TODO: 在此添加额外的初始化代码
		HMODULE hModule = GetModuleHandleW(L"ntdll.dll");
		MyNtFreeVirtualMemory = (pfunNtFreeVirtualMemory)GetProcAddress(hModule, "NtFreeVirtualMemory");
		if (MyNtFreeVirtualMemory == NULL)
		{
			LOGE("[OnInitDialog] NtFreeVirtualMemory not found in ntdll.dll");
			exit(1);
		}
		LOGI("[OnInitDialog] NtFreeVirtualMemory=%p", MyNtFreeVirtualMemory);

#define DRIVER_NAME (L"MyDriver64.sys")
#define DRIVER_PATH (L"MyDriver64.sys")
		//加载驱动
		g_LoadDriver.SetFileNameAndPath(DRIVER_NAME, DRIVER_PATH);

		LOGI("[OnInitDialog] LoadDriverFun() start");
		BOOL bRet = g_LoadDriver.LoadDriverFun();	//加载驱动
		LOGI("[OnInitDialog] LoadDriverFun() returned %d (GLE=%lu)", (int)bRet, GetLastError());
		if (!bRet)
		{
			AfxMessageBox(TEXT("Driver驱动加载失败!"));
			exit(1);
		}
		// 启动看门狗：保证任务管理器强杀后驱动也能被卸载
		extern void SpawnWatchdog();
		SpawnWatchdog();
		//建立通信
#define MINIFILTER_PORT_NAME L"\\58DF4FB5-D464-4DF9-B14E-3565FDE02AED"
		LOGI("[OnInitDialog] ConnectDriver() start");
		BOOL bConn = g_LoadDriver.ConnectDriver(MINIFILTER_PORT_NAME);
		LOGI("[OnInitDialog] ConnectDriver() returned %d (GLE=%lu)", (int)bConn, GetLastError());
		if (!bConn)
		{
			AfxMessageBox(L"通信创建失败!");
			g_LoadDriver.~_LoadDriver();
			exit(1);
		}
		//添加初始化任务到线程池


		//此Event用于等待此_LoadDriver::Um_UserCallBackType_InitData事件完成返回
#define INITDATA_EVENT (L"Global\\7028001A-27A3-4C83-B359-0CFC333A50BB")
		LOGI("[OnInitDialog] CreateEventW(INITDATA_EVENT) start");
		HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, INITDATA_EVENT);
		LOGI("[OnInitDialog] CreateEventW returned hEvent=%p (GLE=%lu)", hEvent, GetLastError());
		if (hEvent)
		{
			LOGI("OnInitDialog: posting InitData task.");
			g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_InitData, this });

			LOGI("[OnInitDialog] WaitForSingleObject(InitDataEvent, INFINITE) start");
			if (WaitForSingleObject(hEvent, INFINITE) != WAIT_OBJECT_0)
			{
				LOGE("OnInitDialog: InitData wait failed.");
				g_LoadDriver.~_LoadDriver();
				OnClose();
				exit(1);

			}
			LOGI("OnInitDialog: InitData done.");

			g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_Test, this });
		}
	}

	LOGI("[OnInitDialog] InitTableControl start");
	InitTableControl();
	LOGI("[OnInitDialog] InitTableControl done");

	// 创建状态栏（左下角显示 PDB 下载进度等全局状态）
	{
		static UINT statusBarIndicators[] = { ID_SEPARATOR };
		if (m_StatusBar.Create(this) &&
			m_StatusBar.SetIndicators(statusBarIndicators, _countof(statusBarIndicators)))
		{
			m_StatusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, 0);
			RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
			m_StatusBar.SetPaneText(0, L"Ready");
		}
	}
	LOGI("[OnInitDialog] status bar created");

	// 状态栏创建后强制重新布局，让 Tab 让出底部高度
	{
		CRect rcClient;
		GetClientRect(&rcClient);
		SendMessage(WM_SIZE, SIZE_RESTORED, MAKELPARAM(rcClient.Width(), rcClient.Height()));
	}

	// PDB 按需解析服务启动（后台线程）
	LOGI("[OnInitDialog] PdbResolver_Init() start");
	PdbResolver_Init(GetSafeHwnd());
	// 预先请求 Wdf01000.sys 的 PDB，UI 不阻塞，状态栏会显示进度
	PdbResolver_Request(L"Wdf01000.sys", L"C:\\Windows\\System32\\drivers\\Wdf01000.sys");
	LOGI("[OnInitDialog] PdbResolver init done");

	RegHostKey();
	LOGI("[OnInitDialog] END (RegHostKey done)");

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}



// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CMyPCHunter64Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CMyPCHunter64Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

VOID CMyPCHunter64Dlg::InitTableControl()
{
	CString TableTitleStr[] = { L"进程",L"驱动模块",L"内核",L"内核钩子", L"文件" ,L"注册表",L"设置" };

	m_Control_Tab.InsertItem(DlgType_Process, TableTitleStr[DlgType_Process]);
	m_DlgProcess.Create(ID_DLG_PROCESS, &m_Control_Tab);
	m_DlgProcess.OnDriverRefresh();

	m_Control_Tab.InsertItem(DlgType_DriverModule, TableTitleStr[DlgType_DriverModule]);
	m_DlgDriverModule.Create(ID_DLG_DRIVERMODE, &m_Control_Tab);

	m_Control_Tab.InsertItem(DlgType_Kernel, TableTitleStr[DlgType_Kernel]);
	m_DlgKernel.Create(ID_DLG_KERNEL, &m_Control_Tab);

	m_Control_Tab.InsertItem(DlgType_KernelHook, TableTitleStr[DlgType_KernelHook]);
	m_DlgKernelHook.Create(ID_DLG_KERNELHOOK, &m_Control_Tab);

	m_Control_Tab.InsertItem(DlgType_EnumFile, TableTitleStr[DlgType_EnumFile]);
	m_DlgEnumFile.Create(ID_DLG_ENUMFILE, &m_Control_Tab);

	m_Control_Tab.InsertItem(DlgType_EnumRegistry, TableTitleStr[DlgType_EnumRegistry]);
	m_DlgEnumRegistry.Create(ID_DLG_ENUMREGISTRY, &m_Control_Tab);


	m_Control_Tab.InsertItem(DlgType_Setting, TableTitleStr[DlgType_Setting]);
	m_DlgSetting.Create(ID_DLG_SETTING, &m_Control_Tab);

	RECT TabRect;
	m_Control_Tab.GetClientRect(&TabRect);
	TabRect.top += 30;
	TabRect.left += 5;
	TabRect.right -= 7;

	m_DlgProcess.MoveWindow(&TabRect);
	m_DlgDriverModule.MoveWindow(&TabRect);
	m_DlgKernel.MoveWindow(&TabRect);
	m_DlgKernelHook.MoveWindow(&TabRect);
	m_DlgEnumFile.MoveWindow(&TabRect);
	m_DlgEnumRegistry.MoveWindow(&TabRect);
	m_DlgSetting.MoveWindow(&TabRect);

	m_DlgProcess.ShowWindow(TRUE);
}

void CMyPCHunter64Dlg::OnClickControlMainTab(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	m_DlgProcess.ShowWindow(FALSE);
	m_DlgDriverModule.ShowWindow(FALSE);
	m_DlgKernel.ShowWindow(FALSE);
	m_DlgKernelHook.ShowWindow(FALSE);
	m_DlgEnumFile.ShowWindow(FALSE);
	m_DlgEnumRegistry.ShowWindow(FALSE);
	m_DlgSetting.ShowWindow(FALSE);

	switch (m_Control_Tab.GetCurSel())
	{
	case DlgType_Process:
		m_DlgProcess.OnDriverRefresh();
		m_DlgProcess.ShowWindow(TRUE);
		break;
	case DlgType_DriverModule:
		m_DlgDriverModule.OnDriverRefresh();
		m_DlgDriverModule.ShowWindow(TRUE);
		break;
	case DlgType_Kernel:
		m_DlgKernel.ShowWindow(TRUE);
		break;
	case DlgType_KernelHook:
		m_DlgKernelHook.ShowWindow(TRUE);
		break;
	case DlgType_EnumFile:
		m_DlgEnumFile.ShowWindow(TRUE);
		break;
	case DlgType_EnumRegistry:
		m_DlgEnumRegistry.ShowWindow(TRUE);
		break;
	case DlgType_Setting:
		m_DlgSetting.ShowWindow(TRUE);
		break;
	default:
		AfxMessageBox(L"控件出现错误!");
		break;
	}

	// 切换后强制刷新 Tab 容器，避免旧子对话框的像素残留
	m_Control_Tab.Invalidate();
	m_Control_Tab.UpdateWindow();
}


void CMyPCHunter64Dlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	// 让状态栏自己重新落位（取出它的高度，预留给 Tab）
	int statusBarH = 0;
	if (::IsWindow(m_StatusBar.GetSafeHwnd()))
	{
		RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);
		CRect rcStatus;
		m_StatusBar.GetWindowRect(&rcStatus);
		statusBarH = rcStatus.Height();
	}

	//跟随主窗口移动
	RECT r_Tab = { 0 };
	r_Tab.bottom = cy - 4 - statusBarH;
	r_Tab.right = cx - 4;
	m_Control_Tab.MoveWindow(&r_Tab, TRUE); //设置Tab控件跟随主窗口缩放

	//调整移动大小
	r_Tab.top += 30;
	r_Tab.left += 5;
	r_Tab.right -= 7;

	//设置窗口跟随主窗口
	m_DlgProcess.MoveWindow(&r_Tab, TRUE);
	m_DlgDriverModule.MoveWindow(&r_Tab, TRUE);
	m_DlgKernel.MoveWindow(&r_Tab, TRUE);
	m_DlgKernelHook.MoveWindow(&r_Tab, TRUE);
	m_DlgEnumFile.MoveWindow(&r_Tab, TRUE);
	m_DlgEnumRegistry.MoveWindow(&r_Tab, TRUE);
	m_DlgSetting.MoveWindow(&r_Tab, TRUE);

}

BOOL CMyPCHunter64Dlg::DestroyWindow()
{
	//摧毁窗口
	m_DlgDriverModule.DestroyWindow();
	m_DlgProcess.DestroyWindow();
	m_DlgEnumFile.DestroyWindow();
	m_DlgEnumRegistry.DestroyWindow();
	m_DlgSetting.DestroyWindow();


	if (g_CreateFlagsDlgProcessMonitor)
	{
		g_DlgProcessMonitor.DestroyWindow();
		g_CreateFlagsDlgProcessMonitor = FALSE;
	}


	return CDialogEx::DestroyWindow();
}


VOID CMyPCHunter64Dlg::RegHostKey()
{
	//注册热键
	if (!RegisterHotKey(this->m_hWnd, ID_MENU_MAIN_DLG_MONITORDLG, HOTKEYF_CONTROL, 'M'))
	{
		ErrorMessage(GetLastError(), L"注册Ctrl+M热键失败:");
		return;
	}
}

VOID CMyPCHunter64Dlg::UnHostKey()
{
	if (!UnregisterHotKey(this->m_hWnd, ID_MENU_MAIN_DLG_MONITORDLG))
	{
		ErrorMessage(GetLastError(), L"释放Ctrl+M热键失败:");
		return;
	}
}

void CMyPCHunter64Dlg::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialogEx::OnOK();
}

DWORD ExitThread(PVOID pContext)
{
	WCHAR* pExitEventStr = L"Global\\MyExitEvent";
	HANDLE hEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, pExitEventStr);
	if (NULL != hEvent)
	{

#define EXIT_WAIT_TIME  (1000*60)
		switch (WaitForSingleObject(hEvent, EXIT_WAIT_TIME))
		{
		case WAIT_OBJECT_0:
			CloseHandle(hEvent);
		case WAIT_TIMEOUT:
			return FALSE;
			break;
		default:
			break;
		}
	}
	return GetLastError();
}

// PDB 下载/解析进度（来自后台线程 PostMessage）
LRESULT CMyPCHunter64Dlg::OnPdbProgress(WPARAM wParam, LPARAM /*lParam*/)
{
	std::unique_ptr<PdbProgressMsg> m((PdbProgressMsg*)wParam);
	if (!m) return 0;
	if (!::IsWindow(m_StatusBar.GetSafeHwnd())) return 0;

	CString text;
	switch (m->phase)
	{
	case 0: // Start
		if (m->total > 0)
			text.Format(L"Downloading %s ... 0%% / %.2f MB", m->fileName, m->total / 1048576.0);
		else
			text.Format(L"Downloading %s ...", m->fileName);
		break;
	case 1: // Receive
		if (m->total > 0)
		{
			text.Format(L"Downloading %s ... %llu%% (%.2f / %.2f MB)",
				m->fileName,
				m->received * 100ull / m->total,
				m->received / 1048576.0, m->total / 1048576.0);
		}
		else
		{
			text.Format(L"Downloading %s ... %.2f MB", m->fileName, m->received / 1048576.0);
		}
		break;
	case 2: // Finish HTTP
		text.Format(L"%s downloaded, indexing...", m->fileName);
		break;
	case 3: // Error
		if (m->httpCode != 0)
			text.Format(L"%s failed (HTTP %u)", m->fileName, m->httpCode);
		else
			text.Format(L"%s load failed", m->fileName);
		break;
	case 4: // Loaded - PDB ready
		text.Format(L"%s ready", m->fileName);
		break;
	case 5: // Queued
		text.Format(L"%s queued...", m->fileName);
		break;
	default:
		return 0;
	}
	m_StatusBar.SetPaneText(0, text);
	return 0;
}

void CMyPCHunter64Dlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	//卸载热键
	UnHostKey();

	PdbResolver_Shutdown();

	HANDLE hExitThread = CreateThread(NULL, 0, ExitThread, NULL, 0, NULL);

	// TODO: 在此添加专用代码和/或调用基类
	g_LoadDriver.UnLoadDriverFun();//卸载驱动

	if (hExitThread != NULL)
	{
		WaitForSingleObject(hExitThread, INFINITE);
		DWORD ExitCode = 0;
		if (GetExitCodeThread(hExitThread, &ExitCode))
		{
			if (ExitCode)
			{
				ErrorMessage(ExitCode, L"打开退出通知事件失败! 错误码:");
			}
		}
		CloseHandle(hExitThread);
	}
	CDialogEx::OnClose();
}

void CMyPCHunter64Dlg::OnMenuMainDlgMonitordlg()
{
	// TODO: 在此添加命令处理程序代码
	if (!g_CreateFlagsDlgProcessMonitor)
	{
		g_DlgProcessMonitor.Create(ID_DIALOG_SSDT_MONITOR);
		g_CreateFlagsDlgProcessMonitor = TRUE;
	}
	else
	{
		g_DlgProcessMonitor.ShowWindow(1);
	}
}

void CMyPCHunter64Dlg::OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	if (ID_MENU_MAIN_DLG_MONITORDLG == nHotKeyId)
	{
		if (HOTKEYF_CONTROL == nKey1)
		{
			switch (nKey2)
			{
			case 'M':
				OnMenuMainDlgMonitordlg();
				break;
			}
		}
	}

	CDialogEx::OnHotKey(nHotKeyId, nKey1, nKey2);
}
