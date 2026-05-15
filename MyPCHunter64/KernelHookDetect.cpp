#include "pch.h"
#include "KernelHookDetect.h"
#include "PdbResolver.h"
#include "CLoadDriver.h"
#include "MyPCHunter64.h"   // LOGI/LOGW
#include "../MyDriver64/Struct.h"

#include <Psapi.h>
#pragma comment(lib, "psapi.lib")
#include <algorithm>
#include "include/capstone-5.0-Release/include/capstone/capstone.h"
#pragma comment(lib, "include/capstone-5.0-Release/capstone.lib")

// ----- 关注的 ntoskrnl 内核函数清单 ----------------------------------------
// 这些函数容易被 rootkit / 反作弊 / EDR 直接 inline hook（不走 SSDT）。
// 都是 ntoskrnl 在 PDB 中的真实导出/内部符号名。
static const wchar_t* const kNtoskrnlWatchList[] = {
    // 进程
    L"PsLookupProcessByProcessId",  L"PsLookupThreadByThreadId",
    L"PsCreateSystemThread",        L"PsSetCreateProcessNotifyRoutine",
    L"PsSetCreateProcessNotifyRoutineEx", L"PsSetCreateThreadNotifyRoutine",
    L"PsSetLoadImageNotifyRoutine", L"PspExitThread", L"PspExitProcess",
    // 对象 / 句柄
    L"ObOpenObjectByPointer",       L"ObOpenObjectByName",
    L"ObReferenceObjectByHandle",   L"ObReferenceObjectByName",
    L"ObRegisterCallbacks",         L"ObUnRegisterCallbacks",
    // I/O
    L"IoCreateFile",                L"IoCreateDevice",      L"IoAttachDevice",
    L"IoRegisterDeviceInterface",   L"IofCallDriver",       L"IofCompleteRequest",
    L"IoCreateDriver",
    // 注册表
    L"CmRegisterCallback",          L"CmRegisterCallbackEx",
    L"CmUnRegisterCallback",
    // 内存
    L"MmMapIoSpace",                L"MmGetSystemRoutineAddress",
    L"MmAllocateContiguousMemory",  L"MmProbeAndLockPages",
    L"MmMapLockedPagesSpecifyCache",
    L"ExAllocatePool",              L"ExAllocatePoolWithTag",
    L"ExFreePool",                  L"ExFreePoolWithTag",
    // 调度 / 同步
    L"KeAttachProcess",             L"KeStackAttachProcess",
    L"KeUnstackDetachProcess",      L"KeDetachProcess",
    L"KeServiceDescriptorTable",
    // 调试器 / 异常
    L"KdEnableDebugger",            L"KdDisableDebugger",
    L"DbgkLkmdRegisterCallback",
    // Nt* / Zw*: 部分关键 syscall（SSDT tab 已展示，但有时 EDR 会改 Nt* 入口而不改 KiServiceTable）
    L"NtCreateFile",                L"NtOpenFile",
    L"NtReadFile",                  L"NtWriteFile",
    L"NtDeviceIoControlFile",
    L"NtCreateProcess",             L"NtCreateProcessEx",   L"NtOpenProcess",
    L"NtTerminateProcess",          L"NtCreateThread",      L"NtCreateThreadEx",
    L"NtQuerySystemInformation",    L"NtSetSystemInformation",
    L"NtLoadDriver",                L"NtUnloadDriver",
    L"NtMapViewOfSection",          L"NtUnmapViewOfSection",
    L"NtAllocateVirtualMemory",     L"NtFreeVirtualMemory",
    L"NtProtectVirtualMemory",      L"NtReadVirtualMemory", L"NtWriteVirtualMemory",
    L"NtCreateKey",                 L"NtDeleteKey",         L"NtSetValueKey",
    L"NtOpenKey",                   L"NtQueryValueKey",
};

// 检查是否是 hot-patch prologue（mov edi, edi 等 NOP-like），属于正常字节
static bool IsBenignProlog(const UCHAR* b)
{
    // 常见正常 prologue：
    //  48 89 5C 24 ..   mov [rsp+..], rbx
    //  48 83 EC ..      sub rsp, imm8
    //  48 81 EC ..      sub rsp, imm32
    //  4C 8B DC         mov r11, rsp
    //  40 53            push rbx
    //  40 55 .. 41 .. 41 ..  push rbp + push rNN
    //  4C 89 ..         mov [..], rXX
    //  8B FF            mov edi, edi  (hot-patch slot)
    if (b[0] == 0x48 && (b[1] == 0x89 || b[1] == 0x83 || b[1] == 0x81 || b[1] == 0x8B)) return true;
    if (b[0] == 0x4C && (b[1] == 0x8B || b[1] == 0x89)) return true;
    if (b[0] == 0x40 || b[0] == 0x41 || b[0] == 0x55 || b[0] == 0x53 || b[0] == 0x56 || b[0] == 0x57) return true;
    if (b[0] == 0x8B && b[1] == 0xFF) return true;
    if (b[0] == 0x33 || b[0] == 0x31) return true; // xor reg, reg
    if (b[0] == 0xB8 || b[0] == 0xB9 || b[0] == 0xBA) return true; // mov eax/ecx/edx, imm32
    if (b[0] == 0x65) return true; // gs: prefix (访问 KPCR 常见)
    if (b[0] == 0x0F) return true; // 多种合法 2-byte 操作码
    if (b[0] == 0xE8) return true; // call rel32 ✗ 通常不会在 prologue 但有些 ntoskrnl 函数确实 CALL 开头(/Gh _penter)
    if (b[0] == 0xCC && b[1] == 0xCC) return false; // 全 CC 视为 INT3 hook
    return false;
}

// 分析前若干字节，返回 hook 类型；若不是 hook 返回 None。
// hookTarget 输出：仅 JmpRel32 / JmpAbs / MovJmpAbs 有效。
static KernelHookType ClassifyHook(ULONG64 funcVa, const UCHAR* b, ULONG64& hookTarget)
{
    hookTarget = 0;

    // 第一类：CC = INT3
    if (b[0] == 0xCC)
        return KernelHookType::Int3;

    // 第二类：C3 = 直接 RET（极少作为合法 prologue）
    if (b[0] == 0xC3)
        return KernelHookType::Retn;

    // 第三类：E9 rel32 = JMP near
    if (b[0] == 0xE9)
    {
        LONG rel = *(LONG*)(b + 1);
        hookTarget = funcVa + 5 + (LONG64)rel;
        return KernelHookType::JmpRel32;
    }

    // 第四类：FF 25 disp32 = JMP [RIP+disp32]
    if (b[0] == 0xFF && b[1] == 0x25)
    {
        LONG disp = *(LONG*)(b + 2);
        ULONG64 ptrLoc = funcVa + 6 + (LONG64)disp;
        hookTarget = ptrLoc; // 真正目标需再 deref，但先返回 ptrLoc
        return KernelHookType::JmpAbs;
    }

    // 第五类：48 B8 imm64 ; FF E0  =  mov rax, imm64 ; jmp rax  (14 字节，刚好放进 16)
    if (b[0] == 0x48 && b[1] == 0xB8 && b[10] == 0xFF && b[11] == 0xE0)
    {
        hookTarget = *(ULONG64*)(b + 2);
        return KernelHookType::MovJmpAbs;
    }
    // 变体：49 BB imm64 ; FF E3 (mov r11, imm64; jmp r11)
    if (b[0] == 0x49 && (b[1] == 0xB8 || b[1] == 0xBB) && b[10] == 0xFF && (b[11] == 0xE0 || b[11] == 0xE3))
    {
        hookTarget = *(ULONG64*)(b + 2);
        return KernelHookType::MovJmpAbs;
    }

    // 第六类：68 imm32 ; C3 = push imm32; ret  (PUSH/RET 是 32 位 hook 写法，在 x64 较少)
    if (b[0] == 0x68 && b[5] == 0xC3)
    {
        hookTarget = (ULONG64)(LONG64)(LONG)(*(LONG*)(b + 1));
        return KernelHookType::Push64Ret;
    }

    // 其他形态：起始字节既不像 hook 也不像 prologue 模式，但还活着 → Suspicious
    if (!IsBenignProlog(b))
        return KernelHookType::Suspicious;

    return KernelHookType::None;
}

size_t DetectKernelHooks(std::vector<KernelHookFinding>& out)
{
    KernelHookScanStats unused = { 0 };
    return DetectKernelHooksEx(out, unused);
}

size_t DetectKernelHooksEx(std::vector<KernelHookFinding>& out, KernelHookScanStats& stats)
{
    LOGI("[KHK] DetectKernelHooksEx BEGIN");
    out.clear();
    stats.watchListCount = (unsigned)_countof(kNtoskrnlWatchList);
    stats.resolvedSymbols = 0;
    stats.readableEntries = 0;
    stats.hookCount = 0;

    // 1. 确认 ntoskrnl 基址已知（PdbResolver 启动时会预加载）
    ULONG64 ntBase = PdbResolver_GetModuleBase(L"ntoskrnl.exe");
    LOGI("[KHK] ntBase=0x%I64X", ntBase);
    if (ntBase == 0)
    {
        LOGW("[KHK] ntoskrnl base unknown; PDB resolver not ready");
        return 0;
    }

    // 2. 取 ntoskrnl 完整路径（用于 PdbResolver_Resolve 反查跳转目标符号名时使用）
    std::wstring ntPath;
    {
        wchar_t winDir[MAX_PATH] = { 0 };
        GetSystemDirectoryW(winDir, MAX_PATH);
        ntPath = winDir;
        ntPath += L"\\ntoskrnl.exe";
    }

    // 3. 解析每个函数名 → KVA，丢弃未命中的
    constexpr size_t kListCount = _countof(kNtoskrnlWatchList);
    static_assert(kListCount <= MAX_KHK_PROBE, "watch list too big for one IPC");

    auto probe = std::make_unique<CKernelProbeHeader>();
    RtlZeroMemory(probe.get(), sizeof(*probe));

    struct Slot { const wchar_t* name; ULONG64 kva; };
    std::vector<Slot> slots;
    slots.reserve(kListCount);

    for (auto name : kNtoskrnlWatchList)
    {
        ULONG64 kva = PdbResolver_GetSymbolKva(L"ntoskrnl.exe", name);
        if (kva == 0) continue;
        if (probe->Count >= MAX_KHK_PROBE) break;
        probe->Items[probe->Count].Addr = kva;
        slots.push_back({ name, kva });
        ++probe->Count;
    }
    stats.resolvedSymbols = probe->Count;
    LOGI("[KHK] resolved %u/%zu watch symbols", (unsigned)probe->Count, kListCount);
    if (probe->Count == 0) return 0;

    // 4. 一次 IPC 让驱动读所有地址的前 16 字节
    extern _LoadDriver g_LoadDriver;
    LOGI("[KHK] sending Probe IPC, count=%u", (unsigned)probe->Count);
    g_LoadDriver.SendMsg(um_Cmd_Probe_KernelMemory_info, probe.get(), nullptr, nullptr, nullptr);
    LOGI("[KHK] Probe IPC returned");

    // 5. 逐项分析
    for (ULONG i = 0; i < probe->Count; ++i)
    {
        const auto& it = probe->Items[i];
        if (!it.Valid) continue;
        ++stats.readableEntries;

        ULONG64 tgt = 0;
        KernelHookType type = ClassifyHook(it.Addr, it.Bytes, tgt);
        if (type == KernelHookType::None) continue;

        KernelHookFinding rec;
        rec.funcName = slots[i].name;
        rec.kva = it.Addr;
        memcpy(rec.bytes, it.Bytes, 16);
        rec.valid = it.Valid;
        rec.type = type;
        rec.jumpTarget = tgt;
        rec.modulePath = ntPath;

        // 解析跳转目标到 modulename!Func+0xN（仅在 tgt 看起来是内核 VA 时）
        if (tgt != 0 && (tgt >> 40) >= 0xFF)
        {
            // 找到 tgt 所在的内核模块
            // 简化：让 PdbResolver_Resolve 自己用 modulePath="ntoskrnl.exe" 算 RVA；
            // 如果 tgt 不在 ntoskrnl 范围内，PdbResolver_Resolve 会兜底返回 0x... 原值。
            wchar_t resolved[256] = { 0 };
            // 这里 modulePath 传 ntPath，让 Resolve 内部 lookup 自动判定模块。
            // 注意：PdbResolver_Resolve 需要传"该地址所在模块"信息；
            //       目标可能根本不在 ntoskrnl。简单做法：传空 modulePath 让它仅显示 raw 地址，
            //       后续单独再加一个 R3 端"按 kAddr 反查所有模块名"的接口。
            _snwprintf_s(resolved, _countof(resolved), _TRUNCATE, L"0x%016I64X", tgt);
            rec.jumpTargetDesc = resolved;
        }
        else if (tgt != 0)
        {
            wchar_t buf[64];
            _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"0x%016I64X", tgt);
            rec.jumpTargetDesc = buf;
        }

        out.push_back(std::move(rec));
    }

    LOGI("[KHK] detected %zu hooks out of %u probed", out.size(), (unsigned)probe->Count);
    stats.hookCount = (unsigned)out.size();
    return out.size();
}

// ---------- 全 .text 段扫描 -------------------------------------------------

// 用 Capstone 反汇编 bytes 起始的一条指令，返回 mnemonic + op_str，例如 "jmp 0x..." / "int3"。
// 失败返回空字符串。
static std::wstring DisasmOneAt(ULONG64 va, const UCHAR* bytes, size_t len)
{
    static csh s_handle = 0;
    static bool s_opened = false;
    if (!s_opened)
    {
        if (cs_open(CS_ARCH_X86, CS_MODE_64, &s_handle) == CS_ERR_OK)
        {
            s_opened = true;
        }
        else
        {
            return L"(capstone init failed)";
        }
    }

    cs_insn* insn = nullptr;
    size_t cnt = cs_disasm(s_handle, bytes, len, va, 1, &insn);
    if (cnt == 0)
    {
        return L"(undecodable)";
    }

    wchar_t buf[128];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%hs %hs", insn[0].mnemonic, insn[0].op_str);
    cs_free(insn, cnt);
    return buf;
}

// 在磁盘 PE 中找到 ".text" 段：返回该段在内存映像中的 RVA、字节数，以及该段在文件内的偏移。
static bool FindTextSection(PBYTE peBase, ULONG& outRva, ULONG& outVSize, ULONG& outFOff)
{
    auto* dos = (PIMAGE_DOS_HEADER)peBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = (PIMAGE_NT_HEADERS64)(peBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return false;

    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (USHORT i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
    {
        if (memcmp(sec->Name, ".text", 5) == 0)
        {
            outRva   = sec->VirtualAddress;
            outVSize = sec->Misc.VirtualSize;
            outFOff  = sec->PointerToRawData;
            return true;
        }
    }
    return false;
}

// 一次 IPC 从内核读最多 MAX_KRD_PER_CALL 字节到 R3 缓冲区。返回实际读到字节数。
static ULONG ReadKernelChunk(ULONG64 kAddr, PVOID userBuf, ULONG length)
{
    CKernelRangeReadInfo req = { 0 };
    req.KernelAddr = kAddr;
    req.Length     = length;
    req.UserBuf    = userBuf;

    extern _LoadDriver g_LoadDriver;
    g_LoadDriver.SendMsg(um_Cmd_Read_KernelRange_info, &req, nullptr, nullptr, nullptr);
    return req.BytesRead;
}

// 根据起始字节模式判定 hook 类型（复用上面 ClassifyHook 的逻辑，但对 mid-function hook 也接受）
static KernelHookType ClassifyHookBytes(ULONG64 kva, const UCHAR* b, size_t len, ULONG64& outTarget)
{
    if (len < 12) { outTarget = 0; return KernelHookType::Suspicious; }
    return ClassifyHook(kva, b, outTarget);
}

size_t DetectKernelTextHooks(std::vector<KernelHookFinding>& out, KernelTextScanStats& stats)
{
    LOGI("[KHK-FULL] DetectKernelTextHooks BEGIN (per-function mode)");
    out.clear();
    stats = { 0 };

    // 1. 拿到 ntoskrnl 内核基址
    const ULONG64 ntBase = PdbResolver_GetModuleBase(L"ntoskrnl.exe");
    if (ntBase == 0)
    {
        LOGW("[KHK-FULL] ntoskrnl base unknown");
        return 0;
    }
    wchar_t winDir[MAX_PATH] = { 0 };
    GetSystemDirectoryW(winDir, MAX_PATH);
    std::wstring diskPath = winDir;
    diskPath += L"\\ntoskrnl.exe";

    // 2. 把磁盘上的 ntoskrnl.exe map 进来
    HANDLE hFile = CreateFileW(diskPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        LOGW("[KHK-FULL] open ntoskrnl.exe failed GLE=%lu", GetLastError());
        return 0;
    }
    HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) { CloseHandle(hFile); LOGW("[KHK-FULL] CreateFileMapping failed"); return 0; }
    PBYTE diskBase = (PBYTE)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!diskBase) { CloseHandle(hMap); CloseHandle(hFile); LOGW("[KHK-FULL] MapViewOfFile failed"); return 0; }

    struct Guard { HANDLE f, m; PBYTE p; ~Guard() { if (p) UnmapViewOfFile(p); if (m) CloseHandle(m); if (f != INVALID_HANDLE_VALUE) CloseHandle(f); } } g{ hFile, hMap, diskBase };

    // 3. 找 .text 段
    ULONG textRva = 0, textVSize = 0, textFOff = 0;
    if (!FindTextSection(diskBase, textRva, textVSize, textFOff))
    {
        LOGW("[KHK-FULL] .text section not found");
        return 0;
    }
    stats.textSize = textVSize;
    LOGI("[KHK-FULL] ntoskrnl base=0x%I64X .text RVA=0x%X size=%u file_off=0x%X",
        ntBase, textRva, textVSize, textFOff);

    // 4. 枚举 PDB 中所有 .text 内函数符号
    struct FuncEntry { std::wstring name; ULONG rva; ULONG64 kva; };
    std::vector<FuncEntry> funcs;
    funcs.reserve(20000);

    auto cb = [](const wchar_t* name, unsigned long rva, unsigned long long kva, void* ctx)
    {
        auto* v = (std::vector<FuncEntry>*)ctx;
        v->push_back({ name ? name : L"", (ULONG)rva, (ULONG64)kva });
    };
    PdbResolver_EnumKernelFunctions(L"ntoskrnl.exe", textRva, textVSize, cb, &funcs);
    if (funcs.empty())
    {
        LOGW("[KHK-FULL] no functions enumerated (PDB not ready?)");
        return 0;
    }
    // 去重：按 rva 排序，相同 rva 只保留一个（公共符号常出现别名）
    std::sort(funcs.begin(), funcs.end(),
        [](const FuncEntry& a, const FuncEntry& b) { return a.rva < b.rva; });
    funcs.erase(std::unique(funcs.begin(), funcs.end(),
        [](const FuncEntry& a, const FuncEntry& b) { return a.rva == b.rva; }), funcs.end());
    LOGI("[KHK-FULL] %zu unique function symbols in .text", funcs.size());

    // 5. 批量 IPC 读：每次最多 MAX_KHK_PROBE 个 16-byte 探针
    constexpr ULONG kProbeBytes = 16;
    extern _LoadDriver g_LoadDriver;
    auto probe = std::make_unique<CKernelProbeHeader>();

    for (size_t base = 0; base < funcs.size(); )
    {
        RtlZeroMemory(probe.get(), sizeof(*probe));
        ULONG take = (ULONG)std::min<size_t>(MAX_KHK_PROBE, funcs.size() - base);
        for (ULONG i = 0; i < take; ++i)
            probe->Items[i].Addr = funcs[base + i].kva;
        probe->Count = take;
        g_LoadDriver.SendMsg(um_Cmd_Probe_KernelMemory_info, probe.get(), nullptr, nullptr, nullptr);

        // 6. 比较每条函数入口前 16 字节
        for (ULONG i = 0; i < take; ++i)
        {
            const auto& it = probe->Items[i];
            const auto& f  = funcs[base + i];
            if (!it.Valid) continue;
            ++stats.readBytes;  // 借用统计：当作 readable functions 计数

            // 越界保护：靠近 .text 末尾时少比一些字节
            ULONG rva = f.rva;
            ULONG cmpLen = kProbeBytes;
            if (rva + cmpLen > textRva + textVSize)
                cmpLen = (textRva + textVSize) - rva;

            const UCHAR* diskAt = diskBase + textFOff + (rva - textRva);
            if (memcmp(it.Bytes, diskAt, cmpLen) == 0) continue;

            // 字节有差异 → 当作 hook 候选
            ULONG64 tgt = 0;
            KernelHookType type = ClassifyHook(f.kva, it.Bytes, tgt);
            // 即使 ClassifyHook 返回 None（极少见 - 比如 KPP 重写少数 byte 但首字节看起来仍正常），
            // 仍然记录为 Suspicious 让用户看到 byte 差异。
            if (type == KernelHookType::None) type = KernelHookType::Suspicious;

            std::wstring asm0 = DisasmOneAt(f.kva, it.Bytes, cmpLen);

            KernelHookFinding rec;
            rec.funcName = f.name;
            rec.kva = f.kva;
            RtlZeroMemory(rec.bytes, 16);
            memcpy(rec.bytes, it.Bytes, kProbeBytes);
            RtlZeroMemory(rec.diskBytes, 16);
            memcpy(rec.diskBytes, diskAt, cmpLen);
            rec.valid = 1;
            rec.type = type;
            rec.jumpTarget = tgt;
            rec.modulePath = diskPath;
            if (tgt != 0)
            {
                wchar_t buf[300];
                _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%s  ->  0x%016I64X", asm0.c_str(), tgt);
                rec.jumpTargetDesc = buf;
            }
            else
            {
                rec.jumpTargetDesc = asm0;
            }
            out.push_back(std::move(rec));
        }
        base += take;
    }

    stats.diffRuns = (unsigned)out.size();
    stats.hookCount = (unsigned)out.size();
    LOGI("[KHK-FULL] produced %u findings out of %zu functions", stats.hookCount, funcs.size());
    return out.size();
}
