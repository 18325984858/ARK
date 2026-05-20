#pragma once

#include "framework.h"
#include "BaseClass.h"

//功能函数类
class _CFunction :public _MyCBaseDataObject
{
public:
	BOOL _CFunction::DeviceDosPathToNtPath(wchar_t* pszDosPath, wchar_t* pszNtPath);							//将DeviceDoS路径转换NT路径

	//Driver路径转换
	CString _CFunction::PathTransForm(WCHAR* Path);

	//获取文件信息
	bool _CFunction::GetFileDescription(const CString& szModuleName, CString& RetStr);							//获取文件描述
	bool _CFunction::GetFileVersion(const CString& szModuleName, CString& RetStr);								//获取文件版本
	bool _CFunction::GetInternalName(const CString& szModuleName, CString& RetStr);								//获取文件内部名字
	bool _CFunction::GetCompanyName(const CString& szModuleName, CString& RetStr);								//获取文件公司名称
	bool _CFunction::GetLegalCopyright(const CString& szModuleName, CString& RetStr);							//获取文件版权信息
	bool _CFunction::GetOriginalFilename(const CString& szModuleName, CString& RetStr);							//获取源文件名
	bool _CFunction::GetProductName(const CString& szModuleName, CString& RetStr);								//获取产品名称
	bool _CFunction::GetProductVersion(const CString& szModuleName, CString& RetStr);							//获取产品版本
	bool _CFunction::FsQueryValue(const CString& wsValueName, const CString& wsModuleName, CString& wsRetStr);	//查询文件信息函数
	LONG _CFunction::GetSoftSign(TCHAR* v_pszFilePath, TCHAR* v_pszSign, int v_iBufSize);						//获取签名信息

	CHAR _CFunction::CopyBufferToClipboard(CListCtrl* m_CListCtrl, SIZE_T ItemTextIndex);						//拷贝指定CListCtrl的数据

	// 右键菜单 helper：构建 "复制 ▸ 各列 / 刷新" 两级菜单并阻塞弹出。
	// cols/colNames 数组成对，length=n。返回值：-1=取消/未点击，0=点了"刷新"，1..n=点了"复制 第 cols[i-1] 列"
	// hasSelection=false 时复制项灰显。
	// listForPath：传入对应的 CListCtrl 用于探测选中行是否含有文件路径；非空时菜单末尾会自动追加
	// "使用资源管理器打开"，命中后内部直接打开 Explorer 并定位文件，函数返回 -1。
	int ShowListCopyRefreshMenu(const int* cols, const wchar_t* const* colNames, int n, bool hasSelection, CWnd* owner, CListCtrl* listForPath = nullptr);

	// 自适应版：直接从 CListCtrl 的表头读列标题。
	// 返回值: -1=未点击, 0=刷新, >0 表示要复制第 (返回值-1) 列(0-based)。
	int ShowListContextMenu(CListCtrl* list, CWnd* owner);

	// 在传入的 menu 末尾追加“使用资源管理器打开”菜单项。
	// 仅当选中行某列是磁盘上真实存在的文件路径时才追加。
	// 返回值：命中时返回 outPath 并输出使用的 cmd id（传给 caller 供 TrackPopupMenu 后比对）；
	// 未追加时 outPath 为空，返回 0。
	UINT AppendOpenInExplorerItem(CMenu& menu, CListCtrl* list, CString& outPath);

	// 检查 cmd 是否是 AppendOpenInExplorerItem 返回的 id，是的话打开 Explorer 高亮文件，返回 true。
	bool HandleOpenInExplorerCmd(UINT cmd, UINT expectedCmdId, const CString& path);

	// 在 parentMenu 末尾追加一个 “复制” 弹出子菜单，子菜单条目按 ListCtrl 表头各列生成。
	// 命令 ID 范围 [kCopyBase, kCopyBase + 返回值)；返回值 == 列数，为 0 时未追加。
	// 子菜单 HMENU 已交给 parentMenu，调用方无需手动 Detach。
	int AppendCopyColumnsSubmenu(CMenu& parentMenu, CListCtrl* list,
		UINT kCopyBase, bool hasSel, LPCWSTR label = L"复制");

	// 判断 cmd 是否落在 AppendCopyColumnsSubmenu 的 ID 范围内，是的话执行复制并返回 true。
	bool TryHandleCopyColumnsCmd(UINT cmd, UINT kCopyBase, int nCols, CListCtrl* list);
};

using CFunction = _CFunction;
using PCFunction = _CFunction*;