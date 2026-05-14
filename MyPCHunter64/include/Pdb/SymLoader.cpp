#include "SymLoader.h"

#include <wininet.h>

#include <string>
#include <vector>
#include <atomic>

#pragma comment(lib, "wininet.lib")

namespace Scoped
{

	class Inet
	{
	private:
		HINTERNET m_hInet;

	public:
		Inet() : m_hInet(nullptr)
		{
		}

		explicit Inet(const HINTERNET hInet) : m_hInet(hInet)
		{
		}

		Inet(const Inet&) = delete;
		Inet(Inet&&) = delete;
		Inet& operator = (const Inet&) = delete;
		Inet& operator = (Inet&&) = delete;

		~Inet()
		{
			close();
		}

		void close() noexcept
		{
			if (m_hInet != nullptr)
			{
				InternetCloseHandle(std::exchange(m_hInet, nullptr));
			}
		}

		operator HINTERNET() const noexcept
		{
			return m_hInet;
		}
	};

} // namespace Scoped


namespace Pdb
{

	bool WinInetAbstractDownloader::download(const wchar_t* const url) noexcept
	{
		const Scoped::Inet hInet(InternetOpenW(L"HttpDownloader", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0));
		if (!hInet)
		{
			return false;
		}

		const Scoped::Inet hUrl(InternetOpenUrlW(hInet, url, nullptr, 0, INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RESYNCHRONIZE, 0));
		if (!hUrl)
		{
			return false;
		}

		const auto queryHttpDword = [](const HINTERNET hUrl, const unsigned long info) -> std::pair<unsigned long, bool>
			{
				unsigned long result = 0;
				unsigned long sizeOfResult = sizeof(result);
				unsigned long index = 0;
				const bool status = !!HttpQueryInfoW(hUrl, info | HTTP_QUERY_FLAG_NUMBER, &result, &sizeOfResult, &index);
				return std::make_pair(result, status);
			};

		const auto httpCode = queryHttpDword(hUrl, HTTP_QUERY_STATUS_CODE);
		if (httpCode.second && (httpCode.first > 400))
		{
			onError(httpCode.first);
			return false;
		}

		const auto contentLength = queryHttpDword(hUrl, HTTP_QUERY_CONTENT_LENGTH);
		onStart(url, contentLength.second ? contentLength.first : 0);

		constexpr unsigned int k_chunkSize = 32768u;

		std::vector<unsigned char> buf((contentLength.first && (contentLength.first < k_chunkSize)) ? contentLength.first : k_chunkSize);
		void* const bufPtr = buf.data();
		unsigned long readBytes = 0;
		while (InternetReadFile(hUrl, bufPtr, static_cast<unsigned long>(buf.size()), &readBytes) && readBytes)
		{
			const Action action = onReceive(bufPtr, readBytes);
			if (action == Action::cancel)
			{
				onCancel();
				return false;
			}
		}

		onFinish();
		return true;
	}


	HANDLE WinInetFileDownloader::createFileWithHierarchy(const wchar_t* filePath, const unsigned long access, const unsigned long share) noexcept
	{
		if (!filePath)
		{
			return INVALID_HANDLE_VALUE;
		}

		std::wstring path(filePath);
		if (path.empty())
		{
			return INVALID_HANDLE_VALUE;
		}

		wchar_t* const pathBuf = &path[0];

		size_t lastSkippedSlash = 0;

		const auto isSlash = [](const wchar_t sym) -> bool
			{
				return (sym == L'\\') || (sym == L'/');
			};

		if (path.size() >= 3)
		{
			if ((pathBuf[1] == L':') && isSlash(pathBuf[2]))
			{
				// "X:\..."
				//    ^
				lastSkippedSlash = 2;
			}
			else if (path.size() >= 4)
			{
				if ((*reinterpret_cast<const unsigned int*>(pathBuf) == '\\.\\\\') || (*reinterpret_cast<const unsigned int*>(pathBuf) == '\\??\\'))
				{
					for (size_t i = 4; i < path.size(); ++i)
					{
						if (isSlash(pathBuf[i]))
						{
							// "\\.\Root\..."
							// "\??\Root\..."
							//          ^
							lastSkippedSlash = i;
							break;
						}
					}
				}
			}
		}

		std::vector<size_t> createdDirs;
		createdDirs.reserve(10);

		const auto discardChanges = [](const std::vector<size_t> dirs, wchar_t* const mutablePath)
			{
				for (auto it = dirs.crbegin(); it != dirs.crend(); ++it)
				{
					const size_t slashPos = *it;
					const wchar_t backupDelim = mutablePath[slashPos];
					RemoveDirectoryW(mutablePath);
					mutablePath[slashPos] = backupDelim;
				}
			};

		size_t symPos = 0;
		for (wchar_t& sym : path)
		{
			if (symPos <= lastSkippedSlash)
			{
				++symPos;
				continue;
			}

			if (isSlash(sym))
			{
				const wchar_t delim = sym;
				sym = L'\0';
				const bool status = !!CreateDirectoryW(pathBuf, nullptr);
				if (status)
				{
					createdDirs.emplace_back(symPos);
				}
				else
				{
					const auto lastError = GetLastError();
					if (lastError != ERROR_ALREADY_EXISTS)
					{
						// Discard created directories:
						sym = delim;
						discardChanges(createdDirs, pathBuf);
						return INVALID_HANDLE_VALUE;
					}
				}
				sym = delim;
			}

			++symPos;
		}

		const HANDLE hFile = CreateFileW(filePath, access, share, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			// Discard created directories:
			discardChanges(createdDirs, pathBuf);
			return INVALID_HANDLE_VALUE;
		}

		return hFile;
	}

	void WinInetFileDownloader::onStart(const wchar_t* /*url*/, const size_t /*contentLength*/)
	{
	}

	WinInetAbstractDownloader::Action WinInetFileDownloader::onReceive(const void* buf, size_t size)
	{
		unsigned long written = 0;
		const bool status = !!WriteFile(m_hFile, buf, static_cast<unsigned int>(size), &written, nullptr);
		if (!status)
		{
			return Action::cancel;
		}
		return Action::proceed;
	}

	void WinInetFileDownloader::onFinish()
	{
		closeFile();
	}

	void WinInetFileDownloader::onError(const unsigned int /*httpCode*/)
	{
		closeFile();
	}

	void WinInetFileDownloader::onCancel()
	{
		closeFile();
	}


	void WinInetFileDownloader::closeFile() noexcept
	{
		if (valid())
		{
			CloseHandle(std::exchange(m_hFile, INVALID_HANDLE_VALUE));
		}
	}


	WinInetFileDownloader::WinInetFileDownloader()
	{
	}

	WinInetFileDownloader::WinInetFileDownloader(const wchar_t* filePath) noexcept : m_hFile(createFileWithHierarchy(filePath, GENERIC_WRITE, 0))
	{
	}

	WinInetFileDownloader::~WinInetFileDownloader() noexcept
	{
	}

	VOID WinInetFileDownloader::SetFilePath(const wchar_t* filePath)
	{
		m_hFile = createFileWithHierarchy(filePath, GENERIC_WRITE, 0);
	}

	bool WinInetFileDownloader::valid() const noexcept
	{
		return m_hFile != INVALID_HANDLE_VALUE;
	}



	bool SymLoader::download(const wchar_t* url, DownloaderInterface& downloader)
	{
		return downloader.valid() && downloader.download(url);
	}

} // namespace Pdb

// === PDB 下载进度全局回调 ===
static PdbProgressCallback g_PdbProgressSink = nullptr;

void SetPdbProgressSink(PdbProgressCallback cb)
{
	g_PdbProgressSink = cb;
}

void EmitPdbProgress(const wchar_t* fileName, int phase,
	unsigned long long received, unsigned long long total, unsigned int httpCode)
{
	PdbProgressCallback cb = g_PdbProgressSink;
	if (cb) cb(fileName ? fileName : L"", phase, received, total, httpCode);
}

static PdbDiagCallback g_PdbDiagSink = nullptr;

void SetPdbDiagSink(PdbDiagCallback cb)
{
	g_PdbDiagSink = cb;
}

void EmitPdbDiag(const wchar_t* fmt, ...)
{
	PdbDiagCallback cb = g_PdbDiagSink;
	if (!cb || !fmt) return;
	wchar_t buf[1024];
	va_list ap;
	va_start(ap, fmt);
	_vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
	va_end(ap);
	cb(buf);
}

bool MyPdb::InitPDB(PWSTR path)
{
	if (!path)
		return false;

	m_path = path;

	EmitPdbDiag(L"[InitPDB] BEGIN path='%s'", path);

	// 文件存在 + 大小？
	{
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		if (GetFileAttributesExW(path, GetFileExInfoStandard, &fad))
		{
			EmitPdbDiag(L"[InitPDB] target image size=%llu",
				((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow);
		}
		else
		{
			EmitPdbDiag(L"[InitPDB] target image not found (GLE=%lu)", GetLastError());
			return false;
		}
	}

	std::wstring pdbPathRel;
	std::wstring url;
	std::wstring symFolder = L"C:\\Symbols\\";
	std::wstring symFolderPath;
	GUID    wantGuid{};
	DWORD   wantAge = 0;
	try
	{
		const auto pdbInfo = m_prov.getPdbInfo(m_path.c_str());
		pdbPathRel = pdbInfo.pdbPath();
		url = std::wstring(Pdb::Prov::k_microsoftSymbolServerSecure) + L"/" + pdbInfo.pdbUrl();
		symFolderPath = symFolder + pdbPathRel;
		wantGuid = pdbInfo.info().guid;
		wantAge  = pdbInfo.info().age;
	}
	catch (...)
	{
		EmitPdbDiag(L"[InitPDB] getPdbInfo() threw -- image has no debug directory or invalid PDB record");
		return false;
	}

	EmitPdbDiag(L"[InitPDB] pdbPath='%s'", pdbPathRel.c_str());
	EmitPdbDiag(L"[InitPDB] url='%s'",     url.c_str());
	EmitPdbDiag(L"[InitPDB] local='%s'",   symFolderPath.c_str());

	// 命中缓存判断：MS 符号服务器的目录布局是 <PdbName>\<GUID><Age>\<PdbName>，
	// 路径本身就唯一标识版本。再用 SymSrvGetFileIndexInfoW 复核本地 PDB 的
	// GUID/Age 是否与目标模块匹配，防止半下载或被替换的脏文件。
	auto isLocalPdbUpToDate = [&]() -> bool
	{
		const DWORD attr = GetFileAttributesW(symFolderPath.c_str());
		if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
		{
			return false;
		}

		// 至少要有内容，0 字节文件视为残留
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		if (!GetFileAttributesExW(symFolderPath.c_str(), GetFileExInfoStandard, &fad))
		{
			return false;
		}
		if (fad.nFileSizeHigh == 0 && fad.nFileSizeLow == 0)
		{
			return false;
		}

		SYMSRV_INDEX_INFOW localInfo{};
		localInfo.sizeofstruct = sizeof(localInfo);
		if (!SymSrvGetFileIndexInfoW(symFolderPath.c_str(), &localInfo, 0))
		{
			return false;
		}

		if (localInfo.age != wantAge)
		{
			return false;
		}
		// GUID 完全匹配才能确认是同一版本
		return memcmp(&localInfo.guid, &wantGuid, sizeof(GUID)) == 0;
	};

	bool downloadStatus = true;
	if (isLocalPdbUpToDate())
	{
		EmitPdbDiag(L"[InitPDB] local cache HIT (skip download)");
	}
	else
	{
		EmitPdbDiag(L"[InitPDB] local cache MISS - downloading from %s", url.c_str());
		m_loader.SetFilePath(symFolderPath.c_str());
		const wchar_t* slash = wcsrchr(pdbPathRel.c_str(), L'\\');
		m_loader.SetDisplayName(slash ? slash + 1 : pdbPathRel.c_str());
		downloadStatus = Pdb::SymLoader::download(url.c_str(), m_loader);
		EmitPdbDiag(L"[InitPDB] download finished status=%d GLE=%lu",
			downloadStatus ? 1 : 0, GetLastError());
	}

	if (!downloadStatus)
	{
		EmitPdbDiag(L"[InitPDB] FAIL - download unsuccessful");
		printf("Unable to download the symbols");
		return false;
	}

	m_prov.setSymPath(symFolder.c_str());
	EmitPdbDiag(L"[InitPDB] calling SymLoadModuleExW for image '%s'", m_path.c_str());

	bool initOk = m_mod.init(m_path.c_str());
	EmitPdbDiag(L"[InitPDB] m_mod.init -> %s base=0x%I64X GLE=%lu",
		initOk ? L"OK" : L"FAIL", (ULONG64)m_mod.base(), GetLastError());
	return initOk;
}

Pdb::Sym MyPdb::Find(PWSTR Name)
{
	return m_mod.find(Name);
}

ULONG64 MyPdb::GetFunAddrInfo(PWSTR Name, PSYMBOL_INFOW* pInfo)
{
	return m_mod.findfun(Name, pInfo);
}

ULONG64 MyPdb::LoadEx(PWSTR exPath)
{
	if (!exPath)
	{
		return false;
	}
	m_path = exPath;

	WCHAR szBuf[MAXBYTE] = { 0 };
	wcscpy(szBuf, exPath);

	WCHAR* last_backslash = wcsrchr(szBuf, '\\');
	last_backslash != NULL ? (*last_backslash = L'\0') : NULL;

	m_prov.setSymPath(szBuf);
	return m_mod.init(m_path.c_str());
}

ULONG64 MyPdb::GetInfo(PIMAGEHLP_MODULE64 pMoudleInfo)
{
	if (!pMoudleInfo)
	{
		return false;
	}

	m_mod.GetModuleInfo(pMoudleInfo);

	return true;
}

ULONG64 MyPdb::GetMemberOffset(PWSTR structName, PWSTR memberName)
{
	if (!structName && !memberName)
	{
		return -1;
	}

	return m_mod.GetMemberOffset(structName, memberName);
}

ULONG64 MyPdb::GetStructSize(PWSTR structName)
{
	return m_mod.find(structName).size();
}

ULONG64 MyPdb::GetGlobalVariablesOffset(PWSTR VarName)
{
	if (!VarName)
	{
		return NULL;
	}

	return m_mod.findGlobalVariables(VarName);
}

bool MyPdb::GetSymbolByAddr(ULONG64 inAddr, PWSTR outName, ULONG outNameCch, PULONG64 outDisp)
{
	if (!outName || outNameCch == 0) return false;
	outName[0] = 0;
	if (m_mod.base() == 0) return false;

	BYTE buf[sizeof(SYMBOL_INFOW) + (MAX_SYM_NAME + 1) * sizeof(WCHAR)] = { 0 };
	PSYMBOL_INFOW info = (PSYMBOL_INFOW)buf;
	info->SizeOfStruct = sizeof(SYMBOL_INFOW);
	info->MaxNameLen   = MAX_SYM_NAME;

	ULONG64 disp = 0;
	if (!SymFromAddrW(Pdb::Prov::uid(), inAddr, &disp, info))
	{
		return false;
	}

	// dbghelp 的 SymFromAddrW 会返回"最近"符号，可能落在该地址之前的数据符号上
	// (Disp 是 ULONG64，但实际是 Address - SymbolAddress，符号在 Address 之后时变成超大无符号)。
	// 1) 拒绝非函数符号（数据/标签）：只接受 SymTagFunction(5) / SymTagPublicSymbol(10) 中的函数项
	// 2) 拒绝太大的位移（> 1MB 视为无效命中）
	if ((LONG64)disp < 0 || disp > 0x100000)
	{
		return false;
	}
	// SYMBOL_INFOW.Tag: 5=Function, 10=PublicSymbol
	// 公共 PDB(微软符号服务器拉的)绝大部分 entry 是 Tag=10 PublicSymbol，
	// 且常常 *没有* SYMFLAG_FUNCTION 标志（该标志只在含类型信息的私有 PDB 上稳定）。
	// 之前严格要求 SYMFLAG_FUNCTION 导致 ntoskrnl 多数地址被错误丢弃。
	// 现在的策略：
	//   - Tag==5 (Function) 直接收
	//   - Tag==10 (PublicSymbol) 收，但若同时设置了 SYMFLAG_PUBLIC_CODE=0 且明显是 data 则拒
	//     dbghelp 没有显式 "PUBLIC_DATA" 标志；保守起见，全部收下，让上层用 disp 范围筛
	//   - 其它 Tag (7=Data / 8=Annotation / 12=BaseType / ...): 拒
	{
		const auto tag = info->Tag;
		const bool tagOk = (tag == 5 /*SymTagFunction*/) || (tag == 10 /*SymTagPublicSymbol*/);
		const bool flagOk = (info->Flags & SYMFLAG_FUNCTION) != 0;
		if (!tagOk && !flagOk)
		{
			static std::atomic<int> s_rejLog{ 0 };
			if (s_rejLog.fetch_add(1) < 20)
			{
				EmitPdbDiag(L"[Resolve] reject sym tag=%u flags=0x%X disp=0x%I64X name='%s'",
					(unsigned)tag, (unsigned)info->Flags, disp, info->Name);
			}
			return false;
		}
	}
	// 去除 C++ 修饰，只保留裸函数名（例 ?Foo@@YAX... -> Foo）
	if (info->Name[0] == L'?')
	{
		WCHAR undec[256] = { 0 };
		if (UnDecorateSymbolNameW(info->Name, undec, _countof(undec), UNDNAME_NAME_ONLY) > 0)
		{
			wcsncpy_s(outName, outNameCch, undec, _TRUNCATE);
		}
		else
		{
			wcsncpy_s(outName, outNameCch, info->Name, _TRUNCATE);
		}
	}
	else
	{
		wcsncpy_s(outName, outNameCch, info->Name, _TRUNCATE);
	}
	if (outDisp) *outDisp = disp;
	return true;
}
