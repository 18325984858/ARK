// MyDriver64/Inject/MMapDriver.c
// 手动映射加载内核驱动 —— 参考 Blackbone BBMMapDriver 思路。
//
// 流程：
//   1) 把 .sys 文件读到 PagedPool（PASSIVE_LEVEL OK）
//   2) PE 解析 + NonPagedPool 申请 SizeOfImage 块（必须 NX-able 即可执行）
//   3) 拷头 + 按节拷数据 → base relocations
//   4) 解 IAT：每个 import dll 用 ZwQuerySystemInformation(SystemModuleInformation) 找到内核模块
//      的 ImageBase，再扫该模块的 EAT；forwarder 字符串("nt!Xxx" / "api-ms-...!Xxx")则递归到目标模块
//   5) 给一个 fake DRIVER_OBJECT 与 fake RegistryPath，调 DriverEntry
//   6) 出参回填基址 / 入口点 / DriverEntry 返回值
//
// 已知限制：
//   - 不入 PsLoadedModuleList，PCHunter / 第三方 ARK 列表里看不到
//   - api-ms-win-* API Set 仅做按文件名兜底（去掉 api-ms- 前缀按短名匹配），覆盖大多数 ntoskrnl 转发
//   - 没建 SEH 表（_C_specific_handler 仍可用，因为我们的 image 在 NonPagedPool；
//     需要 try/except 的驱动建议另用 IoCreateDriver 路径）

#include "../Head.h"
#include "../Define.h"
#include <ntimage.h>

#define MMD_TAG 'DmMm'
#define MMD_GS_COOKIE_X64 0x00002B992DDFA232ULL

// 项目里 KernelStruct.h 已经定义了 SYSTEM_INFORMATION_CLASS / RTL_PROCESS_MODULES /
// ZwQuerySystemInformation —— 直接复用即可。

// ---------- 读文件 ----------
static NTSTATUS MmdReadFile(const wchar_t* dosPath, PUCHAR* outBuf, ULONG* outSize)
{
    *outBuf = NULL; *outSize = 0;
    WCHAR ntPath[300] = { 0 };
    wcscat_s(ntPath, 300, L"\\??\\");
    wcscat_s(ntPath, 300, dosPath);

    UNICODE_STRING us; RtlInitUnicodeString(&us, ntPath);
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    HANDLE h = NULL; IO_STATUS_BLOCK iosb = { 0 };
    NTSTATUS st = ZwCreateFile(&h, FILE_READ_DATA | SYNCHRONIZE, &oa, &iosb, NULL,
        FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, NULL, 0);
    if (!NT_SUCCESS(st)) return st;
    FILE_STANDARD_INFORMATION fsi = { 0 };
    st = ZwQueryInformationFile(h, &iosb, &fsi, sizeof(fsi), FileStandardInformation);
    if (!NT_SUCCESS(st)) { ZwClose(h); return st; }
    if (fsi.EndOfFile.QuadPart <= 0 || fsi.EndOfFile.QuadPart > 0x4000000) { ZwClose(h); return STATUS_INVALID_FILE_FOR_SECTION; }
    ULONG size = (ULONG)fsi.EndOfFile.QuadPart;
    PUCHAR buf = (PUCHAR)ExAllocatePool2(POOL_FLAG_PAGED, size, MMD_TAG);
    if (!buf) { ZwClose(h); return STATUS_INSUFFICIENT_RESOURCES; }
    LARGE_INTEGER zero = { 0 };
    st = ZwReadFile(h, NULL, NULL, NULL, &iosb, buf, size, &zero, NULL);
    ZwClose(h);
    if (!NT_SUCCESS(st)) { ExFreePoolWithTag(buf, MMD_TAG); return st; }
    *outBuf = buf; *outSize = size;
    return STATUS_SUCCESS;
}

static BOOLEAN MmdHasSysExtension(const WCHAR* path)
{
    const WCHAR* ext = NULL;

    if (!path) return FALSE;

    for (const WCHAR* p = path; *p; ++p)
    {
        if (*p == L'.')
            ext = p;
    }

    return ext && _wcsicmp(ext, L".sys") == 0;
}

// ---------- 按节保护 ----------
static ULONG MmdSectionProtection(ULONG ch)
{
    BOOLEAN x = (ch & IMAGE_SCN_MEM_EXECUTE) != 0;
    BOOLEAN r = (ch & IMAGE_SCN_MEM_READ) != 0;
    BOOLEAN w = (ch & IMAGE_SCN_MEM_WRITE) != 0;
    if (x && r && w) return PAGE_EXECUTE_READWRITE;
    if (x && r)      return PAGE_EXECUTE_READ;
    if (x)           return PAGE_EXECUTE;
    if (r && w)      return PAGE_READWRITE;
    if (r)           return PAGE_READONLY;
    if (w)           return PAGE_READWRITE;
    return PAGE_NOACCESS;
}

// ---------- 内核模块表 ----------
static NTSTATUS MmdQueryKernelModules(PRTL_PROCESS_MODULES* outBuf)
{
    *outBuf = NULL;
    ULONG need = 0;
    ZwQuerySystemInformation((ULONG)SystemModuleInformation, NULL, 0, &need);
    if (need == 0) return STATUS_UNSUCCESSFUL;
    need += 0x1000;
    PVOID buf = ExAllocatePool2(POOL_FLAG_NON_PAGED, need, MMD_TAG);
    if (!buf) return STATUS_INSUFFICIENT_RESOURCES;
    ULONG got = 0;
    NTSTATUS st = ZwQuerySystemInformation((ULONG)SystemModuleInformation, buf, need, &got);
    if (!NT_SUCCESS(st)) { ExFreePoolWithTag(buf, MMD_TAG); return st; }
    *outBuf = (PRTL_PROCESS_MODULES)buf;
    return STATUS_SUCCESS;
}

// 按 basename 比较（不分大小写）
static BOOLEAN MmdNameEqualsA(const char* a, const char* b)
{
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return FALSE;
        ++a; ++b;
    }
    return *a == 0 && *b == 0;
}

static BOOLEAN MmdIsApiSetNameA(const char* dllNameA)
{
    if (!dllNameA) return FALSE;
    return _strnicmp(dllNameA, "api-ms-", 7) == 0 || _strnicmp(dllNameA, "ext-ms-", 7) == 0;
}

// 把 api-ms-win-... 形式简化为 ntoskrnl 兜底（90% 的内核 API Set 都转发到 nt）
static PVOID MmdFindKernelImageBase(PRTL_PROCESS_MODULES mods, const char* dllNameA)
{
    if (!mods || !dllNameA) return NULL;
    // 1) 直接按基本名匹配
    for (ULONG i = 0; i < mods->NumberOfModules; ++i)
    {
        PRTL_PROCESS_MODULE_INFORMATION m = &mods->Modules[i];
        const char* base = (const char*)(m->FullPathName + m->OffsetToFileName);
        if (MmdNameEqualsA(base, dllNameA)) return m->ImageBase;
    }
    // 2) api-ms-win-* / ext-ms-* 一律 fallback 到 ntoskrnl.exe
    if (MmdIsApiSetNameA(dllNameA))
    {
        for (ULONG i = 0; i < mods->NumberOfModules; ++i)
        {
            PRTL_PROCESS_MODULE_INFORMATION m = &mods->Modules[i];
            const char* base = (const char*)(m->FullPathName + m->OffsetToFileName);
            if (MmdNameEqualsA(base, "ntoskrnl.exe")) return m->ImageBase;
        }
    }
    return NULL;
}

// 在某个内核模块的 EAT 里按名字查找；forwarder 字符串（"dll.func"）递归到目标 dll。
// 失败 / 找不到返回 0；调用方应 fail。
static ULONG_PTR MmdResolveKernelExport(PRTL_PROCESS_MODULES mods, ULONG_PTR modBase, const char* funcName, ULONG depth);

static ULONG_PTR MmdResolveSystemRoutine(const char* funcName)
{
    if (!funcName) return 0;

    WCHAR nameBuf[128] = { 0 };
    size_t i = 0;
    for (; i < 127 && funcName[i]; ++i)
        nameBuf[i] = (WCHAR)(UCHAR)funcName[i];

    UNICODE_STRING us;
    RtlInitUnicodeString(&us, nameBuf);
    return (ULONG_PTR)MmGetSystemRoutineAddress(&us);
}

static ULONG_PTR MmdResolveKernelExportAny(PRTL_PROCESS_MODULES mods, const char* funcName)
{
    ULONG_PTR addr = MmdResolveSystemRoutine(funcName);
    if (addr) return addr;

    if (!mods || !funcName) return 0;

    for (ULONG i = 0; i < mods->NumberOfModules; ++i)
    {
        ULONG_PTR modBase = (ULONG_PTR)mods->Modules[i].ImageBase;
        addr = MmdResolveKernelExport(mods, modBase, funcName, 0);
        if (addr) return addr;
    }

    return 0;
}

static BOOLEAN MmdBuildApiSetExportName(const char* dllNameA, const char* funcName, char* outName, SIZE_T outChars)
{
    SIZE_T pos = 0;

    if (!MmdIsApiSetNameA(dllNameA) || !funcName || !outName || outChars == 0)
        return FALSE;

    for (SIZE_T source = 0; dllNameA[source] && pos + 1 < outChars; ++source)
    {
        char ch = dllNameA[source];
        if (ch == '.')
        {
            if (_stricmp(dllNameA + source, ".dll") == 0)
                break;
            ch = '_';
        }
        else if (ch == '-')
        {
            ch = '_';
        }
        outName[pos++] = ch;
    }

    if (pos == 0 || pos + 1 >= outChars)
        return FALSE;

    outName[pos++] = '_';

    for (SIZE_T source = 0; funcName[source] && pos + 1 < outChars; ++source)
        outName[pos++] = funcName[source];

    outName[pos] = 0;
    return funcName[0] != 0 && pos + 1 < outChars;
}

static ULONG_PTR MmdResolveApiSetExport(PRTL_PROCESS_MODULES mods, ULONG_PTR depBase, const char* dllNameA, const char* funcName)
{
    char exportName[192] = { 0 };

    if (!MmdBuildApiSetExportName(dllNameA, funcName, exportName, sizeof(exportName)))
        return 0;

    ULONG_PTR addr = MmdResolveKernelExport(mods, depBase, exportName, 0);
    if (addr) return addr;

    return MmdResolveKernelExportAny(mods, exportName);
}

static BOOLEAN MmdImageVaToPtr(PVOID modBase, ULONG sizeOfImage, ULONG64 origBase, ULONG64 va, PVOID* outPtr)
{
    ULONG64 base = (ULONG64)modBase;
    ULONG64 rva = 0;

    *outPtr = NULL;

    if (va >= base && va < base + sizeOfImage)
        rva = va - base;
    else if (va >= origBase && va < origBase + sizeOfImage)
        rva = va - origBase;
    else if (va < sizeOfImage)
        rva = va;
    else
        return FALSE;

    *outPtr = (PUCHAR)modBase + rva;
    return TRUE;
}

static ULONG64 MmdMakeSecurityCookie(PVOID modBase, ULONG sizeOfImage)
{
    LARGE_INTEGER perf = KeQueryPerformanceCounter(NULL);
    ULONG64 cookie = (ULONG64)perf.QuadPart;

    cookie ^= (ULONG64)modBase;
    cookie ^= (ULONG64)(ULONG_PTR)&cookie;
    cookie ^= (ULONG64)(ULONG_PTR)PsGetCurrentProcessId();
    cookie ^= ((ULONG64)sizeOfImage << 32) | sizeOfImage;
    cookie &= 0x0000FFFFFFFFFFFFULL;

    if (cookie == 0 || cookie == MMD_GS_COOKIE_X64)
        cookie ^= 0x00004711A5A55A5AULL;

    return cookie;
}

static VOID MmdInitSecurityCookie(PVOID modBase, ULONG sizeOfImage, ULONG64 origBase, PIMAGE_NT_HEADERS64 nt)
{
    IMAGE_DATA_DIRECTORY cfgDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
    ULONG minSize = FIELD_OFFSET(IMAGE_LOAD_CONFIG_DIRECTORY64, SecurityCookie) + sizeof(ULONGLONG);

    if (!cfgDir.VirtualAddress || cfgDir.Size < minSize || cfgDir.VirtualAddress >= sizeOfImage)
        return;

    if ((SIZE_T)cfgDir.VirtualAddress + minSize > sizeOfImage)
        return;

    PIMAGE_LOAD_CONFIG_DIRECTORY64 cfg = (PIMAGE_LOAD_CONFIG_DIRECTORY64)((PUCHAR)modBase + cfgDir.VirtualAddress);
    if (!cfg->SecurityCookie)
        return;

    PULONG64 cookiePtr = NULL;
    if (!MmdImageVaToPtr(modBase, sizeOfImage, origBase, cfg->SecurityCookie, (PVOID*)&cookiePtr))
        return;

    ULONG64 cookie = MmdMakeSecurityCookie(modBase, sizeOfImage);
    *cookiePtr = cookie;

    if ((PUCHAR)(cookiePtr + 1) + sizeof(ULONG64) <= (PUCHAR)modBase + sizeOfImage)
        *(cookiePtr + 1) = ~cookie;
}

static ULONG_PTR MmdResolveForwarder(PRTL_PROCESS_MODULES mods, const char* fwd, ULONG depth)
{
    if (depth >= 4) return 0;
    // 形如 "NTOSKRNL.RtlInitializeSListHead" / "api-ms-win-core-foo.bar"
    char dllName[64] = { 0 };
    const char* dot = NULL;
    for (const char* p = fwd; *p; ++p) { if (*p == '.') { dot = p; break; } }
    if (!dot) return 0;
    size_t dlen = (size_t)(dot - fwd);
    if (dlen >= 60) return 0;
    for (size_t i = 0; i < dlen; ++i) dllName[i] = fwd[i];
    // 没后缀的话补 .dll / .exe？大多 forwarder 不带后缀，需补
    // 简单做法：先按原名找；不到再追加 .dll 再找；再追加 .sys
    PVOID b = MmdFindKernelImageBase(mods, dllName);
    if (!b)
    {
        char alt[80]; size_t L = 0;
        for (size_t i = 0; i < dlen && L < 76; ++i) alt[L++] = dllName[i];
        const char* suffixes[] = { ".dll", ".sys", ".exe" };
        for (int s = 0; s < 3 && !b; ++s)
        {
            size_t L2 = L; const char* sx = suffixes[s];
            for (int j = 0; sx[j] && L2 < 79; ++j) alt[L2++] = sx[j];
            alt[L2] = 0;
            b = MmdFindKernelImageBase(mods, alt);
        }
    }
    if (!b) return 0;
    return MmdResolveKernelExport(mods, (ULONG_PTR)b, dot + 1, depth + 1);
}

static ULONG_PTR MmdResolveKernelExport(PRTL_PROCESS_MODULES mods, ULONG_PTR modBase, const char* funcName, ULONG depth)
{
    if (!modBase || !funcName) return 0;
    __try
    {
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)modBase;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
        PIMAGE_NT_HEADERS64 nth = (PIMAGE_NT_HEADERS64)(modBase + dos->e_lfanew);
        if (nth->Signature != IMAGE_NT_SIGNATURE) return 0;
        IMAGE_DATA_DIRECTORY d = nth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!d.VirtualAddress || !d.Size) return 0;
        PIMAGE_EXPORT_DIRECTORY ed = (PIMAGE_EXPORT_DIRECTORY)(modBase + d.VirtualAddress);
        PULONG funcs = (PULONG)(modBase + ed->AddressOfFunctions);
        PULONG names = (PULONG)(modBase + ed->AddressOfNames);
        PUSHORT ords = (PUSHORT)(modBase + ed->AddressOfNameOrdinals);
        for (ULONG i = 0; i < ed->NumberOfNames; ++i)
        {
            const char* nm = (const char*)(modBase + names[i]);
            if (strcmp(nm, funcName) == 0)
            {
                ULONG fnRva = funcs[ords[i]];
                // forwarder：RVA 落在 export dir 内
                if (fnRva >= d.VirtualAddress && fnRva < d.VirtualAddress + d.Size)
                    return MmdResolveForwarder(mods, (const char*)(modBase + fnRva), depth);
                return fnRva ? (modBase + fnRva) : 0;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 0;
}

// ---------- 主入口 ----------
VOID __vectorcall MMapDriverInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
    UNREFERENCED_PARAMETER(nCmd);
    UNREFERENCED_PARAMETER(pOutData);
    UNREFERENCED_PARAMETER(pParam);

    if (!MmIsAddressValid((PVOID)pIndata)) return;
    PCMMapDriverInfo p = (PCMMapDriverInfo)pIndata;

    WCHAR  path[260] = { 0 };
    ULONG  plen = 0;
    __try
    {
        plen = p->PathLen;
        if (plen == 0 || plen >= 260) { p->Status = MMD_STATUS_BAD_PARAM; goto out; }
        RtlCopyMemory(path, p->SysPath, plen * sizeof(WCHAR));
        path[plen] = 0;
        if (!MmdHasSysExtension(path)) { p->Status = MMD_STATUS_UNSUPPORTED_IMAGE; goto out; }
        p->Status = MMD_STATUS_OK;
        p->ImageBase = 0;
        p->EntryPoint = 0;
        p->SizeOfImage = 0;
        p->EntryStatus = 0;
        p->FailedImportDll[0] = 0;
        p->FailedImportFunc[0] = 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return; }

    PUCHAR fileBuf = NULL; ULONG fileSize = 0;
    if (!NT_SUCCESS(MmdReadFile(path, &fileBuf, &fileSize))) { __try { p->Status = MMD_STATUS_FILE_FAIL; } __except (EXCEPTION_EXECUTE_HANDLER) {} goto out; }

    PRTL_PROCESS_MODULES mods = NULL;
    if (!NT_SUCCESS(MmdQueryKernelModules(&mods))) { ExFreePoolWithTag(fileBuf, MMD_TAG); __try { p->Status = MMD_STATUS_IMPORT_FAIL; } __except (EXCEPTION_EXECUTE_HANDLER) {} goto out; }

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)fileBuf;
    if (fileSize < sizeof(IMAGE_DOS_HEADER) || dos->e_magic != IMAGE_DOS_SIGNATURE) goto bad_pe;
    if ((ULONG)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > fileSize) goto bad_pe;
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(fileBuf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) goto bad_pe;

    ULONG sizeOfImage = nt->OptionalHeader.SizeOfImage;
    ULONG sizeOfHdrs  = nt->OptionalHeader.SizeOfHeaders;
    ULONG nSec        = nt->FileHeader.NumberOfSections;
    ULONG entryRva    = nt->OptionalHeader.AddressOfEntryPoint;
    ULONG64 origBase  = nt->OptionalHeader.ImageBase;
    if (sizeOfImage == 0 || sizeOfImage > 0x4000000 || entryRva == 0) goto bad_pe;
    PIMAGE_SECTION_HEADER secHdr = (PIMAGE_SECTION_HEADER)((PUCHAR)&nt->OptionalHeader + nt->FileHeader.SizeOfOptionalHeader);

    // 在 NonPagedPool 分配整块（RWX 暂时，最后按节降权）
    PVOID modBase = ExAllocatePool2(POOL_FLAG_NON_PAGED_EXECUTE, sizeOfImage, MMD_TAG);
    if (!modBase) { ExFreePoolWithTag(fileBuf, MMD_TAG); ExFreePoolWithTag(mods, MMD_TAG); __try { p->Status = MMD_STATUS_ALLOC_FAIL; } __except (EXCEPTION_EXECUTE_HANDLER) {} goto out; }

    BOOLEAN ok = FALSE;
    NTSTATUS entrySt = STATUS_UNSUCCESSFUL;
    __try
    {
        // 拷头 + 节
        RtlCopyMemory(modBase, fileBuf, sizeOfHdrs);
        for (ULONG i = 0; i < nSec; ++i)
        {
            ULONG vSize = secHdr[i].Misc.VirtualSize;
            ULONG rSize = secHdr[i].SizeOfRawData;
            ULONG copyLen = (rSize < vSize) ? rSize : vSize;
            if (copyLen == 0) continue;
            if ((SIZE_T)secHdr[i].VirtualAddress + copyLen > sizeOfImage) continue;
            if ((SIZE_T)secHdr[i].PointerToRawData + copyLen > fileSize) continue;
            RtlCopyMemory((PUCHAR)modBase + secHdr[i].VirtualAddress, fileBuf + secHdr[i].PointerToRawData, copyLen);
        }

        // base relocations
        LONG64 delta = (LONG64)((ULONG64)modBase - origBase);
        if (delta != 0)
        {
            ULONG rRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
            ULONG rSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
            if (rRva && rSize && rRva + rSize <= sizeOfImage)
            {
                PUCHAR p0 = (PUCHAR)modBase + rRva;
                PUCHAR pEnd = p0 + rSize;
                while (p0 + sizeof(IMAGE_BASE_RELOCATION) <= pEnd)
                {
                    PIMAGE_BASE_RELOCATION br = (PIMAGE_BASE_RELOCATION)p0;
                    if (br->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION) || br->SizeOfBlock >(ULONG)(pEnd - p0)) break;
                    ULONG nEnt = (br->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(USHORT);
                    PUSHORT entries = (PUSHORT)(br + 1);
                    for (ULONG j = 0; j < nEnt; ++j)
                    {
                        USHORT e = entries[j];
                        if (((e >> 12) & 0xF) == IMAGE_REL_BASED_DIR64)
                            *(ULONG64*)((PUCHAR)modBase + br->VirtualAddress + (e & 0xFFF)) += (ULONG64)delta;
                    }
                    p0 += br->SizeOfBlock;
                }
            }
        }

        MmdInitSecurityCookie(modBase, sizeOfImage, origBase, nt);

        // IAT
        ULONG iRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        ULONG iSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
        if (iRva && iSize && iRva + iSize <= sizeOfImage)
        {
            PIMAGE_IMPORT_DESCRIPTOR desc = (PIMAGE_IMPORT_DESCRIPTOR)((PUCHAR)modBase + iRva);
            for (; desc->Name; ++desc)
            {
                const char* dllNameA = (const char*)((PUCHAR)modBase + desc->Name);
                ULONG_PTR depBase = (ULONG_PTR)MmdFindKernelImageBase(mods, dllNameA);
                if (!depBase)
                {
                    __try
                    {
                        p->Status = MMD_STATUS_IMPORT_FAIL;
                        for (int i = 0; i < 63 && dllNameA[i]; ++i) p->FailedImportDll[i] = dllNameA[i];
                    } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    __leave;
                }
                ULONG oftRva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
                ULONG ftRva  = desc->FirstThunk;
                ULONG64* oft = (ULONG64*)((PUCHAR)modBase + oftRva);
                ULONG64* ft  = (ULONG64*)((PUCHAR)modBase + ftRva);
                for (ULONG i = 0; oft[i]; ++i)
                {
                    ULONG_PTR addr = 0;
                    const char* impName = NULL;
                    if (oft[i] & IMAGE_ORDINAL_FLAG64)
                    {
                        // 极少见，简化处理：失败
                        addr = 0;
                    }
                    else
                    {
                        PIMAGE_IMPORT_BY_NAME ibn = (PIMAGE_IMPORT_BY_NAME)((PUCHAR)modBase + oft[i]);
                        impName = (const char*)ibn->Name;
                        addr = MmdResolveKernelExport(mods, depBase, impName, 0);
                        if (!addr && MmdIsApiSetNameA(dllNameA))
                        {
                            addr = MmdResolveApiSetExport(mods, depBase, dllNameA, impName);
                            if (!addr)
                                addr = MmdResolveKernelExportAny(mods, impName);
                        }
                    }
                    if (!addr)
                    {
                        __try
                        {
                            p->Status = MMD_STATUS_IMPORT_FAIL;
                            for (int j = 0; j < 63 && dllNameA[j]; ++j) p->FailedImportDll[j] = dllNameA[j];
                            if (impName) for (int j = 0; j < 63 && impName[j]; ++j) p->FailedImportFunc[j] = impName[j];
                        } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        __leave;
                    }
                    ft[i] = (ULONG64)addr;
                }
            }
        }

        ok = TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        __try { p->Status = MMD_STATUS_EXCEPTION; } __except (EXCEPTION_EXECUTE_HANDLER) {}
        ok = FALSE;
    }

    if (!ok)
    {
        // p->Status 已被填，释放
        ExFreePoolWithTag(modBase, MMD_TAG);
        ExFreePoolWithTag(fileBuf, MMD_TAG);
        ExFreePoolWithTag(mods, MMD_TAG);
        goto out;
    }

    // 构造 fake DRIVER_OBJECT 与 RegistryPath，调 DriverEntry
    DRIVER_OBJECT fakeDrv = { 0 };
    fakeDrv.Type = 4;
    fakeDrv.Size = sizeof(DRIVER_OBJECT);
    fakeDrv.DriverInit = (PVOID)((PUCHAR)modBase + entryRva);
    fakeDrv.DriverStart = modBase;
    fakeDrv.DriverSize = sizeOfImage;

    WCHAR regPathBuf[] = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\MMapDriver";
    UNICODE_STRING regPath; RtlInitUnicodeString(&regPath, regPathBuf);

    __try
    {
        entrySt = ((PDRIVER_INITIALIZE)fakeDrv.DriverInit)(&fakeDrv, &regPath);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        entrySt = STATUS_UNHANDLED_EXCEPTION;
    }

    __try
    {
        p->Status      = NT_SUCCESS(entrySt) ? MMD_STATUS_OK : MMD_STATUS_ENTRY_NTSTATUS;
        p->ImageBase   = (ULONG64)modBase;
        p->EntryPoint  = (ULONG64)modBase + entryRva;
        p->SizeOfImage = sizeOfImage;
        p->EntryStatus = entrySt;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    ExFreePoolWithTag(fileBuf, MMD_TAG);
    ExFreePoolWithTag(mods, MMD_TAG);
    // ⚠ 不释放 modBase —— 驱动已经在跑，释放就崩。Unmap 路径单独命令处理。
    goto out;

bad_pe:
    if (fileBuf) ExFreePoolWithTag(fileBuf, MMD_TAG);
    if (mods) ExFreePoolWithTag(mods, MMD_TAG);
    __try { p->Status = MMD_STATUS_BAD_PE; } __except (EXCEPTION_EXECUTE_HANDLER) {}

out:
    if (MmIsAddressValid((PVOID)pRet))
    {
        __try { *(PULONG64)pRet = (ULONG64)p->Status; } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}
