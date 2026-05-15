#define  _CRT_NON_CONFORMING_SWPRINTFS
#include "framework.h"
#include "MyPCHunter64.h"   // LOGI/LOGW/LOGE
#include <vector>


void ErrorMessage(ULONG unErrorCode, CString Msg)/*将系统错误码转换成中文以信息框方式打印*/
{
	// 打日志便于定位"不知道哪冒出来的"系统错误弹窗
	// （含调用方传入的前缀；前缀为空时说明纯粹是 FormatMessage 的系统串）
	LOGW("[ErrorMessage] code=%lu (0x%lX) prefix='%ls'",
		(unsigned long)unErrorCode, (unsigned long)unErrorCode,
		Msg.IsEmpty() ? L"(empty)" : (LPCWSTR)Msg.GetString());

	LPVOID lpBuffer = nullptr;
	DWORD dwSize = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		unErrorCode,															/*要查找的错误码*/
		MAKELANGID(LANG_SYSTEM_DEFAULT, LANG_USER_DEFAULT),						/*语言*/
		(LPTSTR)&lpBuffer,														/*缓冲区*/
		0,																		/*缓冲区大小*/
		NULL);

	if (dwSize == NULL)
	{
		return;
	}
	TCHAR szBuf[MAXBYTE] = { 0 };

	if (Msg != "")
	{
		wsprintf(szBuf, TEXT("%s%s"), Msg.GetString(), lpBuffer);
	}
	else
	{
		wsprintf(szBuf, TEXT("%s"), lpBuffer);
	}

	::MessageBox(NULL, szBuf, TEXT("Error"), MB_OK | MB_ICONWARNING);
	LocalFree(lpBuffer);
	return;
}

VOID::UniCodeToAsciiString(WCHAR* szwchar, CHAR* szchar)//字符转换函数
{
	DWORD num = WideCharToMultiByte(CP_ACP, 0, szwchar, -1, NULL, 0, NULL, 0);
	WideCharToMultiByte(CP_ACP, 0, szwchar, -1, szchar, num, NULL, 0);
}

VOID::AsciitoUniCodeString(CHAR* szchar, WCHAR* szwchar)//字符转换函数
{
	DWORD num = MultiByteToWideChar(CP_ACP, 0, szchar, -1, 0, 0);
	MultiByteToWideChar(CP_ACP, 0, szchar, -1, szwchar, num);
}

BOOL _CFunction::DeviceDosPathToNtPath(wchar_t* pszDosPath, wchar_t* pszNtPath)
{
	static TCHAR    szDriveStr[MAX_PATH] = { 0 };
	static TCHAR    szDevName[MAX_PATH] = { 0 };
	TCHAR            szDrive[3];
	INT             cchDevName;
	INT             i;

	//检查参数  
	if (IsBadReadPtr(pszDosPath, 1) != 0)return FALSE;
	if (IsBadWritePtr(pszNtPath, 1) != 0)return FALSE;

	//获取本地磁盘字符串  
	ZeroMemory(szDriveStr, ARRAYSIZE(szDriveStr));
	ZeroMemory(szDevName, ARRAYSIZE(szDevName));
	if (GetLogicalDriveStrings(sizeof(szDriveStr), szDriveStr))
	{
		for (i = 0; szDriveStr[i]; i += 4)
		{
			if (!lstrcmpi(&(szDriveStr[i]), _T("A:\\")) || !lstrcmpi(&(szDriveStr[i]), _T("B:\\")))
				continue;

			szDrive[0] = szDriveStr[i];
			szDrive[1] = szDriveStr[i + 1];
			szDrive[2] = '\0';
			if (!QueryDosDevice(szDrive, szDevName, MAX_PATH))//查询 Dos 设备名  
				return FALSE;

			cchDevName = lstrlen(szDevName);
			if (_tcsnicmp(pszDosPath, szDevName, cchDevName) == 0)//命中  
			{
				lstrcpy(pszNtPath, szDrive);//复制驱动器  
				lstrcat(pszNtPath, pszDosPath + cchDevName);//复制路径  

				return TRUE;
			}
		}
	}
	lstrcpy(pszNtPath, pszDosPath);
	return FALSE;
}

CString _CFunction::PathTransForm(WCHAR* Path)
{
	if (Path[0] == L'\0' && Path[1] == L'\0')
	{
		return L"";
	}

	WCHAR szBuf[MAX_PATH] = { 0 };
	swprintf(szBuf, L"%ws", Path);

	const WCHAR* pDststr1 = { L"\\SystemRoot\\System32" };
	const WCHAR* pDststr2 = { L"\\??\\" };

	WCHAR SystemRootName[MAX_PATH] = { 0 };
	GetSystemDirectoryW(SystemRootName, sizeof(SystemRootName));		//获取系统SystemRooot目录

	for (int i = 0; i < wcslen(Path) - wcslen(pDststr1); i++)
	{

		if (_wcsnicmp(pDststr1, &Path[i], wcslen(pDststr1)) == 0)
		{
			swprintf(szBuf, L"%ws%ws", SystemRootName, &Path[i + wcslen(pDststr1)]);
			break;
		}
		else if (_wcsnicmp(pDststr2, &Path[i], wcslen(pDststr2)) == 0)
		{
			swprintf(szBuf, L"%ws", &Path[i + wcslen(pDststr2)]);
			break;
		}
	}
	return szBuf;
}

bool _CFunction::GetFileDescription(const CString& szModuleName, CString& RetStr)
{
	return this->FsQueryValue(L"FileDescription", szModuleName, RetStr);
}

bool _CFunction::GetFileVersion(const CString& szModuleName, CString& RetStr)
{
	return this->FsQueryValue(L"FileVersion", szModuleName, RetStr);
}

bool _CFunction::GetInternalName(const CString& szModuleName, CString& RetStr)
{
	return this->FsQueryValue(L"InternalName", szModuleName, RetStr);
}

bool _CFunction::GetCompanyName(const CString& szModuleName, CString& RetStr)
{
	return this->FsQueryValue(L"CompanyName", szModuleName, RetStr);
}

bool _CFunction::GetLegalCopyright(const CString& szModuleName, CString& RetStr)
{
	return this->FsQueryValue(L"LegalCopyright", szModuleName, RetStr);
}

bool _CFunction::GetOriginalFilename(const CString& szModuleName, CString& RetStr)
{
	return this->FsQueryValue(L"OriginalFilename", szModuleName, RetStr);
}

bool _CFunction::GetProductName(const CString& szModuleName, CString& RetStr)
{
	return this->FsQueryValue(L"ProductName", szModuleName, RetStr);
}

bool _CFunction::GetProductVersion(const CString& szModuleName, CString& RetStr)
{
	return this->FsQueryValue(L"ProductVersion", szModuleName, RetStr);
}

bool _CFunction::FsQueryValue(const CString& wsValueName, const CString& wsModuleName, CString& wsRetStr)
{
	bool bSuccess = FALSE;
	BYTE* lpVersionData = NULL;
	DWORD  dwLangCharset = 0;
	TCHAR* pStr = NULL;

	do
	{
		if (wsValueName.IsEmpty() || wsModuleName.IsEmpty())
			break;

		DWORD dwHandle;
		// 判断系统能否检索到指定文件的版本信息
		DWORD dwDataSize = ::GetFileVersionInfoSize((LPCWSTR)wsModuleName.GetString(), &dwHandle);
		if (dwDataSize == 0)
			break;

		lpVersionData = new (std::nothrow) BYTE[dwDataSize];// 分配缓冲区
		if (NULL == lpVersionData)
			break;

		// 检索信息
		if (!::GetFileVersionInfo((LPCWSTR)wsModuleName.GetString(), dwHandle, dwDataSize, (void*)lpVersionData))
			break;

		UINT nQuerySize;
		DWORD* pTransTable;
		// 设置语言
		if (!::VerQueryValue(lpVersionData, L"\\VarFileInfo\\Translation", (void**)&pTransTable, &nQuerySize))
			break;

		dwLangCharset = MAKELONG(HIWORD(pTransTable[0]), LOWORD(pTransTable[0]));
		if (lpVersionData == NULL)
			break;

		pStr = new (std::nothrow) TCHAR[128];// 分配缓冲区
		if (NULL == pStr)
			break;

		wsprintf(pStr, L"\\StringFileInfo\\%08lx\\%s", dwLangCharset, wsValueName.GetString());

		LPVOID lpData;

		// 调用此函数查询前需要先依次调用函数GetFileVersionInfoSize和GetFileVersionInfo
		if (::VerQueryValue((void*)lpVersionData, pStr, &lpData, &nQuerySize))
			wsRetStr = (TCHAR*)lpData;

		bSuccess = TRUE;

	} while (FALSE);

	// 销毁缓冲区
	if (lpVersionData)
	{
		delete[] lpVersionData;
		lpVersionData = NULL;
	}
	if (pStr)
	{
		delete[] pStr;
		pStr = NULL;
	}

	return bSuccess;
}

CHAR _CFunction::CopyBufferToClipboard(CListCtrl* m_CListCtrl, SIZE_T ItemTextIndex)
{
	POSITION pos = m_CListCtrl->GetFirstSelectedItemPosition() - 1;//获取选中行的行数  pos = 行数 - 1
	CString Data = m_CListCtrl->GetItemText((int)pos, ItemTextIndex);//获取信息 参数一 : 行数 参数二 :列数

	if (!::OpenClipboard(NULL))//打开粘贴板
	{
		return FALSE;
	}

	::EmptyClipboard();//获取粘贴板使用权限
	int len = Data.GetLength();//获取数据长度
	int size = (len + 1) * 2;//+'\0' 宽字节*2
	HGLOBAL clipbuffer = GlobalAlloc(GMEM_DDESHARE, size);//从堆分配全局内存
	if (!clipbuffer)
	{
		::CloseClipboard();
		return FALSE;
	}
	char* buffer = (char*)::GlobalLock(clipbuffer);//锁定堆内存 (缓冲区)
	memcpy_s(buffer, size, Data.GetBuffer(), size);//拷贝数据到指定缓冲区中
	Data.ReleaseBuffer();//释放内存
	::GlobalUnlock(clipbuffer);//解锁堆内存 (缓冲区)
	::SetClipboardData(CF_UNICODETEXT, clipbuffer);//指定的数据格式(UNICODE_TEXT)存放到粘贴板上
	::CloseClipboard();//关闭粘贴板
	return TRUE;
}

// 右键菜单 helper：两级 "复制 ▸ 各列 / 刷新"
int _CFunction::ShowListCopyRefreshMenu(const int* cols, const wchar_t* const* colNames, int n, bool hasSelection, CWnd* owner)
{
	if (n < 0) n = 0;
	const UINT kRefresh = 9000;
	const UINT kCopyBase = 9001; // 9001..9000+n

	CMenu copySub;
	copySub.CreatePopupMenu();
	for (int i = 0; i < n; ++i)
	{
		copySub.AppendMenuW(MF_STRING | (hasSelection ? 0 : MF_GRAYED), kCopyBase + i, colNames[i] ? colNames[i] : L"");
	}

	CMenu menu;
	menu.CreatePopupMenu();
	if (n > 0)
	{
		menu.AppendMenuW(MF_POPUP | (hasSelection ? 0 : MF_GRAYED), (UINT_PTR)copySub.GetSafeHmenu(), L"复制");
	}
	menu.AppendMenuW(MF_STRING, kRefresh, L"刷新");

	POINT pt = { 0 };
	GetCursorPos(&pt);
	UINT cmd = menu.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, owner);
	copySub.Detach(); // 所有权已交给 menu，避免析构时双重销毁

	if (cmd == 0) return -1;
	if (cmd == kRefresh) return 0;
	if (cmd >= kCopyBase && cmd < kCopyBase + (UINT)n)
	{
		(void)cols; // 调用方拿到 1..n 后自己映射列；保留 cols 形参以便将来需要
		return (int)(cmd - kCopyBase + 1);
	}
	return -1;
}

// 右键菜单 helper（自适应版）：自动从 list 的 header 读列标题，
// 弹出 "复制 ▸ <列1>/<列2>/... / 刷新" 菜单。
// 返回值: -1=未点击, 0=刷新, 否则 = 选中要复制的列号(0-based)+1。
int _CFunction::ShowListContextMenu(CListCtrl* list, CWnd* owner)
{
	if (!list || !list->GetSafeHwnd()) return -1;
	CHeaderCtrl* hdr = list->GetHeaderCtrl();
	int nCols = hdr ? hdr->GetItemCount() : 0;
	std::vector<CString> titles;
	titles.reserve(nCols);
	std::vector<const wchar_t*> ptrs;
	ptrs.reserve(nCols);
	for (int i = 0; i < nCols; ++i)
	{
		wchar_t buf[128] = { 0 };
		HDITEMW hi = { 0 };
		hi.mask = HDI_TEXT;
		hi.pszText = buf;
		hi.cchTextMax = _countof(buf);
		Header_GetItem(hdr->GetSafeHwnd(), i, &hi);
		titles.emplace_back(buf);
	}
	for (auto& s : titles) ptrs.push_back(s.GetString());
	bool hasSel = (list->GetFirstSelectedItemPosition() != NULL);
	return ShowListCopyRefreshMenu(nullptr, ptrs.data(), nCols, hasSel, owner);
}

LONG _CFunction::GetSoftSign(TCHAR* v_pszFilePath, TCHAR* v_pszSign, int v_iBufSize)
{
	//首先判断参数是否正确
	if (v_pszFilePath == NULL) return -1;

	HCERTSTORE		  hStore = NULL;
	HCRYPTMSG		  hMsg = NULL;
	PCCERT_CONTEXT    pCertContext = NULL;
	BOOL			  bResult;
	DWORD dwEncoding, dwContentType, dwFormatType;
	PCMSG_SIGNER_INFO pSignerInfo = NULL;
	PCMSG_SIGNER_INFO pCounterSignerInfo = NULL;
	DWORD			  dwSignerInfo;
	CERT_INFO		  CertInfo;
	SYSTEMTIME        st;
	LONG              lRet;
	DWORD             dwDataSize = 0;

	char   chTemp[MAX_PATH] = { 0 };

	do
	{
		//从签名文件中获取存储句柄
		bResult = CryptQueryObject(
			CERT_QUERY_OBJECT_FILE,
			v_pszFilePath,
			CERT_QUERY_CONTENT_FLAG_ALL,
			CERT_QUERY_FORMAT_FLAG_ALL,
			0,
			&dwEncoding,
			&dwContentType,
			&dwFormatType,
			&hStore,
			&hMsg,
			NULL
		);



		if (!bResult)
		{
			int j = GetLastError();

			lRet = -1;
			break;
		}


		//获取签名信息所需的缓冲区大小
		bResult = CryptMsgGetParam(
			hMsg,
			CMSG_SIGNER_INFO_PARAM,
			0,
			NULL,
			&dwSignerInfo
		);
		if (!bResult)
		{
			lRet = -1;
			break;
		}

		//分配缓冲区
		pSignerInfo = (PCMSG_SIGNER_INFO)LocalAlloc(LPTR, dwSignerInfo);
		if (pSignerInfo == NULL)
		{
			lRet = -1;
			break;
		}


		//获取签名信息
		bResult = CryptMsgGetParam(
			hMsg,
			CMSG_SIGNER_INFO_PARAM,
			0,
			pSignerInfo,
			&dwSignerInfo
		);
		if (!bResult)
		{
			lRet = -1;
			break;
		}

		CertInfo.Issuer = pSignerInfo->Issuer;
		CertInfo.SerialNumber = pSignerInfo->SerialNumber;

		pCertContext = CertFindCertificateInStore(
			hStore,
			X509_ASN_ENCODING,
			0,
			CERT_FIND_SUBJECT_CERT,
			(PVOID)&CertInfo,
			NULL
		);
		if (pCertContext == NULL)
		{
			lRet = -1;
			break;
		}


		//获取数字键名
		//没有给定缓冲区，那么说明只要获取下需要的长度
		if (v_pszSign == NULL)
		{
			dwDataSize = CertGetNameString(
				pCertContext,
				CERT_NAME_SIMPLE_DISPLAY_TYPE,
				0,
				NULL,
				NULL,
				0
			);
			if (dwDataSize != 0)
			{
				lRet = dwDataSize;
			}
			else
			{
				lRet = -1;
			}

			break;
		}

		if (!(CertGetNameString(
			pCertContext,
			CERT_NAME_SIMPLE_DISPLAY_TYPE,
			0,
			NULL,
			v_pszSign,
			v_iBufSize
		)
			)
			)
		{

			lRet = -1;
			break;
		}

		lRet = 0;

	} while (FALSE);

	if (pSignerInfo != NULL)
	{
		LocalFree((HLOCAL)pSignerInfo);
	}

	return lRet;
}
