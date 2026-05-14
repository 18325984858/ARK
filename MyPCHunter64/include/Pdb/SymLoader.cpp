#include "SymLoader.h"

#include <wininet.h>

#include <string>
#include <vector>

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

bool MyPdb::InitPDB(PWSTR path)
{
	if (!path)
		return false;

	m_path = path;

	const auto pdbInfo = m_prov.getPdbInfo(m_path.c_str());

	const auto url = std::wstring(Pdb::Prov::k_microsoftSymbolServerSecure) + L"/" + pdbInfo.pdbUrl();
	const std::wstring symFolder = L"C:\\Symbols\\";
	const std::wstring symFolderPath = symFolder + pdbInfo.pdbPath();

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

		const auto& want = pdbInfo.info();
		if (localInfo.age != want.age)
		{
			return false;
		}
		// GUID 完全匹配才能确认是同一版本
		return memcmp(&localInfo.guid, &want.guid, sizeof(GUID)) == 0;
	};

	bool downloadStatus = true;
	if (isLocalPdbUpToDate())
	{
		// 已有匹配的 PDB，跳过下载
	}
	else
	{
		m_loader.SetFilePath(symFolderPath.c_str());
		downloadStatus = Pdb::SymLoader::download(url.c_str(), m_loader);
	}

	if (!downloadStatus)
	{
		printf("Unable to download the symbols");
		return false;
	}

	m_prov.setSymPath(symFolder.c_str());

	return m_mod.init(m_path.c_str());
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
