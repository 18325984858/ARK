// DlgKernelHookList.cpp: 内核钩子检测结果对话框
#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgKernelHookList.h"
#include "DlgDisasm.h"
#include "KernelHookDetect.h"
#include "Thread.h"
#include "CThreadPool.h"

IMPLEMENT_DYNAMIC(DlgKernelHookList, CDialogEx)

DlgKernelHookList::DlgKernelHookList(CWnd* pParent /*=nullptr*/)
    : CDialogEx(ID_DLG_DPC, pParent) // 复用模板
{
}

DlgKernelHookList::~DlgKernelHookList()
{
}

void DlgKernelHookList::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, ID_DPC_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgKernelHookList, CDialogEx)
    ON_WM_SIZE()
    ON_NOTIFY(NM_RCLICK, ID_DPC_LIST, &DlgKernelHookList::OnNMRClickList)
END_MESSAGE_MAP()


void DlgKernelHookList::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    CRect rect;
    GetClientRect(&rect);
    m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}

BOOL DlgKernelHookList::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_CListCtrl.InsertColumn(Col_FuncName,   _T("函数名"),       LVCFMT_LEFT, 240);
    m_CListCtrl.InsertColumn(Col_Address,    _T("当前函数地址"), LVCFMT_LEFT, 140);
    m_CListCtrl.InsertColumn(Col_HookType,   _T("HOOK 类型"),    LVCFMT_LEFT, 100);
    m_CListCtrl.InsertColumn(Col_HookTarget, _T("跳转去向"),     LVCFMT_LEFT, 200);
    m_CListCtrl.InsertColumn(Col_Bytes,      _T("Hook 字节"),    LVCFMT_LEFT, 240);
    m_CListCtrl.InsertColumn(Col_DiskBytes,  _T("原始字节"),     LVCFMT_LEFT, 240);
    m_CListCtrl.InsertColumn(Col_Module,     _T("所在模块"),     LVCFMT_LEFT, 280);

    m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    return TRUE;
}

static LPCWSTR HookTypeName(KernelHookType t)
{
    switch (t)
    {
    case KernelHookType::JmpRel32:  return L"JMP rel32 (E9)";
    case KernelHookType::JmpAbs:    return L"JMP [rip] (FF 25)";
    case KernelHookType::MovJmpAbs: return L"MOV+JMP (48 B8 .. FF E0)";
    case KernelHookType::Push64Ret: return L"PUSH/RET (68 .. C3)";
    case KernelHookType::Int3:      return L"INT3 (CC)";
    case KernelHookType::Retn:      return L"RETN (C3)";
    case KernelHookType::Suspicious:return L"可疑";
    default:                        return L"";
    }
}

void DlgKernelHookList::OnRefresh()
{
    LOGI("[KHK] OnRefresh BEGIN (tid=%lu, this=%p, cb=%d)",
        GetCurrentThreadId(), this, (int)_LoadDriver::Um_UserCallBackType_UserEnumKernelHookInfo);
    // 仅做 UI 侧准备：清空列表、插一行"扫描中..."，然后把真正的扫描扔到线程池。
    m_CListCtrl.DeleteAllItems();
    int row = m_CListCtrl.InsertItem(0, L"正在扫描内核函数 inline hook ...");
    m_CListCtrl.SetItemText(row, Col_HookType, L"扫描中");

    g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumKernelHookInfo, this });
    LOGI("[KHK] OnRefresh: task posted");
}

void DlgKernelHookList::DoScanAndFill()
{
    LOGI("[KHK] DoScanAndFill BEGIN (tid=%lu)", GetCurrentThreadId());
    std::vector<KernelHookFinding> findings;
    KernelTextScanStats stats = { 0 };
    DetectKernelTextHooks(findings, stats);
    LOGI("[KHK] DoScanAndFill: detect returned, findings=%zu textSize=%u readBytes=%u diffRuns=%u",
        findings.size(), stats.textSize, stats.readBytes, stats.diffRuns);

    // 回到这里时仍在工作线程；CListCtrl 在 MFC 里的 Insert/SetItemText 不要求严格 UI 线程，
    // 项目里其它 dialog（DlgDpc/DlgWorkerThread 等）也都是从线程池里直接更新 list，沿用同一模式。
    m_CListCtrl.DeleteAllItems();

    if (findings.empty())
    {
        CString s;
        s.Format(L"扫描 .text=%u 字节，已读 %u 字节，发现 %u 个差异段，未识别到 inline hook",
            stats.textSize, stats.readBytes, stats.diffRuns);
        int row = m_CListCtrl.InsertItem(0, s);
        m_CListCtrl.SetItemText(row, Col_HookType, stats.readBytes == 0 ? L"驱动未就绪" : L"OK");
        return;
    }

    for (size_t i = 0; i < findings.size(); ++i)
    {
        const auto& f = findings[i];
        CString s;
        int row = m_CListCtrl.InsertItem((int)i, f.funcName.c_str());

        s.Format(L"%016I64X", f.kva);
        m_CListCtrl.SetItemText(row, Col_Address, s);

        m_CListCtrl.SetItemText(row, Col_HookType, HookTypeName(f.type));
        m_CListCtrl.SetItemText(row, Col_HookTarget, f.jumpTargetDesc.c_str());

        // Hook 字节 hex dump（内核内存）
        wchar_t hookBytes[3 * 16 + 1] = { 0 };
        wchar_t* p = hookBytes;
        for (int b = 0; b < 16; ++b)
        {
            _snwprintf_s(p, 4, _TRUNCATE, L"%02X ", f.bytes[b]);
            p += 3;
        }
        m_CListCtrl.SetItemText(row, Col_Bytes, hookBytes);

        // 原始字节 hex dump（磁盘 PE）
        wchar_t diskBytes[3 * 16 + 1] = { 0 };
        p = diskBytes;
        for (int b = 0; b < 16; ++b)
        {
            _snwprintf_s(p, 4, _TRUNCATE, L"%02X ", f.diskBytes[b]);
            p += 3;
        }
        m_CListCtrl.SetItemText(row, Col_DiskBytes, diskBytes);

        m_CListCtrl.SetItemText(row, Col_Module, f.modulePath.c_str());
    }
}

void DlgKernelHookList::OnNMRClickList(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = 0;

    bool hasSel = (m_CListCtrl.GetFirstSelectedItemPosition() != NULL);

    const UINT kCopyBase   = 9001;
    const UINT kRefresh    = 9000;
    const UINT kDisasm     = 9100;

    CMenu menu;
    menu.CreatePopupMenu();
    int nCols = AppendCopyColumnsSubmenu(menu, &m_CListCtrl, kCopyBase, hasSel);
    menu.AppendMenuW(MF_STRING, kRefresh, L"刷新");
    menu.AppendMenuW(MF_SEPARATOR);
    menu.AppendMenuW(MF_STRING | (hasSel ? 0 : MF_GRAYED), kDisasm, L"反汇编 ...");

    CString explorerPath;
    UINT explorerCmd = AppendOpenInExplorerItem(menu, &m_CListCtrl, explorerPath);

    POINT pt = { 0 }; GetCursorPos(&pt);
    UINT cmd = menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, this);

    if (HandleOpenInExplorerCmd(cmd, explorerCmd, explorerPath)) return;

    if (cmd == kRefresh) { OnRefresh(); return; }
    if (TryHandleCopyColumnsCmd(cmd, kCopyBase, nCols, &m_CListCtrl)) return;
    if (cmd == kDisasm && hasSel)
    {
        // 取选中行的"当前函数地址"列做为入口
        POSITION p = m_CListCtrl.GetFirstSelectedItemPosition();
        int rowIdx = (int)p - 1;
        CString addrStr = m_CListCtrl.GetItemText(rowIdx, Col_Address);
        ULONG64 kva = _wcstoui64(addrStr.GetBuffer(), nullptr, 16);
        addrStr.ReleaseBuffer();
        if (kva == 0) return;

        // modeless：在堆上 new，让 PostNcDestroy 自删除。这样调用方立即返回，
        // 反汇编窗口独立显示，多次右键可以叠多个。
        DlgDisasm* pDlg = new DlgDisasm(kva, nullptr);
        pDlg->Create(ID_DLG_DPC, nullptr);
        pDlg->ShowWindow(SW_SHOW);
        return;
    }
}
