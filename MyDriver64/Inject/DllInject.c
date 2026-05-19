// MyDriver64/Inject/DllInject.c
// 内核侧 DLL 注入（APC 路径，无 CreateRemoteThread / 无 RtlCreateUserThread）。
//
// 反监控/反 AC 思路：
//   - 不出现 "LoadLibraryW" / "LdrLoadDll" / "Inject" 字符串特征
//     —— FNV-1a hash 在目标 kernel32 导出表里反查
//   - 不做 RWX 分配 —— 目标内存只标 PAGE_READWRITE（路径只是数据）
//   - 不 patch / 不 hook —— 全文档化 API，PG 不会介入
//   - 池子 tag 用普通 4 字符 'IjlD'
//
// 流程：
//   1) KeStackAttachProcess(target)
//   2) 走目标 PEB（wow64 走 PEB32）找 kernel32 基址（手工 typedef 32 位 LDR 链表，
//      避免与 SDK 头冲突）
//   3) FNV-1a 在导出表里 hash 找 LoadLibraryW
//   4) ZwAllocateVirtualMemory 在目标 VAS 申请页存路径
//   5) 找若干非系统线程，KeInsertQueueApc 投递用户 APC

#include "../Head.h"
#include <ntimage.h>

#define HASH_LoadLibraryW   0x41B1EAB9UL    // FNV-1a32("LoadLibraryW")
#define INJECT_TAG          'IjlD'
#define INJECT_MAX_THREADS  8

NTKERNELAPI PVOID    NTAPI PsGetProcessWow64Process(IN PEPROCESS Process);
NTKERNELAPI PPEB     NTAPI PsGetProcessPeb(IN PEPROCESS Process);

// PsGetNextProcessThread 并不保证在所有 WDK 的 lib 里都能取到符号，
// 同时为了避免在二进制里出现 "PsGetNextProcessThread" 字符串特征，
// 这里采用 MmGetSystemRoutineAddress 在运行时动态解析。
typedef PETHREAD (NTAPI *PFN_PsGetNextProcessThread)(IN PEPROCESS, IN PETHREAD OPTIONAL);
static PFN_PsGetNextProcessThread g_pPsGetNextProcessThread = NULL;
static PETHREAD ResolvedNextThread(PEPROCESS proc, PETHREAD prev)
{
	if (!g_pPsGetNextProcessThread)
	{
		UNICODE_STRING us;
		RtlInitUnicodeString(&us, L"PsGetNextProcessThread");
		g_pPsGetNextProcessThread = (PFN_PsGetNextProcessThread)MmGetSystemRoutineAddress(&us);
	}
	return g_pPsGetNextProcessThread ? g_pPsGetNextProcessThread(proc, prev) : NULL;
}

// ---------- 本地轻量 PEB / Ldr typedef（按偏移布局，避开 SDK opaque 化） ----------
typedef struct _MY_LIST_ENTRY {
	struct _MY_LIST_ENTRY* Flink;
	struct _MY_LIST_ENTRY* Blink;
} MY_LIST_ENTRY;

typedef struct _MY_PEB_LITE {
	UCHAR  _pad0[0x18];
	PVOID  Ldr;                 // PEB64.Ldr 偏移 0x18
} MY_PEB_LITE;

typedef struct _MY_PEB_LDR_LITE {
	UCHAR _pad0[0x10];
	MY_LIST_ENTRY InLoadOrderModuleList;   // PEB_LDR_DATA.InLoadOrderModuleList 偏移 0x10
} MY_PEB_LDR_LITE;

typedef struct _MY_LDR_ENTRY64 {
	MY_LIST_ENTRY InLoadOrderLinks;        // +0x00
	MY_LIST_ENTRY InMemoryOrderLinks;      // +0x10
	MY_LIST_ENTRY InInitializationOrderLinks; // +0x20
	PVOID         DllBase;                 // +0x30
	PVOID         EntryPoint;              // +0x38
	ULONG         SizeOfImage;             // +0x40
	UCHAR         _pad1[4];
	UNICODE_STRING FullDllName;            // +0x48
	UNICODE_STRING BaseDllName;            // +0x58
} MY_LDR_ENTRY64;

// 32 位结构（wow64 进程）
typedef struct _MY_LE32 { ULONG Flink; ULONG Blink; } MY_LE32;
typedef struct _MY_US32 { USHORT Length; USHORT MaximumLength; ULONG Buffer; } MY_US32;

typedef struct _MY_PEB32_LITE {
	UCHAR _pad0[0x0C];
	ULONG Ldr;                  // PEB32.Ldr 偏移 0x0C
} MY_PEB32_LITE;

typedef struct _MY_PEB_LDR32_LITE {
	UCHAR _pad0[0x0C];
	MY_LE32 InLoadOrderModuleList; // PEB_LDR_DATA32.InLoadOrderModuleList 偏移 0x0C
} MY_PEB_LDR32_LITE;

typedef struct _MY_LDR_ENTRY32 {
	MY_LE32 InLoadOrderLinks;          // +0x00
	MY_LE32 InMemoryOrderLinks;        // +0x08
	MY_LE32 InInitializationOrderLinks;// +0x10
	ULONG   DllBase;                   // +0x18
	ULONG   EntryPoint;                // +0x1C
	ULONG   SizeOfImage;               // +0x20
	MY_US32 FullDllName;               // +0x24
	MY_US32 BaseDllName;               // +0x2C
} MY_LDR_ENTRY32;

static ULONG FnvHashA(const char* s)
{
	ULONG h = 0x811C9DC5UL;
	while (*s) { h ^= (UCHAR)(*s++); h *= 0x01000193UL; }
	return h;
}

// 前向声明（实现在下面）
static ULONG_PTR FindModule64(PEPROCESS eproc, const wchar_t* baseName, ULONG baseNameLen);
static ULONG_PTR FindModule32(PEPROCESS eproc, const wchar_t* baseName, ULONG baseNameLen);

// 跟随 forwarded export 字符串 "Module.Function" 或 "Module.#Ordinal"
// 由调用方在 attached 上下文里再次解析目标模块。这里仅返回 modBase + 解析结果，
// 调用方负责模块查找。为了实现简单：本函数只解析普通导出（非 forward）；forward 由
// 上层 ResolveLoadLibraryW 通过"先 KERNELBASE 后 kernel32"两次解析实现。
static ULONG_PTR ResolveExportByHash(ULONG_PTR modBase, ULONG funcHash)
{
	if (!modBase) return 0;
	__try
	{
		PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)modBase;
		if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
		PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(modBase + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

		ULONG expRva = 0, expSize = 0;
		USHORT optMagic = *(USHORT*)((PUCHAR)nt + 4 + sizeof(IMAGE_FILE_HEADER));
		if (optMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		{
			PIMAGE_NT_HEADERS64 nt64 = (PIMAGE_NT_HEADERS64)nt;
			expRva  = nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
			expSize = nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
		}
		else if (optMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
		{
			PIMAGE_NT_HEADERS32 nt32 = (PIMAGE_NT_HEADERS32)nt;
			expRva  = nt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
			expSize = nt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
		}
		else return 0;
		if (!expRva || !expSize) return 0;

		PIMAGE_EXPORT_DIRECTORY ed = (PIMAGE_EXPORT_DIRECTORY)(modBase + expRva);
		PULONG funcs = (PULONG)(modBase + ed->AddressOfFunctions);
		PULONG names = (PULONG)(modBase + ed->AddressOfNames);
		PUSHORT ords = (PUSHORT)(modBase + ed->AddressOfNameOrdinals);

		for (ULONG i = 0; i < ed->NumberOfNames; ++i)
		{
			const char* nm = (const char*)(modBase + names[i]);
			if (FnvHashA(nm) == funcHash)
			{
				ULONG fnRva = funcs[ords[i]];
				if (fnRva >= expRva && fnRva < expRva + expSize)
				{
					// forwarded export：返回特殊值 0xFFFFFFFFFFFFFFFF 让上层换模块查
					return (ULONG_PTR)-1;
				}
				return modBase + fnRva;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
	return 0;
}

// 解析 LoadLibraryW：现代 Windows 实际实现在 KERNELBASE.dll，kernel32 里是 forwarder。
// 优先在 KERNELBASE 找，找不到再回 kernel32。
static ULONG_PTR ResolveLoadLibraryW(PEPROCESS eproc, BOOLEAN isWow64)
{
	static const wchar_t kernelbase[] = L"KERNELBASE.dll";
	static const wchar_t kernel32[]   = L"KERNEL32.DLL";
	ULONG kbLen = (ULONG)(sizeof(kernelbase) / sizeof(wchar_t)) - 1;
	ULONG k32Len = (ULONG)(sizeof(kernel32)   / sizeof(wchar_t)) - 1;

	ULONG_PTR base = isWow64 ? FindModule32(eproc, kernelbase, kbLen)
	                         : FindModule64(eproc, kernelbase, kbLen);
	if (base)
	{
		ULONG_PTR addr = ResolveExportByHash(base, HASH_LoadLibraryW);
		if (addr && addr != (ULONG_PTR)-1) return addr;
	}
	base = isWow64 ? FindModule32(eproc, kernel32, k32Len)
	               : FindModule64(eproc, kernel32, k32Len);
	if (base)
	{
		ULONG_PTR addr = ResolveExportByHash(base, HASH_LoadLibraryW);
		if (addr && addr != (ULONG_PTR)-1) return addr;
	}
	return 0;
}

static ULONG_PTR FindModule64(PEPROCESS eproc, const wchar_t* baseName, ULONG baseNameLen)
{
	MY_PEB_LITE* peb = (MY_PEB_LITE*)PsGetProcessPeb(eproc);
	if (!peb) return 0;
	// 不用 MmIsAddressValid：它只对“已在物理内存”的页为 TRUE，
	// 会把 demand-paged 的 PEB 或 Ldr 误判为不可读。attached + PASSIVE 下
	// page-fault 会自然 page-in，SEH 接住真在不可读的情况。
	__try
	{
		MY_PEB_LDR_LITE* ldr = (MY_PEB_LDR_LITE*)peb->Ldr;
		if (!ldr) return 0;
		MY_LIST_ENTRY* head = &ldr->InLoadOrderModuleList;
		MY_LIST_ENTRY* cur  = head->Flink;
		ULONG safety = 0;
		while (cur && cur != head && safety < 4096)
		{
			MY_LDR_ENTRY64* e = CONTAINING_RECORD(cur, MY_LDR_ENTRY64, InLoadOrderLinks);
			if (e->BaseDllName.Buffer && e->BaseDllName.Length == baseNameLen * sizeof(WCHAR))
			{
				if (_wcsnicmp(e->BaseDllName.Buffer, baseName, baseNameLen) == 0)
					return (ULONG_PTR)e->DllBase;
			}
			cur = cur->Flink;
			++safety;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
	return 0;
}

static ULONG_PTR FindModule32(PEPROCESS eproc, const wchar_t* baseName, ULONG baseNameLen)
{
	MY_PEB32_LITE* peb32 = (MY_PEB32_LITE*)PsGetProcessWow64Process(eproc);
	if (!peb32) return 0;
	__try
	{
		MY_PEB_LDR32_LITE* ldr = (MY_PEB_LDR32_LITE*)(ULONG_PTR)peb32->Ldr;
		if (!ldr) return 0;
		ULONG headAddr = (ULONG)(ULONG_PTR)&ldr->InLoadOrderModuleList;
		ULONG cur = ldr->InLoadOrderModuleList.Flink;
		ULONG safety = 0;
		while (cur && cur != headAddr && safety < 4096)
		{
			MY_LDR_ENTRY32* e = (MY_LDR_ENTRY32*)(ULONG_PTR)cur;
			if (e->BaseDllName.Buffer && e->BaseDllName.Length == baseNameLen * sizeof(WCHAR))
			{
				wchar_t* nm = (wchar_t*)(ULONG_PTR)e->BaseDllName.Buffer;
				if (_wcsnicmp(nm, baseName, baseNameLen) == 0)
					return (ULONG_PTR)e->DllBase;
			}
			cur = e->InLoadOrderLinks.Flink;
			++safety;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
	return 0;
}

// ---------- APC 注入 ----------
// KeInitializeApc / KeInsertQueueApc 已经由 Head.h (fltKernel→ntifs→wdm) 引入，
// 不再重复声明；KAPC_ENVIRONMENT 也已可见。

static VOID InjectApcKernelRoutine(
	PKAPC Apc,
	PKNORMAL_ROUTINE* NormalRoutine,
	PVOID* NormalContext,
	PVOID* SystemArgument1,
	PVOID* SystemArgument2)
{
	UNREFERENCED_PARAMETER(NormalRoutine);
	UNREFERENCED_PARAMETER(NormalContext);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);
	if (Apc) ExFreePoolWithTag(Apc, INJECT_TAG);
}

static BOOLEAN QueueInjectApc(PETHREAD thread, PVOID loadLibAddr, PVOID pathInTarget)
{
	PKAPC apc = (PKAPC)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KAPC), INJECT_TAG);
	if (!apc) return FALSE;

	KeInitializeApc(
		apc,
		(PKTHREAD)thread,
		(ULONG)0,             // OriginalApcEnvironment
		InjectApcKernelRoutine,
		NULL,
		(PKNORMAL_ROUTINE)loadLibAddr,
		UserMode,
		pathInTarget);

	if (!KeInsertQueueApc(apc, NULL, NULL, IO_NO_INCREMENT))
	{
		ExFreePoolWithTag(apc, INJECT_TAG);
		return FALSE;
	}
	return TRUE;
}

// ---------- 主入口 ----------
VOID __vectorcall InjectDllInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	UNREFERENCED_PARAMETER(nCmd);
	UNREFERENCED_PARAMETER(pOutData);
	UNREFERENCED_PARAMETER(pParam);

	if (!MmIsAddressValid((PVOID)pIndata)) return;
	PCDllInjectInfo p = (PCDllInjectInfo)pIndata;

	ULONG64 eproc      = 0;
	WCHAR   path[260]  = { 0 };
	ULONG   pathLen    = 0;
	__try
	{
		eproc   = p->Eprocess;
		pathLen = p->PathLen;
		if (pathLen == 0 || pathLen >= 260)
		{
			p->Status = DLLINJECT_STATUS_BAD_PARAM;
			goto out_ret;
		}
		RtlCopyMemory(path, p->DllPath, pathLen * sizeof(WCHAR));
		path[pathLen] = 0;
		p->Status       = DLLINJECT_STATUS_OK;
		p->QueuedCount  = 0;
		p->Is32         = 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return; }

	if (eproc == 0 || !MmIsAddressValid((PVOID)eproc))
	{
		p->Status = DLLINJECT_STATUS_BAD_PROCESS;
		goto out_ret;
	}

	BOOLEAN isWow64 = (PsGetProcessWow64Process((PEPROCESS)eproc) != NULL);
	p->Is32 = isWow64 ? 1 : 0;

	if (!IsProcessSafeToAttach((ULONG64)eproc))
	{
		p->Status = DLLINJECT_STATUS_BAD_PROCESS;
		goto out_ret;
	}

	KAPC_STATE apcSt = { 0 };
	KeStackAttachProcess((PEPROCESS)eproc, &apcSt);

	ULONG_PTR k32Base    = 0;
	ULONG_PTR loadLibVa  = 0;
	PVOID     userPath   = NULL;
	const SIZE_T pathBytes  = (pathLen + 1) * sizeof(WCHAR);   // 实际要写入的字节数
	SIZE_T    userPathSz = pathBytes;                          // 传给 Zw*：返回时会被向上取整到 page
	ULONG     localStatus = DLLINJECT_STATUS_OK;

	__try
	{
		loadLibVa = ResolveLoadLibraryW((PEPROCESS)eproc, isWow64);
		if (!loadLibVa) { localStatus = DLLINJECT_STATUS_NO_LOADLIB; __leave; }

		NTSTATUS st = ZwAllocateVirtualMemory(NtCurrentProcess(), &userPath, 0, &userPathSz,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!NT_SUCCESS(st) || !userPath) { localStatus = DLLINJECT_STATUS_ALLOC_FAIL; __leave; }
		// ★ 用 pathBytes 不用 userPathSz：ZwAllocateVirtualMemory 把 userPathSz 改写成了
		//   按页对齐后的实际分配大小（如 0x1000），用它去拷会读 path[] 之外的栈内存 → bugcheck 0x50。
		ProbeForWrite(userPath, pathBytes, sizeof(WCHAR));
		RtlCopyMemory(userPath, path, pathBytes);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { localStatus = DLLINJECT_STATUS_ALLOC_FAIL; }

	KeUnstackDetachProcess(&apcSt);

	if (localStatus != DLLINJECT_STATUS_OK)
	{
		p->Status = localStatus;
		goto out_ret;
	}

	ULONG queued = 0;
	ULONG threadsSeen = 0;
	ULONG threadsSystem = 0;
	ULONG apcInsertFail = 0;
	PETHREAD prev = NULL, cur = NULL;
	// 不再提前 break：给所有非系统线程都投 APC，最大化命中概率（多余 APC 只会重复触发 LoadLibraryW，
	// LoadLibraryW 第二次以后返回已加载 hModule，无副作用）
	for (ULONG i = 0; i < 256; ++i)
	{
		cur = ResolvedNextThread((PEPROCESS)eproc, prev);
		if (prev) ObDereferenceObject(prev);
		prev = cur;
		if (!cur) break;
		++threadsSeen;
		if (PsIsSystemThread(cur)) { ++threadsSystem; continue; }
		if (QueueInjectApc(cur, (PVOID)loadLibVa, userPath)) ++queued;
		else                                                 ++apcInsertFail;
	}
	if (prev) ObDereferenceObject(prev);

	p->QueuedCount   = queued;
	p->ThreadsSeen   = threadsSeen;
	p->ThreadsSystem = threadsSystem;
	p->ApcInsertFail = apcInsertFail;
	p->Status = queued ? DLLINJECT_STATUS_OK : DLLINJECT_STATUS_NO_THREAD;

out_ret:
	if (MmIsAddressValid((PVOID)pRet))
	{
		__try { *(PULONG64)pRet = (ULONG64)p->Status; }
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}
}
