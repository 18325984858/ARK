#pragma once

#include <Windows.h>
#include <vector>
#include <string>

// 内核 inline-hook 类型
enum class KernelHookType
{
    None,           // 未检测到 hook（在结果列表中不会出现，仅供内部使用）
    JmpRel32,       // E9 rel32 直接 jmp
    JmpAbs,         // FF 25 [rip+disp32] 通过指针跳转
    MovJmpAbs,      // 48 B8 imm64 ; FF E0 (mov rax, imm64; jmp rax)
    Push64Ret,      // 68 imm32 ; C3   或 PUSH/RET 序列
    Int3,           // CC INT3 调试中断
    Retn,           // C3 RET 直接禁用
    Suspicious      // 起始字节与已知合法 prologue 模式都不匹配，但确实改了
};

struct KernelHookFinding
{
    std::wstring funcName;          // 例如 "NtCreateFile"
    ULONG64      kva;               // 函数在内核中的 VA
    UCHAR        bytes[16];         // 内存里读到的前 16 字节（hook 之后）
    UCHAR        diskBytes[16];     // 磁盘 PE 上同位置的前 16 字节（hook 之前的原始）
    UCHAR        valid;             // 驱动是否成功读到字节
    KernelHookType type;
    ULONG64      jumpTarget;        // 解析出的跳转目标 KVA（不适用时 0）
    std::wstring jumpTargetDesc;    // 跳转目标的 "module!Symbol+0xN" 或 raw 0x...
    std::wstring modulePath;        // 所在模块路径（一般 ntoskrnl）
};

// 同步扫描内核钩子。会阻塞当前线程几百毫秒（IPC + 解析）。
// 返回值：检测到 hook 的项数（已写入 out）。未发现 hook 的不会进 out。
size_t DetectKernelHooks(std::vector<KernelHookFinding>& out);

// 扫描统计（供 UI 显示）
struct KernelHookScanStats
{
    unsigned watchListCount;    // 关注列表总数
    unsigned resolvedSymbols;   // 成功解析到 KVA 的符号数
    unsigned readableEntries;   // 驱动成功读到字节的项数
    unsigned hookCount;         // 检测到 hook 的项数
};

// 同上但返回扫描统计。out 仍只放检测到的 hook。
size_t DetectKernelHooksEx(std::vector<KernelHookFinding>& out, KernelHookScanStats& stats);

// 全 .text 段扫描：把磁盘上的 ntoskrnl.exe 与内核内存逐字节比对，
// 任何差异点都用 Capstone 反汇编识别 hook 类型，并通过 PDB 反查所在函数名。
// 比 DetectKernelHooks 慢（~5MB 内存读 + 比对），但能发现 mid-function hook。
struct KernelTextScanStats
{
    unsigned   textSize;        // .text 段大小（字节）
    unsigned   readBytes;       // 实际从内核读到的字节
    unsigned   diffRuns;        // 不同字节连续段的数量
    unsigned   hookCount;       // 最终判定为 hook 的项数
};

size_t DetectKernelTextHooks(std::vector<KernelHookFinding>& out, KernelTextScanStats& stats);

