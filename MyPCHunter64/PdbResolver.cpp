#include "pch.h"
#include "PdbResolver.h"
#include "Pdb/SymLoader.h"
#include "MyPCHunter64.h"   // LOGI/LOGW/LOGE
#include <string>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <Psapi.h>
#pragma comment(lib, "psapi.lib")

// ---- 内部状态 -----------------------------------------------------------------
namespace
{
    struct Entry
    {
        std::wstring fullPath;
        std::unique_ptr<MyPdb> pdb;
        std::atomic<bool>     ready{ false };  // PDB 加载完成可解析符号
        std::atomic<bool>     failed{ false }; // 加载失败（避免反复重试）
    };

    // basename(小写) -> Entry
    std::unordered_map<std::wstring, std::unique_ptr<Entry>> g_table;
    std::mutex                  g_tableMtx;

    // 待加载队列
    std::deque<std::wstring>    g_queue;     // 存 basename
    std::mutex                  g_queueMtx;
    std::condition_variable     g_queueCv;
    std::thread                 g_worker;
    std::atomic<bool>           g_stopFlag{ false };

    HWND                        g_notifyHwnd = nullptr;

    std::wstring ToLowerCopy(const std::wstring& s)
    {
        std::wstring r = s;
        for (auto& c : r) c = (wchar_t)towlower(c);
        return r;
    }

    // 从 modulePath 抽取 basename（不含路径，保留大小写）
    std::wstring BaseNameOf(const wchar_t* path)
    {
        if (!path || !*path) return L"";
        const wchar_t* slash = wcsrchr(path, L'\\');
        return slash ? std::wstring(slash + 1) : std::wstring(path);
    }

    // 内核风格路径 → Win32 路径
    //   \??\X:\foo       -> X:\foo
    //   \SystemRoot\foo  -> %WINDIR%\foo
    // 其它 (\Device\HarddiskVolumeN\...) 暂不处理。
    std::wstring TranslateNtPath(const wchar_t* p)
    {
        if (!p || !*p) return L"";
        std::wstring s = p;

        if (s.size() >= 4 && _wcsnicmp(s.c_str(), L"\\??\\", 4) == 0)
        {
            s.erase(0, 4);
        }
        else if (s.size() >= 12 && _wcsnicmp(s.c_str(), L"\\SystemRoot\\", 12) == 0)
        {
            wchar_t winDir[MAX_PATH] = { 0 };
            UINT len = GetSystemWindowsDirectoryW(winDir, _countof(winDir));
            if (len > 0 && len < _countof(winDir))
            {
                s = std::wstring(winDir) + L"\\" + s.substr(12);
            }
        }
        return s;
    }

    // === 内核模块 basename -> 内核基址 缓存 ===
    std::unordered_map<std::wstring, ULONG64> g_modBaseCache;
    std::mutex                                g_modBaseMtx;
    std::atomic<bool>                         g_modBaseBuilt{ false };

    void BuildKernelModuleBaseCache()
    {
        std::vector<LPVOID> drivers(2048);
        DWORD cbNeeded = 0;
        for (int attempt = 0; attempt < 3; attempt++)
        {
            if (EnumDeviceDrivers(drivers.data(), (DWORD)(drivers.size() * sizeof(LPVOID)), &cbNeeded))
            {
                if (cbNeeded <= drivers.size() * sizeof(LPVOID)) break;
                drivers.resize(cbNeeded / sizeof(LPVOID) + 256);
            }
            else
            {
                LOGW("EnumDeviceDrivers failed GLE=%lu", GetLastError());
                return;
            }
        }
        DWORD count = cbNeeded / sizeof(LPVOID);

        std::lock_guard<std::mutex> lk(g_modBaseMtx);
        g_modBaseCache.clear();
        for (DWORD i = 0; i < count; i++)
        {
            wchar_t name[MAX_PATH] = { 0 };
            if (GetDeviceDriverBaseNameW(drivers[i], name, _countof(name)) > 0)
            {
                std::wstring lc = ToLowerCopy(name);
                ULONG64 addr = (ULONG64)drivers[i];
                auto it = g_modBaseCache.find(lc);
                // 同名条目（如 Win11 上 ntoskrnl.exe 出现多次）取地址最低的——真内核在启动最早，加载基址最低。
                if (it == g_modBaseCache.end() || addr < it->second)
                {
                    g_modBaseCache[lc] = addr;
                }
            }
        }
        g_modBaseBuilt.store(true, std::memory_order_release);
        LOGI("PdbResolver: kernel module base cache built (%u entries)", count);
    }

    ULONG64 LookupKernelModuleBase(const wchar_t* basename)
    {
        if (!g_modBaseBuilt.load(std::memory_order_acquire)) BuildKernelModuleBaseCache();
        if (!basename || !*basename) return 0;
        std::wstring lc = ToLowerCopy(basename);
        std::lock_guard<std::mutex> lk(g_modBaseMtx);
        auto it = g_modBaseCache.find(lc);
        if (it != g_modBaseCache.end()) return it->second;
        // 未命中：打前 20 次到 log，便于排查（比如内核以 ntkrnlmp.exe 等别名注册）
        static std::atomic<int> s_missLog{ 0 };
        if (s_missLog.fetch_add(1) < 20)
        {
            LOGW("[PDB] LookupKernelModuleBase miss '%ls' (cache size=%zu)", lc.c_str(), g_modBaseCache.size());
        }
        return 0;
    }

    // 把进度事件 PostMessage 给主窗口（堆分配，UI 处理后 delete）
    void PostProgress(const wchar_t* fileName, int phase,
        unsigned long long received, unsigned long long total, unsigned int httpCode)
    {
        if (!g_notifyHwnd) return;
        PdbProgressMsg* m = new PdbProgressMsg{};
        m->phase    = phase;
        m->received = received;
        m->total    = total;
        m->httpCode = httpCode;
        wcsncpy_s(m->fileName, _countof(m->fileName), fileName ? fileName : L"", _TRUNCATE);
        if (!PostMessageW(g_notifyHwnd, WM_USER_PDB_PROGRESS, (WPARAM)m, 0))
        {
            delete m;
        }
    }

    // SymDownloader 进度回调（从下载工作线程调用）
    void OnSymProgress(const wchar_t* fileName, int phase,
        unsigned long long received, unsigned long long total, unsigned int httpCode)
    {
        PostProgress(fileName, phase, received, total, httpCode);
    }

    // SymLoader 阶段诊断回调（来自 InitPDB 内部各步骤）
    void OnSymDiag(const wchar_t* msg)
    {
        if (msg && *msg) LOGW("[PDB] %ls", msg);
    }

    void WorkerProc()
    {
        for (;;)
        {
            std::wstring nameLc;
            {
                std::unique_lock<std::mutex> lk(g_queueMtx);
                g_queueCv.wait(lk, [] { return g_stopFlag || !g_queue.empty(); });
                if (g_stopFlag) return;
                nameLc = std::move(g_queue.front());
                g_queue.pop_front();
            }

            // 取 entry
            Entry* e = nullptr;
            std::wstring fullPath;
            std::wstring fileName;
            {
                std::lock_guard<std::mutex> lk(g_tableMtx);
                auto it = g_table.find(nameLc);
                if (it != g_table.end())
                {
                    e = it->second.get();
                    fullPath = e->fullPath;
                    fileName = nameLc;
                }
            }
            if (!e) continue;

            // 通知 UI：已开始处理（5 = Queued/Processing）
            PostProgress(fileName.c_str(), 5, 0, 0, 0);
            LOGI("[PDB] worker: BEGIN '%ls' (image='%ls')", fileName.c_str(), fullPath.c_str());

            // 同步加载（InitPDB 内部 SymDownloader 会通过 sink 推进度）
            bool ok = false;
            try
            {
                ok = e->pdb->InitPDB((PWSTR)fullPath.c_str());
            }
            catch (...) { ok = false; LOGE("[PDB] worker: InitPDB threw C++ exception for '%ls'", fileName.c_str()); }

            if (ok)
            {
                e->ready.store(true, std::memory_order_release);
                PostProgress(fileName.c_str(), 4, 0, 0, 0);
                LOGI("PdbResolver: %ls ready", fileName.c_str());
            }
            else
            {
                e->failed.store(true, std::memory_order_release);
                PostProgress(fileName.c_str(), 3, 0, 0, 0);
                LOGW("PdbResolver: %ls failed to load", fileName.c_str());
            }
        }
    }
} // anonymous

// ---- 公开接口 -----------------------------------------------------------------

void PdbResolver_Init(HWND notifyHwnd)
{
    g_notifyHwnd = notifyHwnd;
    SetPdbProgressSink(&OnSymProgress);
    SetPdbDiagSink(&OnSymDiag);
    // 预先构建内核模块基址表，避免首次解析时阻塞 UI
    BuildKernelModuleBaseCache();
    if (!g_worker.joinable())
    {
        g_stopFlag = false;
        g_worker = std::thread(&WorkerProc);
    }
}

void PdbResolver_Shutdown()
{
    {
        std::lock_guard<std::mutex> lk(g_queueMtx);
        g_stopFlag = true;
    }
    g_queueCv.notify_all();
    if (g_worker.joinable()) g_worker.join();
    SetPdbProgressSink(nullptr);
    SetPdbDiagSink(nullptr);
    g_notifyHwnd = nullptr;
}

void PdbResolver_Request(const wchar_t* basename, const wchar_t* fullPath)
{
    if (!basename || !*basename || !fullPath || !*fullPath) return;

    std::wstring nameLc = ToLowerCopy(basename);
    std::wstring win32Path = TranslateNtPath(fullPath);

    bool needEnqueue = false;
    {
        std::lock_guard<std::mutex> lk(g_tableMtx);
        auto it = g_table.find(nameLc);
        if (it == g_table.end())
        {
            auto e = std::make_unique<Entry>();
            e->fullPath = win32Path;
            e->pdb = std::make_unique<MyPdb>();
            g_table.emplace(nameLc, std::move(e));
            needEnqueue = true;
        }
    }
    if (needEnqueue)
    {
        {
            std::lock_guard<std::mutex> lk(g_queueMtx);
            g_queue.push_back(nameLc);
        }
        g_queueCv.notify_one();
        // 通知 UI：已入队（5）— 立即出现状态条
        PostProgress(basename, 5, 0, 0, 0);
    }
}

void PdbResolver_Resolve(unsigned long long kAddr, unsigned long long kModuleBase,
    const wchar_t* modulePath, wchar_t* outBuf, size_t outBufCch)
{
    if (!outBuf || outBufCch == 0) return;
    outBuf[0] = 0;

    if (kAddr == 0 || !modulePath || !*modulePath)
    {
        if (kAddr == 0) wcsncpy_s(outBuf, outBufCch, L"(null)", _TRUNCATE);
        else            _snwprintf_s(outBuf, outBufCch, _TRUNCATE, L"0x%016llX", kAddr);
        return;
    }

    std::wstring base = BaseNameOf(modulePath);
    if (base.empty())
    {
        _snwprintf_s(outBuf, outBufCch, _TRUNCATE, L"0x%016llX", kAddr);
        return;
    }

    // 调用方未提供 ModuleBase 时，从内核模块缓存里查
    if (kModuleBase == 0)
    {
        kModuleBase = LookupKernelModuleBase(base.c_str());
    }

    unsigned long long rva = (kAddr >= kModuleBase && kModuleBase != 0) ? (kAddr - kModuleBase) : 0;
    std::wstring nameLc = ToLowerCopy(base);

    Entry* e = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_tableMtx);
        auto it = g_table.find(nameLc);
        if (it != g_table.end()) e = it->second.get();
    }

    if (e && e->ready.load(std::memory_order_acquire))
    {
        WCHAR sym[256] = { 0 };
        ULONG64 disp = 0;
        ULONG64 pdbBase = e->pdb->m_mod.base();
        if (pdbBase != 0 && e->pdb->GetSymbolByAddr(pdbBase + rva, sym, _countof(sym), &disp))
        {
            if (disp != 0) _snwprintf_s(outBuf, outBufCch, _TRUNCATE, L"%s+0x%I64X", sym, disp);
            else           wcsncpy_s(outBuf, outBufCch, sym, _TRUNCATE);
            return;
        }
        // 命中 PDB 但 SymFromAddrW 找不到此地址。前 20 次写日志便于排查。
        static std::atomic<int> s_missLog{ 0 };
        if (s_missLog.fetch_add(1) < 20)
        {
            EmitPdbDiag(L"[Resolve] miss %s kAddr=0x%I64X kModBase=0x%I64X rva=0x%I64X pdbBase=0x%I64X lookup=0x%I64X gle=%lu",
                base.c_str(), kAddr, kModuleBase, (ULONG64)rva, pdbBase, pdbBase + rva, GetLastError());
        }
    }
    else if (e && !e->ready.load(std::memory_order_acquire))
    {
        // PDB 在队列里还没 ready；不打日志（每帧很多次很吵）
    }
    else if (!e)
    {
        // 未注册过：触发按需下载（不阻塞）
        PdbResolver_Request(base.c_str(), modulePath);
    }
    // 兜底：modulename+0xRVA（rva==0 时省略 "+0x0"，更整洁）
    if (rva == 0)
    {
        wcsncpy_s(outBuf, outBufCch, base.c_str(), _TRUNCATE);
    }
    else
    {
        _snwprintf_s(outBuf, outBufCch, _TRUNCATE, L"%s+0x%I64X", base.c_str(), rva);
    }
}

unsigned long long PdbResolver_GetModuleBase(const wchar_t* basename)
{
    return (unsigned long long)LookupKernelModuleBase(basename);
}

unsigned long long PdbResolver_GetSymbolKva(const wchar_t* basename, const wchar_t* funcName)
{
    if (!basename || !*basename || !funcName || !*funcName) return 0;
    std::wstring nameLc = ToLowerCopy(basename);

    Entry* e = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_tableMtx);
        auto it = g_table.find(nameLc);
        if (it != g_table.end()) e = it->second.get();
    }
    if (!e || !e->ready.load(std::memory_order_acquire) || !e->pdb) return 0;

    ULONG64 pdbBase = e->pdb->m_mod.base();
    if (pdbBase == 0) return 0;

    // 用 SymFromName 查符号 KVA（在 dbghelp 加载的合成基址空间里）
    BYTE buf[sizeof(SYMBOL_INFOW) + (MAX_SYM_NAME + 1) * sizeof(WCHAR)] = { 0 };
    PSYMBOL_INFOW info = (PSYMBOL_INFOW)buf;
    info->SizeOfStruct = sizeof(SYMBOL_INFOW);
    info->MaxNameLen = MAX_SYM_NAME;
    if (!SymFromNameW(Pdb::Prov::uid(), funcName, info)) return 0;
    if (info->Address < pdbBase) return 0;
    ULONG64 rva = info->Address - pdbBase;

    ULONG64 kModBase = LookupKernelModuleBase(basename);
    if (kModBase == 0) return 0;

    return (unsigned long long)(kModBase + rva);
}

namespace
{
    struct EnumCtx
    {
        ULONG64 pdbBase;
        ULONG64 kModBase;
        ULONG textRva;
        ULONG textVSize;
        PdbFunctionCallback userCb;
        void* userCtx;
        size_t count;
    };

    // SymEnumSymbolsW 回调。我们要的是：Tag=SymTagFunction(5) 或 SymTagPublicSymbol(10)
    // 且地址在 .text 范围内的条目。
    BOOL CALLBACK EnumKernelFunctionsCb(PSYMBOL_INFOW pInfo, ULONG /*sz*/, PVOID userCtx)
    {
        EnumCtx* c = (EnumCtx*)userCtx;
        if (!pInfo) return TRUE;
        // Tag 过滤：5=Function, 10=PublicSymbol（ntoskrnl public PDB 只能拿到这种）
        if (pInfo->Tag != 5 && pInfo->Tag != 10) return TRUE;
        if (pInfo->Address < c->pdbBase) return TRUE;
        ULONG64 rva = pInfo->Address - c->pdbBase;
        if (rva < c->textRva || rva >= (ULONG64)c->textRva + c->textVSize) return TRUE;
        if (pInfo->NameLen == 0) return TRUE;

        c->userCb(pInfo->Name, (unsigned long)rva, c->kModBase + rva, c->userCtx);
        ++c->count;
        return TRUE;
    }
}

size_t PdbResolver_EnumKernelFunctions(const wchar_t* basename,
    unsigned long textRva, unsigned long textVSize,
    PdbFunctionCallback cb, void* ctx)
{
    if (!basename || !*basename || !cb) return 0;
    std::wstring nameLc = ToLowerCopy(basename);

    Entry* e = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_tableMtx);
        auto it = g_table.find(nameLc);
        if (it != g_table.end()) e = it->second.get();
    }
    if (!e || !e->ready.load(std::memory_order_acquire) || !e->pdb)
    {
        LOGW("[PdbEnum] %ls PDB not ready", basename);
        return 0;
    }

    ULONG64 pdbBase = e->pdb->m_mod.base();
    if (pdbBase == 0) return 0;

    ULONG64 kModBase = LookupKernelModuleBase(basename);
    if (kModBase == 0)
    {
        LOGW("[PdbEnum] %ls kernel base unknown", basename);
        return 0;
    }

    EnumCtx ec{ pdbBase, kModBase, textRva, textVSize, cb, ctx, 0 };
    if (!SymEnumSymbolsW(Pdb::Prov::uid(), pdbBase, L"*", EnumKernelFunctionsCb, &ec))
    {
        LOGW("[PdbEnum] SymEnumSymbolsW failed gle=%lu", GetLastError());
    }
    LOGI("[PdbEnum] %ls: %zu functions in .text", basename, ec.count);
    return ec.count;
}
