#pragma once
#include <Windows.h>
#include <string>

namespace DllInject
{
    struct Result
    {
        unsigned long status      = 0;      // DLLINJECT_STATUS_*
        unsigned long queuedCount = 0;
        bool          is32        = false;
    };

    // 通过驱动 IPC 把 dllPath 注入到目标进程（EPROCESS 字符串形式，与 DlgProcessModule
    // 的 m_StrEprocess 一致）。
    //   不使用 CreateRemoteThread / WriteProcessMemory，全部在内核侧通过 APC 完成。
    bool InjectDll(const wchar_t* eprocessHex,
                   const wchar_t* dllPath,
                   Result& outResult,
                   std::wstring& errMsg);

    // 把驱动返回的状态码翻译成中文描述
    const wchar_t* StatusToText(unsigned long status);

    // 第 3 档：完整 manual map。零 LDR 痕迹，绕过 LoadLibraryW / LdrLoadDll hook。
    // DLL 必须用 /ENTRY:DllMain 链接，且 import 仅依赖目标已加载模块；仅 x64 目标。
    struct ManualResult
    {
        unsigned long       status      = 0;     // MMAP_STATUS_*
        unsigned long       queuedCount = 0;
        unsigned long long  moduleBase  = 0;
        unsigned long long  entryPoint  = 0;
        unsigned long       sizeOfImage = 0;
        std::wstring        failedImportDll;
        unsigned long long  originalRip = 0;     // HIJACK 模式下记录被劫持线程原 Rip
        unsigned long       threadsSeen    = 0;
        unsigned long       threadsSystem  = 0;
        unsigned long       apcFails       = 0;
    };
    bool ManualMapDll(const wchar_t* eprocessHex,
                      const wchar_t* dllPath,
                      ManualResult& outResult,
                      std::wstring& errMsg);
    const wchar_t* ManualStatusToText(unsigned long status);

    // 第 C 档：线程劫持。复用 manual map 的映射/重定位/IAT 解析；不走 APC，
    // 而是 ObOpenObjectByPointer + ZwSuspendThread + ZwGet/SetContextThread 改 Rip。
    // 对 M365Copilot 这种 APC 被拒的强化进程有效。
    bool HijackInjectDll(const wchar_t* eprocessHex,
                         const wchar_t* dllPath,
                         ManualResult& outResult,
                         std::wstring& errMsg);
}
