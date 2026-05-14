#pragma once

#include "Pdb.h"

namespace Pdb
{



	class DownloaderInterface
	{
	public:
		virtual ~DownloaderInterface() = default;
		virtual bool valid() const noexcept = 0;
		virtual bool download(const wchar_t* url) = 0;
	};

	class WinInetAbstractDownloader : public DownloaderInterface
	{
	protected:
		enum class Action
		{
			cancel,
			proceed
		};

	protected:
		virtual void onStart(const wchar_t* url, size_t contentLength) = 0;
		virtual Action onReceive(const void* buf, size_t size) = 0;
		virtual void onFinish() = 0;
		virtual void onError(unsigned int httpCode) = 0;
		virtual void onCancel() = 0;

	public:
		virtual bool download(const wchar_t* url) noexcept override;
	};

	class WinInetFileDownloader : public WinInetAbstractDownloader
	{
	private:
		HANDLE m_hFile;

	protected:
		static HANDLE createFileWithHierarchy(const wchar_t* filePath, unsigned long access, unsigned long share) noexcept;

	protected:
		virtual void onStart(const wchar_t* url, size_t contentLength) override;
		virtual Action onReceive(const void* buf, size_t size);
		virtual void onFinish() override;
		virtual void onError(unsigned int httpCode) override;
		virtual void onCancel() override;

	protected:
		void closeFile() noexcept;

	public:
		WinInetFileDownloader();
		WinInetFileDownloader(const wchar_t* filePath) noexcept;
		~WinInetFileDownloader() noexcept;
		VOID SetFilePath(const wchar_t* filePath);
		virtual bool valid() const noexcept override;
	};

	struct SymLoader
	{
		static bool download(const wchar_t* url, DownloaderInterface& downloader);
	};



} // namespace Pdb

// === PDB 下载进度全局回调（PdbResolver 安装）。phase: 0=Start, 1=Receive, 2=Finish, 3=Error。
typedef void (*PdbProgressCallback)(const wchar_t* fileName, int phase,
	unsigned long long received, unsigned long long total, unsigned int httpCode);
void SetPdbProgressSink(PdbProgressCallback cb);
void EmitPdbProgress(const wchar_t* fileName, int phase,
	unsigned long long received, unsigned long long total, unsigned int httpCode);

// === PDB 诊断日志回调（PdbResolver 装钩；InitPDB 各阶段把详情写到这里）
typedef void (*PdbDiagCallback)(const wchar_t* msg);
void SetPdbDiagSink(PdbDiagCallback cb);
void EmitPdbDiag(const wchar_t* fmt, ...);

class SymDownloader : public Pdb::WinInetFileDownloader
{
private:
	using Super = Pdb::WinInetFileDownloader;

private:
	size_t m_totalSize{ 0 };
	size_t m_downloaded{ 0 };
	std::wstring m_displayName;  // 用于进度回调的展示名（PDB 文件名）

public:
	void SetDisplayName(const wchar_t* name) { m_displayName = name ? name : L""; }
	const wchar_t* GetDisplayName() const { return m_displayName.c_str(); }

private:
	static std::pair<float, const char*> formatSize(size_t size) noexcept
	{
		const char* sizeSuffix = nullptr;
		float formattedSize = 0.0f;
		if (size > 1048576)
		{
			sizeSuffix = "Mb";
			formattedSize = static_cast<float>(size) / 1048576;
		}
		else if (size > 1024)
		{
			sizeSuffix = "Kb";
			formattedSize = static_cast<float>(size) / 1024;
		}
		else
		{
			sizeSuffix = "Bytes";
			formattedSize = static_cast<float>(size);
		}

		return std::make_pair(formattedSize, sizeSuffix);
	}

protected:

	virtual void onError(const unsigned int httpCode) override
	{
		Super::onError(httpCode);
		printf("HTTP Error: %u\n", httpCode);
		EmitPdbProgress(m_displayName.c_str(), 3, m_downloaded, m_totalSize, httpCode);
	}

	virtual void onStart(const wchar_t* const url, const size_t fileSize) noexcept override
	{
		const auto formattedSize = formatSize(fileSize);

		printf("Downloading:\n  * '%ws'\n  * %.2f %s\n", url, formattedSize.first, formattedSize.second);
		m_totalSize = fileSize;
		m_downloaded = 0;
		EmitPdbProgress(m_displayName.c_str(), 0, 0, (unsigned long long)fileSize, 0);
	}

	virtual Super::Action onReceive(const void* buf, const size_t size) override
	{
		const auto action = Super::onReceive(buf, size);
		if (action == Super::Action::cancel)
		{
			printf("Cancelled\n");
			return action;
		}

		m_downloaded += size;

		const auto formattedDownloaded = formatSize(m_downloaded);
		const auto formattedTotal = formatSize(m_totalSize);

		printf("Downloaded %u%% (%.2f %s from %.2f %s)\n",
			static_cast<unsigned int>(m_downloaded * 100 / (m_totalSize ? m_totalSize : 1)),
			formattedDownloaded.first, formattedDownloaded.second,
			formattedTotal.first, formattedTotal.second
		);
		EmitPdbProgress(m_displayName.c_str(), 1,
			(unsigned long long)m_downloaded, (unsigned long long)m_totalSize, 0);

		return Super::Action::proceed;
	}

	virtual void onFinish() override
	{
		Super::onFinish();
		EmitPdbProgress(m_displayName.c_str(), 2,
			(unsigned long long)m_downloaded, (unsigned long long)m_totalSize, 0);
	}

public:
	using Super::Super;
};

class MyPdb
{
public:
	bool InitPDB(PWSTR path);
	Pdb::Sym Find(PWSTR Name);
	ULONG64 GetFunAddrInfo(PWSTR Name, PSYMBOL_INFOW* pInfo = nullptr);
	ULONG64 LoadEx(PWSTR exPath);
	ULONG64 GetInfo(PIMAGEHLP_MODULE64 pMoudleInfo);
	ULONG64 GetMemberOffset(PWSTR structName, PWSTR memberName);
	ULONG64 GetStructSize(PWSTR structName);
	ULONG64 GetGlobalVariablesOffset(PWSTR VarName);
	// 根据 PDB 加载后的虚拟地址查符号名；inAddr = m_mod.base() + RVA。
	// 成功返回 true，outName 充填符号名（不含偏移），outDisp 为从符号起点的偏移。
	bool GetSymbolByAddr(ULONG64 inAddr, PWSTR outName, ULONG outNameCch, PULONG64 outDisp);

public:
	std::wstring m_path;
	Pdb::Prov m_prov;
	Pdb::Mod m_mod;
	SymDownloader m_loader;
};