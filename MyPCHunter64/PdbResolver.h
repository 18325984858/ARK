#pragma once
#include <Windows.h>

// 主窗口收到的 PDB 进度消息
// wParam = (PdbProgressMsg*)，主窗口处理完后 delete
#define WM_USER_PDB_PROGRESS (WM_USER + 0x701)

struct PdbProgressMsg
{
    // 0=Start, 1=Receive, 2=Finish (下载完成), 3=Error, 4=Loaded (PDB 完成解析就绪), 5=Queued
    int                phase;
    unsigned long long received;
    unsigned long long total;
    unsigned int       httpCode;
    wchar_t            fileName[128];  // 例如 "Wdf01000.pdb"
};

// notifyHwnd: 主对话框 hwnd，接收 WM_USER_PDB_PROGRESS
void PdbResolver_Init(HWND notifyHwnd);
void PdbResolver_Shutdown();

// 注册一个模块的 PDB（线程安全；同模块只下载一次）。fullPath 是磁盘上模块完整路径。
// 该调用立即返回；PDB 加载在后台线程中串行进行。
void PdbResolver_Request(const wchar_t* basename, const wchar_t* fullPath);

// 把内核地址解析为函数名。
//   kAddr        : 当前函数在内核地址空间的值
//   kModuleBase  : 该地址所属模块的内核基址
//   modulePath   : 模块完整路径（用于派生 basename）
//   outBuf       : 输出 "Symbol+0xN" 或 "basename+0xRVA"，从不失败
//   outBufCch    : outBuf 字符数容量
// 若该模块尚未注册，会自动 PdbResolver_Request 启动后台下载，本次返回 RVA 兜底，
// 下次再调用同一地址时若 PDB 已就绪即返回真实符号名。
void PdbResolver_Resolve(unsigned long long kAddr, unsigned long long kModuleBase,
    const wchar_t* modulePath, wchar_t* outBuf, size_t outBufCch);

// 查询某个模块的内核加载基址。未加载/未识别返回 0。
unsigned long long PdbResolver_GetModuleBase(const wchar_t* basename);

// 根据函数名（必须是 PDB 中的符号，例如 "NtCreateFile"）返回该函数在内核中的 KVA。
// 0 = 该模块 PDB 未就绪、或该符号不存在。
unsigned long long PdbResolver_GetSymbolKva(const wchar_t* basename, const wchar_t* funcName);

// 枚举该模块 PDB 中所有"函数级"符号（Tag = SymTagFunction 或 SymTagPublicSymbol），
// 只回调那些落在 [textRvaStart, textRvaStart+textVSize) 范围内的符号。
//   basename: 模块文件名，如 L"ntoskrnl.exe"
//   textRva / textVSize: .text 段在 PE 中的 RVA 和大小，用来过滤掉 .data/.rdata 中的符号
//   cb(name, rva, kva, ctx): 回调；name 为符号名，rva 是相对模块基址的偏移，kva 是该符号当前内核 VA
// 返回回调被调用的次数。PDB 未就绪或 basename 未注册返回 0。
typedef void (*PdbFunctionCallback)(const wchar_t* name, unsigned long rva, unsigned long long kva, void* ctx);
size_t PdbResolver_EnumKernelFunctions(const wchar_t* basename,
    unsigned long textRva, unsigned long textVSize,
    PdbFunctionCallback cb, void* ctx);
