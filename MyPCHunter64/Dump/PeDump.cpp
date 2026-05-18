// MyPCHunter64/Dump/PeDump.cpp
//
// R3 端 PE Dump 流程：
//   1) 通过驱动 IPC 探测目标模块 SizeOfImage / 位数（UserBuf=NULL）
//   2) 分配缓冲，再次 IPC 让驱动把整块 SizeOfImage 拷过来
//   3) 修头：FixupDumpedPE — 把每个 section 的 PointerToRawData=VirtualAddress、
//      SizeOfRawData=VirtualSize；FileAlignment=SectionAlignment；
//      让 IDA / x64dbg 能按"内存布局"识别为合法 PE。
//   4) 写盘
//
// 容错点：
//   - DOS / NT 签名: 校验但允许畸形头, 只在完全无 PE\0\0 时拒绝
//   - NumberOfSections 上限 96
//   - 不重建 IAT（属于 unpack 范畴，独立工程）

#include "../pch.h"
#include "PeDump.h"
#include "../CLoadDriver.h"
#include "../../MyDriver64/Struct.h"

#include <vector>

extern _LoadDriver g_LoadDriver;
extern void AppLog_Write(const char* level, const char* fmt, ...);
#define DLOG(...) AppLog_Write("INFO ", __VA_ARGS__)
#define DERR(...) AppLog_Write("ERROR", __VA_ARGS__)

namespace
{
    // 在已映射为线性内存布局的缓冲中定位 NT 头。
    // 返回指向 NtHeaders 的指针；找不到时返回 nullptr。
    // 兼容畸形 PE：e_lfanew 不可靠时回退扫描前 4KB 找 "PE\0\0" 签名。
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
        // 畸形 fallback：扫前 4KB 找 PE\0\0
        ULONG scanEnd = (len < 0x1000) ? len : 0x1000;
        if (scanEnd < 4) return nullptr;
        for (ULONG i = 0; i + 4 <= scanEnd; ++i)
        {
            if (*(DWORD*)(buf + i) == IMAGE_NT_SIGNATURE &&
                i + sizeof(IMAGE_NT_HEADERS32) <= len)
            {
                return (PIMAGE_NT_HEADERS32)(buf + i);
            }
        }
        return nullptr;
    }

    // 把按内存布局拷过来的 buf 改成"file 布局即 memory 布局"的可用 PE。
    // 不重建 IAT；不动 directory；只动 SectionHeader + 文件对齐。
    bool FixupDumpedPE(PBYTE buf, ULONG len, std::wstring& err)
    {
        PIMAGE_NT_HEADERS32 nt = LocateNt(buf, len);
        if (!nt) { err = L"PE 头无法定位（畸形/已加壳/被损坏）"; return false; }

        USHORT magic = nt->OptionalHeader.Magic;
        bool is64 = (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
        bool is32 = (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC);
        if (!is32 && !is64) { err = L"OptionalHeader.Magic 非 PE32/PE32+"; return false; }

        USHORT nSec = nt->FileHeader.NumberOfSections;
        if (nSec == 0 || nSec > 96) { err = L"NumberOfSections 非法"; return false; }

        // 节表起始
        auto* sec = (PIMAGE_SECTION_HEADER)((PBYTE)&nt->OptionalHeader + nt->FileHeader.SizeOfOptionalHeader);
        if ((PBYTE)(sec + nSec) > buf + len) { err = L"节表越界"; return false; }

        // 修每个节
        for (USHORT i = 0; i < nSec; ++i)
        {
            ULONG va = sec[i].VirtualAddress;
            ULONG vs = sec[i].Misc.VirtualSize;
            if (va == 0 || va >= len)
            {
                sec[i].PointerToRawData = 0;
                sec[i].SizeOfRawData = 0;
                continue;
            }
            if ((ULONG64)va + vs > len) vs = len - va;
            sec[i].PointerToRawData = va;
            sec[i].SizeOfRawData = vs;
        }

        // FileAlignment = SectionAlignment
        if (is64)
        {
            auto* nt64 = (PIMAGE_NT_HEADERS64)nt;
            ULONG sa = nt64->OptionalHeader.SectionAlignment;
            if (sa == 0) sa = 0x1000;
            nt64->OptionalHeader.FileAlignment = sa;
            nt64->OptionalHeader.SectionAlignment = sa;
        }
        else
        {
            ULONG sa = nt->OptionalHeader.SectionAlignment;
            if (sa == 0) sa = 0x1000;
            nt->OptionalHeader.FileAlignment = sa;
            nt->OptionalHeader.SectionAlignment = sa;
        }
        return true;
    }

    bool WriteAllBytes(const wchar_t* path, const void* data, ULONG len, std::wstring& err)
    {
        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            wchar_t buf[256];
            _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"CreateFile 失败 GLE=%lu", GetLastError());
            err = buf;
            return false;
        }
        DWORD wrote = 0;
        BOOL ok = WriteFile(h, data, len, &wrote, nullptr);
        CloseHandle(h);
        if (!ok || wrote != len) { err = L"WriteFile 失败"; return false; }
        return true;
    }
}

namespace PeDump
{
    bool DumpModuleFromProcess(const wchar_t* eprocessHex,
                               unsigned long long imageBase,
                               const std::vector<ImportRebuilder::LoadedModule>& modules,
                               const wchar_t* savePath,
                               RebuildResult& rebuildOut,
                               std::wstring& errMsg)
    {
        rebuildOut = RebuildResult{};
        errMsg.clear();
        if (!eprocessHex || !*eprocessHex || imageBase == 0 || !savePath || !*savePath)
        {
            errMsg = L"参数错误";
            return false;
        }
        ULONG64 eproc = _wcstoui64(eprocessHex, nullptr, 16);
        if (eproc == 0) { errMsg = L"EPROCESS 解析失败"; return false; }

        // 阶段一：探测 SizeOfImage
        CDumpPEInfo probe = { 0 };
        probe.Eprocess = eproc;
        probe.ImageBase = imageBase;
        probe.UserBuf = nullptr;
        probe.UserBufLen = 0;
        g_LoadDriver.SendMsg(um_Cmd_Dump_ProcessPE_info, &probe, nullptr, nullptr, nullptr);
        DLOG("[PeDump] probe: status=%lu size=%lu machine=0x%X is64=%u",
            (unsigned long)probe.Status, (unsigned long)probe.SizeOfImage,
            (unsigned)probe.Machine, (unsigned)probe.Is64);
        if (probe.Status != DUMPPE_STATUS_OK || probe.SizeOfImage == 0)
        {
            wchar_t b[128];
            _snwprintf_s(b, _countof(b), _TRUNCATE, L"探测失败 status=%lu", (unsigned long)probe.Status);
            errMsg = b;
            return false;
        }

        // 阶段二：分配缓冲 + 拉取整块
        std::vector<BYTE> buf(probe.SizeOfImage, 0);
        CDumpPEInfo req = { 0 };
        req.Eprocess = eproc;
        req.ImageBase = imageBase;
        req.UserBuf = buf.data();
        req.UserBufLen = (ULONG)buf.size();
        g_LoadDriver.SendMsg(um_Cmd_Dump_ProcessPE_info, &req, nullptr, nullptr, nullptr);
        DLOG("[PeDump] pull : status=%lu written=%lu",
            (unsigned long)req.Status, (unsigned long)req.BytesWritten);
        if (req.Status != DUMPPE_STATUS_OK || req.BytesWritten == 0)
        {
            wchar_t b[128];
            _snwprintf_s(b, _countof(b), _TRUNCATE, L"读取失败 status=%lu", (unsigned long)req.Status);
            errMsg = b;
            return false;
        }

        // 阶段三：修头
        std::wstring fixErr;
        if (!FixupDumpedPE(buf.data(), (ULONG)buf.size(), fixErr))
        {
            DERR("[PeDump] fixup failed: %ls — 仍按原样写盘以便手工分析", fixErr.c_str());
        }

        // 阶段四：尝试 IAT 重建（best-effort）
        if (!modules.empty())
        {
            rebuildOut.attempted = true;

            // .iat.log 与 dump 同目录、同 basename
            std::wstring logPath = savePath;
            size_t dot = logPath.find_last_of(L'.');
            if (dot != std::wstring::npos) logPath = logPath.substr(0, dot);
            logPath += L".iat.log";

            std::wstring rerr;
            bool rok = ImportRebuilder::RebuildImports(buf, imageBase, modules, logPath,
                rebuildOut.stats, rerr);
            rebuildOut.success = rok;
            rebuildOut.errMsg = rerr;
            rebuildOut.iatLogPath = logPath;
            DLOG("[PeDump] IAT rebuild ok=%d total=%lu resolved=%lu unresolved=%lu",
                rok ? 1 : 0,
                rebuildOut.stats.totalIatEntries,
                rebuildOut.stats.resolved,
                rebuildOut.stats.unresolved);
        }

        // 阶段五：写盘
        if (!WriteAllBytes(savePath, buf.data(), (ULONG)buf.size(), errMsg))
        {
            return false;
        }
        DLOG("[PeDump] OK -> %ls (%lu bytes, %s)",
            savePath, (unsigned long)buf.size(), req.Is64 ? "x64" : "x86");
        return true;
    }
}
