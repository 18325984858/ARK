// DlgEnumRegistry.cpp: 实现文件
//
#define _CRT_NON_CONFORMING_SWPRINTFS
#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgEnumRegistry.h"
#include "DlgEditRegValue.h"
#include "Thread.h"
#include <vector>
#include <functional>
#include <shellapi.h>
#include <commdlg.h>
#include <sddl.h>
#include <aclui.h>
#include <aclapi.h>
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "aclui.lib")

// 顶部路径输入框的控件 ID（动态创建，不走 .rc）
static const UINT kIdPathEdit = 5050;
static const int  kPathBarH   = 22;

// "新建" 子菜单命令 ID（树/列表共用）
static const UINT kRegNewKey      = 9702;
static const UINT kRegNewSz       = 9710;
static const UINT kRegNewBinary   = 9711;
static const UINT kRegNewDword    = 9712;
static const UINT kRegNewQword    = 9713;
static const UINT kRegNewMultiSz  = 9714;
static const UINT kRegNewExpandSz = 9715;
// DlgEnumRegistry 对话框

IMPLEMENT_DYNAMIC(DlgEnumRegistry, CDialogEx)

DlgEnumRegistry::DlgEnumRegistry(CWnd* pParent /*=nullptr*/)
	: CDialogEx(ID_DLG_ENUMREGISTRY, pParent)
{
	m_ThreadFlags = FALSE;
}

DlgEnumRegistry::~DlgEnumRegistry()
{
}

void DlgEnumRegistry::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, ID_ENUMREGSITRY_TREE, m_CTreeCtrl);
	DDX_Control(pDX, ID_ENUMREGSITRY_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgEnumRegistry, CDialogEx)
	ON_WM_SIZE()
	ON_NOTIFY(NM_DBLCLK, ID_ENUMREGSITRY_TREE, &DlgEnumRegistry::OnNMDblclkEnumregsitryTree)
	ON_NOTIFY(NM_DBLCLK, ID_ENUMREGSITRY_LIST, &DlgEnumRegistry::OnNMDblclkEnumregsitryList)
	ON_NOTIFY(NM_RCLICK, ID_ENUMREGSITRY_TREE, &DlgEnumRegistry::OnNMRClickEnumregsitryTree)
	ON_NOTIFY(NM_RCLICK, ID_ENUMREGSITRY_LIST, &DlgEnumRegistry::OnNMRClickEnumregsitryList)
	ON_NOTIFY(TVN_ENDLABELEDIT, ID_ENUMREGSITRY_TREE, &DlgEnumRegistry::OnTvnEndlabeleditEnumregsitryTree)
	ON_NOTIFY(LVN_ENDLABELEDIT, ID_ENUMREGSITRY_LIST, &DlgEnumRegistry::OnLvnEndlabeleditEnumregsitryList)
	ON_NOTIFY(TVN_SELCHANGED, ID_ENUMREGSITRY_TREE, &DlgEnumRegistry::OnTvnSelchangedEnumregsitryTree)
	ON_NOTIFY(NM_CLICK, ID_ENUMREGSITRY_TREE, &DlgEnumRegistry::OnNMClickEnumregsitryTree)
END_MESSAGE_MAP()

namespace
{
	static bool CopyTextToClipboard(HWND hWnd, const CString& text)
	{
		if (!::OpenClipboard(hWnd)) return false;
		::EmptyClipboard();
		SIZE_T bytes = (text.GetLength() + 1) * sizeof(WCHAR);
		HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (!hMem)
		{
			::CloseClipboard();
			return false;
		}
		void* p = ::GlobalLock(hMem);
		if (!p)
		{
			::GlobalFree(hMem);
			::CloseClipboard();
			return false;
		}
		memcpy(p, (LPCWSTR)text, bytes);
		::GlobalUnlock(hMem);
		::SetClipboardData(CF_UNICODETEXT, hMem);
		::CloseClipboard();
		return true;
	}

	static bool KernelPathToRootSub(const CString& kernelPathIn, HKEY& root, CString& sub)
	{
		CString p = kernelPathIn;
		while (!p.IsEmpty() && p[p.GetLength() - 1] == L'\\')
		{
			p.Delete(p.GetLength() - 1);
		}

		const CString kMachine = L"\\Registry\\Machine";
		const CString kUser = L"\\Registry\\User";

		if (p.GetLength() >= kMachine.GetLength() && p.Left(kMachine.GetLength()).CompareNoCase(kMachine) == 0)
		{
			root = HKEY_LOCAL_MACHINE;
			sub = p.Mid(kMachine.GetLength());
		}
		else if (p.GetLength() >= kUser.GetLength() && p.Left(kUser.GetLength()).CompareNoCase(kUser) == 0)
		{
			root = HKEY_USERS;
			sub = p.Mid(kUser.GetLength());
		}
		else
		{
			return false;
		}

		if (!sub.IsEmpty() && sub[0] == L'\\') sub = sub.Mid(1);
		return true;
	}

	static CString RootName(HKEY root)
	{
		if (root == HKEY_LOCAL_MACHINE) return L"HKEY_LOCAL_MACHINE";
		if (root == HKEY_USERS) return L"HKEY_USERS";
		if (root == HKEY_CLASSES_ROOT) return L"HKEY_CLASSES_ROOT";
		if (root == HKEY_CURRENT_USER) return L"HKEY_CURRENT_USER";
		return L"";
	}

	static CString RootShortName(HKEY root)
	{
		if (root == HKEY_LOCAL_MACHINE) return L"HKLM";
		if (root == HKEY_USERS) return L"HKU";
		if (root == HKEY_CLASSES_ROOT) return L"HKCR";
		if (root == HKEY_CURRENT_USER) return L"HKCU";
		return L"";
	}

	static CString ToDisplayPath(HKEY root, const CString& sub)
	{
		CString n = RootName(root);
		if (sub.IsEmpty()) return n;
		return n + L"\\" + sub;
	}

	static CString ToRegExePath(HKEY root, const CString& sub)
	{
		CString n = RootShortName(root);
		if (sub.IsEmpty()) return n;
		return n + L"\\" + sub;
	}

	static CString LeafName(const CString& sub)
	{
		int p = sub.ReverseFind(L'\\');
		return p < 0 ? sub : sub.Mid(p + 1);
	}

	static CString ParentSub(const CString& sub)
	{
		int p = sub.ReverseFind(L'\\');
		return p < 0 ? CString() : sub.Left(p);
	}

	static size_t GuessDataSize(ULONG64 type, const std::vector<BYTE>& data)
	{
		if (type == REG_DWORD || type == REG_DWORD_BIG_ENDIAN) return 4;
		if (type == REG_QWORD) return 8;
		if (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_LINK)
		{
			const WCHAR* s = (const WCHAR*)data.data();
			size_t n = wcslen(s) + 1;
			return n * sizeof(WCHAR);
		}
		if (type == REG_MULTI_SZ)
		{
			const WCHAR* p = (const WCHAR*)data.data();
			size_t cap = data.size() / sizeof(WCHAR);
			for (size_t i = 0; i + 1 < cap; ++i)
			{
				if (p[i] == 0 && p[i + 1] == 0)
				{
					return (i + 2) * sizeof(WCHAR);
				}
			}
			return data.size();
		}

		size_t n = data.size();
		while (n > 0 && data[n - 1] == 0) --n;
		return n;
	}

	// 注册表项的 ISecurityInformation 实现：给 EditSecurity 弹系统权限对话框用。
	class CRegSecurityInformation : public ISecurityInformation
	{
	public:
		LONG    m_ref = 1;
		HKEY    m_hKey = NULL;       // 必须用 READ_CONTROL | WRITE_DAC 打开
		CString m_objName;
		CString m_pageTitle;

		~CRegSecurityInformation() { if (m_hKey) RegCloseKey(m_hKey); }

		// IUnknown
		STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
		{
			if (!ppv) return E_POINTER;
			if (riid == IID_IUnknown || riid == IID_ISecurityInformation)
			{
				*ppv = static_cast<ISecurityInformation*>(this);
				AddRef(); return S_OK;
			}
			*ppv = NULL; return E_NOINTERFACE;
		}
		STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
		STDMETHODIMP_(ULONG) Release() override
		{
			LONG r = InterlockedDecrement(&m_ref);
			if (r == 0) delete this;
			return r;
		}

		// ISecurityInformation
		STDMETHODIMP GetObjectInformation(PSI_OBJECT_INFO pInfo) override
		{
			ZeroMemory(pInfo, sizeof(*pInfo));
			pInfo->dwFlags = SI_EDIT_PERMS | SI_ADVANCED | SI_NO_ACL_PROTECT;
			pInfo->hInstance = AfxGetInstanceHandle();
			pInfo->pszServerName = NULL;
			pInfo->pszObjectName = (LPWSTR)(LPCWSTR)m_objName;
			pInfo->pszPageTitle = (LPWSTR)(LPCWSTR)m_pageTitle;
			return S_OK;
		}
		STDMETHODIMP GetSecurity(SECURITY_INFORMATION si,
			PSECURITY_DESCRIPTOR* ppSD, BOOL fDefault) override
		{
			if (fDefault) return E_NOTIMPL;
			PSECURITY_DESCRIPTOR pSD = NULL;
			DWORD st = GetSecurityInfo(m_hKey, SE_REGISTRY_KEY, si,
				NULL, NULL, NULL, NULL, &pSD);
			if (st != ERROR_SUCCESS) return HRESULT_FROM_WIN32(st);
			*ppSD = pSD; // 调用方用 LocalFree 释放
			return S_OK;
		}
		STDMETHODIMP SetSecurity(SECURITY_INFORMATION si,
			PSECURITY_DESCRIPTOR pSD) override
		{
			PSID owner = NULL, group = NULL;
			PACL dacl = NULL, sacl = NULL;
			BOOL bDef = FALSE, bPresent = FALSE;
			if (si & OWNER_SECURITY_INFORMATION)
				GetSecurityDescriptorOwner(pSD, &owner, &bDef);
			if (si & GROUP_SECURITY_INFORMATION)
				GetSecurityDescriptorGroup(pSD, &group, &bDef);
			if (si & DACL_SECURITY_INFORMATION)
				GetSecurityDescriptorDacl(pSD, &bPresent, &dacl, &bDef);
			if (si & SACL_SECURITY_INFORMATION)
				GetSecurityDescriptorSacl(pSD, &bPresent, &sacl, &bDef);
			DWORD st = SetSecurityInfo(m_hKey, SE_REGISTRY_KEY, si,
				owner, group, dacl, sacl);
			return st == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(st);
		}
		STDMETHODIMP GetAccessRights(const GUID*, DWORD,
			PSI_ACCESS* ppAccess, ULONG* pcAccesses,
			ULONG* piDefaultAccess) override
		{
			static SI_ACCESS s_acc[] = {
				{ &GUID_NULL, KEY_ALL_ACCESS,        L"完全控制", SI_ACCESS_GENERAL | SI_ACCESS_SPECIFIC },
				{ &GUID_NULL, KEY_READ,              L"读取",     SI_ACCESS_GENERAL | SI_ACCESS_SPECIFIC },
				{ &GUID_NULL, KEY_QUERY_VALUE,       L"查询值",   SI_ACCESS_SPECIFIC },
				{ &GUID_NULL, KEY_SET_VALUE,         L"设置值",   SI_ACCESS_SPECIFIC },
				{ &GUID_NULL, KEY_CREATE_SUB_KEY,    L"创建子项", SI_ACCESS_SPECIFIC },
				{ &GUID_NULL, KEY_ENUMERATE_SUB_KEYS,L"枚举子项", SI_ACCESS_SPECIFIC },
				{ &GUID_NULL, KEY_NOTIFY,            L"通知",     SI_ACCESS_SPECIFIC },
				{ &GUID_NULL, DELETE,                L"删除",     SI_ACCESS_SPECIFIC },
				{ &GUID_NULL, WRITE_DAC,             L"写 DAC",   SI_ACCESS_SPECIFIC },
				{ &GUID_NULL, WRITE_OWNER,           L"写所有者", SI_ACCESS_SPECIFIC },
				{ &GUID_NULL, READ_CONTROL,          L"读控制",   SI_ACCESS_SPECIFIC },
			};
			*ppAccess = s_acc;
			*pcAccesses = _countof(s_acc);
			*piDefaultAccess = 0;
			return S_OK;
		}
		STDMETHODIMP MapGeneric(const GUID*, UCHAR*, ACCESS_MASK* pMask) override
		{
			static GENERIC_MAPPING gm = { KEY_READ, KEY_WRITE, KEY_EXECUTE, KEY_ALL_ACCESS };
			MapGenericMask(pMask, &gm);
			return S_OK;
		}
		STDMETHODIMP GetInheritTypes(PSI_INHERIT_TYPE*, ULONG*) override { return E_NOTIMPL; }
		STDMETHODIMP PropertySheetPageCallback(HWND, UINT, SI_PAGE_TYPE) override { return S_OK; }
	};

	// regedit 风格的 "查找" 对话框，无 .rc，运行时构建 DLGTEMPLATE。
	// "查找下一个" 按钮不会关闭对话框；命中由 parent 负责导航。

	// ---- 内部小工具：构建 DLGTEMPLATE 字节流（与 DlgEditRegValue.cpp 同款写法） ----
	inline void PushBytes(std::vector<BYTE>& v, const void* p, size_t n)
	{
		const BYTE* b = (const BYTE*)p;
		v.insert(v.end(), b, b + n);
	}
	inline void PushW (std::vector<BYTE>& v, WORD  w) { PushBytes(v, &w, 2); }
	inline void PushDW(std::vector<BYTE>& v, DWORD d) { PushBytes(v, &d, 4); }
	inline void PushWStr(std::vector<BYTE>& v, LPCWSTR s)
	{
		if (!s) { PushW(v, 0); return; }
		while (*s) { PushW(v, (WORD)*s); ++s; }
		PushW(v, 0);
	}
	inline void AlignDW(std::vector<BYTE>& v) { while (v.size() & 3) v.push_back(0); }
	const WORD kAtomButton = 0x0080;
	const WORD kAtomEdit   = 0x0081;
	const WORD kAtomStatic = 0x0082;
	inline void AddItem(std::vector<BYTE>& v,
		DWORD style, DWORD exStyle,
		short x, short y, short cx, short cy,
		WORD id, WORD atom, LPCWSTR text)
	{
		AlignDW(v);
		PushDW(v, style);
		PushDW(v, exStyle);
		PushW(v, x); PushW(v, y); PushW(v, cx); PushW(v, cy);
		PushW(v, id);
		PushW(v, 0xFFFF);
		PushW(v, atom);
		PushWStr(v, text);
		PushW(v, 0);
	}

	enum {
		IDC_FIND_EDIT = 1001,
		IDC_LK_KEYS   = 1002,
		IDC_LK_VALUES = 1003,
		IDC_LK_DATA   = 1004,
		IDC_LK_WHOLE  = 1005,
		IDC_GROUP     = 1006,
	};

	// 调试日志占位：发布版禁用文件写入，需要时把函数体恢复即可。
	static void RegDbg(LPCWSTR /*fmt*/, ...) {}

	// 构建 "查找" 对话框模板
	static void BuildRegFindTemplate(std::vector<BYTE>& buf)
	{
		RegDbg(L"BuildTpl: enter");
		buf.clear();
		const short cxAll = 250, cyAll = 110;
		DWORD style = DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | DS_CENTER
			| WS_POPUP | WS_CAPTION | WS_SYSMENU;
		PushDW(buf, style);
		PushDW(buf, 0);
		PushW(buf, 0);              // cdit, 回填
		PushW(buf, 0); PushW(buf, 0);
		PushW(buf, cxAll); PushW(buf, cyAll);
		PushW(buf, 0); PushW(buf, 0);
		PushWStr(buf, L"查找");
		PushW(buf, 9);
		PushWStr(buf, L"MS Shell Dlg");
		RegDbg(L"BuildTpl: header done, size=%llu", (unsigned long long)buf.size());

		WORD cdit = 0;
		AddItem(buf, WS_CHILD | WS_VISIBLE | SS_LEFT, 0,
			7, 10, 55, 9, (WORD)-1, kAtomStatic, L"查找目标(&N):"); ++cdit;
		AddItem(buf, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0,
			64, 8, 178, 12, IDC_FIND_EDIT, kAtomEdit, L""); ++cdit;
		AddItem(buf, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0,
			7, 26, 170, 60, IDC_GROUP, kAtomButton, L"查看"); ++cdit;
		AddItem(buf, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0,
			15, 38, 120, 10, IDC_LK_KEYS,   kAtomButton, L"项(&K)"); ++cdit;
		AddItem(buf, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0,
			15, 52, 120, 10, IDC_LK_VALUES, kAtomButton, L"值(&V)"); ++cdit;
		AddItem(buf, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0,
			15, 66, 120, 10, IDC_LK_DATA,   kAtomButton, L"数据(&D)"); ++cdit;
		AddItem(buf, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0,
			7, 92, 140, 10, IDC_LK_WHOLE, kAtomButton, L"全字匹配(&W)"); ++cdit;
		AddItem(buf, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0,
			183, 26, 60, 14, IDOK, kAtomButton, L"查找下一个(&F)"); ++cdit;
		AddItem(buf, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
			183, 44, 60, 14, IDCANCEL, kAtomButton, L"取消"); ++cdit;
		RegDbg(L"BuildTpl: items done, cdit=%u size=%llu", cdit, (unsigned long long)buf.size());

		memcpy(&buf[8], &cdit, sizeof(WORD));
		RegDbg(L"BuildTpl: exit");
	}

	// 用 DLGPROC 自己处理消息，避开 MFC CDialog 子类化路径
	INT_PTR CALLBACK RegFindDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
	{
		switch (msg)
		{
		case WM_INITDIALOG:
		{
			RegDbg(L"DlgProc: WM_INITDIALOG hDlg=%p lp=%p", hDlg, (void*)lp);
			SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)lp);
			DlgEnumRegistry* p = (DlgEnumRegistry*)lp;
			SetDlgItemTextW(hDlg, IDC_FIND_EDIT, p->m_FindText);
			CheckDlgButton(hDlg, IDC_LK_KEYS,   p->m_FindLookKeys   ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hDlg, IDC_LK_VALUES, p->m_FindLookValues ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hDlg, IDC_LK_DATA,   p->m_FindLookData   ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hDlg, IDC_LK_WHOLE,  p->m_FindWhole      ? BST_CHECKED : BST_UNCHECKED);
			SetFocus(GetDlgItem(hDlg, IDC_FIND_EDIT));
			RegDbg(L"DlgProc: WM_INITDIALOG done");
			return FALSE;
		}
		case WM_COMMAND:
		{
			DlgEnumRegistry* p = (DlgEnumRegistry*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
			WORD id = LOWORD(wp);
			RegDbg(L"DlgProc: WM_COMMAND id=%u p=%p", id, p);
			if (id == IDCANCEL)
			{
				EndDialog(hDlg, IDCANCEL);
				return TRUE;
			}
			if (id == IDOK && p)
			{
				WCHAR buf[1024] = { 0 };
				GetDlgItemTextW(hDlg, IDC_FIND_EDIT, buf, _countof(buf));
				p->m_FindText       = buf;
				p->m_FindLookKeys   = (IsDlgButtonChecked(hDlg, IDC_LK_KEYS)   == BST_CHECKED);
				p->m_FindLookValues = (IsDlgButtonChecked(hDlg, IDC_LK_VALUES) == BST_CHECKED);
				p->m_FindLookData   = (IsDlgButtonChecked(hDlg, IDC_LK_DATA)   == BST_CHECKED);
				p->m_FindWhole      = (IsDlgButtonChecked(hDlg, IDC_LK_WHOLE)  == BST_CHECKED);
				RegDbg(L"DlgProc: find='%s' k=%d v=%d d=%d whole=%d",
					(LPCWSTR)p->m_FindText, (int)p->m_FindLookKeys,
					(int)p->m_FindLookValues, (int)p->m_FindLookData,
					(int)p->m_FindWhole);

				if (p->m_FindText.IsEmpty())
				{
					MessageBoxW(hDlg, L"请输入要查找的内容。", L"查找", MB_OK | MB_ICONINFORMATION);
					SetFocus(GetDlgItem(hDlg, IDC_FIND_EDIT));
					return TRUE;
				}
				if (!p->m_FindLookKeys && !p->m_FindLookValues && !p->m_FindLookData)
				{
					MessageBoxW(hDlg, L"请至少勾选一项 \"查看\" 范围。", L"查找", MB_OK | MB_ICONINFORMATION);
					return TRUE;
				}
				ShowWindow(hDlg, SW_HIDE);
				RegDbg(L"DlgProc: calling RegFindNext...");
				bool hit = false;
				__try
				{
					hit = p->RegFindNext();
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					RegDbg(L"DlgProc: !!! SEH exception in RegFindNext code=0x%08X",
						GetExceptionCode());
				}
				RegDbg(L"DlgProc: RegFindNext returned hit=%d", (int)hit);
				if (!hit)
				{
					MessageBoxW(hDlg, L"已搜索完注册表的剩余部分。", L"查找", MB_OK | MB_ICONINFORMATION);
				}
				ShowWindow(hDlg, SW_SHOW);
				return TRUE;
			}
			return FALSE;
		}
		case WM_CLOSE:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		return FALSE;
	}
}

// DlgEnumRegistry 消息处理程序

void DlgEnumRegistry::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	CRect rect;
	GetClientRect(&rect);

	float fwidth = rect.Width() / 4;

	if (m_PathEdit.GetSafeHwnd())
	{
		m_PathEdit.SetWindowPos(NULL, 0, 0, rect.Width(), kPathBarH, SWP_NOZORDER);
	}
	const int top = m_PathEdit.GetSafeHwnd() ? kPathBarH : 0;
	m_CTreeCtrl.SetWindowPos(NULL, 0, top, fwidth, rect.Height() - top, SWP_NOZORDER);
	m_CListCtrl.SetWindowPos(NULL, fwidth, top, rect.Width() - fwidth, rect.Height() - top, SWP_NOZORDER);

	// TODO: 在此处添加消息处理程序代码
}

BOOL DlgEnumRegistry::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	{
		WCHAR exe[MAX_PATH] = { 0 };
		GetModuleFileNameW(NULL, exe, MAX_PATH);
		RegDbg(L"===== DlgEnumRegistry::OnInitDialog: exe='%s' pid=%lu =====",
			exe, GetCurrentProcessId());
	}
	m_CTreeCtrl.ModifyStyle(0, TVS_EDITLABELS);
	m_CListCtrl.ModifyStyle(0, LVS_EDITLABELS);

	InitControl();

	// 顶部动态插一个纯输入框。不改 .rc 是为了不体依赖资源编辑状态，
	// 位置/尺寸在 OnSize 里同意重新摆一次。回车动作靠 PreTranslateMessage 拦。
	CRect rc;
	GetClientRect(&rc);
	m_PathEdit.CreateEx(WS_EX_CLIENTEDGE,
		L"EDIT", L"",
		WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
		CRect(0, 0, rc.Width(), kPathBarH), this, kIdPathEdit);
	m_PathEdit.SetFont(GetFont());
	// Cue 提示（Vista+）——跳转用法提示
	//m_PathEdit.SendMessageW(EM_SETCUEBANNER, TRUE,
	//	(LPARAM)L"输入注册表路径后回车：例 HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip 或 HKLM\\SYSTEM\\...");

	// 默认显示 "Computer" 根路径，与 regedit 一致
	m_PathEdit.SetWindowTextW(L"Computer");

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

BOOL DlgEnumRegistry::PreTranslateMessage(MSG* pMsg)
{
	// 路径输入框的回车默认会变成 IDOK 并关掉对话框。这里拦下来转
	// 为 NavigateToPath，并返回 TRUE 吃掉这个 WM_KEYDOWN。
	if (pMsg->message == WM_KEYDOWN
		&& pMsg->wParam == VK_RETURN
		&& m_PathEdit.GetSafeHwnd()
		&& pMsg->hwnd == m_PathEdit.GetSafeHwnd())
	{
		CString p;
		m_PathEdit.GetWindowTextW(p);
		NavigateToPath(p);
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

static LPCWSTR ResolveRegRoot(const CString& seg)
{
	static const struct { LPCWSTR alias; LPCWSTR full; } kAlias[] = {
		{ L"HKLM",               L"HKEY_LOCAL_MACHINE" },
		{ L"HKU",                L"HKEY_USERS" },
		{ L"HKCR",               L"HKEY_CLASSES_ROOT" },
		{ L"HKCU",               L"HKEY_CURRENT_USER" },
		{ L"HKEY_LOCAL_MACHINE", L"HKEY_LOCAL_MACHINE" },
		{ L"HKEY_USERS",         L"HKEY_USERS" },
		{ L"HKEY_CLASSES_ROOT",  L"HKEY_CLASSES_ROOT" },
		{ L"HKEY_CURRENT_USER",  L"HKEY_CURRENT_USER" },
	};
	for (auto& a : kAlias)
	{
		if (seg.CompareNoCase(a.alias) == 0) return a.full;
	}
	return NULL;
}

void DlgEnumRegistry::NavigateToPath(const CString& userPath)
{
	CString p = userPath;
	p.TrimLeft(); p.TrimRight();
	if (p.IsEmpty()) return;
	p.Replace(L'/', L'\\');

	// regedit 拷过来的 "计算机\\HKEY_..." 前缀 / "Computer\\..." 都去掉
	auto stripPrefix = [&](LPCWSTR pfx)
	{
		int n = (int)wcslen(pfx);
		if (p.GetLength() > n && _wcsnicmp(p, pfx, n) == 0)
		{
			p = p.Mid(n);
			if (!p.IsEmpty() && p[0] == L'\\') p = p.Mid(1);
		}
	};
	stripPrefix(L"计算机\\");
	stripPrefix(L"计算机");
	stripPrefix(L"Computer\\");
	stripPrefix(L"Computer");
	if (p.IsEmpty()) return;

	// 拆成段
	std::vector<CString> seg;
	int from = 0;
	while (from < p.GetLength())
	{
		int s = p.Find(L'\\', from);
		if (s < 0) { CString one = p.Mid(from); if (!one.IsEmpty()) seg.push_back(one); break; }
		if (s > from) seg.push_back(p.Mid(from, s - from));
		from = s + 1;
	}
	if (seg.empty()) return;

	LPCWSTR rootName = ResolveRegRoot(seg[0]);
	if (!rootName)
	{
		AfxMessageBox(L"无法识别的根键：" + seg[0] +
			L"\n可接受：HKEY_LOCAL_MACHINE / HKEY_USERS / HKLM / HKU 等");
		return;
	}

	// 在根节点里找匹配的项（HKEY_xxx 现在挂在 "计算机" 占位节点下）
	HTREEITEM hRoot = NULL;
	HTREEITEM hTop = m_CTreeCtrl.GetRootItem();
	while (hTop && !hRoot)
	{
		HTREEITEM hScan = (m_CTreeCtrl.GetItemData(hTop) == 0)
			? m_CTreeCtrl.GetChildItem(hTop) : hTop;
		bool topSelf = (hScan == hTop);
		while (hScan)
		{
			if (m_CTreeCtrl.GetItemText(hScan).CompareNoCase(rootName) == 0)
			{
				hRoot = hScan; break;
			}
			if (topSelf) break;
			hScan = m_CTreeCtrl.GetNextSiblingItem(hScan);
		}
		hTop = m_CTreeCtrl.GetNextSiblingItem(hTop);
	}
	if (!hRoot)
	{
		AfxMessageBox(CString(L"未找到根节点：") + rootName);
		return;
	}

	// 选中根 + 加载子项并展开
	m_CTreeCtrl.Select(hRoot, TVGN_CARET);
	InsertCtrlListControl();  // 会填充 hRoot 的子项到树里
	m_CTreeCtrl.Expand(hRoot, TVE_EXPAND);

	HTREEITEM hCur = hRoot;
	for (size_t i = 1; i < seg.size(); ++i)
	{
		// 在当前节点的子节点里找名字匹配的
		HTREEITEM hChild = m_CTreeCtrl.GetChildItem(hCur);
		HTREEITEM hMatch = NULL;
		while (hChild)
		{
			if (m_CTreeCtrl.GetItemText(hChild).CompareNoCase(seg[i]) == 0)
			{
				hMatch = hChild; break;
			}
			hChild = m_CTreeCtrl.GetNextSiblingItem(hChild);
		}
		if (!hMatch)
		{
			CString msg;
			msg.Format(L"找不到子项：%s\n已定位到上一级。", (LPCWSTR)seg[i]);
			AfxMessageBox(msg);
			m_CTreeCtrl.Select(hCur, TVGN_CARET);
			m_CTreeCtrl.EnsureVisible(hCur);
			return;
		}
		hCur = hMatch;
		m_CTreeCtrl.Select(hCur, TVGN_CARET);
		InsertCtrlListControl();   // 填充 hCur 的子项 + 右边值列表
		m_CTreeCtrl.Expand(hCur, TVE_EXPAND);
		m_CTreeCtrl.EnsureVisible(hCur);
	}
	m_CTreeCtrl.Select(hCur, TVGN_CARET);
	m_CTreeCtrl.EnsureVisible(hCur);
}

ULONG64 DlgEnumRegistry::InitControl()
{
	//初始化Tree控件
	m_CTreeCtrl.SetExtendedStyle(m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS, m_CTreeCtrl.GetExtendedStyle() | TVS_FULLROWSELECT | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS);

	// 顶层 "Computer" 占位节点（无 ItemData），与 regedit 一致
	HTREEITEM hComputer = m_CTreeCtrl.InsertItem(L"Computer");

	// 当前用户 SID，给 HKEY_CURRENT_USER 用
	CString curUserSid;
	{
		HANDLE hTok = NULL;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hTok))
		{
			DWORD len = 0;
			GetTokenInformation(hTok, TokenUser, NULL, 0, &len);
			if (len)
			{
				std::vector<BYTE> buf(len);
				if (GetTokenInformation(hTok, TokenUser, buf.data(), len, &len))
				{
					LPWSTR pSid = NULL;
					TOKEN_USER* tu = (TOKEN_USER*)buf.data();
					if (ConvertSidToStringSidW(tu->User.Sid, &pSid))
					{
						curUserSid = pSid;
						LocalFree(pSid);
					}
				}
			}
			CloseHandle(hTok);
		}
	}

	CString hkcuKernel = L"\\Registry\\User\\";
	if (!curUserSid.IsEmpty()) hkcuKernel += curUserSid + L"\\";

	// 五大根的显示名与对应内核路径，顺序与 regedit 一致
	CString TreeTitle[] = {
		L"HKEY_CLASSES_ROOT",
		L"HKEY_CURRENT_USER",
		L"HKEY_LOCAL_MACHINE",
		L"HKEY_USERS",
		L"HKEY_CURRENT_CONFIG",
	};
	CString TreeValue[] = {
		L"\\Registry\\Machine\\SOFTWARE\\Classes\\",
		hkcuKernel,
		L"\\Registry\\Machine\\",
		L"\\Registry\\User\\",
		L"\\Registry\\Machine\\System\\CurrentControlSet\\Hardware Profiles\\Current\\",
	};

	for (int i = 0; i < sizeof(TreeTitle) / sizeof(CString); i++)
	{
		HTREEITEM  hTreeItrm = m_CTreeCtrl.InsertItem(TreeTitle[i], hComputer, TVI_LAST);
		CString* pData = new CString(TreeValue[i]);
		m_CTreeCtrl.SetItemData(hTreeItrm, (DWORD_PTR)pData);
	}

	m_CTreeCtrl.Expand(hComputer, TVE_EXPAND);

	m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	CString ListTitle[] = { L"名称",L"类型",L"数据" };
	for (int i = 0; i < sizeof(ListTitle) / sizeof(CString); i++)
	{
		m_CListCtrl.InsertColumn(i, ListTitle[i]);
		m_CListCtrl.SetColumnWidth(i, 200);
	}
	return TRUE;
}

void DlgEnumRegistry::DelTreeChild(HTREEITEM pNode)
{
	auto CurrentChild = m_CTreeCtrl.GetChildItem(pNode); /*获取子集*/

	while (CurrentChild != NULL)
	{
		/*获取绑定的数据,并释放空间*/
		CString* pStr = (CString*)m_CTreeCtrl.GetItemData(CurrentChild);

		if (pStr != NULL)
		{
			delete pStr; //释放开辟的空间
		}

		/*备份要删除的位置*/
		auto Del = CurrentChild;

		/*获取下一个同级的数据*/
		CurrentChild = m_CTreeCtrl.GetNextSiblingItem(CurrentChild);

		/*删除*/
		m_CTreeCtrl.DeleteItem(Del);
	}
}

void DlgEnumRegistry::InsertCtrlListControl()
{
	//获取选择的项
	auto hSelectItem = m_CTreeCtrl.GetSelectedItem();

	//获取绑定的数据
	CString* pStr = (CString*)m_CTreeCtrl.GetItemData(hSelectItem);

	CString strtmp = pStr->GetString();
	//判断是否是目录不是返回

	DelTreeChild(hSelectItem);/*删除当前选择节点的所有孩子*/
	m_CListCtrl.DeleteAllItems();//删除全部数据
	m_RowMeta.clear();

	//发送消息
	PCRegistryInfo pRegistryInfo = NULL;
	g_LoadDriver.SendMsg(um_Cmd_Enum_Registry_info, strtmp.GetBuffer(), (PVOID64*)&pRegistryInfo);

	if (pRegistryInfo == NULL)
	{
		return;
	}

	SIZE_T FreeSize = 0;
	PCLIST_ENTRY pCurList = &pRegistryInfo->List;
	do
	{
		if (pCurList == NULL)
		{
			return;
		}

		PCRegistryInfo pCurRegistryInfo = (PCRegistryInfo)pCurList;

		if (pCurRegistryInfo->nType == 0)
		{
			//0为 _KeyInfo结构
			//CRegistryInfo::Reserve1::_KeyInfo* pKey = (CRegistryInfo::Reserve1::_KeyInfo*)((ULONG64)pCurRegistryInfo + sizeof(CLIST_ENTRY) + (sizeof(ULONG64) * 2));
			//插入到Tree中

						//目录插入到Tree控件中
			CString* pNewFilePath = new CString();
			//拼接字符串  
			pNewFilePath->Format(L"%ws%ws\\", strtmp.GetBuffer(), pCurRegistryInfo->KeyName);

			TVINSERTSTRUCTW tvInsertTree = { 0 };

			tvInsertTree.hParent = hSelectItem;
			tvInsertTree.hInsertAfter = TVI_LAST;
			tvInsertTree.itemex.mask = TVIF_TEXT | TVIF_CHILDREN;
			tvInsertTree.itemex.pszText = pCurRegistryInfo->KeyName;
			tvInsertTree.itemex.cchTextMax = wcslen(pCurRegistryInfo->KeyName) + 1;
			tvInsertTree.itemex.cChildren = 1;

			auto hCurrentItem = m_CTreeCtrl.InsertItem(&tvInsertTree);
			/*设置与当前行绑定的数据*/
			m_CTreeCtrl.SetItemData(hCurrentItem, (DWORD_PTR)pNewFilePath);
		}
		else
		{
			//1为 _ValueInfo结构
			//CRegistryInfo::Reserve1::_ValueInfo* pValue = (CRegistryInfo::Reserve1::_ValueInfo*)((ULONG64)pCurRegistryInfo + sizeof(CLIST_ENTRY) + (sizeof(ULONG64) * 2));
			//插入到List控件中

			bool bDefault = (pCurRegistryInfo->ValueName[0] == 0 && pCurRegistryInfo->ValueName[1] == 0);
			if (bDefault)
			{
				m_CListCtrl.InsertItem(0, L"默认");
			}
			else
			{
				m_CListCtrl.InsertItem(0, pCurRegistryInfo->ValueName);
			}

			// 记录该行的原始数据，供双击编辑使用（List 在 0 头插，meta 同样在前面插）
			{
				RegRowMeta meta;
				meta.type = pCurRegistryInfo->ValueType;
				meta.name = bDefault ? CString(L"") : CString(pCurRegistryInfo->ValueName);
				const BYTE* pb = (const BYTE*)pCurRegistryInfo->ValueData;
				meta.data.assign(pb, pb + sizeof(pCurRegistryInfo->ValueData));
				m_RowMeta.insert(m_RowMeta.begin(), std::move(meta));
			}

			WCHAR szBuf[MY_MAX_PATH * 4] = { 0 };
			// 驱动里 ValueData 是 WCHAR[MY_MAX_PATH] 且整体已清零；驱动没回传 DataLength，
			// 这里用“尾部连续 0 字节”反推一个近似有效长度，仅用于二进制/默认分支显示。
			auto guessByteLen = [&]() -> size_t
			{
				const BYTE* pb = (const BYTE*)pCurRegistryInfo->ValueData;
				size_t cap = sizeof(pCurRegistryInfo->ValueData);
				size_t n = cap;
				while (n > 0 && pb[n - 1] == 0) --n;
				return n;
			};
			switch (pCurRegistryInfo->ValueType)
			{
			case REG_SZ:
				m_CListCtrl.SetItemText(0, 1, L"REG_SZ");
				m_CListCtrl.SetItemText(0, 2, pCurRegistryInfo->ValueData);
				break;
			case REG_EXPAND_SZ:
				m_CListCtrl.SetItemText(0, 1, L"REG_EXPAND_SZ");
				m_CListCtrl.SetItemText(0, 2, pCurRegistryInfo->ValueData);
				break;
			case REG_MULTI_SZ:
			{
				m_CListCtrl.SetItemText(0, 1, L"REG_MULTI_SZ");
				// 多字符串：以 \0 分隔，整段以 \0\0 结尾。拼成 "a; b; c"
				CString joined;
				const WCHAR* p = pCurRegistryInfo->ValueData;
				const WCHAR* end = pCurRegistryInfo->ValueData
					+ _countof(pCurRegistryInfo->ValueData);
				while (p < end && *p)
				{
					if (!joined.IsEmpty()) joined += L"; ";
					joined += p;
					p += wcslen(p) + 1;
				}
				m_CListCtrl.SetItemText(0, 2, joined);
				break;
			}
			case REG_DWORD:
			case REG_DWORD_BIG_ENDIAN:
			{
				m_CListCtrl.SetItemText(0, 1,
					pCurRegistryInfo->ValueType == REG_DWORD ? L"REG_DWORD" : L"REG_DWORD_BIG_ENDIAN");
				ULONG32 v = *(PULONG32)pCurRegistryInfo->ValueData;
				if (pCurRegistryInfo->ValueType == REG_DWORD_BIG_ENDIAN) v = _byteswap_ulong(v);
				swprintf(szBuf, L"0x%08X (%u)", v, v);
				m_CListCtrl.SetItemText(0, 2, szBuf);
				break;
			}
			case REG_QWORD:
			{
				m_CListCtrl.SetItemText(0, 1, L"REG_QWORD");
				ULONG64 v = *(PULONG64)pCurRegistryInfo->ValueData;
				swprintf(szBuf, L"0x%016I64X (%I64u)", v, v);
				m_CListCtrl.SetItemText(0, 2, szBuf);
				break;
			}
			case REG_NONE:
				m_CListCtrl.SetItemText(0, 1, L"REG_NONE");
				m_CListCtrl.SetItemText(0, 2, L"");
				break;
			case REG_LINK:
				m_CListCtrl.SetItemText(0, 1, L"REG_LINK");
				m_CListCtrl.SetItemText(0, 2, pCurRegistryInfo->ValueData);
				break;
			case REG_BINARY:
			default:
			{
				m_CListCtrl.SetItemText(0, 1,
					pCurRegistryInfo->ValueType == REG_BINARY ? L"REG_BINARY" : L"其它");
				const BYTE* pb = (const BYTE*)pCurRegistryInfo->ValueData;
				size_t nBytes = guessByteLen();
				// 最多显示 64 字节，超出 ... 省略
				size_t show = nBytes > 64 ? 64 : nBytes;
				size_t off = 0;
				for (size_t i = 0; i < show && off + 4 < _countof(szBuf); ++i)
				{
					off += swprintf(szBuf + off, L"%02X ", pb[i]);
				}
				if (show < nBytes && off + 4 < _countof(szBuf))
				{
					swprintf(szBuf + off, L"...");
				}
				m_CListCtrl.SetItemText(0, 2, szBuf);
				break;
			}
			}
		}
		pCurList = pCurList->Flink;
		if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pCurRegistryInfo, &FreeSize, MEM_RELEASE) != 0)
		{
			AfxMessageBox(L"释放空间失败!");
		}
	} while (pCurList != &pRegistryInfo->List);
}

void DlgEnumRegistry::OnNMDblclkEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
	RegDbg(L"TreeDblClick: enter");

	//获取选择的项
	auto hSelectItem = m_CTreeCtrl.GetSelectedItem();
	if (hSelectItem == NULL) //当没有绑定的数据时返回 
	{
		return;
	}

	//获取绑定的数据
	CString* pStr = (CString*)m_CTreeCtrl.GetItemData(hSelectItem);
	if (pStr == NULL)
	{
		return;
	}

	CString strtmp = pStr->GetString();
	//判断是否是目录不是返回
	if (strtmp[strtmp.GetLength() - 1] != TEXT('\\'))
	{
		return;
	}

	g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumRegistryInfo, this });

}

void DlgEnumRegistry::OnNMDblclkEnumregsitryList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	LPNMITEMACTIVATE p = (LPNMITEMACTIVATE)pNMHDR;
	int idx = p ? p->iItem : -1;
	if (idx < 0)
	{
		POSITION pos = m_CListCtrl.GetFirstSelectedItemPosition();
		if (pos) idx = m_CListCtrl.GetNextSelectedItem(pos);
	}
	if (idx < 0 || idx >= (int)m_RowMeta.size()) return;

	const RegRowMeta& m = m_RowMeta[idx];
	CDlgEditRegValue dlg(m.type, m.name, m.data.data(), m.data.size());
	dlg.ShowModal(this);
}

void DlgEnumRegistry::OnNMRClickEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	*pResult = 0;
	RegDbg(L"TreeRClick: enter");

	POINT ptScreen = { 0 };
	GetCursorPos(&ptScreen);
	POINT ptClient = ptScreen;
	m_CTreeCtrl.ScreenToClient(&ptClient);
	UINT flags = 0;
	HTREEITEM hHit = m_CTreeCtrl.HitTest(ptClient, &flags);
	if (hHit) m_CTreeCtrl.SelectItem(hHit);

	HTREEITEM hSel = m_CTreeCtrl.GetSelectedItem();
	CString kernelPath;
	if (hSel)
	{
		CString* pData = (CString*)m_CTreeCtrl.GetItemData(hSel);
		if (pData) kernelPath = *pData;
	}

	HKEY root = NULL;
	CString sub;
	bool okPath = !kernelPath.IsEmpty() && KernelPathToRootSub(kernelPath, root, sub);
	bool isRoot = (hSel != NULL) && (m_CTreeCtrl.GetParentItem(hSel) == NULL);
	RegDbg(L"TreeRClick: hSel=%p okPath=%d isRoot=%d kernel='%s'",
		hSel, (int)okPath, (int)isRoot, (LPCWSTR)kernelPath);

	const UINT kExpand = 9701;
	const UINT kNewKey = kRegNewKey;
	const UINT kFind = 9703;
	const UINT kDelete = 9704;
	const UINT kRename = 9705;
	const UINT kExport = 9706;
	const UINT kPerm = 9707;
	const UINT kCopyName = 9708;
	const UINT kNewSz = kRegNewSz;
	const UINT kNewBinary = kRegNewBinary;
	const UINT kNewDword = kRegNewDword;
	const UINT kNewQword = kRegNewQword;
	const UINT kNewMultiSz = kRegNewMultiSz;
	const UINT kNewExpandSz = kRegNewExpandSz;

	CMenu newSub;
	newSub.CreatePopupMenu();
	newSub.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kNewKey, L"项");
	newSub.AppendMenuW(MF_SEPARATOR, 0, (LPCTSTR)NULL);
	newSub.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kNewSz, L"字符串值");
	newSub.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kNewBinary, L"二进制值");
	newSub.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kNewDword, L"DWORD (32 位) 值");
	newSub.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kNewQword, L"QWORD (64 位) 值");
	newSub.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kNewMultiSz, L"多字符串值");
	newSub.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kNewExpandSz, L"可扩展字符串值");

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kExpand, L"展开");
	menu.AppendMenuW(MF_POPUP | (okPath ? 0 : MF_GRAYED), (UINT_PTR)newSub.GetSafeHmenu(), L"新建");
	menu.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kFind, L"查找...");
	menu.AppendMenuW(MF_SEPARATOR, 0, (LPCTSTR)NULL);
	menu.AppendMenuW(MF_STRING | (okPath && !isRoot ? 0 : MF_GRAYED), kDelete, L"删除");
	menu.AppendMenuW(MF_STRING | (okPath && !isRoot ? 0 : MF_GRAYED), kRename, L"重命名");
	menu.AppendMenuW(MF_SEPARATOR, 0, (LPCTSTR)NULL);
	menu.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kExport, L"导出");
	menu.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kPerm, L"权限...");
	menu.AppendMenuW(MF_SEPARATOR, 0, (LPCTSTR)NULL);
	menu.AppendMenuW(MF_STRING | (okPath ? 0 : MF_GRAYED), kCopyName, L"复制项名称");

	UINT cmd = menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, ptScreen.x, ptScreen.y, this);
	RegDbg(L"TreeRClick: TrackPopupMenu cmd=%u", cmd);
	if (cmd == 0 || !okPath) return;

	if (cmd == kExpand)
	{
		if (hSel)
		{
			m_CTreeCtrl.SelectItem(hSel);
			InsertCtrlListControl();
			m_CTreeCtrl.Expand(hSel, TVE_EXPAND);
		}
		return;
	}

	if (cmd == kFind)
	{
		RegDbg(L"kFind: enter, this=%p", this);
		ShowRegFindDialog();
		RegDbg(L"kFind: exit");
		return;
	}

	if (cmd == kCopyName)
	{
		CopyTextToClipboard(GetSafeHwnd(), ToDisplayPath(root, sub));
		return;
	}

	if (cmd == kNewKey)
	{
		HKEY hParent = NULL;
		if (RegOpenKeyExW(root, sub.IsEmpty() ? NULL : (LPCWSTR)sub, 0, KEY_CREATE_SUB_KEY | KEY_WRITE, &hParent) != ERROR_SUCCESS)
		{
			AfxMessageBox(L"打开父项失败，无法新建。", MB_ICONERROR);
			return;
		}

		CString base = L"新项";
		for (int i = 1; i <= 99; ++i)
		{
			CString name;
			if (i == 1) name = base;
			else name.Format(L"%s%d", (LPCWSTR)base, i);

			HKEY hNew = NULL;
			LONG st = RegCreateKeyExW(hParent, name, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &hNew, NULL);
			if (st == ERROR_SUCCESS)
			{
				RegCloseKey(hNew);
				RegCloseKey(hParent);
				InsertCtrlListControl();
				HTREEITEM hChild = m_CTreeCtrl.GetChildItem(hSel);
				while (hChild)
				{
					if (m_CTreeCtrl.GetItemText(hChild).Compare(name) == 0)
					{
						m_CTreeCtrl.SelectItem(hChild);
						m_CTreeCtrl.EnsureVisible(hChild);
						break;
					}
					hChild = m_CTreeCtrl.GetNextSiblingItem(hChild);
				}
				return;
			}
		}

		RegCloseKey(hParent);
		AfxMessageBox(L"新建项失败。", MB_ICONERROR);
		return;
	}

	if (cmd == kNewSz || cmd == kNewBinary || cmd == kNewDword
		|| cmd == kNewQword || cmd == kNewMultiSz || cmd == kNewExpandSz)
	{
		CreateNewValueAtSelectedKey(cmd);
		return;
	}

	if (cmd == kDelete && !isRoot)
	{
		CString prompt;
		prompt.Format(L"确认删除该注册表项及其子项？\n\n%s", (LPCWSTR)ToDisplayPath(root, sub));
		if (MessageBoxW(prompt, L"删除注册表项", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK) return;

		LONG st = RegDeleteTreeW(root, sub);
		if (st != ERROR_SUCCESS)
		{
			st = RegDeleteKeyW(root, sub);
		}
		if (st == ERROR_SUCCESS)
		{
			HTREEITEM hParent = m_CTreeCtrl.GetParentItem(hSel);
			DelTreeChild(hSel);
			CString* pData = (CString*)m_CTreeCtrl.GetItemData(hSel);
			if (pData) { delete pData; m_CTreeCtrl.SetItemData(hSel, 0); }
			m_CTreeCtrl.DeleteItem(hSel);
			if (hParent) m_CTreeCtrl.SelectItem(hParent);
			InsertCtrlListControl();
		}
		else
		{
			AfxMessageBox(L"删除失败。", MB_ICONERROR);
		}
		return;
	}

	if (cmd == kRename && !isRoot)
	{
		m_PendingTreeOldKernelPath = kernelPath;
		m_CTreeCtrl.SetFocus();
		m_CTreeCtrl.EditLabel(hSel);
		return;
	}

	if (cmd == kExport)
	{
		CString defaultName = LeafName(sub);
		if (defaultName.IsEmpty()) defaultName = RootShortName(root);
		defaultName += L".reg";

		CFileDialog dlg(FALSE, L"reg", defaultName,
			OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
			L"注册表文件 (*.reg)|*.reg||", this);
		if (dlg.DoModal() != IDOK) return;

		CString target = dlg.GetPathName();
		CString regPath = ToRegExePath(root, sub);
		CString args;
		args.Format(L"export \"%s\" \"%s\" /y", (LPCWSTR)regPath, (LPCWSTR)target);

		SHELLEXECUTEINFOW sei = { 0 };
		sei.cbSize = sizeof(sei);
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;
		sei.hwnd = GetSafeHwnd();
		sei.lpVerb = L"open";
		sei.lpFile = L"reg.exe";
		sei.lpParameters = args;
		sei.nShow = SW_HIDE;
		if (ShellExecuteExW(&sei) && sei.hProcess)
		{
			WaitForSingleObject(sei.hProcess, INFINITE);
			DWORD code = 1;
			GetExitCodeProcess(sei.hProcess, &code);
			CloseHandle(sei.hProcess);
			if (code != 0)
			{
				AfxMessageBox(L"导出失败。", MB_ICONERROR);
			}
		}
		else
		{
			AfxMessageBox(L"启动 reg.exe 失败。", MB_ICONERROR);
		}
		return;
	}

	if (cmd == kPerm)
	{
		HKEY hKey = NULL;
		if (RegOpenKeyExW(root, sub.IsEmpty() ? NULL : (LPCWSTR)sub, 0,
			READ_CONTROL | WRITE_DAC, &hKey) != ERROR_SUCCESS)
		{
			// 退化：只读权限也行，仅查看
			if (RegOpenKeyExW(root, sub.IsEmpty() ? NULL : (LPCWSTR)sub, 0,
				READ_CONTROL, &hKey) != ERROR_SUCCESS)
			{
				AfxMessageBox(L"无法打开注册表项以编辑权限。", MB_ICONERROR);
				return;
			}
		}
		CString objName = LeafName(sub);
		if (objName.IsEmpty()) objName = RootName(root);
		CRegSecurityInformation* psi = new CRegSecurityInformation();
		psi->m_hKey = hKey;
		psi->m_objName = objName;
		psi->m_pageTitle = objName + L" 的权限";
		EditSecurity(GetSafeHwnd(), psi);
		psi->Release();
		return;
	}
}

void DlgEnumRegistry::OnNMRClickEnumregsitryList(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	*pResult = 0;
	RegDbg(L"ListRClick: enter");

	POINT ptScreen = { 0 };
	GetCursorPos(&ptScreen);
	POINT ptClient = ptScreen;
	m_CListCtrl.ScreenToClient(&ptClient);

	LVHITTESTINFO hti = { 0 };
	hti.pt = ptClient;
	int hit = m_CListCtrl.HitTest(&hti);
	if (hit >= 0) m_CListCtrl.SetItemState(hit, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	int idx = hit;
	bool hasSel = idx >= 0 && idx < (int)m_RowMeta.size();

	// 空白区右键 —— 只显示 "新建" 子菜单，和 regedit 行为一致
	if (!hasSel)
	{
		HTREEITEM hTreeSel = m_CTreeCtrl.GetSelectedItem();
		bool keyOk = false;
		if (hTreeSel)
		{
			CString* pData = (CString*)m_CTreeCtrl.GetItemData(hTreeSel);
			if (pData)
			{
				HKEY root = NULL; CString sub;
				keyOk = KernelPathToRootSub(*pData, root, sub);
			}
		}

		CMenu newSub;
		newSub.CreatePopupMenu();
		newSub.AppendMenuW(MF_STRING | (keyOk ? 0 : MF_GRAYED), kRegNewKey, L"项");
		newSub.AppendMenuW(MF_SEPARATOR, 0, (LPCTSTR)NULL);
		newSub.AppendMenuW(MF_STRING | (keyOk ? 0 : MF_GRAYED), kRegNewSz, L"字符串值");
		newSub.AppendMenuW(MF_STRING | (keyOk ? 0 : MF_GRAYED), kRegNewBinary, L"二进制值");
		newSub.AppendMenuW(MF_STRING | (keyOk ? 0 : MF_GRAYED), kRegNewDword, L"DWORD (32 位) 值");
		newSub.AppendMenuW(MF_STRING | (keyOk ? 0 : MF_GRAYED), kRegNewQword, L"QWORD (64 位) 值");
		newSub.AppendMenuW(MF_STRING | (keyOk ? 0 : MF_GRAYED), kRegNewMultiSz, L"多字符串值");
		newSub.AppendMenuW(MF_STRING | (keyOk ? 0 : MF_GRAYED), kRegNewExpandSz, L"可扩展字符串值");

		CMenu blankMenu;
		blankMenu.CreatePopupMenu();
		blankMenu.AppendMenuW(MF_POPUP | (keyOk ? 0 : MF_GRAYED),
			(UINT_PTR)newSub.GetSafeHmenu(), L"新建");

		UINT cmd = blankMenu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
			ptScreen.x, ptScreen.y, this);
		if (cmd == 0 || !keyOk) return;

		if (cmd == kRegNewKey)
		{
			// 触发树侧的新建项分支（直接选中再调用）
			m_CTreeCtrl.SetFocus();
			// 走一遍和树菜单完全相同的路径
			HKEY root = NULL; CString sub;
			CString* pData = (CString*)m_CTreeCtrl.GetItemData(hTreeSel);
			if (pData && KernelPathToRootSub(*pData, root, sub))
			{
				HKEY hParent = NULL;
				if (RegOpenKeyExW(root, sub.IsEmpty() ? NULL : (LPCWSTR)sub, 0,
					KEY_CREATE_SUB_KEY | KEY_WRITE, &hParent) == ERROR_SUCCESS)
				{
					CString base = L"新项";
					for (int i = 1; i <= 99; ++i)
					{
						CString name;
						if (i == 1) name = base;
						else name.Format(L"%s%d", (LPCWSTR)base, i);
						HKEY hNew = NULL;
						LONG st = RegCreateKeyExW(hParent, name, 0, NULL, 0,
							KEY_READ | KEY_WRITE, NULL, &hNew, NULL);
						if (st == ERROR_SUCCESS)
						{
							RegCloseKey(hNew);
							RegCloseKey(hParent);
							InsertCtrlListControl();
							HTREEITEM hChild = m_CTreeCtrl.GetChildItem(hTreeSel);
							while (hChild)
							{
								if (m_CTreeCtrl.GetItemText(hChild).Compare(name) == 0)
								{
									m_CTreeCtrl.SelectItem(hChild);
									m_CTreeCtrl.EnsureVisible(hChild);
									break;
								}
								hChild = m_CTreeCtrl.GetNextSiblingItem(hChild);
							}
							return;
						}
					}
					RegCloseKey(hParent);
				}
				AfxMessageBox(L"新建项失败。", MB_ICONERROR);
			}
			return;
		}

		CreateNewValueAtSelectedKey(cmd);
		return;
	}

	const UINT kModify = 9801;
	const UINT kModifyBin = 9802;
	const UINT kDelete = 9803;
	const UINT kRename = 9804;

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kModify, L"修改...");
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kModifyBin, L"修改二进制数据...");
	menu.AppendMenuW(MF_SEPARATOR, 0, (LPCTSTR)NULL);
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kDelete, L"删除");
	menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kRename, L"重命名");

	UINT cmd = menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, ptScreen.x, ptScreen.y, this);
	if (cmd == 0 || !hasSel) return;

	const RegRowMeta& m = m_RowMeta[idx];
	if (cmd == kModify)
	{
		CDlgEditRegValue dlg(m.type, m.name, m.data.data(), m.data.size());
		dlg.ShowModal(this);
		return;
	}
	if (cmd == kModifyBin)
	{
		CDlgEditRegValue dlg(REG_BINARY, m.name, m.data.data(), m.data.size());
		dlg.ShowModal(this);
		return;
	}

	HTREEITEM hSel = m_CTreeCtrl.GetSelectedItem();
	if (!hSel) return;
	CString* pData = (CString*)m_CTreeCtrl.GetItemData(hSel);
	if (!pData) return;

	HKEY root = NULL;
	CString sub;
	if (!KernelPathToRootSub(*pData, root, sub)) return;

	HKEY hKey = NULL;
	if (RegOpenKeyExW(root, sub.IsEmpty() ? NULL : (LPCWSTR)sub, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
	{
		AfxMessageBox(L"打开注册表项失败。", MB_ICONERROR);
		return;
	}

	if (cmd == kDelete)
	{
		if (m.name.IsEmpty())
		{
			RegCloseKey(hKey);
			AfxMessageBox(L"默认值不支持删除。", MB_ICONWARNING);
			return;
		}
		LONG st = RegDeleteValueW(hKey, m.name);
		RegCloseKey(hKey);
		if (st == ERROR_SUCCESS)
		{
			InsertCtrlListControl();
		}
		else
		{
			AfxMessageBox(L"删除失败。", MB_ICONERROR);
		}
		return;
	}

	if (cmd == kRename)
	{
		if (m.name.IsEmpty())
		{
			RegCloseKey(hKey);
			AfxMessageBox(L"默认值不支持重命名。", MB_ICONWARNING);
			return;
		}
		RegCloseKey(hKey);
		m_PendingListRenameRow = idx;
		m_CListCtrl.SetFocus();
		m_CListCtrl.EditLabel(idx);
		return;
	}

	RegCloseKey(hKey);
}

void DlgEnumRegistry::OnTvnEndlabeleditEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTVDISPINFO* pInfo = (NMTVDISPINFO*)pNMHDR;
	*pResult = FALSE;
	CString oldKernel = m_PendingTreeOldKernelPath;
	m_PendingTreeOldKernelPath.Empty();
	if (!pInfo || !pInfo->item.hItem || !pInfo->item.pszText) return;

	CString newName = pInfo->item.pszText;
	newName.Trim();
	if (newName.IsEmpty()) return;

	if (oldKernel.IsEmpty()) return;

	HKEY root = NULL;
	CString oldSub;
	if (!KernelPathToRootSub(oldKernel, root, oldSub)) return;

	CString oldLeaf = LeafName(oldSub);
	CString parentSub = ParentSub(oldSub);
	if (oldLeaf.CompareNoCase(newName) == 0)
	{
		*pResult = TRUE;
		return;
	}

	HKEY hParent = NULL;
	LONG st = RegOpenKeyExW(root, parentSub.IsEmpty() ? NULL : (LPCWSTR)parentSub,
		0, KEY_WRITE | KEY_CREATE_SUB_KEY, &hParent);
	if (st != ERROR_SUCCESS) return;

	st = RegRenameKey(hParent, oldLeaf, newName);
	RegCloseKey(hParent);
	if (st != ERROR_SUCCESS)
	{
		AfxMessageBox(L"重命名失败。", MB_ICONERROR);
		return;
	}

	CString newSub = parentSub.IsEmpty() ? newName : (parentSub + L"\\" + newName);
	CString* pData = (CString*)m_CTreeCtrl.GetItemData(pInfo->item.hItem);
	if (pData)
	{
		CString kernel;
		if (root == HKEY_LOCAL_MACHINE) kernel = L"\\Registry\\Machine\\" + newSub + L"\\";
		else kernel = L"\\Registry\\User\\" + newSub + L"\\";
		*pData = kernel;
	}

	DelTreeChild(pInfo->item.hItem);
	m_CTreeCtrl.SetItemState(pInfo->item.hItem, TVIS_EXPANDEDONCE, TVIS_EXPANDEDONCE);
	InsertCtrlListControl();
	*pResult = TRUE;
}

void DlgEnumRegistry::OnLvnEndlabeleditEnumregsitryList(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVDISPINFO* pInfo = (NMLVDISPINFO*)pNMHDR;
	*pResult = FALSE;
	int pendingRow = m_PendingListRenameRow;
	m_PendingListRenameRow = -1;
	if (!pInfo || pInfo->item.iItem < 0 || pInfo->item.iItem >= (int)m_RowMeta.size()) return;
	if (pendingRow != pInfo->item.iItem) return;
	if (!pInfo->item.pszText) return;

	CString newName = pInfo->item.pszText;
	newName.Trim();
	if (newName.IsEmpty()) return;

	const RegRowMeta& m = m_RowMeta[pInfo->item.iItem];
	if (m.name.IsEmpty()) return;
	if (m.name.CompareNoCase(newName) == 0)
	{
		*pResult = TRUE;
		return;
	}

	HTREEITEM hSel = m_CTreeCtrl.GetSelectedItem();
	if (!hSel) return;
	CString* pData = (CString*)m_CTreeCtrl.GetItemData(hSel);
	if (!pData) return;

	HKEY root = NULL;
	CString sub;
	if (!KernelPathToRootSub(*pData, root, sub)) return;

	HKEY hKey = NULL;
	if (RegOpenKeyExW(root, sub.IsEmpty() ? NULL : (LPCWSTR)sub, 0,
		KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
	{
		AfxMessageBox(L"打开注册表项失败。", MB_ICONERROR);
		return;
	}

	DWORD type = (DWORD)m.type;
	size_t dataLen = GuessDataSize(m.type, m.data);
	LONG st = RegSetValueExW(hKey, newName, 0, type,
		(const BYTE*)m.data.data(), (DWORD)dataLen);
	if (st == ERROR_SUCCESS)
	{
		st = RegDeleteValueW(hKey, m.name);
	}
	RegCloseKey(hKey);

	if (st == ERROR_SUCCESS)
	{
		*pResult = TRUE;
		InsertCtrlListControl();
	}
	else
	{
		AfxMessageBox(L"重命名失败。", MB_ICONERROR);
	}
}

void DlgEnumRegistry::OnTvnSelchangedEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTREEVIEW* p = (NMTREEVIEW*)pNMHDR;
	*pResult = 0;
	if (p && p->itemNew.hItem)
		SyncPathEditFromTreeItem(p->itemNew.hItem);
	else
		SyncPathEditFromTreeItem(m_CTreeCtrl.GetSelectedItem());
}

void DlgEnumRegistry::SyncPathEditFromTreeItem(HTREEITEM hItem)
{
	if (!m_PathEdit.GetSafeHwnd() || !hItem) return;
	CString* pData = (CString*)m_CTreeCtrl.GetItemData(hItem);
	if (!pData)
	{
		// 占位根节点（如 "计算机"）：直接用节点显示文本
		m_PathEdit.SetWindowTextW(m_CTreeCtrl.GetItemText(hItem));
		return;
	}
	HKEY root = NULL;
	CString sub;
	if (!KernelPathToRootSub(*pData, root, sub)) return;
	m_PathEdit.SetWindowTextW(L"Computer\\" + ToDisplayPath(root, sub));
}

void DlgEnumRegistry::OnNMClickEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult)
{
	UNREFERENCED_PARAMETER(pNMHDR);
	*pResult = 0;

	POINT pt = { 0 };
	GetCursorPos(&pt);
	m_CTreeCtrl.ScreenToClient(&pt);
	UINT flags = 0;
	HTREEITEM hHit = m_CTreeCtrl.HitTest(pt, &flags);
	if (!hHit) return;
	// 仅命中节点正文/图标时同步，点 +/- 展开按钮不动路径
	if (!(flags & (TVHT_ONITEMLABEL | TVHT_ONITEMICON | TVHT_ONITEMRIGHT)))
		return;
	SyncPathEditFromTreeItem(hHit);
}

void DlgEnumRegistry::CreateNewValueAtSelectedKey(UINT newCmdId)
{
	HTREEITEM hSel = m_CTreeCtrl.GetSelectedItem();
	if (!hSel) return;
	CString* pData = (CString*)m_CTreeCtrl.GetItemData(hSel);
	if (!pData) return;

	HKEY root = NULL;
	CString sub;
	if (!KernelPathToRootSub(*pData, root, sub)) return;

	HKEY hKey = NULL;
	if (RegOpenKeyExW(root, sub.IsEmpty() ? NULL : (LPCWSTR)sub, 0,
		KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
	{
		AfxMessageBox(L"打开注册表项失败，无法新建值。", MB_ICONERROR);
		return;
	}

	auto PickNewValueName = [&](HKEY key) -> CString
	{
		for (int i = 1; i <= 999; ++i)
		{
			CString name;
			if (i == 1) name = L"新值 #1";
			else name.Format(L"新值 #%d", i);
			LONG st = RegQueryValueExW(key, name, NULL, NULL, NULL, NULL);
			if (st == ERROR_FILE_NOT_FOUND) return name;
		}
		return L"新值";
	};

	CString valueName = PickNewValueName(hKey);
	DWORD type = REG_SZ;
	const BYTE* pInit = NULL;
	DWORD cbInit = 0;
	DWORD dwordZero = 0;
	ULONGLONG qwordZero = 0;
	WCHAR twoZero[2] = { 0, 0 };

	if (newCmdId == kRegNewSz)
	{
		type = REG_SZ;
		cbInit = sizeof(WCHAR);
		pInit = (const BYTE*)L"";
	}
	else if (newCmdId == kRegNewBinary)
	{
		type = REG_BINARY;
		cbInit = 0;
		pInit = NULL;
	}
	else if (newCmdId == kRegNewDword)
	{
		type = REG_DWORD;
		cbInit = sizeof(dwordZero);
		pInit = (const BYTE*)&dwordZero;
	}
	else if (newCmdId == kRegNewQword)
	{
		type = REG_QWORD;
		cbInit = sizeof(qwordZero);
		pInit = (const BYTE*)&qwordZero;
	}
	else if (newCmdId == kRegNewMultiSz)
	{
		type = REG_MULTI_SZ;
		cbInit = sizeof(twoZero);
		pInit = (const BYTE*)twoZero;
	}
	else if (newCmdId == kRegNewExpandSz)
	{
		type = REG_EXPAND_SZ;
		cbInit = sizeof(WCHAR);
		pInit = (const BYTE*)L"";
	}
	else
	{
		RegCloseKey(hKey);
		return;
	}

	LONG st = RegSetValueExW(hKey, valueName, 0, type, pInit, cbInit);
	RegCloseKey(hKey);
	if (st != ERROR_SUCCESS)
	{
		AfxMessageBox(L"新建值失败。", MB_ICONERROR);
		return;
	}

	InsertCtrlListControl();
	for (int i = 0; i < m_CListCtrl.GetItemCount(); ++i)
	{
		if (m_CListCtrl.GetItemText(i, 0).Compare(valueName) == 0)
		{
			m_CListCtrl.SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_CListCtrl.EnsureVisible(i, FALSE);
			m_PendingListRenameRow = i;
			m_CListCtrl.SetFocus();
			m_CListCtrl.EditLabel(i);
			break;
		}
	}
}

void DlgEnumRegistry::ShowRegFindDialog()
{
	RegDbg(L"ShowRegFindDialog: enter, this=%p", this);
	HTREEITEM hSel = m_CTreeCtrl.GetSelectedItem();
	if (!hSel)
	{
		RegDbg(L"ShowRegFindDialog: no tree selection");
		AfxMessageBox(L"请先在树中选择一个项作为查找的起点。", MB_ICONINFORMATION);
		return;
	}
	CString* pData = (CString*)m_CTreeCtrl.GetItemData(hSel);
	if (!pData)
	{
		RegDbg(L"ShowRegFindDialog: tree item has no data");
		AfxMessageBox(L"请选择具体的注册表项（不要在 \"Computer\" 节点上）。", MB_ICONINFORMATION);
		return;
	}
	HKEY root = NULL; CString sub;
	if (!KernelPathToRootSub(*pData, root, sub))
	{
		RegDbg(L"ShowRegFindDialog: KernelPathToRootSub failed kernel='%s'", (LPCWSTR)*pData);
		return;
	}
	RegDbg(L"ShowRegFindDialog: root=%p sub='%s'", root, (LPCWSTR)sub);

	// 起点变化时重置游标
	if (m_FindRoot != root || m_FindStartSub.CompareNoCase(sub) != 0)
	{
		RegDbg(L"ShowRegFindDialog: cursor reset block enter");
		RegDbg(L"  pre-assign: &m_FindRoot=%p &m_FindStartSub=%p",
			(void*)&m_FindRoot, (void*)&m_FindStartSub);
		m_FindRoot = root;
		RegDbg(L"  step1: m_FindRoot assigned");
		m_FindStartSub = sub;
		RegDbg(L"  step2: m_FindStartSub assigned len=%d", m_FindStartSub.GetLength());
		m_FindCursorValid = false;
		RegDbg(L"  step3: m_FindCursorValid=false");
		m_FindCursorSub.Empty();
		RegDbg(L"  step4: m_FindCursorSub.Empty()");
		m_FindCursorValue.Empty();
		RegDbg(L"  step5: m_FindCursorValue.Empty()");
		RegDbg(L"ShowRegFindDialog: cursor reset block done");
	}

	RegDbg(L"ShowRegFindDialog: about to construct tpl vector");
	std::vector<BYTE> tpl;
	RegDbg(L"ShowRegFindDialog: about to BuildRegFindTemplate");
	BuildRegFindTemplate(tpl);
	RegDbg(L"ShowRegFindDialog: tpl size=%llu, calling DialogBoxIndirectParamW",
		(unsigned long long)tpl.size());
	INT_PTR r = DialogBoxIndirectParamW(AfxGetInstanceHandle(),
		(LPCDLGTEMPLATEW)tpl.data(), GetSafeHwnd(),
		RegFindDlgProc, (LPARAM)this);
	RegDbg(L"ShowRegFindDialog: DialogBoxIndirectParamW returned %lld (LastError=%lu)",
		(long long)r, GetLastError());
}

bool DlgEnumRegistry::RegFindNext()
{
	if (!m_FindRoot || m_FindText.IsEmpty()) return false;

	CString needle = m_FindText;
	needle.MakeLower();

	bool skipping = m_FindCursorValid;
	const CString cursorSub   = m_FindCursorSub;
	const CString cursorValue = m_FindCursorValue;
	const CString startSub    = m_FindStartSub;
	const HKEY    rootKey     = m_FindRoot;
	const BOOL    lookKeys    = m_FindLookKeys;
	const BOOL    lookValues  = m_FindLookValues;
	const BOOL    lookData    = m_FindLookData;
	const BOOL    wholeOnly   = m_FindWhole;

	auto matchStr = [&](const CString& s) -> bool
	{
		CString l = s; l.MakeLower();
		return wholeOnly ? (l == needle) : (l.Find(needle) >= 0);
	};

	CString hitSub, hitValue;

	std::function<bool(const CString&)> walk = [&](const CString& curSub) -> bool
	{
		// 1) Key 名匹配（起点自身不参与）
		if (skipping)
		{
			if (cursorSub.CompareNoCase(curSub) == 0 && cursorValue.IsEmpty())
			{
				skipping = false; // 上次命中就是这个 key 名，跳过它继续
			}
		}
		else if (lookKeys && !curSub.IsEmpty() && curSub.CompareNoCase(startSub) != 0)
		{
			if (matchStr(LeafName(curSub)))
			{
				hitSub = curSub; hitValue.Empty();
				return true;
			}
		}

		// 2) 枚举本 key 的 values
		if (lookValues || lookData)
		{
			HKEY hKey = NULL;
			if (RegOpenKeyExW(rootKey, curSub.IsEmpty() ? NULL : (LPCWSTR)curSub, 0,
				KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
			{
				for (DWORD idx = 0; ; ++idx)
				{
					WCHAR name[1024]; DWORD nameLen = _countof(name);
					DWORD type = 0; DWORD dataLen = 0;
					LONG st = RegEnumValueW(hKey, idx, name, &nameLen, NULL,
						&type, NULL, &dataLen);
					if (st == ERROR_NO_MORE_ITEMS) break;
					if (st != ERROR_SUCCESS && st != ERROR_MORE_DATA) continue;

					CString vname = name;

					if (skipping)
					{
						if (cursorSub.CompareNoCase(curSub) == 0
							&& cursorValue.CompareNoCase(vname) == 0)
							skipping = false;
						continue;
					}

					if (lookValues && matchStr(vname))
					{
						hitSub = curSub; hitValue = vname;
						RegCloseKey(hKey);
						return true;
					}
					if (lookData && (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ)
						&& dataLen > 0 && dataLen < 0x100000)
					{
						std::vector<BYTE> data(dataLen + 2, 0);
						DWORD dl = dataLen;
						nameLen = _countof(name);
						LONG st2 = RegEnumValueW(hKey, idx, name, &nameLen, NULL,
							&type, data.data(), &dl);
						if (st2 == ERROR_SUCCESS)
						{
							LPCWSTR ws = (LPCWSTR)data.data();
							bool hitData = false;
							if (type == REG_MULTI_SZ)
							{
								while (*ws && !hitData)
								{
									if (matchStr(CString(ws))) hitData = true;
									ws += wcslen(ws) + 1;
								}
							}
							else
							{
								if (matchStr(CString(ws))) hitData = true;
							}
							if (hitData)
							{
								hitSub = curSub; hitValue = vname;
								RegCloseKey(hKey);
								return true;
							}
						}
					}
				}
				RegCloseKey(hKey);
			}
		}

		// 3) 递归子项
		HKEY hKey2 = NULL;
		if (RegOpenKeyExW(rootKey, curSub.IsEmpty() ? NULL : (LPCWSTR)curSub, 0,
			KEY_ENUMERATE_SUB_KEYS, &hKey2) == ERROR_SUCCESS)
		{
			for (DWORD i = 0; ; ++i)
			{
				WCHAR subBuf[256]; DWORD subLen = _countof(subBuf);
				LONG st = RegEnumKeyExW(hKey2, i, subBuf, &subLen,
					NULL, NULL, NULL, NULL);
				if (st == ERROR_NO_MORE_ITEMS) break;
				if (st != ERROR_SUCCESS) continue;
				CString next = curSub.IsEmpty() ? CString(subBuf)
					: (curSub + L"\\" + subBuf);
				if (walk(next)) { RegCloseKey(hKey2); return true; }
			}
			RegCloseKey(hKey2);
		}
		return false;
	};

	if (!walk(startSub))
	{
		m_FindCursorValid = false;
		return false;
	}

	// 更新游标
	m_FindCursorValid = true;
	m_FindCursorSub   = hitSub;
	m_FindCursorValue = hitValue;

	// 导航到命中 key
	NavigateToPath(ToDisplayPath(rootKey, hitSub));

	// 命中是 value/data：在列表里选中对应行
	if (!hitValue.IsEmpty())
	{
		for (int i = 0; i < m_CListCtrl.GetItemCount(); ++i)
		{
			if (m_CListCtrl.GetItemText(i, 0).CompareNoCase(hitValue) == 0)
			{
				m_CListCtrl.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
				m_CListCtrl.SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED,
					LVIS_SELECTED | LVIS_FOCUSED);
				m_CListCtrl.EnsureVisible(i, FALSE);
				m_CListCtrl.SetFocus();
				break;
			}
		}
	}
	else
	{
		m_CTreeCtrl.SetFocus();
	}
	return true;
}
