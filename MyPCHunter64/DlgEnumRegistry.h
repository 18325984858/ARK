#pragma once
#include "afxdialogex.h"
#include <vector>


// DlgEnumRegistry 对话框

class DlgEnumRegistry : public CDialogEx, public CFunction
{
	DECLARE_DYNAMIC(DlgEnumRegistry)

public:
	DlgEnumRegistry(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DlgEnumRegistry();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = ID_DLG_ENUMREGISTRY };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	void DlgEnumRegistry::DelTreeChild(HTREEITEM pNode);
	void InsertCtrlListControl();
	// 路径跳转：HKLM\\X\\Y 或 HKEY_LOCAL_MACHINE\\X\\Y，递层展开 + 选中
	void NavigateToPath(const CString& path);
public:
	CTreeCtrl m_CTreeCtrl;
	CListCtrl m_CListCtrl;
	CEdit     m_PathEdit;        // 顶部路径输入框，回车跳转
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	ULONG64 InitControl();
	afx_msg void OnNMDblclkEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMDblclkEnumregsitryList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMRClickEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMRClickEnumregsitryList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTvnEndlabeleditEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLvnEndlabeleditEnumregsitryList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTvnSelchangedEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMClickEnumregsitryTree(NMHDR* pNMHDR, LRESULT* pResult);

	// 把指定树节点的内核路径（ItemData 里的 CString*）转换成显示路径并写入 m_PathEdit。
	void SyncPathEditFromTreeItem(HTREEITEM hItem);

	// 在当前选中树节点对应的注册表项里新建一个值，类型由 newCmdId 决定（kNewSz/kNewBinary/
	// kNewDword/kNewQword/kNewMultiSz/kNewExpandSz）。新建成功后会刷新列表并进入重命名。
	void CreateNewValueAtSelectedKey(UINT newCmdId);

	// 弹出 regedit 风格的 "查找" 对话框（基于当前选中树节点开始搜索）。
	void ShowRegFindDialog();
	// 执行一次 "查找下一个"。命中时定位树/列表并返回 true。
	bool RegFindNext();

	// 与 List 控件行并行的元信息：类型 + 原始字节。按 List 实际顺序存储（插入 0 头）。
	struct RegRowMeta {
		ULONG64 type;
		std::vector<BYTE> data;
		CString name;
	};
	std::vector<RegRowMeta> m_RowMeta;

	// Tree/List 原位重命名时缓存上下文
	CString m_PendingTreeOldKernelPath;
	int     m_PendingListRenameRow = -1;

	// 查找会话状态
	CString m_FindText;
	BOOL    m_FindLookKeys   = TRUE;
	BOOL    m_FindLookValues = TRUE;
	BOOL    m_FindLookData   = TRUE;
	BOOL    m_FindWhole      = FALSE;
	// 起点（点 "查找..." 时的树节点）
	HKEY    m_FindRoot       = NULL;
	CString m_FindStartSub;   // 起点 sub（如 "SYSTEM\\..."，可为空表示根 hive）
	// 游标：下一次搜索从该位置之后开始
	bool    m_FindCursorValid = false;
	CString m_FindCursorSub;     // 命中的 sub（包含起点开始的相对路径）
	CString m_FindCursorValue;   // 命中所在 key 中的 value 名；空表示命中的是 key 名
};
