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
