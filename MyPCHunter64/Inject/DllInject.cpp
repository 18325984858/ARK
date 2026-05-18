// MyPCHunter64/Inject/DllInject.cpp
// R3 端 thin wrapper：组 CDllInjectInfo，发 IPC，把结果翻成中文。

#include "../pch.h"
#include "DllInject.h"
#include "../CLoadDriver.h"
#include "../../MyDriver64/Struct.h"

extern _LoadDriver g_LoadDriver;
extern void AppLog_Write(const char* level, const char* fmt, ...);
#define ILOG(...) AppLog_Write("INFO ", __VA_ARGS__)
#define IERR(...) AppLog_Write("ERROR", __VA_ARGS__)

namespace DllInject
{
    const wchar_t* StatusToText(unsigned long status)
    {
        switch (status)
        {
        case DLLINJECT_STATUS_OK:           return L"成功";
        case DLLINJECT_STATUS_BAD_PARAM:    return L"参数错误";
        case DLLINJECT_STATUS_BAD_PROCESS:  return L"目标进程无效 / 已退出";
        case DLLINJECT_STATUS_NO_LDR:       return L"目标进程未初始化 PEB/Ldr";
        case DLLINJECT_STATUS_NO_K32:       return L"目标进程内未找到 kernel32";
        case DLLINJECT_STATUS_NO_LOADLIB:   return L"未找到 LoadLibraryW 导出（hash 不匹配）";
        case DLLINJECT_STATUS_ALLOC_FAIL:   return L"目标 VAS 分配失败";
        case DLLINJECT_STATUS_NO_THREAD:    return L"目标进程无可注入的用户线程";
        case DLLINJECT_STATUS_INSERT_FAIL:  return L"APC 入队失败";
        }
        return L"未知错误";
    }

    bool InjectDll(const wchar_t* eprocessHex,
                   const wchar_t* dllPath,
                   Result& outResult,
                   std::wstring& errMsg)
    {
        errMsg.clear();
        if (!eprocessHex || !*eprocessHex || !dllPath || !*dllPath)
        {
            errMsg = L"参数错误"; return false;
        }
        size_t plen = wcslen(dllPath);
        if (plen == 0 || plen >= 260)
        {
            errMsg = L"DLL 路径长度非法"; return false;
        }
        ULONG64 eproc = _wcstoui64(eprocessHex, nullptr, 16);
        if (eproc == 0) { errMsg = L"EPROCESS 解析失败"; return false; }

        CDllInjectInfo req = { 0 };
        req.Eprocess = eproc;
        req.PathLen  = (ULONG)plen;
        wmemcpy(req.DllPath, dllPath, plen);

        g_LoadDriver.SendMsg(um_Cmd_Inject_Dll_info, &req, nullptr, nullptr, nullptr);

        outResult.status      = req.Status;
        outResult.queuedCount = req.QueuedCount;
        outResult.is32        = req.Is32 != 0;

        ILOG("[DllInject] status=%lu queued=%lu seen=%lu sys=%lu apcFail=%lu is32=%u path=%ls",
            (unsigned long)req.Status, (unsigned long)req.QueuedCount,
            (unsigned long)req.ThreadsSeen, (unsigned long)req.ThreadsSystem,
            (unsigned long)req.ApcInsertFail,
            (unsigned)req.Is32, dllPath);

        if (req.Status != DLLINJECT_STATUS_OK)
        {
            wchar_t diag[256];
            _snwprintf_s(diag, _countof(diag), _TRUNCATE,
                L"%ls\n[诊断] 枚举线程=%lu 系统=%lu KeInsertQueueApc 拒绝=%lu",
                StatusToText(req.Status),
                (unsigned long)req.ThreadsSeen,
                (unsigned long)req.ThreadsSystem,
                (unsigned long)req.ApcInsertFail);
            errMsg = diag;
            return false;
        }
        return true;
    }

    const wchar_t* ManualStatusToText(unsigned long status)
    {
        switch (status)
        {
        case MMAP_STATUS_OK:             return L"成功";
        case MMAP_STATUS_BAD_PARAM:      return L"参数错误";
        case MMAP_STATUS_BAD_PROCESS:    return L"目标进程无效";
        case MMAP_STATUS_FILE_FAIL:      return L"读 DLL 文件失败（路径错或权限不足）";
        case MMAP_STATUS_BAD_PE:         return L"PE 头不合法 / 非 64 位 DLL";
        case MMAP_STATUS_X86_NOT_IMPL:   return L"目标是 wow64 进程，本版 manual map 仅支持 x64";
        case MMAP_STATUS_ALLOC_FAIL:     return L"目标 VAS 分配失败";
        case MMAP_STATUS_RELOC_FAIL:     return L"基址重定位失败";
        case MMAP_STATUS_IMPORT_FAIL:    return L"依赖的 DLL 未在目标进程加载";
        case MMAP_STATUS_PROTECT_FAIL:   return L"页保护设置失败";
        case MMAP_STATUS_SHELLCODE_FAIL: return L"shellcode 分配失败";
        case MMAP_STATUS_NO_THREAD:      return L"无可注入的用户线程";
        case MMAP_STATUS_INSERT_FAIL:    return L"APC 入队失败";
        case MMAP_STATUS_HIJACK_OPENHND: return L"无法打开线程内核句柄（Context API 解析失败或权限不足）";
        case MMAP_STATUS_HIJACK_SUSPEND: return L"ZwSuspendThread 失败";
        case MMAP_STATUS_HIJACK_GETCTX:  return L"ZwGetContextThread 失败";
        case MMAP_STATUS_HIJACK_SETCTX:  return L"ZwSetContextThread 失败";
        case MMAP_STATUS_HIJACK_APIFAIL: return L"Context/Suspend API 解析失败（MmGetSystemRoutineAddress 与导出表均未命中）";
        }
        return L"未知错误";
    }

    static bool DoManualMapImpl(const wchar_t* eprocessHex,
                                const wchar_t* dllPath,
                                unsigned long mode,
                                ManualResult& outResult,
                                std::wstring& errMsg)
    {
        errMsg.clear();
        if (!eprocessHex || !*eprocessHex || !dllPath || !*dllPath)
        { errMsg = L"参数错误"; return false; }
        size_t plen = wcslen(dllPath);
        if (plen == 0 || plen >= 260) { errMsg = L"DLL 路径长度非法"; return false; }
        ULONG64 eproc = _wcstoui64(eprocessHex, nullptr, 16);
        if (eproc == 0) { errMsg = L"EPROCESS 解析失败"; return false; }

        CManualMapInfo req = { 0 };
        req.Eprocess = eproc;
        req.PathLen  = (ULONG)plen;
        req.Mode     = mode;
        wmemcpy(req.DllPath, dllPath, plen);

        g_LoadDriver.SendMsg(um_Cmd_Inject_Dll_Manual_info, &req, nullptr, nullptr, nullptr);

        outResult.status      = req.Status;
        outResult.queuedCount = req.QueuedCount;
        outResult.moduleBase  = req.ModuleBase;
        outResult.entryPoint  = req.EntryPoint;
        outResult.sizeOfImage = req.SizeOfImage;
        outResult.failedImportDll = req.FailedImportDll;
        outResult.originalRip = req.OriginalRip;
        outResult.threadsSeen   = (unsigned long)req.ThreadsSeen;
        outResult.threadsSystem = (unsigned long)req.ThreadsSystem;
        outResult.apcFails      = (unsigned long)req.ApcInsertFail;

        ILOG("[ManualMap mode=%lu] status=%lu queued=%lu seen=%lu sys=%lu apcFail=%lu base=0x%016llX entry=0x%016llX origRip=0x%016llX",
            mode,
            (unsigned long)req.Status, (unsigned long)req.QueuedCount,
            (unsigned long)req.ThreadsSeen, (unsigned long)req.ThreadsSystem,
            (unsigned long)req.ApcInsertFail,
            (unsigned long long)req.ModuleBase, (unsigned long long)req.EntryPoint,
            (unsigned long long)req.OriginalRip);

        if (req.Status != MMAP_STATUS_OK)
        {
            std::wstring t = ManualStatusToText(req.Status);
            if (req.Status == MMAP_STATUS_IMPORT_FAIL && req.FailedImportDll[0])
            {
                t += L"\n失败 dll: ";
                t += req.FailedImportDll;
            }
            wchar_t diag[256];
            _snwprintf_s(diag, _countof(diag), _TRUNCATE,
                L"\n[诊断] 枚举线程=%lu 系统=%lu KeInsertQueueApc 拒绝=%lu",
                (unsigned long)req.ThreadsSeen,
                (unsigned long)req.ThreadsSystem,
                (unsigned long)req.ApcInsertFail);
            errMsg = t + diag;
            return false;
        }
        return true;
    }

    bool ManualMapDll(const wchar_t* eprocessHex,
                      const wchar_t* dllPath,
                      ManualResult& outResult,
                      std::wstring& errMsg)
    {
        return DoManualMapImpl(eprocessHex, dllPath, MMAP_MODE_APC, outResult, errMsg);
    }

    bool HijackInjectDll(const wchar_t* eprocessHex,
                         const wchar_t* dllPath,
                         ManualResult& outResult,
                         std::wstring& errMsg)
    {
        return DoManualMapImpl(eprocessHex, dllPath, MMAP_MODE_HIJACK, outResult, errMsg);
    }
}
