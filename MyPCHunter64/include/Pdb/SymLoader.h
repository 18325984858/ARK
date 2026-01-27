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

class SymDownloader : public Pdb::WinInetFileDownloader
{
private:
	using Super = Pdb::WinInetFileDownloader;

private:
	size_t m_totalSize{ 0 };
	size_t m_downloaded{ 0 };

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
	}

	virtual void onStart(const wchar_t* const url, const size_t fileSize) noexcept override
	{
		const auto formattedSize = formatSize(fileSize);

		printf("Downloading:\n  * '%ws'\n  * %.2f %s\n", url, formattedSize.first, formattedSize.second);
		m_totalSize = fileSize;
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
			static_cast<unsigned int>(m_downloaded * 100 / m_totalSize),
			formattedDownloaded.first, formattedDownloaded.second,
			formattedTotal.first, formattedTotal.second
		);

		return Super::Action::proceed;
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

public:
	std::wstring m_path;
	Pdb::Prov m_prov;
	Pdb::Mod m_mod;
	SymDownloader m_loader;
};