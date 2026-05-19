#include "pch.h"
#include "MyPCHunter64.h"
#include "afxdialogex.h"
#include "DlgDirectIo.h"
#include "Thread.h"

IMPLEMENT_DYNAMIC(DlgDirectIo, CDialogEx)

static CString FormatDeviceFlags(ULONG64 Flags)
{
    CString Text;
    auto AppendFlag = [&](ULONG64 Flag, LPCTSTR Name)
    {
        if (Flags & Flag)
        {
            if (!Text.IsEmpty()) Text += L"|";
            Text += Name;
        }
    };

    AppendFlag(0x00000004, L"DO_BUFFERED_IO");
    AppendFlag(0x00000010, L"DO_DIRECT_IO");
    AppendFlag(0x00000080, L"DO_DEVICE_INITIALIZING");
    AppendFlag(0x00000800, L"DO_SHUTDOWN_REGISTERED");
    AppendFlag(0x00002000, L"DO_POWER_PAGABLE");
    AppendFlag(0x00004000, L"DO_POWER_INRUSH");

    CString StrBuf;
    StrBuf.Format(L"0x%08I64X", Flags);
    if (!Text.IsEmpty())
    {
        StrBuf += L" ";
        StrBuf += Text;
    }
    return StrBuf;
}

DlgDirectIo::DlgDirectIo(CWnd* pParent /*=nullptr*/)
    : CDialogEx(ID_DLG_DPC, pParent)
{
}

DlgDirectIo::~DlgDirectIo()
{
}

void DlgDirectIo::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, ID_DPC_LIST, m_CListCtrl);
}

BEGIN_MESSAGE_MAP(DlgDirectIo, CDialogEx)
    ON_NOTIFY(NM_RCLICK, ID_DPC_LIST, &DlgDirectIo::OnNMRClickDirectIoList)
    ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL DlgDirectIo::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_CListCtrl.InsertColumn(um_DirectIo_Type, _T("类型"), LVCFMT_LEFT, 100);
    m_CListCtrl.InsertColumn(um_DirectIo_DeviceObject, _T("设备对象"), LVCFMT_LEFT, 150);
    m_CListCtrl.InsertColumn(um_DirectIo_DeviceName, _T("设备名称"), LVCFMT_LEFT, 220);
    m_CListCtrl.InsertColumn(um_DirectIo_DriverObject, _T("驱动对象"), LVCFMT_LEFT, 150);
    m_CListCtrl.InsertColumn(um_DirectIo_DriverName, _T("驱动名称"), LVCFMT_LEFT, 220);
    m_CListCtrl.InsertColumn(um_DirectIo_DeviceType, _T("DeviceType"), LVCFMT_LEFT, 90);
    m_CListCtrl.InsertColumn(um_DirectIo_DeviceFlags, _T("Flags"), LVCFMT_LEFT, 260);
    m_CListCtrl.InsertColumn(um_DirectIo_AttachedDevice, _T("AttachedDevice"), LVCFMT_LEFT, 150);
    m_CListCtrl.InsertColumn(um_DirectIo_NextDevice, _T("NextDevice"), LVCFMT_LEFT, 150);
    m_CListCtrl.SetExtendedStyle(m_CListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    return TRUE;
}

void DlgDirectIo::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    CRect rect;
    GetClientRect(&rect);
    m_CListCtrl.SetWindowPos(NULL, 0, 0, rect.Width(), rect.Height(), SWP_NOZORDER);
}

void DlgDirectIo::OnNMRClickDirectIoList(NMHDR* pNMHDR, LRESULT* pResult)
{
    UNREFERENCED_PARAMETER(pNMHDR);
    *pResult = 0;

    static const int cols[] = {
        um_DirectIo_Type, um_DirectIo_DeviceObject, um_DirectIo_DeviceName,
        um_DirectIo_DriverObject, um_DirectIo_DriverName, um_DirectIo_DeviceType,
        um_DirectIo_DeviceFlags, um_DirectIo_AttachedDevice, um_DirectIo_NextDevice
    };
    static const wchar_t* const names[] = {
        L"类型", L"设备对象", L"设备名称", L"驱动对象", L"驱动名称",
        L"DeviceType", L"Flags", L"AttachedDevice", L"NextDevice"
    };

    bool hasSel = (m_CListCtrl.GetFirstSelectedItemPosition() != NULL);
    int r = ShowListCopyRefreshMenu(cols, names, (int)_countof(cols), hasSel, this);
    if (r == 0) { if (this->m_ThreadFlags != 1) OnDirectIoRefresh(); }
    else if (r > 0) CopyBufferToClipboard(&m_CListCtrl, cols[r - 1]);
}

void DlgDirectIo::OnDirectIoRefresh()
{
    m_CListCtrl.DeleteAllItems();
    g_ThreadPool.AddTask(new _CThreadPack{ _LoadDriver::Um_UserCallBackType_UserEnumDirectIoInfo, this });
}

void DlgDirectIo::InsertCtrlListControl(PCFileSystemDeviceInfo pFileSystemDeviceInfo)
{
    if (pFileSystemDeviceInfo == NULL)
    {
        return;
    }

    static const CString TypeName[] = { L"Disk", L"CdRom", L"Network", L"Tape" };
    PCLIST_ENTRY pCurList = &pFileSystemDeviceInfo->List.List;

    do
    {
        if (pCurList == NULL)
        {
            break;
        }

        int i = m_CListCtrl.GetItemCount();
        PCFileSystemDeviceInfo pInfo = (PCFileSystemDeviceInfo)pCurList;
        CString StrBuf;

        CString type = L"Unknown";
        if (pInfo->nType < _countof(TypeName))
            type = TypeName[pInfo->nType];
        m_CListCtrl.InsertItem(i, type);

        StrBuf.Format(L"%016I64X", pInfo->DeviceObject);
        m_CListCtrl.SetItemText(i, um_DirectIo_DeviceObject, StrBuf);

        m_CListCtrl.SetItemText(i, um_DirectIo_DeviceName, pInfo->DeviceName[0] ? pInfo->DeviceName : L"--");

        StrBuf.Format(L"%016I64X", pInfo->DriverObject);
        m_CListCtrl.SetItemText(i, um_DirectIo_DriverObject, StrBuf);

        m_CListCtrl.SetItemText(i, um_DirectIo_DriverName, pInfo->DriverName[0] ? pInfo->DriverName : L"--");

        StrBuf.Format(L"0x%08I64X", pInfo->DeviceType);
        m_CListCtrl.SetItemText(i, um_DirectIo_DeviceType, StrBuf);

        m_CListCtrl.SetItemText(i, um_DirectIo_DeviceFlags, FormatDeviceFlags(pInfo->DeviceFlags));

        StrBuf.Format(L"%016I64X", pInfo->AttachedDevice);
        m_CListCtrl.SetItemText(i, um_DirectIo_AttachedDevice, StrBuf);

        StrBuf.Format(L"%016I64X", pInfo->NextDevice);
        m_CListCtrl.SetItemText(i, um_DirectIo_NextDevice, StrBuf);

        pCurList = pCurList->Blink;

        SIZE_T FreeSize = 0;
        if (MyNtFreeVirtualMemory(GetCurrentProcess(), (LPVOID*)&pInfo, &FreeSize, MEM_RELEASE) != 0)
        {
            AfxMessageBox(L"释放空间失败!");
        }
    } while (pCurList != &pFileSystemDeviceInfo->List.List);
}