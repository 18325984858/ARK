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
};

using CFunction = _CFunction;
using PCFunction = _CFunction*;