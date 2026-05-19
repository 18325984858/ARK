#pragma once
#include "afxdialogex.h"

typedef struct _FileSystemDeviceInfo* PCFileSystemDeviceInfo;

class DlgDirectIo : public CDialogEx, public CFunction
{
    DECLARE_DYNAMIC(DlgDirectIo)
public:
    enum UMDirectIoInfo
    {
        um_DirectIo_Type,
        um_DirectIo_DeviceObject,
        um_DirectIo_DeviceName,
        um_DirectIo_DriverObject,
        um_DirectIo_DriverName,
        um_DirectIo_DeviceType,
        um_DirectIo_DeviceFlags,
        um_DirectIo_AttachedDevice,
        um_DirectIo_NextDevice,
    };

public:
    DlgDirectIo(CWnd* pParent = nullptr);
    virtual ~DlgDirectIo();

#ifdef AFX_DESIGN_TIME
    enum { IDD = ID_DLG_DPC };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()

public:
    virtual BOOL OnInitDialog();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnNMRClickDirectIoList(NMHDR* pNMHDR, LRESULT* pResult);
    void OnDirectIoRefresh();
    void InsertCtrlListControl(PCFileSystemDeviceInfo pFileSystemDeviceInfo);

public:
    CListCtrl m_CListCtrl;
};