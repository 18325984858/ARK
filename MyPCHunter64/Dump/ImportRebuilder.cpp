// MyPCHunter64/Dump/ImportRebuilder.cpp
// 见 ImportRebuilder.h 顶部注释。
//
// 实现要点：
//   - 同时支持 PE32 (4-byte thunks) 与 PE32+ (8-byte thunks)；用模板封装
//   - 模块导出表通过本地 CreateFile + MapViewOfFile + 手工解析（不用 LoadLibrary，
//     避免 DllMain 副作用 + 32 位 dll 也能解析）
//   - "畸形 PE" 兜底：LocateNt 回退扫 PE\0\0；OptionalHeader 字段越界检查
//   - "加密 PE" 兜底：DataDirectory[IAT] 缺失 → 扫 .rdata 找指针 run

#include "../pch.h"
#include "ImportRebuilder.h"

#include <algorithm>
#include <unordered_map>

extern void AppLog_Write(const char* level, const char* fmt, ...);
#define IRLOG(...) AppLog_Write("INFO ", __VA_ARGS__)
#define IRWRN(...) AppLog_Write("WARN ", __VA_ARGS__)
#define IRERR(...) AppLog_Write("ERROR", __VA_ARGS__)

namespace
{
    // ---------- 工具：定位 PE 头（兼容畸形头）----------
    PIMAGE_NT_HEADERS32 LocateNt(PBYTE buf, ULONG len)
    {
        if (len < sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS32)) return nullptr;
        auto* dos = (PIMAGE_DOS_HEADER)buf;
        LONG lfanew = dos->e_lfanew;
        if (lfanew > 0 && (ULONG)lfanew + sizeof(IMAGE_NT_HEADERS32) <= len)
        {
            auto* nt = (PIMAGE_NT_HEADERS32)(buf + lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE) return nt;
        }
        ULONG scanEnd = (len < 0x1000) ? len : 0x1000;
        for (ULONG i = 0; i + sizeof(IMAGE_NT_HEADERS32) <= scanEnd; ++i)
        {
            if (*(DWORD*)(buf + i) == IMAGE_NT_SIGNATURE)
                return (PIMAGE_NT_HEADERS32)(buf + i);
        }
        return nullptr;
    }

    PIMAGE_SECTION_HEADER FirstSection(PIMAGE_NT_HEADERS32 nt)
    {
        return (PIMAGE_SECTION_HEADER)((PBYTE)&nt->OptionalHeader + nt->FileHeader.SizeOfOptionalHeader);
    }

    // ---------- 模块导出表解析 ----------
    struct ExportEntry
    {
        ULONG       rva;        // 函数在自身模块中的 RVA（forward 时是 forward 字符串 RVA）
        std::string name;       // ASCII 函数名；序数导出时为 "#NNN"
        bool        forwarded = false;
    };

    // 同一个 module 解析一次后缓存
    struct ParsedModule
    {
        std::vector<ExportEntry> exports;            // 已按 rva 升序排序
        std::vector<ULONG>       sortedRvas;         // 只放 rva，便于二分
    };

    // 自己实现 RVA→FileOffset，按 SectionHeaders 分段
    static ULONG RvaToFileOffset(PBYTE peBase, ULONG peSize, ULONG rva)
    {
        auto* nt = LocateNt(peBase, peSize);
        if (!nt) return 0;
        // 头本身：RVA == 文件偏移
        if (rva < nt->OptionalHeader.SizeOfHeaders) return rva;
        auto* sec = FirstSection(nt);
        USHORT n = nt->FileHeader.NumberOfSections;
        if (n > 96) return 0;
        for (USHORT i = 0; i < n; i++)
        {
            ULONG vEnd = sec[i].VirtualAddress + sec[i].Misc.VirtualSize;
            if (rva >= sec[i].VirtualAddress && rva < vEnd)
            {
                ULONG off = rva - sec[i].VirtualAddress + sec[i].PointerToRawData;
                if (off < peSize) return off;
                return 0;
            }
        }
        return 0;
    }

    // 通过本地文件解析模块的 ExportDirectory
    bool ParseModuleExports(const std::wstring& fullPath, ParsedModule& out)
    {
        HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMap) { CloseHandle(hFile); return false; }
        PBYTE base = (PBYTE)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
        DWORD peSize = GetFileSize(hFile, nullptr);
        bool ok = false;
        if (base && peSize != INVALID_FILE_SIZE)
        {
            auto* nt = LocateNt(base, peSize);
            if (nt && nt->FileHeader.SizeOfOptionalHeader >= sizeof(IMAGE_OPTIONAL_HEADER32))
            {
                ULONG expRva = 0, expSize = 0;
                USHORT optMagic = nt->OptionalHeader.Magic;
                if (optMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
                {
                    auto* nt64 = (PIMAGE_NT_HEADERS64)nt;
                    if (nt64->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_EXPORT)
                    {
                        expRva  = nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
                        expSize = nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
                    }
                }
                else if (optMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
                {
                    if (nt->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_EXPORT)
                    {
                        expRva  = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
                        expSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
                    }
                }

                if (expRva && expSize)
                {
                    ULONG expOff = RvaToFileOffset(base, peSize, expRva);
                    if (expOff && expOff + sizeof(IMAGE_EXPORT_DIRECTORY) <= peSize)
                    {
                        auto* ed = (PIMAGE_EXPORT_DIRECTORY)(base + expOff);
                        ULONG nFuncs   = ed->NumberOfFunctions;
                        ULONG nNames   = ed->NumberOfNames;
                        ULONG funcsRva = ed->AddressOfFunctions;
                        ULONG namesRva = ed->AddressOfNames;
                        ULONG ordsRva  = ed->AddressOfNameOrdinals;

                        ULONG funcsOff = RvaToFileOffset(base, peSize, funcsRva);
                        ULONG namesOff = RvaToFileOffset(base, peSize, namesRva);
                        ULONG ordsOff  = RvaToFileOffset(base, peSize, ordsRva);
                        if (funcsOff && nFuncs && nFuncs < 200000
                            && funcsOff + nFuncs * sizeof(ULONG) <= peSize)
                        {
                            auto* funcs = (PULONG)(base + funcsOff);
                            bool namesValid = (namesOff && nNames
                                && namesOff + nNames * sizeof(ULONG) <= peSize
                                && ordsOff && ordsOff + nNames * sizeof(USHORT) <= peSize);
                            auto* names = namesValid ? (PULONG)(base + namesOff) : nullptr;
                            auto* ords  = namesValid ? (PUSHORT)(base + ordsOff) : nullptr;

                            std::unordered_map<USHORT, const char*> ordToName;
                            if (names && ords)
                            {
                                for (ULONG i = 0; i < nNames; ++i)
                                {
                                    ULONG nameOff = RvaToFileOffset(base, peSize, names[i]);
                                    if (nameOff && nameOff < peSize)
                                        ordToName[ords[i]] = (const char*)(base + nameOff);
                                }
                            }

                            out.exports.reserve(nFuncs);
                            for (ULONG i = 0; i < nFuncs; ++i)
                            {
                                ULONG fnRva = funcs[i];
                                if (fnRva == 0) continue;
                                ExportEntry e;
                                e.rva = fnRva;
                                e.forwarded = (fnRva >= expRva && fnRva < expRva + expSize);
                                auto it = ordToName.find((USHORT)i);
                                if (it != ordToName.end())
                                {
                                    e.name = it->second;
                                }
                                else
                                {
                                    char nm[32];
                                    _snprintf_s(nm, _countof(nm), _TRUNCATE,
                                        "#%lu", (unsigned long)(i + ed->Base));
                                    e.name = nm;
                                }
                                out.exports.push_back(std::move(e));
                            }

                            std::sort(out.exports.begin(), out.exports.end(),
                                [](const ExportEntry& a, const ExportEntry& b) { return a.rva < b.rva; });
                            out.sortedRvas.reserve(out.exports.size());
                            for (auto& e : out.exports) out.sortedRvas.push_back(e.rva);
                            ok = true;
                        }
                    }
                }
            }
        }
        if (base) UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return ok;
    }

    // 在已解析的模块导出表里反查 rva → 函数名。
    // 必须 rva 精确等于某个 export entry（即"clean" import）才返回 true；
    // 若是入函数中间（disp > 0），通常是壳的 inline，认定 unresolved。
    bool LookupExport(const ParsedModule& m, ULONG rva, std::string& outName)
    {
        if (m.sortedRvas.empty()) return false;
        auto it = std::lower_bound(m.sortedRvas.begin(), m.sortedRvas.end(), rva);
        if (it == m.sortedRvas.end() || *it != rva)
        {
            // 不精确：尝试容忍 1~2 字节差（极少数 forwarded export 修正）
            return false;
        }
        size_t idx = it - m.sortedRvas.begin();
        outName = m.exports[idx].name;
        return true;
    }

    // ---------- 对齐 ----------
    ULONG AlignUp(ULONG v, ULONG a) { return a ? ((v + a - 1) & ~(a - 1)) : v; }

    // 一个 IAT 区段
    struct IatRegion
    {
        ULONG rva;
        ULONG byteLen;     // 字节数
    };

    // ---------- 自动定位 IAT（用于 DataDirectory[IAT] 被加壳擦掉的情况）----------
    template <typename Thunk>
    void AutoLocateIatTpl(PBYTE buf, ULONG bufLen, PIMAGE_NT_HEADERS32 nt,
                          const std::vector<ImportRebuilder::LoadedModule>& modules,
                          std::vector<IatRegion>& outRegions)
    {
        const ULONG step = (ULONG)sizeof(Thunk);
        auto* sec = FirstSection(nt);
        USHORT nSec = nt->FileHeader.NumberOfSections;

        // 简单的范围归属判断：指针是否落入任一模块
        auto inAnyModule = [&](unsigned long long ptr) -> bool
        {
            for (auto& m : modules)
            {
                if (ptr >= m.base && ptr < m.base + m.size) return true;
            }
            return false;
        };

        for (USHORT s = 0; s < nSec; ++s)
        {
            // 只扫看起来像 .rdata / .idata 的段（包含 IMAGE_SCN_MEM_READ 但不可执行）
            if (sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;
            if (!(sec[s].Characteristics & IMAGE_SCN_MEM_READ)) continue;
            ULONG vStart = sec[s].VirtualAddress;
            ULONG vSize  = sec[s].Misc.VirtualSize;
            if (vStart + vSize > bufLen) vSize = bufLen - vStart;
            if (vSize < step * 4) continue;

            PBYTE p = buf + vStart;
            ULONG runStart = 0;
            ULONG runCnt   = 0;
            bool  inRun    = false;

            auto endRun = [&](ULONG i)
            {
                if (inRun && runCnt >= 4)
                {
                    outRegions.push_back({ vStart + runStart, (i - runStart) });
                }
                inRun  = false;
                runCnt = 0;
            };

            for (ULONG i = 0; i + step <= vSize; i += step)
            {
                unsigned long long ptr = (step == 8) ? *(unsigned long long*)(p + i) : *(unsigned long*)(p + i);
                bool isPtr = (ptr != 0) && inAnyModule(ptr);
                if (isPtr)
                {
                    if (!inRun) { runStart = i; inRun = true; runCnt = 0; }
                    runCnt++;
                }
                else if (ptr == 0 && inRun)
                {
                    // 0 sentinel：扩展当前 run 一格再切（兼容 per-DLL 分组）
                    runCnt++;
                    // 看下一格
                    if (i + step * 2 > vSize) { endRun(i + step); break; }
                    unsigned long long next = (step == 8) ? *(unsigned long long*)(p + i + step)
                                                          : *(unsigned long*)(p + i + step);
                    if (next == 0 || !inAnyModule(next))
                    {
                        endRun(i + step);
                    }
                }
                else
                {
                    endRun(i);
                }
            }
            endRun(vSize);
        }
    }

    // ---------- 主流程模板（PE32 / PE32+ 通过 Thunk 区分）----------
    template <typename Thunk, USHORT MagicVal, ULONG OrdinalMask>
    bool DoRebuildTpl(std::vector<BYTE>& buf,
                      unsigned long long originalImageBase,
                      const std::vector<ImportRebuilder::LoadedModule>& modules,
                      const std::wstring& iatLogPath,
                      ImportRebuilder::Stats& stats,
                      std::wstring& errMsg)
    {
        PBYTE base = buf.data();
        ULONG bufLen = (ULONG)buf.size();
        auto* nt = LocateNt(base, bufLen);
        if (!nt) { errMsg = L"PE 头无法定位"; return false; }

        const bool is64 = (sizeof(Thunk) == 8);
        auto* nt64 = (PIMAGE_NT_HEADERS64)nt;

        // 取 DataDirectory[IAT] / IMPORT
        auto getDir = [&](int idx, ULONG& rva, ULONG& size) -> bool
        {
            if (is64)
            {
                if (nt64->OptionalHeader.NumberOfRvaAndSizes <= (ULONG)idx) return false;
                rva  = nt64->OptionalHeader.DataDirectory[idx].VirtualAddress;
                size = nt64->OptionalHeader.DataDirectory[idx].Size;
            }
            else
            {
                if (nt->OptionalHeader.NumberOfRvaAndSizes <= (ULONG)idx) return false;
                rva  = nt->OptionalHeader.DataDirectory[idx].VirtualAddress;
                size = nt->OptionalHeader.DataDirectory[idx].Size;
            }
            return true;
        };

        // 1) 确定 IAT 区段
        std::vector<IatRegion> regions;
        ULONG iatRva = 0, iatSize = 0;
        getDir(IMAGE_DIRECTORY_ENTRY_IAT, iatRva, iatSize);
        if (iatRva && iatSize && iatRva + iatSize <= bufLen)
        {
            regions.push_back({ iatRva, iatSize });
            IRLOG("[ImpRebuild] using DataDirectory[IAT] rva=0x%X size=0x%X", iatRva, iatSize);
        }
        else
        {
            IRWRN("[ImpRebuild] DataDirectory[IAT] empty/invalid, auto-locating...");
            AutoLocateIatTpl<Thunk>(base, bufLen, nt, modules, regions);
            stats.autoLocated = true;
            IRLOG("[ImpRebuild] auto-located %zu IAT region(s)", regions.size());
        }
        if (regions.empty())
        {
            errMsg = L"未能定位 IAT 区域（壳/畸形 PE，DataDirectory 已被擦除且扫描无果）";
            return false;
        }
        stats.iatRegions = (unsigned)regions.size();

        // 2) 解析每个模块的导出表（按需缓存）
        std::unordered_map<std::wstring, ParsedModule> moduleCache;
        auto getModule = [&](const ImportRebuilder::LoadedModule& m) -> ParsedModule*
        {
            auto it = moduleCache.find(m.fullPath);
            if (it != moduleCache.end()) return &it->second;
            ParsedModule pm;
            if (!ParseModuleExports(m.fullPath, pm))
            {
                IRWRN("[ImpRebuild] failed to parse exports of %ls", m.fullPath.c_str());
                pm.sortedRvas.clear();
            }
            return &moduleCache.emplace(m.fullPath, std::move(pm)).first->second;
        };

        // 找指针对应的模块
        auto findModule = [&](unsigned long long ptr) -> const ImportRebuilder::LoadedModule*
        {
            for (auto& m : modules)
            {
                if (ptr >= m.base && ptr < m.base + m.size) return &m;
            }
            return nullptr;
        };

        // 3) 扫描每个 IAT 区段，逐条解析
        struct ResolvedEntry
        {
            ULONG       iatRva;     // 这个 IAT 槽位的 RVA（dumped image 内）
            std::string moduleBase; // "kernel32.dll"（lowercase 用于分组）
            std::string func;       // 函数名 或 "#NNN"
            bool        ok;
        };
        std::vector<ResolvedEntry> entries;
        entries.reserve(2048);

        for (auto& r : regions)
        {
            ULONG step = (ULONG)sizeof(Thunk);
            if (r.byteLen < step) continue;
            for (ULONG off = 0; off + step <= r.byteLen; off += step)
            {
                Thunk v = *(Thunk*)(base + r.rva + off);
                if (v == 0)
                {
                    // 分组分隔符
                    ResolvedEntry e{ r.rva + off, "", "", false };
                    entries.push_back(std::move(e));
                    continue;
                }
                stats.totalIatEntries++;

                const ImportRebuilder::LoadedModule* m = findModule((unsigned long long)v);
                if (!m)
                {
                    // 未知 stub 区域 → unresolved
                    ResolvedEntry e{ r.rva + off, "", "", false };
                    entries.push_back(std::move(e));
                    stats.unresolved++;
                    continue;
                }
                ParsedModule* pm = getModule(*m);
                ULONG rvaInMod = (ULONG)((unsigned long long)v - m->base);
                std::string name;
                if (pm && LookupExport(*pm, rvaInMod, name))
                {
                    // basename lowercase 用于分组
                    std::wstring lc = m->name;
                    for (auto& c : lc) c = (wchar_t)towlower(c);
                    char ascii[260] = { 0 };
                    WideCharToMultiByte(CP_UTF8, 0, lc.c_str(), -1, ascii, sizeof(ascii), nullptr, nullptr);
                    ResolvedEntry e{ r.rva + off, ascii, name, true };
                    entries.push_back(std::move(e));
                    stats.resolved++;
                }
                else
                {
                    ResolvedEntry e{ r.rva + off, "", "", false };
                    entries.push_back(std::move(e));
                    stats.unresolved++;
                }
            }
            // 区段末尾自然终止
            ResolvedEntry sep{ r.rva + r.byteLen, "", "", false };
            entries.push_back(std::move(sep));
        }

        // 4) 把同一 module 的连续条目分组成 IMAGE_IMPORT_DESCRIPTOR
        struct Desc
        {
            std::string         dll;            // basename lowercase
            ULONG               firstThunkRva;  // 指向原 IAT 区的 RVA
            std::vector<std::string> names;     // 该 descriptor 下每个 imp 的函数名
        };
        std::vector<Desc> descs;
        {
            Desc cur;
            cur.firstThunkRva = 0;
            for (auto& e : entries)
            {
                if (!e.ok)
                {
                    if (!cur.names.empty()) { descs.push_back(std::move(cur)); cur = Desc{}; cur.firstThunkRva = 0; }
                    continue;
                }
                if (cur.names.empty())
                {
                    cur.dll = e.moduleBase;
                    cur.firstThunkRva = e.iatRva;
                    cur.names.push_back(e.func);
                }
                else if (cur.dll == e.moduleBase)
                {
                    cur.names.push_back(e.func);
                }
                else
                {
                    descs.push_back(std::move(cur));
                    cur = Desc{};
                    cur.dll = e.moduleBase;
                    cur.firstThunkRva = e.iatRva;
                    cur.names.push_back(e.func);
                }
            }
            if (!cur.names.empty()) descs.push_back(std::move(cur));
        }
        stats.modulesUsed = (unsigned)descs.size();

        if (descs.empty())
        {
            errMsg = L"IAT 全部无法解析（重度加密/壳 stub）";
            return false;
        }

        // 5) 构造 .impnew 内容
        //    布局：[Descriptors][INT 数组 per dll][IMAGE_IMPORT_BY_NAME 列表][DLL ASCII 名]
        std::vector<BYTE> sectionContent;
        sectionContent.reserve(4096);

        // 先占位 descriptor 区
        ULONG nDescs = (ULONG)descs.size();
        ULONG descTableSize = (nDescs + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR);
        sectionContent.resize(descTableSize, 0);

        struct DescOffsets
        {
            ULONG intOffset;       // INT 数组在 section 内的偏移
            ULONG nameOffset;      // DLL 名字 ASCII 在 section 内偏移
            std::vector<ULONG> nameRvaInSection;   // 每个 IMAGE_IMPORT_BY_NAME 在 section 内偏移
        };
        std::vector<DescOffsets> descOff(nDescs);

        // 先追加：INT 数组占位
        for (ULONG i = 0; i < nDescs; ++i)
        {
            descOff[i].intOffset = (ULONG)sectionContent.size();
            ULONG intBytes = (ULONG)(descs[i].names.size() + 1) * sizeof(Thunk);
            sectionContent.resize(sectionContent.size() + intBytes, 0);
        }

        // 再追加 IMAGE_IMPORT_BY_NAME（hint=0 + name + '\0', WORD 对齐）
        for (ULONG i = 0; i < nDescs; ++i)
        {
            for (auto& fn : descs[i].names)
            {
                if (sectionContent.size() & 1) sectionContent.push_back(0);
                descOff[i].nameRvaInSection.push_back((ULONG)sectionContent.size());
                USHORT hint = 0;
                sectionContent.push_back((BYTE)(hint & 0xFF));
                sectionContent.push_back((BYTE)(hint >> 8));
                sectionContent.insert(sectionContent.end(), fn.begin(), fn.end());
                sectionContent.push_back(0);
            }
        }

        // 最后追加 DLL ASCII 名（'\0' 结尾）
        for (ULONG i = 0; i < nDescs; ++i)
        {
            descOff[i].nameOffset = (ULONG)sectionContent.size();
            sectionContent.insert(sectionContent.end(), descs[i].dll.begin(), descs[i].dll.end());
            sectionContent.push_back(0);
        }

        // 6) 新节的 RVA 与对齐
        ULONG sectionAlignment = is64 ? nt64->OptionalHeader.SectionAlignment : nt->OptionalHeader.SectionAlignment;
        ULONG fileAlignment    = is64 ? nt64->OptionalHeader.FileAlignment    : nt->OptionalHeader.FileAlignment;
        if (sectionAlignment == 0) sectionAlignment = 0x1000;
        if (fileAlignment == 0)    fileAlignment = sectionAlignment;  // dump 已修头：File==Section

        ULONG currentSizeOfImage = is64 ? nt64->OptionalHeader.SizeOfImage : nt->OptionalHeader.SizeOfImage;
        ULONG newSecRva  = AlignUp(currentSizeOfImage, sectionAlignment);
        ULONG newSecVSize= (ULONG)sectionContent.size();
        ULONG newSecRaw  = newSecRva;   // 保持 File==Section
        ULONG newSecRSize= AlignUp(newSecVSize, fileAlignment);

        // 7) 现在用真实 RVA 回填 descriptor 与 INT
        // descriptor.Name 与 OriginalFirstThunk(INT) 都要相对 newSecRva 加上偏移变成绝对 RVA
        auto* pDescs = (PIMAGE_IMPORT_DESCRIPTOR)sectionContent.data();
        for (ULONG i = 0; i < nDescs; ++i)
        {
            pDescs[i].OriginalFirstThunk = newSecRva + descOff[i].intOffset;
            pDescs[i].TimeDateStamp      = 0;
            pDescs[i].ForwarderChain     = 0;
            pDescs[i].Name               = newSecRva + descOff[i].nameOffset;
            pDescs[i].FirstThunk         = descs[i].firstThunkRva;  // ← 原 IAT 区域里的位置

            // 填 INT
            Thunk* intArr = (Thunk*)(sectionContent.data() + descOff[i].intOffset);
            for (size_t k = 0; k < descs[i].names.size(); ++k)
            {
                intArr[k] = (Thunk)(newSecRva + descOff[i].nameRvaInSection[k]);
            }
            intArr[descs[i].names.size()] = 0;  // 终止
        }

        // 8) 把 sectionContent 拼到 buf 末尾（按 fileAlignment / sectionAlignment 对齐）
        if (buf.size() < newSecRaw) buf.resize(newSecRaw, 0);
        buf.insert(buf.end(), sectionContent.begin(), sectionContent.end());
        // pad 到 sectionAlignment（保持 SizeOfImage 整齐）
        while (buf.size() < (size_t)newSecRva + AlignUp(newSecVSize, sectionAlignment))
            buf.push_back(0);

        // base 指针可能因 buf.resize 失效，重新取
        base = buf.data();
        nt   = LocateNt(base, (ULONG)buf.size());
        nt64 = (PIMAGE_NT_HEADERS64)nt;

        // 9) 新增节表项
        USHORT secCount = nt->FileHeader.NumberOfSections;
        // 节表起点在 OptionalHeader 之后
        PIMAGE_SECTION_HEADER secTable = FirstSection(nt);
        // 检查节表后是否还有 40 字节空间（不能踩到 SizeOfHeaders 之后的第一节首字节）
        ULONG headersEnd = (ULONG)((PBYTE)(secTable + secCount + 1) - base);
        // 找最早开始的节 raw / virtual
        ULONG firstSecRaw = (ULONG)-1;
        for (USHORT i = 0; i < secCount; ++i)
        {
            if (secTable[i].PointerToRawData && secTable[i].PointerToRawData < firstSecRaw)
                firstSecRaw = secTable[i].PointerToRawData;
        }
        if (firstSecRaw == (ULONG)-1) firstSecRaw = nt->OptionalHeader.SizeOfHeaders;
        if (headersEnd > firstSecRaw)
        {
            errMsg = L"节表无空闲槽位（PE 头被裁剪/加壳塞满）";
            return false;
        }

        PIMAGE_SECTION_HEADER newSec = &secTable[secCount];
        RtlZeroMemory(newSec, sizeof(*newSec));
        memcpy(newSec->Name, ".impnew", 7);
        newSec->VirtualAddress       = newSecRva;
        newSec->Misc.VirtualSize     = newSecVSize;
        newSec->PointerToRawData     = newSecRaw;
        newSec->SizeOfRawData        = newSecRSize;
        newSec->Characteristics      = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;

        nt->FileHeader.NumberOfSections = secCount + 1;

        // 10) 更新 OptionalHeader：SizeOfImage / DataDirectory[IMPORT]
        ULONG newSizeOfImage = AlignUp(newSecRva + newSecVSize, sectionAlignment);
        if (is64)
        {
            nt64->OptionalHeader.SizeOfImage = newSizeOfImage;
            nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = newSecRva;
            nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = descTableSize;
            // 清掉 bound import（绑定信息已无效）
            if (nt64->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT)
            {
                nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress = 0;
                nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size = 0;
            }
        }
        else
        {
            nt->OptionalHeader.SizeOfImage = newSizeOfImage;
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = newSecRva;
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = descTableSize;
            if (nt->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT)
            {
                nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].VirtualAddress = 0;
                nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT].Size = 0;
            }
        }

        // 11) 把原 IAT 区域里的"已解析"位置写 0（loader 用 INT 重新填进来）
        //     未解析的位置保留原绝对地址，便于 IDA 至少不显示成 0
        for (auto& e : entries)
        {
            if (!e.ok) continue;
            if (e.iatRva + sizeof(Thunk) > buf.size()) continue;
            *(Thunk*)(buf.data() + e.iatRva) = 0;
        }

        // 12) 写诊断日志（可选）
        if (!iatLogPath.empty())
        {
            HANDLE hLog = CreateFileW(iatLogPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hLog != INVALID_HANDLE_VALUE)
            {
                auto writeLine = [&](const char* fmt, ...)
                {
                    char tmp[1024];
                    va_list ap; va_start(ap, fmt);
                    int n = _vsnprintf_s(tmp, sizeof(tmp), _TRUNCATE, fmt, ap);
                    va_end(ap);
                    if (n > 0) { DWORD w; WriteFile(hLog, tmp, (DWORD)n, &w, nullptr); }
                };
                writeLine("# IAT rebuild report\r\n");
                writeLine("# total=%lu resolved=%lu unresolved=%lu modules=%lu regions=%lu autoLocated=%d\r\n",
                    stats.totalIatEntries, stats.resolved, stats.unresolved,
                    stats.modulesUsed, stats.iatRegions, stats.autoLocated ? 1 : 0);
                writeLine("# format: <iatRva> <ok|FAIL> <ptr> <module!func>\r\n");
                for (auto& e : entries)
                {
                    if (e.iatRva == 0 && !e.ok && e.moduleBase.empty()) continue;
                    Thunk v = 0;
                    if (e.iatRva + sizeof(Thunk) <= buf.size())
                        v = *(Thunk*)(buf.data() + e.iatRva);
                    if (e.ok)
                        writeLine("0x%08X OK   0x%016llX  %s!%s\r\n",
                            e.iatRva, (unsigned long long)v, e.moduleBase.c_str(), e.func.c_str());
                    else if (!e.moduleBase.empty() || e.iatRva)
                        writeLine("0x%08X FAIL 0x%016llX  <unresolved>\r\n",
                            e.iatRva, (unsigned long long)v);
                }
                CloseHandle(hLog);
            }
        }

        IRLOG("[ImpRebuild] done total=%lu resolved=%lu unresolved=%lu modules=%lu regions=%lu",
            stats.totalIatEntries, stats.resolved, stats.unresolved,
            stats.modulesUsed, stats.iatRegions);
        return true;
    }
}

namespace ImportRebuilder
{
    bool RebuildImports(std::vector<unsigned char>& buf,
                        unsigned long long originalImageBase,
                        const std::vector<LoadedModule>& modules,
                        const std::wstring& iatLogPath,
                        Stats& stats,
                        std::wstring& errMsg)
    {
        errMsg.clear();
        stats = Stats{};
        if (buf.size() < 0x1000) { errMsg = L"buf 太小"; return false; }

        // 判断位数
        auto* nt = LocateNt(buf.data(), (ULONG)buf.size());
        if (!nt) { errMsg = L"PE 头无法定位"; return false; }
        USHORT magic = nt->OptionalHeader.Magic;
        if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        {
            return DoRebuildTpl<unsigned long long, IMAGE_NT_OPTIONAL_HDR64_MAGIC, IMAGE_ORDINAL_FLAG64>(
                buf, originalImageBase, modules, iatLogPath, stats, errMsg);
        }
        else if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        {
            return DoRebuildTpl<unsigned long, IMAGE_NT_OPTIONAL_HDR32_MAGIC, IMAGE_ORDINAL_FLAG32>(
                buf, originalImageBase, modules, iatLogPath, stats, errMsg);
        }
        errMsg = L"非法 OptionalHeader.Magic";
        return false;
    }
}
