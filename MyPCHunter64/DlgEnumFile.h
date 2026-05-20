#pragma once
#include "afxdialogex.h"
#include <vector>


// DlgEnumFile 对话框

class DlgEnumFile : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgEnumFile)

public:
	DlgEnumFile(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgEnumFile();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum {
		IDD = ID_DLG_ENUMFILE
	};
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	//删除子节点
	void DlgEnumFile::DelTreeChild(HTREEITEM pNode);

	//清空List控件中所有绑定的数据
	void DlgEnumFile::DelListItemData();
	//枚举文件
	void DlgEnumFile::EnunFile();

	//外部调用：在本页面里展开路径、选中文件所在目录、并高亮文件。
	void DlgEnumFile::NavigateToFile(const CString& fullPath);

	// 文件剪贴板操作：将当前列表选中行的文件路径复制/剪切到系统剪贴板，
	// 以及把剪贴板上的文件粘贴到当前目录 m_CurPath。
	void DlgEnumFile::CopySelectedToClipboard(bool isCut);
	void DlgEnumFile::PasteFromClipboardToCurDir();

	//删除文件
	void DlgEnumFile::DelteFile();

	//解除占用
	void DlgEnumFile::FileFiledeoccupy();

public:
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	CListCtrl m_CListCtrl;
	CTreeCtrl m_CTreeCtrl;
	CString m_CurPath;
	afx_msg void OnDblclkEnumfileTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnFileDelete();
	afx_msg void OnFileRefresh();
	afx_msg void OnNMRClickEnumfileList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnFileFiledeoccupy();
	afx_msg void OnEndLabelEditEnumfileList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMRClickEnumfileTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndLabelEditEnumfileTree(NMHDR* pNMHDR, LRESULT* pResult);

	// ===== 顶部地址栏 (后退/前进 + 路径输入 + 搜索) =====
	CButton m_BtnBack;
	CButton m_BtnForward;
	CEdit   m_PathEdit;
	CButton m_BtnGo;
	CEdit   m_SearchEdit;

	// 浏览历史，索引指向 m_NavHistory[m_NavIndex] 是"当前"目录
	std::vector<CString> m_NavHistory;
	int  m_NavIndex = -1;
	bool m_NavSuppressHistory = false; // 后退/前进期间内部 Navigate 不写历史

	// 跳转到目录（带 trailing \）；addHistory=true 时写入历史栈
	void NavigateToDirectory(const CString& dirWithSlash, bool addHistory);
	// 根据 m_NavHistory[m_NavIndex] 同步按钮可用状态
	void UpdateNavButtons();
	// 应用搜索过滤：把当前列表中不匹配的行删掉
	void ApplyListSearchFilter(const CString& keyword);

	afx_msg void OnBtnNavBack();
	afx_msg void OnBtnNavForward();
	afx_msg void OnBtnNavGo();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
};
