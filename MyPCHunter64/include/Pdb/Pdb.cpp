#include "Pdb.h"

#define _NO_CVCONST_H

#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <sstream>
#include <iomanip>
#include <vector>
#include <atomic>

namespace Pdb
{
#define DEBUG_PRINFT OutputDebugString

	std::set<std::shared_ptr<PdbFunInfo>, Greater> g_PdbInfo = { 0 };

	const wchar_t* Prov::k_microsoftPriverSymbolServer = L"https://symweb.azurefd.net";
	const wchar_t* Prov::k_microsoftSymbolServer = L"http://msdl.microsoft.com/download/symbols";
	const wchar_t* Prov::k_microsoftSymbolServerSecure = L"https://msdl.microsoft.com/download/symbols";
	const wchar_t* Prov::k_defaultSymPath = L"srv*C:\\Symbols*http://msdl.microsoft.com/download/symbols";
	const uint32_t Prov::k_defaultOptions = SYMOPT_UNDNAME | SYMOPT_DEBUG | SYMOPT_LOAD_ANYTHING;

	size_t Prov::s_initCount = 0;

	InstUid Prov::uid()
	{
		if (!s_initCount)
		{
			throw NotInitialized(__FUNCTIONW__ L": A symbols provider isn't created yet. Create the Pdb::Prov instance before the call.");
		}
		return &s_initCount;
	}




	const wchar_t* PdbInfo::extractFileName(const wchar_t* const path, const size_t length) noexcept
	{
		if (!length)
		{
			return path;
		}

		const wchar_t* name = &path[length - 1];
		while ((name != path) && (*name != L'\\') && (*name != L'/'))
		{
			--name;
		}

		return name;
	}

	PdbInfo PdbInfo::get(const wchar_t* const path) noexcept(false)
	{
		SYMSRV_INDEX_INFOW info{};
		info.sizeofstruct = sizeof(info);
		const bool status = !!SymSrvGetFileIndexInfoW(path, &info, 0);
		if (!status)
		{
			const auto lastError = GetLastError();
			throw DbgHelpFailure(__FUNCTIONW__ L": Unable to get a file index info.", lastError);
		}

		PdbInfo pdbInfo;
		pdbInfo.m_info.timestamp = info.timestamp;
		pdbInfo.m_info.imageFileSize = info.size;
		pdbInfo.m_info.age = info.age;
		pdbInfo.m_info.guid = info.guid;
		wcscpy_s(pdbInfo.m_info.file, info.file);
		wcscpy_s(pdbInfo.m_info.dbgFile, info.dbgfile);
		wcscpy_s(pdbInfo.m_info.pdbFile, info.pdbfile);
		pdbInfo.m_info.stripped = info.stripped;
		pdbInfo.m_type = ((info.sig == info.guid.Data1) && (info.guid.Data2 == 0) && (info.guid.Data3 == 0) && (*reinterpret_cast<const uint64_t*>(info.guid.Data4) == 0))
			? Type::pdb20
			: Type::pdb70;

		return pdbInfo;
	}

	PdbInfo::Type PdbInfo::type() const noexcept
	{
		return m_type;
	}

	const PdbInfo::IndexInfo& PdbInfo::info() const noexcept
	{
		return m_info;
	}

	std::wstring PdbInfo::makeFullPath(const wchar_t delimiter) const
	{
		const size_t pathLength = wcslen(m_info.pdbFile);
		if (!pathLength)
		{
			return {};
		}

		const wchar_t* const pdbPath = m_info.pdbFile;
		const wchar_t* const pdbName = extractFileName(pdbPath, pathLength);
		const unsigned int age = m_info.age;

		switch (m_type)
		{
		case Type::pdb70:
		{
			const auto& guid = m_info.guid;

			std::wstringstream stream;
			stream << std::uppercase << std::hex << std::setfill(L'0')
				<< pdbName
				<< delimiter
				<< std::setw(8) << guid.Data1
				<< std::setw(4) << guid.Data2
				<< std::setw(4) << guid.Data3
				<< std::setw(2) << guid.Data4[0]
				<< std::setw(2) << guid.Data4[1]
				<< std::setw(2) << guid.Data4[2]
				<< std::setw(2) << guid.Data4[3]
				<< std::setw(2) << guid.Data4[4]
				<< std::setw(2) << guid.Data4[5]
				<< std::setw(2) << guid.Data4[6]
				<< std::setw(2) << guid.Data4[7]
				<< std::setw(1) << age
				<< delimiter
				<< pdbPath;

			return stream.str();
		}
		case Type::pdb20:
		{
			const auto sig = m_info.signature;

			std::wstringstream stream;
			stream << std::uppercase << std::hex << std::setfill(L'0')
				<< pdbName
				<< delimiter
				<< std::setw(8) << sig
				<< std::setw(1) << age
				<< delimiter
				<< pdbPath;

			return stream.str();
		}
		default:
		{
			break;
		}
		}

		return {};
	}

	std::wstring PdbInfo::pdbSig() const
	{
		const unsigned int age = m_info.age;

		switch (m_type)
		{
		case Type::pdb70:
		{
			const auto& guid = m_info.guid;

			std::wstringstream stream;
			stream << std::uppercase << std::hex << std::setfill(L'0')
				<< std::setw(8) << guid.Data1
				<< std::setw(4) << guid.Data2
				<< std::setw(4) << guid.Data3
				<< std::setw(2) << guid.Data4[0]
				<< std::setw(2) << guid.Data4[1]
				<< std::setw(2) << guid.Data4[2]
				<< std::setw(2) << guid.Data4[3]
				<< std::setw(2) << guid.Data4[4]
				<< std::setw(2) << guid.Data4[5]
				<< std::setw(2) << guid.Data4[6]
				<< std::setw(2) << guid.Data4[7]
				<< std::setw(1) << age;

			return stream.str();
		}
		case Type::pdb20:
		{
			const auto sig = m_info.signature;

			std::wstringstream stream;
			stream << std::uppercase << std::hex << std::setfill(L'0')
				<< std::setw(8) << sig
				<< std::setw(1) << age;

			return stream.str();
		}
		default:
		{
			break;
		}
		}

		return {};
	}

	std::wstring PdbInfo::pdbPath() const
	{
		return makeFullPath('\\');
	}

	std::wstring PdbInfo::pdbUrl() const
	{
		return makeFullPath('/');
	}



	Prov::Prov() noexcept(false) : Prov(k_defaultSymPath)
	{
	}

	Prov::Prov(const wchar_t* symPath) noexcept(false)
	{
		if (!s_initCount)
		{
			const bool status = !!SymInitializeW(&s_initCount, symPath, false);
			if (!status)
			{
				const auto lastError = GetLastError();
				throw DbgHelpFailure(__FUNCTIONW__ L": Unable to create the Prov instance: 'SymInitializeW' failure.", lastError);
			}

			const auto options = getOptions();
			setOptions(options | k_defaultOptions);
		}
		++s_initCount;
	}

	Prov::~Prov()
	{
		--s_initCount;
		if (!s_initCount)
		{
			SymCleanup(&s_initCount);
		}
	}

	uint32_t Prov::getOptions() const noexcept
	{
		return SymGetOptions();
	}

	void Prov::setOptions(uint32_t options) noexcept
	{
		SymSetOptions(options);
	}

	std::wstring Prov::getSymPath() const noexcept(false)
	{
		constexpr auto k_sizeStep = 384u;
		std::wstring buf(k_sizeStep, L'\0');
		while (true)
		{
			const bool status = !!SymGetSearchPathW(uid(), &buf[0], static_cast<uint32_t>(buf.size()));
			if (status)
			{
				buf.resize(wcslen(buf.c_str()));
				return buf;
			}

			const auto lastError = GetLastError();
			if (lastError != ERROR_INSUFFICIENT_BUFFER)
			{
				throw DbgHelpFailure(__FUNCTIONW__ L": Unable to obtain a symbol path: 'SymGetSearchPathW' failure.", lastError);
			}

			buf.resize(buf.size() + k_sizeStep);
		}
	}

	void Prov::setSymPath(const wchar_t* symPath) noexcept(false)
	{
		const bool status = !!SymSetSearchPathW(uid(), symPath);
		if (!status)
		{
			const auto lastError = GetLastError();
			throw DbgHelpFailure(__FUNCTIONW__ L": Unable to set a symbol path: 'SymSetSearchPathW' failure.", lastError);
		}
	}

	PdbInfo Prov::getPdbInfo(const wchar_t* const filePath) noexcept(false)
	{
		return PdbInfo::get(filePath);
	}



	template <>
	const wchar_t* const TypeHolder<SymTag>::s_names[]
	{
		L"(SymTagNull)",
		L"Executable (Global)",
		L"Compiland",
		L"CompilandDetails",
		L"CompilandEnv",
		L"Function",
		L"Block",
		L"Data",
		L"Annotation",
		L"Label",
		L"PublicSymbol",
		L"UserDefinedType",
		L"Enum",
		L"FunctionType",
		L"PointerType",
		L"ArrayType",
		L"BaseType",
		L"Typedef",
		L"BaseClass",
		L"Friend",
		L"FunctionArgType",
		L"FuncDebugStart",
		L"FuncDebugEnd",
		L"UsingNamespace",
		L"VTableShape",
		L"VTable",
		L"Custom",
		L"Thunk",
		L"CustomType",
		L"ManagedType",
		L"Dimension",
		L"CallSite",
		L"InlineSite",
		L"BaseInterface",
		L"VectorType",
		L"MatrixType",
		L"HLSLType",
		L"Caller",
		L"Callee",
		L"Export",
		L"HeapAllocationSite",
		L"CoffGroup",
		L"Inlinee"
	};

	template <>
	const wchar_t* const TypeHolder<BaseType>::s_names[]
	{
		L"<NoType>",
		L"void",
		L"char",
		L"wchar_t",
		L"signed char",
		L"unsigned char",
		L"int",
		L"unsigned int",
		L"float",
		L"<BCD>",
		L"bool",
		L"short",
		L"unsigned short",
		L"long",
		L"unsigned long",
		L"__int8",
		L"__int16",
		L"__int32",
		L"__int64",
		L"__int128",
		L"unsigned __int8",
		L"unsigned __int16",
		L"unsigned __int32",
		L"unsigned __int64",
		L"unsigned __int128",
		L"<currency>",
		L"<date>",
		L"VARIANT",
		L"<complex>",
		L"<bit>",
		L"BSTR",
		L"HRESULT",
		L"char16_t",
		L"char32_t",
		L"char8_t"
	};

	template <>
	const wchar_t* const TypeHolder<DataKind>::s_names[]
	{
		L"Unknown",
		L"Local",
		L"Static Local",
		L"Param",
		L"Object Ptr",
		L"File Static",
		L"Global",
		L"Member",
		L"Static Member",
		L"Constant"
	};

	template <>
	const wchar_t* const TypeHolder<UdtKind>::s_names[]
	{
		L"struct",
		L"class",
		L"union",
		L"interface"
	};



	Children::ChildrenList* Children::makeList(uint32_t count) noexcept
	{
		const auto size = sizeof(ChildrenList) + count * sizeof(*ChildrenList::id);
		auto* const buf = reinterpret_cast<ChildrenList*>(new (std::nothrow) uint8_t[size]);
		if (buf)
		{
			memset(buf, 0, size);
			buf->count = count;
		}
		return buf;
	}

	Children::Children(const Mod& mod, ChildrenList* children)
		: m_mod(mod)
		, m_children(children)
	{
	}

	Children::Children(const Children& children)
		: m_mod(children.m_mod)
		, m_children(nullptr)
	{
		copy(children.m_children);
	}

	Children::Children(Children&& children) noexcept
		: m_mod(children.m_mod)
		, m_children(std::exchange(children.m_children, nullptr))
	{
	}

	Children::~Children()
	{
		reset();
	}

	void Children::copy(const ChildrenList* children)
	{
		reset();
		if (children)
		{
			m_children = makeList(children->count);
			*m_children = *children;
			memcpy(m_children->id, children->id, children->count * sizeof(*ChildrenList::id));
		}
	}

	bool Children::valid() const
	{
		return m_children != nullptr;
	}

	void Children::reset()
	{
		if (valid())
		{
			delete[] reinterpret_cast<uint8_t*>(m_children);
			m_children = nullptr;
		}
	}

	uint32_t Children::count() const noexcept
	{
		if (!m_children)
		{
			return 0;
		}

		return m_children->count;
	}

	Sym Children::find(const wchar_t* name) const noexcept(false)
	{
		if (!name)
		{
			throw SymNotFound(__FUNCTIONW__ L": Name is NULL.", L"<null>");
		}

		for (const auto sym : *this)
		{
			const auto symName = sym.name();
			if (wcscmp(symName.c_str(), name) == 0)
			{
				return sym;
			}
		}

		throw SymNotFound(std::wstring(__FUNCTIONW__ ": Symbol '").append(name).append(L"' not found."), name);
	}

	Children::Iterator Children::begin() const
	{
		return (count() > 0) ? Iterator(this) : end();
	}

	Children::Iterator Children::end() const
	{
		return Iterator(nullptr);
	}

	Children::operator bool() const
	{
		return valid();
	}

	Children::Iterator::Iterator(const Children* children)
		: m_children(children)
		, m_counter(children ? children->m_children->start : 0)
	{
	}

	Sym Children::Iterator::operator * () const
	{
		return Sym(m_children->m_mod, m_children->m_children->id[m_counter]);
	}

	Children::Iterator& Children::Iterator::operator ++ ()
	{
		if (!m_children || !m_children->m_children)
		{
			return *this;
		}

		++m_counter;

		if (m_counter == m_children->m_children->count)
		{
			m_counter = 0;
			m_children = nullptr;
		}

		return *this;
	}

	Children::Iterator Children::Iterator::operator ++ (int)
	{
		auto it = *this;
		++(*this);
		return it;
	}

	bool Children::Iterator::operator == (const Iterator& it) const
	{
		return (m_counter == it.m_counter) && (m_children == it.m_children);
	}

	bool Children::Iterator::operator != (const Iterator& it) const
	{
		return !operator == (it);
	}


	Sym::Sym(const Mod& mod, TypeId index) : m_mod(mod), m_typeId(index)
	{
	}

	const Mod& Sym::mod() const noexcept
	{
		return m_mod;
	}

	TypeId Sym::id() const noexcept
	{
		return m_typeId;
	}

	bool Sym::queryNoexcept(SymInfo info, void* buf) const noexcept
	{
		return !!SymGetTypeInfo(
			Prov::uid(),
			m_mod.base(),
			m_typeId,
			static_cast<IMAGEHLP_SYMBOL_TYPE_INFO>(info),
			buf
		);
	}

	void Sym::query(SymInfo info, void* buf) const noexcept(false)
	{
		const bool status = queryNoexcept(info, buf);
		if (!status)
		{
			//VARIANT var;
			const auto lastError = GetLastError();
			throw DbgHelpFailure(__FUNCTIONW__ L": Unable to query a symbol info: 'SymGetTypeInfo' failure.", lastError);
		}
	}

	std::wstring Sym::name() const noexcept(false)
	{
		wchar_t* const buf = query<SymInfo::GetSymName>();
		if (buf)
		{
			const std::wstring result(buf);
			LocalFree(buf);
			return result;
		}
		return {};
	}

	SymTag Sym::tag() const noexcept(false)
	{
		return query<SymInfo::GetSymTag>();
	}

	DataKind Sym::dataKind() const noexcept(false)
	{
		return query<SymInfo::GetDataKind>();
	}

	UdtKind Sym::udtKind() const noexcept(false)
	{
		return query<SymInfo::GetUdtKind>();
	}

	BaseType Sym::baseType() const noexcept(false)
	{
		return query<SymInfo::GetBaseType>();
	}

	Sym Sym::type() const noexcept(false)
	{
		return Sym(m_mod, query<SymInfo::GetType>());
	}

	Sym Sym::typeId() const noexcept(false)
	{
		return Sym(m_mod, query<SymInfo::GetTypeId>());
	}

	Sym Sym::arrayIndexTypeId() const noexcept(false)
	{
		return Sym(m_mod, query<SymInfo::GetArrayIndexTypeId>());
	}

	Sym Sym::symIndex() const noexcept(false)
	{
		return Sym(m_mod, query<SymInfo::GetSymIndex>());
	}

	uint64_t Sym::address() const noexcept(false)
	{
		return query<SymInfo::GetAddress>();
	}

	uint32_t Sym::addressOffset() const noexcept(false)
	{
		return query<SymInfo::GetAddressOffset>();
	}

	uint32_t Sym::offset() const noexcept(false)
	{
		return query<SymInfo::GetOffset>();
	}

	uint64_t Sym::size() const noexcept(false)
	{
		return query<SymInfo::GetLength>();
	}

	uint32_t Sym::count() const noexcept(false)
	{
		return query<SymInfo::GetCount>();
	}

	Variant Sym::value() const noexcept(false)
	{
		return query<SymInfo::GetValue>();
	}

	uint32_t Sym::bitpos() const noexcept(false)
	{
		return query<SymInfo::GetBitPosition>();
	}

	Convention Sym::convention() const noexcept(false)
	{
		return query<SymInfo::GetCallingConvention>();
	}

	uint32_t Sym::childrenCount() const noexcept(false)
	{
		return query<SymInfo::GetChildrenCount>();
	}

	Children Sym::children() const noexcept(false)
	{
		const auto count = childrenCount();
		if (!count)
		{
			return Children(m_mod, nullptr);
		}

		auto* const buf = Children::makeList(count);
		query(SymInfo::FindChildren, buf);
		return Children(m_mod, buf);
	}




	std::wstring SymType::name() const noexcept(false)
	{
		const auto tag = SymType::tag();
		switch (tag)
		{
		case SymTag::BaseType:
		{
			return cast<SymTypeBase>().name();
		}
		case SymTag::UDT:
		{
			return cast<SymTypeUdtGeneric>().name();
		}
		case SymTag::PointerType:
		{
			return cast<SymTypePtr>().name();
		}
		case SymTag::ArrayType:
		{
			return cast<SymTypeArray>().name();
		}
		default:
		{
			return Sym::name();
		}
		}
	}


	Mod::Mod()
	{
	}

	Mod::Mod(const wchar_t* path) noexcept(false)
		: Mod(path, nullptr, 0, 0)
	{
	}

	Mod::Mod(const wchar_t* path, const wchar_t* synonym) noexcept(false)
		: Mod(path, synonym, 0, 0)
	{
	}

	Mod::Mod(const wchar_t* path, uint64_t imageBase, uint32_t imageSize) noexcept(false)
		: Mod(path, nullptr, imageBase, imageSize)
	{
	}

	Mod::Mod(const wchar_t* path, const wchar_t* synonym, uint64_t imageBase, uint32_t imageSize) noexcept(false)
	{
		m_base = SymLoadModuleExW(Prov::uid(), nullptr, path, synonym, imageBase, imageSize, nullptr, 0);
		if (!m_base)
		{
			const auto lastError = GetLastError();
			throw DbgHelpFailure(__FUNCTIONW__ L": Unable to load module: 'SymLoadModuleExW' failure.", lastError);
		}
	}

	Mod::Mod(Mod&& mod) noexcept : m_base(std::exchange(mod.m_base, 0))
	{
	}

	Mod::~Mod()
	{
		if (m_base)
		{
			SymUnloadModule64(Prov::uid(), m_base);
		}
	}

	Mod& Mod::operator = (Mod&& mod) noexcept
	{
		if (&mod == this)
		{
			return *this;
		}

		m_base = std::exchange(mod.m_base, 0);
		return *this;
	}

	uint64_t Mod::base() const
	{
		return m_base;
	}

	BOOL CALLBACK EnumSymbolsCallback(PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext)
	{
		DWORD64 functionStart = pSymInfo->Address;
		DWORD64 functionEnd = functionStart + SymbolSize;

		auto pInfo = std::make_shared<PdbFunInfo>(functionStart, functionEnd, SymbolSize, pSymInfo);
		if (pInfo)
		{
			g_PdbInfo.insert(pInfo);
		}
		return TRUE;
	}

	bool Mod::init(const wchar_t* path)
	{
		if (!path)
		{
			return 0;
		}

		SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES);

		// 第一次尝试 base=0（让 dbghelp 自己定）。
		m_base = SymLoadModuleExW(Prov::uid(), nullptr, path, nullptr, 0, 0, nullptr, 0);

		// 失败常见原因：内核驱动 image 的 PE preferred base 是高位内核地址，
		// dbghelp 在用户态进程里映射会拒绝（ERROR_INVALID_ADDRESS=487）。
		// 重试一次：用合成基址，避开 PE 头里的偏好。每个模块给一段独立用户态地址区间。
		if (!m_base)
		{
			static std::atomic<ULONG64> s_synthBase{ 0x10000000ULL };
			const ULONG64 synth = s_synthBase.fetch_add(0x1000000ULL); // 每个模块 16MB
			m_base = SymLoadModuleExW(Prov::uid(), nullptr, path, nullptr,
				synth, 0x1000000UL, nullptr, 0);
		}

		if (!m_base)
		{
			return 0;
		}

		if (!SymEnumSymbols(Prov::uid(), m_base, NULL, EnumSymbolsCallback, NULL))
		{
			//const auto lastError = GetLastError();
			//throw DbgHelpFailure(__FUNCTIONW__ L":Failed to enumerate symbols", lastError);
			return 0;
		}

		return 1;
	}

	Sym Mod::find(const wchar_t* name) const noexcept(false)
	{
		if (!name)
		{
			throw SymNotFound(__FUNCTIONW__ L": Name is NULL.", L"<null>");
		}

		constexpr auto k_size = sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t);
		unsigned char buf[k_size]{};
		auto* const info = reinterpret_cast<SYMBOL_INFOW*>(buf);
		info->SizeOfStruct = k_size;
		info->MaxNameLen = MAX_SYM_NAME;
		const bool status = !!SymGetTypeFromNameW(Prov::uid(), base(), name, info);
		if (!status)
		{
			const auto lastError = GetLastError();

			switch (lastError)
			{
			case ERROR_INVALID_FUNCTION:
			{
				throw SymNotFound(std::wstring(__FUNCTIONW__ ": Symbol '").append(name).append(L"' not found."), name);
			}
			case ERROR_INVALID_PARAMETER:
			{
				throw DbgHelpFailure(
					__FUNCTIONW__ L": Unable to get type from name: 'SymGetTypeFromNameW' failure. "
					"Ensure that 'symsrv.dll' and 'dbghelp.dll' are present in the folder of this program or "
					"that symbols are present in the symbols folder.",
					lastError
				);
			}
			default:
			{
				throw DbgHelpFailure(__FUNCTIONW__ L": Unable to get type from name: 'SymGetTypeFromNameW' failure.", lastError);
			}
			}
		}

		return Sym(*this, info->TypeIndex);
	}

	void Mod::GetModuleInfo(PIMAGEHLP_MODULE64 pModuleInfo)
	{
		IMAGEHLP_MODULE64 moduleInfo = { 0 };
		moduleInfo.SizeOfStruct = sizeof(moduleInfo);

		if (SymGetModuleInfo64(Prov::uid(), m_base, &moduleInfo))
		{
			if (pModuleInfo)
			{
				memcpy_s(pModuleInfo, sizeof(IMAGEHLP_MODULE64), &moduleInfo, sizeof(moduleInfo));
			}
		}
	}

	ULONG64 Mod::GetMemberOffset(const WCHAR* structName, const WCHAR* memberName)
	{
		ULONG64 memberOffset = -1;

		SYMBOL_INFOW info = { 0 };
		if (!SymGetTypeFromNameW(Prov::uid(), m_base, structName, &info))
		{
			return memberOffset;
		}

		DWORD childCount = 0;
		if (!SymGetTypeInfo(Prov::uid(), m_base, info.TypeIndex, TI_GET_CHILDRENCOUNT, &childCount))
		{
			return memberOffset;
		}

		std::vector<BYTE> buffer(sizeof(TI_FINDCHILDREN_PARAMS) + sizeof(ULONG) * childCount);
		TI_FINDCHILDREN_PARAMS* children = reinterpret_cast<TI_FINDCHILDREN_PARAMS*>(buffer.data());
		children->Count = childCount;
		children->Start = 0;

		if (!SymGetTypeInfo(Prov::uid(), m_base, info.TypeIndex, TI_FINDCHILDREN, children))
		{
			return memberOffset;
		}

		for (ULONG i = 0; i < children->Count; ++i) {
			WCHAR* name = nullptr;
			if (SymGetTypeInfo(Prov::uid(), m_base, children->ChildId[i], TI_GET_SYMNAME, &name))
			{
				std::wstring currentName(name);
				LocalFree(name);

				if (currentName == memberName)
				{
					DWORD offset = 0;
					if (SymGetTypeInfo(Prov::uid(), m_base, children->ChildId[i], TI_GET_OFFSET, &offset))
					{
						memberOffset = offset;
						break;
					}
				}
			}
		}

		return memberOffset;
	}


	int utf8_to_utf16(const char* utf8_str, unsigned short** utf16_str) {
		const unsigned char* p = (const unsigned char*)utf8_str;
		size_t len = strlen(utf8_str);
		size_t utf16_len = 0;
		unsigned short* result = NULL;
		unsigned int code_point;

		// 计算所需的UTF-16缓冲区大小
		for (size_t i = 0; i < len; ) {
			unsigned char byte = p[i];
			if ((byte & 0x80) == 0) {
				// 单字节字符 (0xxxxxxx)
				code_point = byte;
				i += 1;
			}
			else if ((byte & 0xE0) == 0xC0) {
				// 双字节字符 (110xxxxx 10xxxxxx)
				code_point = ((byte & 0x1F) << 6) | (p[i + 1] & 0x3F);
				i += 2;
			}
			else if ((byte & 0xF0) == 0xE0) {
				// 三字节字符 (1110xxxx 10xxxxxx 10xxxxxx)
				code_point = ((byte & 0x0F) << 12) | ((p[i + 1] & 0x3F) << 6) | (p[i + 2] & 0x3F);
				i += 3;
			}
			else if ((byte & 0xF8) == 0xF0) {
				// 四字节字符 (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
				code_point = ((byte & 0x07) << 18) | ((p[i + 1] & 0x3F) << 12) |
					((p[i + 2] & 0x3F) << 6) | (p[i + 3] & 0x3F);
				i += 4;
			}
			else {
				// 无效的UTF-8序列
				return -1;
			}

			// 计算UTF-16所需的代码单元数量
			if (code_point <= 0xFFFF) {
				utf16_len += 1;
			}
			else {
				// 高代理项和低代理项
				utf16_len += 2;
			}
		}

		// 分配内存
		result = (unsigned short*)malloc((utf16_len + 1) * sizeof(unsigned short));
		if (!result) {
			return -1;
		}

		// 执行转换
		size_t pos = 0;
		for (size_t i = 0; i < len; ) {
			unsigned char byte = p[i];
			if ((byte & 0x80) == 0) {
				code_point = byte;
				i += 1;
			}
			else if ((byte & 0xE0) == 0xC0) {
				code_point = ((byte & 0x1F) << 6) | (p[i + 1] & 0x3F);
				i += 2;
			}
			else if ((byte & 0xF0) == 0xE0) {
				code_point = ((byte & 0x0F) << 12) | ((p[i + 1] & 0x3F) << 6) | (p[i + 2] & 0x3F);
				i += 3;
			}
			else if ((byte & 0xF8) == 0xF0) {
				code_point = ((byte & 0x07) << 18) | ((p[i + 1] & 0x3F) << 12) |
					((p[i + 2] & 0x3F) << 6) | (p[i + 3] & 0x3F);
				i += 4;
			}
			else {
				free(result);
				return -1;
			}

			// 转换为UTF-16
			if (code_point <= 0xFFFF) {
				result[pos++] = (unsigned short)code_point;
			}
			else {
				// 计算高代理项和低代理项
				code_point -= 0x10000;
				result[pos++] = (unsigned short)((code_point >> 10) + 0xD800);
				result[pos++] = (unsigned short)((code_point & 0x3FF) + 0xDC00);
			}
		}

		// 添加终止符
		result[pos] = 0;
		*utf16_str = result;
		return (int)utf16_len;
	}

	// 将UTF-16字符串转换为UTF-8
	int utf16_to_utf8(const unsigned short* utf16_str, char** utf8_str) {
		size_t utf16_len = 0;
		while (utf16_str[utf16_len] != 0) utf16_len++;

		// 计算所需的UTF-8缓冲区大小
		size_t utf8_len = 0;
		for (size_t i = 0; i < utf16_len; i++) {
			unsigned short code_unit = utf16_str[i];

			// 检查是否为高代理项
			if (code_unit >= 0xD800 && code_unit <= 0xDBFF) {
				// 高代理项 + 低代理项 => 补充字符
				if (i + 1 < utf16_len) {
					unsigned short low_surrogate = utf16_str[i + 1];
					if (low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF) {
						// 计算码点
						unsigned int code_point = 0x10000 +
							((code_unit - 0xD800) << 10) + (low_surrogate - 0xDC00);

						// 4字节UTF-8
						utf8_len += 4;
						i++; // 跳过低代理项
						continue;
					}
				}
			}

			// 普通BMP字符
			if (code_unit < 0x80) {
				utf8_len += 1; // 1字节UTF-8
			}
			else if (code_unit < 0x800) {
				utf8_len += 2; // 2字节UTF-8
			}
			else {
				utf8_len += 3; // 3字节UTF-8
			}
		}

		// 分配内存
		*utf8_str = (char*)malloc((utf8_len + 1) * sizeof(char));
		if (!*utf8_str) return -1;

		// 执行转换
		char* p = *utf8_str;
		for (size_t i = 0; i < utf16_len; i++) {
			unsigned short code_unit = utf16_str[i];

			// 处理代理项对
			if (code_unit >= 0xD800 && code_unit <= 0xDBFF) {
				if (i + 1 < utf16_len) {
					unsigned short low_surrogate = utf16_str[i + 1];
					if (low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF) {
						// 计算码点
						unsigned int code_point = 0x10000 +
							((code_unit - 0xD800) << 10) + (low_surrogate - 0xDC00);

						// 编码为4字节UTF-8
						*p++ = (char)(0xF0 | ((code_point >> 18) & 0x07));
						*p++ = (char)(0x80 | ((code_point >> 12) & 0x3F));
						*p++ = (char)(0x80 | ((code_point >> 6) & 0x3F));
						*p++ = (char)(0x80 | (code_point & 0x3F));

						i++; // 跳过低代理项
						continue;
					}
				}
			}

			// 处理普通BMP字符
			if (code_unit < 0x80) {
				// 1字节UTF-8: 0xxxxxxx
				*p++ = (char)code_unit;
			}
			else if (code_unit < 0x800) {
				// 2字节UTF-8: 110xxxxx 10xxxxxx
				*p++ = (char)(0xC0 | ((code_unit >> 6) & 0x1F));
				*p++ = (char)(0x80 | (code_unit & 0x3F));
			}
			else {
				// 3字节UTF-8: 1110xxxx 10xxxxxx 10xxxxxx
				*p++ = (char)(0xE0 | ((code_unit >> 12) & 0x0F));
				*p++ = (char)(0x80 | ((code_unit >> 6) & 0x3F));
				*p++ = (char)(0x80 | (code_unit & 0x3F));
			}
		}

		// 添加终止符
		*p = '\0';
		return (int)utf8_len;
	}


	ULONG64 Mod::findfun(const wchar_t* name, PSYMBOL_INFOW* pInfo) const noexcept(false)
	{
		if (!name)
		{
			throw SymNotFound(__FUNCTIONW__ L": Name is NULL.", L"<null>");
		}

		constexpr auto k_size = sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t);
		unsigned char buf[k_size]{};
		auto* const info = reinterpret_cast<SYMBOL_INFOW*>(buf);
		info->SizeOfStruct = k_size;
		info->MaxNameLen = MAX_SYM_NAME;
		const bool status = !!SymFromNameW(Prov::uid(), name, info);


		if (!status)
		{
			const auto lastError = GetLastError();

			switch (lastError)
			{
			case ERROR_INVALID_FUNCTION:
			{
				throw SymNotFound(std::wstring(__FUNCTIONW__ ": Symbol '").append(name).append(L"' not found."), name);
			}
			case ERROR_INVALID_PARAMETER:
			{
				throw DbgHelpFailure(
					__FUNCTIONW__ L": Unable to get type from name: 'SymGetTypeFromNameW' failure. "
					"Ensure that 'symsrv.dll' and 'dbghelp.dll' are present in the folder of this program or "
					"that symbols are present in the symbols folder.",
					lastError
				);
			}
			default:
			{
				throw DbgHelpFailure(__FUNCTIONW__ L": Unable to get type from name: 'SymGetTypeFromNameW' failure.", lastError);
			}
			}
		}

		if (pInfo)
		{
			*pInfo = info;
		}

		return info->Address;
	}

	ULONG64 Mod::findGlobalVariables(const wchar_t* pVarName)
	{
		if (!pVarName)
		{
			throw SymNotFound(__FUNCTIONW__ L": Name is NULL.", L"<null>");
		}

		constexpr auto k_size = sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t);
		unsigned char buf[k_size]{};
		auto* const info = reinterpret_cast<SYMBOL_INFOW*>(buf);
		info->SizeOfStruct = k_size;
		info->MaxNameLen = MAX_SYM_NAME;
		const bool status = !!SymFromNameW(Prov::uid(), pVarName, info);


		if (!status)
		{
			const auto lastError = GetLastError();

			switch (lastError)
			{
			case ERROR_INVALID_FUNCTION:
			{
				throw SymNotFound(std::wstring(__FUNCTIONW__ ": Symbol '").append(pVarName).append(L"' not found."), pVarName);
			}
			case ERROR_INVALID_PARAMETER:
			{
				throw DbgHelpFailure(
					__FUNCTIONW__ L": Unable to get type from name: 'SymGetTypeFromNameW' failure. "
					"Ensure that 'symsrv.dll' and 'dbghelp.dll' are present in the folder of this program or "
					"that symbols are present in the symbols folder.",
					lastError
				);
			}
			default:
			{
				throw DbgHelpFailure(__FUNCTIONW__ L": Unable to get type from name: 'SymGetTypeFromNameW' failure.", lastError);
			}
			}
		}
		return info->Address - info->ModBase;
	}




} // namespace Pdb