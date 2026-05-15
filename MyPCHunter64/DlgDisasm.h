#pragma once
#include "afxdialogex.h"
#include "BaseClass.h"

// 简单反汇编查看窗口：复用 ID_DLG_DPC 模板（单 ListCtrl 充满）。
// 创建时给一个内核 VA，OnInitDialog 里去驱动读最多 256 字节然后用 Capstone 反汇编。
class DlgDisasm : public CDialogEx, public CFunction
{
    DECLARE_DYNAMIC(DlgDisasm)
public:
    enum Col
    {
        Col_Addr,
        Col_Bytes,
        Col_Mnem,
        Col_Op,
    };

    DlgDisasm(ULONG64 startKva, CWnd* pParent = nullptr);
    virtual ~DlgDisasm();

#ifdef AFX_DESIGN_TIME
    enum { IDD = ID_DLG_DPC };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnSize(UINT nType, int cx, int cy);
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy() override;
    afx_msg void OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult);

    CListCtrl m_CListCtrl;
    ULONG64   m_StartKva;
};
