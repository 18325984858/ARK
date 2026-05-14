
// MyPCHunter64.cpp: 定义应用程序的类行为。
//

#include "pch.h"
#include "framework.h"
#include "MyPCHunter64.h"
#include "MyPCHunter64Dlg.h"
#include "CLoadDriver.h"
#include <DbgHelp.h>
#include <signal.h>
#include <eh.h>
#include <stdarg.h>
#include <mutex>
#pragma comment(lib, "Dbghelp.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

extern _LoadDriver g_LoadDriver;

//==================================================================
// 简易日志系统：输出到 exe 同目录下 MyPCHunter64.log
// 实现完全基于 Win32 API（CreateFileW / WriteFile），不依赖 CRT，
// 这样即便 CRT 还未完成初始化（全局对象构造期间）日志也能落盘。
//==================================================================
static HANDLE      g_LogH = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_LogCs;
static volatile LONG g_LogCsReady = 0;
static volatile LONG g_ShutdownDone = 0;

static void Log_EnsureCs()
{
	if (InterlockedCompareExchange(&g_LogCsReady, 1, 0) == 0)
	{
		InitializeCriticalSection(&g_LogCs);
	}
}

static void Log_OpenIfNeeded()
{
	if (g_LogH != INVALID_HANDLE_VALUE) return;
	WCHAR exePath[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, exePath, MAX_PATH);
	WCHAR* slash = wcsrchr(exePath, L'\\');
	if (slash) *(slash + 1) = 0;
	WCHAR logPath[MAX_PATH] = { 0 };
	lstrcpynW(logPath, exePath, MAX_PATH);
	lstrcatW(logPath, L"MyPCHunter64.log");

	HANDLE h = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
	if (h == INVALID_HANDLE_VALUE)
	{
		// 退到 %TEMP%
		WCHAR temp[MAX_PATH] = { 0 };
		GetTempPathW(MAX_PATH, temp);
		lstrcatW(temp, L"MyPCHunter64.log");
		h = CreateFileW(temp, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
	}
	if (h == INVALID_HANDLE_VALUE)
	{
		OutputDebugStringW(L"[MyPCHunter64] log file open failed\n");
		return;
	}
	g_LogH = h;

	// 首次写入加 UTF-8 BOM
	LARGE_INTEGER sz = { 0 };
	GetFileSizeEx(h, &sz);
	if (sz.QuadPart == 0)
	{
		DWORD wr;
		static const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
		WriteFile(h, bom, sizeof(bom), &wr, NULL);
	}
	const char open_line[] = "\r\n---- log opened ----\r\n";
	DWORD wr2;
	WriteFile(h, open_line, sizeof(open_line) - 1, &wr2, NULL);
}

void AppLog_Init()
{
	Log_EnsureCs();
	EnterCriticalSection(&g_LogCs);
	Log_OpenIfNeeded();
	LeaveCriticalSection(&g_LogCs);
}

void AppLog_Write(const char* level, const char* fmt, ...)
{
	Log_EnsureCs();
	EnterCriticalSection(&g_LogCs);
	Log_OpenIfNeeded();
	if (g_LogH == INVALID_HANDLE_VALUE) { LeaveCriticalSection(&g_LogCs); return; }

	char buf[2048];
	SYSTEMTIME st; GetLocalTime(&st);
	int n = _snprintf_s(buf, sizeof(buf), _TRUNCATE,
		"[%04d-%02d-%02d %02d:%02d:%02d.%03d][%lu][%s] ",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		GetCurrentThreadId(), level ? level : "?");
	if (n < 0) n = 0;

	va_list ap; va_start(ap, fmt);
	int m = _vsnprintf_s(buf + n, sizeof(buf) - n, _TRUNCATE, fmt, ap);
	va_end(ap);
	if (m < 0) m = 0;
	int total = n + m;
	if (total > (int)sizeof(buf) - 3) total = (int)sizeof(buf) - 3;
	buf[total++] = '\r';
	buf[total++] = '\n';

	DWORD wr;
	WriteFile(g_LogH, buf, total, &wr, NULL);
	LeaveCriticalSection(&g_LogCs);
}

// 在全局对象构造阶段就把日志打开 + 留下首行印迹，便于诊断"InitInstance 之前"的崩溃。
struct _BootLogger
{
	_BootLogger()
	{
		AppLog_Init();
		AppLog_Write("INFO ", "==== global ctor ==== pid=%lu", GetCurrentProcessId());
	}
};
static _BootLogger s_bootLogger;

#define Log_Init()        AppLog_Init()
#define Log_Write(lv,...) AppLog_Write(lv, __VA_ARGS__)
#define LOGI(...) AppLog_Write("INFO ", __VA_ARGS__)
#define LOGW(...) AppLog_Write("WARN ", __VA_ARGS__)
#define LOGE(...) AppLog_Write("ERROR", __VA_ARGS__)

//==================================================================
// 退出兜底：无论何种退出路径都尝试卸载驱动一次
//==================================================================
static void TryUnloadDriverSEH()
{
	__try
	{
		g_LoadDriver.UnLoadDriverFun();
		LOGI("driver unloaded.");
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		LOGE("UnLoadDriverFun raised SEH 0x%08X", GetExceptionCode());
	}
}

static void EmergencyShutdown(const char* reason)
{
	if (InterlockedCompareExchange(&g_ShutdownDone, 1, 0) != 0) return;
	LOGW("EmergencyShutdown via [%s]: unloading driver...", reason ? reason : "?");
	TryUnloadDriverSEH();
}

static LONG WINAPI MyUnhandledExceptionFilter(EXCEPTION_POINTERS* ep)
{
	DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
	PVOID addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : NULL;
	LOGE("UnhandledException code=0x%08X addr=%p", code, addr);
	EmergencyShutdown("UnhandledException");
	return EXCEPTION_EXECUTE_HANDLER;
}

static void __cdecl MySignalHandler(int sig)
{
	LOGE("signal %d received", sig);
	EmergencyShutdown("signal");
	_exit(3);
}

static void MyTerminateHandler()
{
	LOGE("std::terminate called");
	EmergencyShutdown("terminate");
	_exit(4);
}

static void MyPureCallHandler()
{
	LOGE("pure virtual call");
	EmergencyShutdown("purecall");
	_exit(5);
}

static void __cdecl MyInvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t)
{
	LOGE("CRT invalid parameter");
	EmergencyShutdown("invalid_param");
	_exit(6);
}

static void AtExitHook()
{
	LOGI("atexit fired.");
	EmergencyShutdown("atexit");
}

static void InstallCrashHandlers()
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);

	signal(SIGABRT, MySignalHandler);
	signal(SIGSEGV, MySignalHandler);
	signal(SIGILL,  MySignalHandler);
	signal(SIGFPE,  MySignalHandler);
	signal(SIGTERM, MySignalHandler);
	signal(SIGINT,  MySignalHandler);

	std::set_terminate(MyTerminateHandler);
	_set_purecall_handler(MyPureCallHandler);
	_set_invalid_parameter_handler(MyInvalidParameterHandler);

	atexit(AtExitHook);

	// 防止 CRT 弹"abort"对话框
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

	LOGI("crash handlers installed.");
}


// CMyPCHunter64App

BEGIN_MESSAGE_MAP(CMyPCHunter64App, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CMyPCHunter64App 构造

CMyPCHunter64App::CMyPCHunter64App()
{
	// 支持重新启动管理器
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的 CMyPCHunter64App 对象

CMyPCHunter64App theApp;

//互斥体名称
#define MUTEX_NAME (TEXT("00EF0FEE-42F7-4C27-A35C-40E1A7F81F4B"))
HANDLE g_hMutex;
// CMyPCHunter64App 初始化

BOOL CMyPCHunter64App::InitInstance()
{
	// === 最先安装异常 hook + 日志，确保后续任何异常都会卸载驱动 ===
	Log_Init();
	LOGI("==== MyPCHunter64 starting (pid=%lu) ====", GetCurrentProcessId());
	InstallCrashHandlers();

	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。  否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	AfxEnableControlContainer();

	// 创建 shell 管理器，以防对话框包含
	// 任何 shell 树视图控件或 shell 列表视图控件。
	CShellManager* pShellManager = new CShellManager;

	// 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

	//创建互斥体防止多开
	g_hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
	if (g_hMutex == NULL)
	{
		LOGE("CreateMutex failed err=%lu", GetLastError());
		return 0;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		LOGW("another instance already exists, exit.");
		AfxMessageBox(L"请勿多次运行");

		exit(1);
	}
	LOGI("single-instance mutex OK; entering main dialog.");

	WaitForSingleObject(g_hMutex, INFINITE);


	CMyPCHunter64Dlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: 在此放置处理何时用
		//  “确定”来关闭对话框的代码
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: 在此放置处理何时用
		//  “取消”来关闭对话框的代码
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
		TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
	}

	// 删除上面创建的 shell 管理器。
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

	CloseHandle(g_hMutex);//关闭互斥体对象

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// 正常退出路径：主动卸载驱动并写日志
	LOGI("InitInstance returning - normal shutdown.");
	EmergencyShutdown("normal_exit");

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}

