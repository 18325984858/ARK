#pragma once
#include "afxdialogex.h"
#include "BaseClass.h"

// 内核钩子检测结果列表（复用 ID_DLG_DPC 模板：单 ListCtrl 充满）
class DlgKernelHookList : public CDialogEx, public CFunction
{
    DECLARE_DYNAMIC(DlgKernelHookList)
public:
    enum Col
    {
        Col_FuncName,
        Col_Address,
        Col_HookType,
        Col_HookTarget,
        Col_Bytes,          // hook 后的字节（内核内存）
        Col_DiskBytes,      // 原始字节（磁盘 PE）
        Col_Module,
    };

    DlgKernelHookList(CWnd* pParent = nullptr);
    virtual ~DlgKernelHookList();

#ifdef AFX_DESIGN_TIME
    enum { IDD = ID_DLG_DPC };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()

public:
    void OnRefresh();
    void DoScanAndFill();        // 在后台线程执行的扫描 + 回填
    afx_msg void OnSize(UINT nType, int cx, int cy);
    virtual BOOL OnInitDialog();
    afx_msg void OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult);

    CListCtrl m_CListCtrl;
};
