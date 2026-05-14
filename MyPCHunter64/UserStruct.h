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
	int ShowListCopyRefreshMenu(const int* cols, const wchar_t* const* colNames, int n, bool hasSelection, CWnd* owner);

	// 自适应版：直接从 CListCtrl 的表头读列标题。
	// 返回值: -1=未点击, 0=刷新, >0 表示要复制第 (返回值-1) 列(0-based)。
	int ShowListContextMenu(CListCtrl* list, CWnd* owner);
};

using CFunction = _CFunction;
using PCFunction = _CFunction*;