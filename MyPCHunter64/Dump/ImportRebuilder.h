#pragma once
#include <Windows.h>
#include <string>
#include <vector>

// IAT 重建（Scylla 风格的简化版）：在已经 dump 到 buf 里的 PE 上原地重建
// IMPORT 目录，使得 dumped image 仍然能被 loader 加载 / 被 IDA 识别为导入。
//
// 工作原理简述：
//   1) 通过 OptionalHeader.DataDirectory[IAT] 拿到原 IAT 区域；
//      如果该项被加壳擦掉，则在 .rdata 节里扫描"连续 N 个指针，每个都
//      落在某个加载模块的 [base, base+size) 范围内"作为 IAT 边界。
//   2) 把每个 IAT 条目的绝对地址反查回 module.dll + ExportRVA → 导出名。
//      落在未知模块（加壳分配的 stub 区域）的条目 → 记为 unresolved，
//      该位置写 0，并在 .iat.log 报告里列出。
//   3) 在 buf 末尾新增一个节 ".impnew"，把
//        IMAGE_IMPORT_DESCRIPTOR[N+1]  +  INT (per 模块)  +  IMAGE_IMPORT_BY_NAME  +  DLL 名字
//      塞进去；FirstThunk 指回原 IAT 区域（loader 加载时往那里写解析地址）。
//   4) 更新 OptionalHeader.DataDirectory[IMPORT] / SizeOfImage / NumberOfSections。
//
// 不做：
//   - VMProtect/Themida 的 trampoline 反追溯（unresolved 条目交给用户人工补）
//   - OEP 检测
//   - 重定位重建

namespace ImportRebuilder
{
    struct LoadedModule
    {
        std::wstring        name;       // basename: "kernel32.dll"
        std::wstring        fullPath;   // 完整路径，用于本地 map 解析导出表
        unsigned long long  base;       // 模块在目标进程的基址
        unsigned long long  size;       // SizeOfImage（来自模块表）
    };

    struct Stats
    {
        unsigned long totalIatEntries = 0;
        unsigned long resolved        = 0;
        unsigned long unresolved      = 0;
        unsigned long modulesUsed     = 0;
        unsigned long iatRegions      = 0;     // 检出的 IAT 区段数
        bool          autoLocated     = false; // true=DataDirectory 缺失走了扫描
    };

    // 在 dumped buf 上重建导入表。
    //   buf               in/out  dumped PE，可能被 resize 扩大（新增 .impnew 节）
    //   originalImageBase in      目标进程加载基址（用于把 IAT 绝对指针 → 远程 VA 比对）
    //   modules           in      目标进程**全部**已加载模块（含被 dump 的那个本身）
    //   iatLogPath        in      可写日志路径（空字符串 = 不写日志）
    //   stats             out     统计 + 诊断
    //   errMsg            out     失败时的错误描述
    // 返回 true 表示新 IMPORT 目录已写入 buf；false 表示完全失败（buf 不被破坏）。
    bool RebuildImports(std::vector<unsigned char>& buf,
                        unsigned long long originalImageBase,
                        const std::vector<LoadedModule>& modules,
                        const std::wstring& iatLogPath,
                        Stats& stats,
                        std::wstring& errMsg);
}
