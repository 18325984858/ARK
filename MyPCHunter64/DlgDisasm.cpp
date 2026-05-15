// DlgDisasm.cpp: 反汇编查看窗口
#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgDisasm.h"
#include "CLoadDriver.h"
#include "../MyDriver64/Struct.h"
#include "include/capstone-5.0-Release/include/capstone/capstone.h"

extern void AppLog_Write(const char* level, const char* fmt, ...);
#define LOGI(...) AppLog_Write("INFO ", __VA_ARGS__)
#define LOGW(...) AppLog_Write("WARN ", __VA_ARGS__)
#define LOGE(...) AppLog_Write("ERROR", __VA_ARGS__)

IMPLEMENT_DYNAMIC(DlgDisasm, CDialogEx)

DlgDisasm::DlgDisasm(ULONG64 startKva, CWnd* pParent /*=nullptr*/)
    : CDialogEx(ID_DLG_DPC, pParent), m_StartKva(startKva)
{
}

DlgDisasm::~DlgDisasm()
{
}

void DlgDisasm::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, ID_DPC_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgDisasm, CDialogEx)
    ON_WM_SIZE()
    ON_NOTIFY(NM_RCLICK, ID_DPC_LIST, &DlgDisasm::OnNMRClickList)
END_MESSAGE_MAP()


void DlgDisasm::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    CRect rect;
    GetClientRect(&rect);
    m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}

BOOL DlgDisasm::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 模板原是子窗口风格（WS_CHILD），modeless 时改成顶级 popup + 标题栏 + 关闭按钮
    ModifyStyle(WS_CHILD, WS_POPUP | WS_OVERLAPPEDWINDOW);
    SetWindowPos(nullptr, 0, 0, 900, 600, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    CenterWindow();

    m_CListCtrl.InsertColumn(Col_Addr,  _T("地址"),       LVCFMT_LEFT, 150);
    m_CListCtrl.InsertColumn(Col_Bytes, _T("字节"),       LVCFMT_LEFT, 200);
    m_CListCtrl.InsertColumn(Col_Mnem,  _T("助记符"),     LVCFMT_LEFT, 80);
    m_CListCtrl.InsertColumn(Col_Op,    _T("操作数"),     LVCFMT_LEFT, 400);
    m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // 把窗口标题改成显示当前地址
    CString title;
    title.Format(L"反汇编 @ 0x%016I64X", m_StartKva);
    SetWindowTextW(title);

    // 通过驱动读取 256 字节
    const ULONG kReadLen = 256;
    UCHAR buf[kReadLen] = { 0 };
    CKernelRangeReadInfo req = { 0 };
    req.KernelAddr = m_StartKva;
    req.Length     = kReadLen;
    req.UserBuf    = buf;
    g_LoadDriver.SendMsg(um_Cmd_Read_KernelRange_info, &req, nullptr, nullptr, nullptr);
    LOGI("[Disasm] kva=0x%016llX BytesRead=%lu first8=%02X %02X %02X %02X %02X %02X %02X %02X",
        (unsigned long long)m_StartKva, (unsigned long)req.BytesRead,
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    if (req.BytesRead == 0)
    {
        int row = m_CListCtrl.InsertItem(0, L"-");
        m_CListCtrl.SetItemText(row, Col_Mnem, L"读取失败");
        return TRUE;
    }

    // Capstone 反汇编
    csh handle = 0;
    cs_err csErr = cs_open(CS_ARCH_X86, CS_MODE_64, &handle);
    if (csErr != CS_ERR_OK)
    {
        LOGE("[Disasm] cs_open failed err=%d", (int)csErr);
        int row = m_CListCtrl.InsertItem(0, L"-");
        m_CListCtrl.SetItemText(row, Col_Mnem, L"capstone init failed");
        return TRUE;
    }
    // 开启 SKIPDATA：遇到非法 opcode 不停下，而是当 .byte 跳过继续往后翻。
    // 否则 cs_disasm(count=0) 第一条非法就返回 0 条，列表空白。
    cs_option(handle, CS_OPT_SKIPDATA, CS_OPT_ON);

    cs_insn* insn = nullptr;
    size_t cnt = cs_disasm(handle, buf, req.BytesRead, m_StartKva, 0, &insn);
    LOGI("[Disasm] cs_disasm cnt=%zu", cnt);
    if (cnt == 0)
    {
        int row = m_CListCtrl.InsertItem(0, L"-");
        m_CListCtrl.SetItemText(row, Col_Mnem, L"无法反汇编");
    }
    for (size_t i = 0; i < cnt; ++i)
    {
        CString s;
        s.Format(L"%016I64X", insn[i].address);
        int row = m_CListCtrl.InsertItem((int)i, s);

        // 字节 hex
        wchar_t bytes[3 * 16 + 1] = { 0 };
        wchar_t* p = bytes;
        int n = insn[i].size > 15 ? 15 : insn[i].size;
        for (int b = 0; b < n; ++b)
        {
            _snwprintf_s(p, 4, _TRUNCATE, L"%02X ", insn[i].bytes[b]);
            p += 3;
        }
        m_CListCtrl.SetItemText(row, Col_Bytes, bytes);

        // mnemonic / op_str 是 narrow，转宽
        wchar_t mn[32]; _snwprintf_s(mn, _countof(mn), _TRUNCATE, L"%hs", insn[i].mnemonic);
        wchar_t op[256]; _snwprintf_s(op, _countof(op), _TRUNCATE, L"%hs", insn[i].op_str);
        m_CListCtrl.SetItemText(row, Col_Mnem, mn);
        m_CListCtrl.SetItemText(row, Col_Op,   op);
    }
    if (cnt > 0) cs_free(insn, cnt);
    cs_close(&handle);

    return TRUE;
}

void DlgDisasm::OnNMRClickList(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = 0;
    int r = ShowListContextMenu(&m_CListCtrl, this);
    if (r == 0) return;            // 这个窗口不刷新（只显示一次性的反汇编）
    if (r > 0) CopyBufferToClipboard(&m_CListCtrl, r - 1);
}

// modeless 窗口关闭后 MFC 调 PostNcDestroy；我们 new 出来的需要在这里 delete 自己。
void DlgDisasm::PostNcDestroy()
{
    CDialogEx::PostNcDestroy();
    delete this;
}
