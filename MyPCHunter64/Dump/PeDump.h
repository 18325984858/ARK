#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include "ImportRebuilder.h"

namespace PeDump
{
    // 从远程进程 dump 一个加载好的 PE 模块到磁盘文件，并尝试重建 IAT。
    //
    //   eprocessHex : 目标进程 EPROCESS 指针的十六进制字符串
    //                 （沿用 DlgProcessModule::m_StrEprocess 的传参形式）
    //   imageBase   : 模块在目标进程地址空间的基址
    //   modules     : 目标进程**全部**已加载模块（含被 dump 的那个本身），
    //                 由 R3 调用方从已经展示模块列表的 DlgProcessModule 收集后传入；
    //                 用于 IAT 重建反查"指针 → module!func"。可以为空 → 跳过 IAT 重建。
    //   savePath    : 输出文件路径
    //   rebuildOut  : out — 重建统计；如果 IAT 重建失败，rebuildOut.errMsg 非空
    //                 但 PE 文件本身仍按"raw memory snapshot"形式写盘
    //   errMsg      : 失败时输出错误信息
    //
    // 返回 true 表示 PE 文件已写入。IAT 重建是 best-effort，失败不影响文件写盘。
    struct RebuildResult
    {
        bool                     attempted = false;
        bool                     success   = false;
        std::wstring             errMsg;
        ImportRebuilder::Stats   stats;
        std::wstring             iatLogPath;   // 若写了 .iat.log，这里给出路径
    };

    bool DumpModuleFromProcess(const wchar_t* eprocessHex,
                               unsigned long long imageBase,
                               const std::vector<ImportRebuilder::LoadedModule>& modules,
                               const wchar_t* savePath,
                               RebuildResult& rebuildOut,
                               std::wstring& errMsg);
}
