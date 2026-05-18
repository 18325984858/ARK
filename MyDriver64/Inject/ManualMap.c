// MyDriver64/Inject/ManualMap.c
// 第 3 档：完整 manual map。
//
// 复用思路：复用 DllInject.c 已经写好的 PEB Ldr walk / FNV hash / 导出表解析逻辑
// （这里 static 重写一份避免 .c 间符号交叉；体量小不算冗余）。
//
// 流程（详见各 step 的注释）：
//   ① 从磁盘把 DLL 读到 NonPagedPool 内核缓冲（PASSIVE_LEVEL OK）
//   ② 校验 PE 头、定位 SizeOfImage / AddressOfEntryPoint / Sections / .reloc / IMPORT
//   ③ KeStackAttachProcess(target)
//   ④ ZwAllocateVirtualMemory 申请 SizeOfImage（PAGE_READWRITE，便于写入）
//   ⑤ 拷头 + 按节拷数据到目标 VAS
//   ⑥ 应用 base relocations
//   ⑦ 解 IAT：枚举 import descriptors，在目标 PEB Ldr 找各 dll，写回 thunk
//   ⑧ 按 SectionHeader.Characteristics 设页保护
//   ⑨ 申请一小页 RX 写 shellcode（构 DllMain 调用栈帧）
//   ⑩ 找用户线程投 APC → shellcode → DllMain
//   ? Detach

#include "../Head.h"
#include "../Define.h"
#include <ntimage.h>

#define MM_TAG 'MmIj'

NTKERNELAPI PPEB  NTAPI PsGetProcessPeb(IN PEPROCESS Process);
NTKERNELAPI PVOID NTAPI PsGetProcessWow64Process(IN PEPROCESS Process);

// ZwProtectVirtualMemory 在 ntoskrnl 导出但公开头里没有
NTSYSAPI NTSTATUS NTAPI ZwProtectVirtualMemory(
	IN HANDLE ProcessHandle,
	IN OUT PVOID* BaseAddress,
	IN OUT PSIZE_T NumberOfBytesToProtect,
	IN ULONG NewAccessProtection,
	OUT PULONG OldAccessProtection);

// 复用 DllInject 的轻量 PEB / Ldr typedef（重新定义一份避免 .c 间依赖）
typedef struct _MM_LIST_ENTRY {
	struct _MM_LIST_ENTRY* Flink;
	struct _MM_LIST_ENTRY* Blink;
} MM_LIST_ENTRY;
typedef struct _MM_PEB_LITE {
	UCHAR  _pad0[0x18];
	PVOID  Ldr;
} MM_PEB_LITE;
typedef struct _MM_PEB_LDR_LITE {
	UCHAR _pad0[0x10];
	MM_LIST_ENTRY InLoadOrderModuleList;
} MM_PEB_LDR_LITE;
typedef struct _MM_LDR_ENTRY64 {
	MM_LIST_ENTRY InLoadOrderLinks;
	MM_LIST_ENTRY InMemoryOrderLinks;
	MM_LIST_ENTRY InInitializationOrderLinks;
	PVOID         DllBase;
	PVOID         EntryPoint;
	ULONG         SizeOfImage;
	UCHAR         _pad1[4];
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
} MM_LDR_ENTRY64;

// 手工遍历 EPROCESS.ThreadListHead → ETHREAD.ThreadListEntry。
// 复用项目已经定位好的偏移（OffsetDefaults_*.inc / Define.h由 FindModuleData.c 启动时通过 PDB 解析）。
static PETHREAD MmNextThread(PEPROCESS proc, PETHREAD prev)
{
	if (_EPROCESS_ThreadListHead == 0 || _ETHREAD_ThreadListEntry == 0) return NULL;
	PLIST_ENTRY head  = (PLIST_ENTRY)((PUCHAR)proc + _EPROCESS_ThreadListHead);
	PLIST_ENTRY entry = NULL;
	__try
	{
		if (prev == NULL)
			entry = head->Flink;
		else
			entry = ((PLIST_ENTRY)((PUCHAR)prev + _ETHREAD_ThreadListEntry))->Flink;
		if (!entry || entry == head) return NULL;
		PETHREAD t = (PETHREAD)((PUCHAR)entry - _ETHREAD_ThreadListEntry);
		ObReferenceObject(t);
		return t;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

// 在目标 64 位 PEB 里按 basename 找模块
static ULONG_PTR MmFindModule64(PEPROCESS eproc, const char* baseNameA)
{
	MM_PEB_LITE* peb = (MM_PEB_LITE*)PsGetProcessPeb(eproc);
	if (!peb) return 0;
	__try
	{
		MM_PEB_LDR_LITE* ldr = (MM_PEB_LDR_LITE*)peb->Ldr;
		if (!ldr) return 0;
		MM_LIST_ENTRY* head = &ldr->InLoadOrderModuleList;
		MM_LIST_ENTRY* cur  = head->Flink;
		ULONG safety = 0;
		size_t bnLen = strlen(baseNameA);
		while (cur && cur != head && safety < 4096)
		{
			MM_LDR_ENTRY64* e = CONTAINING_RECORD(cur, MM_LDR_ENTRY64, InLoadOrderLinks);
			if (e->BaseDllName.Buffer && e->BaseDllName.Length == bnLen * sizeof(WCHAR))
			{
				BOOLEAN match = TRUE;
				for (size_t i = 0; i < bnLen; ++i)
				{
					WCHAR a = e->BaseDllName.Buffer[i];
					CHAR  b = baseNameA[i];
					if (a >= L'A' && a <= L'Z') a += 32;
					if (b >= 'A'  && b <= 'Z' ) b += 32;
					if (a != (WCHAR)b) { match = FALSE; break; }
				}
				if (match) return (ULONG_PTR)e->DllBase;
			}
			cur = cur->Flink;
			++safety;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
	return 0;
}

// 已 attach 状态：从目标某模块按 name 找导出函数（也支持 ordinal）
static ULONG_PTR MmResolveExport(ULONG_PTR modBase, const char* funcName, USHORT funcOrd, BOOLEAN byOrd)
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
		else return 0;
		if (!expRva || !expSize) return 0;

		PIMAGE_EXPORT_DIRECTORY ed = (PIMAGE_EXPORT_DIRECTORY)(modBase + expRva);
		PULONG  funcs = (PULONG)(modBase + ed->AddressOfFunctions);
		PULONG  names = (PULONG)(modBase + ed->AddressOfNames);
		PUSHORT ords  = (PUSHORT)(modBase + ed->AddressOfNameOrdinals);

		ULONG_PTR found = 0;
		if (byOrd)
		{
			ULONG idx = (ULONG)funcOrd - ed->Base;
			if (idx < ed->NumberOfFunctions)
			{
				ULONG fnRva = funcs[idx];
				if (fnRva >= expRva && fnRva < expRva + expSize) return 0; // forwarded：跳过（v1 不递归追）
				if (fnRva) found = modBase + fnRva;
			}
		}
		else
		{
			for (ULONG i = 0; i < ed->NumberOfNames; ++i)
			{
				const char* nm = (const char*)(modBase + names[i]);
				if (strcmp(nm, funcName) == 0)
				{
					ULONG fnRva = funcs[ords[i]];
					if (fnRva >= expRva && fnRva < expRva + expSize) return 0;
					if (fnRva) found = modBase + fnRva;
					break;
				}
			}
		}
		return found;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// ---------- 文件读取 ----------
static NTSTATUS MmReadDllFile(const wchar_t* dosPath, PUCHAR* outBuf, ULONG* outSize)
{
	*outBuf = NULL; *outSize = 0;

	// DOS 路径 "C:\..." → NT 路径 "\??\C:\..."
	WCHAR ntPath[300]; RtlZeroMemory(ntPath, sizeof(ntPath));
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
	if (fsi.EndOfFile.QuadPart <= 0 || fsi.EndOfFile.QuadPart > 0x10000000) { ZwClose(h); return STATUS_INVALID_FILE_FOR_SECTION; }

	ULONG size = (ULONG)fsi.EndOfFile.QuadPart;
	PUCHAR buf = (PUCHAR)ExAllocatePool2(POOL_FLAG_PAGED, size, MM_TAG);
	if (!buf) { ZwClose(h); return STATUS_INSUFFICIENT_RESOURCES; }

	LARGE_INTEGER off = { 0 };
	st = ZwReadFile(h, NULL, NULL, NULL, &iosb, buf, size, &off, NULL);
	ZwClose(h);
	if (!NT_SUCCESS(st)) { ExFreePoolWithTag(buf, MM_TAG); return st; }

	*outBuf = buf; *outSize = size;
	return STATUS_SUCCESS;
}

// 把 SectionHeader.Characteristics 翻译为页保护
static ULONG MmSectionProtection(ULONG ch)
{
	BOOLEAN x = (ch & IMAGE_SCN_MEM_EXECUTE) != 0;
	BOOLEAN r = (ch & IMAGE_SCN_MEM_READ)    != 0;
	BOOLEAN w = (ch & IMAGE_SCN_MEM_WRITE)   != 0;
	if (x && r && w) return PAGE_EXECUTE_READWRITE;
	if (x && r)      return PAGE_EXECUTE_READ;
	if (x)           return PAGE_EXECUTE;
	if (r && w)      return PAGE_READWRITE;
	if (r)           return PAGE_READONLY;
	if (w)           return PAGE_READWRITE;
	return PAGE_NOACCESS;
}

// ---------- APC kernel routine（释放 APC 结构）----------
static VOID MmApcKernelRoutine(PKAPC Apc, PKNORMAL_ROUTINE* NormalRoutine, PVOID* NormalContext,
	PVOID* SystemArgument1, PVOID* SystemArgument2)
{
	UNREFERENCED_PARAMETER(NormalRoutine);
	UNREFERENCED_PARAMETER(NormalContext);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);
	if (Apc) ExFreePoolWithTag(Apc, MM_TAG);
}

static BOOLEAN MmQueueApc(PETHREAD thread, PVOID shellcodeVa)
{
	PKAPC apc = (PKAPC)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KAPC), MM_TAG);
	if (!apc) return FALSE;
	KeInitializeApc(apc, (PKTHREAD)thread, (ULONG)0, MmApcKernelRoutine, NULL,
		(PKNORMAL_ROUTINE)shellcodeVa, UserMode, NULL);
	if (!KeInsertQueueApc(apc, NULL, NULL, IO_NO_INCREMENT))
	{
		ExFreePoolWithTag(apc, MM_TAG);
		return FALSE;
	}
	return TRUE;
}

// ---------- Blackbone 风格 force-APC：先投 user APC，再投 kernel APC 调 KeTestAlertThread(UserMode) 唤醒线程 ----------
NTKERNELAPI BOOLEAN NTAPI KeTestAlertThread(IN KPROCESSOR_MODE AlertMode);

static VOID MmPrepareKernelRoutine(PKAPC Apc, PKNORMAL_ROUTINE* NormalRoutine, PVOID* NormalContext,
	PVOID* SystemArgument1, PVOID* SystemArgument2)
{
	UNREFERENCED_PARAMETER(NormalRoutine);
	UNREFERENCED_PARAMETER(NormalContext);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);
	KeTestAlertThread(UserMode);                       // 把当前（被投递到的）线程标为 alertable，强迫它处理 user APC 队列
	if (Apc) ExFreePoolWithTag(Apc, MM_TAG);
}

static BOOLEAN MmQueueApcForce(PETHREAD thread, PVOID shellcodeVa)
{
	PKAPC inj = (PKAPC)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KAPC), MM_TAG);
	if (!inj) return FALSE;
	PKAPC prep = (PKAPC)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KAPC), MM_TAG);
	if (!prep) { ExFreePoolWithTag(inj, MM_TAG); return FALSE; }

	KeInitializeApc(inj, (PKTHREAD)thread, (ULONG)0 /*OriginalApcEnvironment*/,
		MmApcKernelRoutine, NULL, (PKNORMAL_ROUTINE)shellcodeVa, UserMode, NULL);
	KeInitializeApc(prep, (PKTHREAD)thread, (ULONG)0 /*OriginalApcEnvironment*/,
		MmPrepareKernelRoutine, NULL, NULL, KernelMode, NULL);

	if (!KeInsertQueueApc(inj, NULL, NULL, IO_NO_INCREMENT))
	{
		ExFreePoolWithTag(inj, MM_TAG);
		ExFreePoolWithTag(prep, MM_TAG);
		return FALSE;
	}
	// kernel APC 用来强制唤醒；即使插不进去也不影响 inj 已经入队的事实
	if (!KeInsertQueueApc(prep, NULL, NULL, IO_NO_INCREMENT))
		ExFreePoolWithTag(prep, MM_TAG);
	return TRUE;
}

// ---------- 主入口 ----------
VOID __vectorcall ManualMapDllInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	UNREFERENCED_PARAMETER(nCmd);
	UNREFERENCED_PARAMETER(pOutData);
	UNREFERENCED_PARAMETER(pParam);

	if (!MmIsAddressValid((PVOID)pIndata)) return;
	PCManualMapInfo p = (PCManualMapInfo)pIndata;

	// Phase 0：snapshot
	ULONG64 eproc = 0;
	WCHAR   path[260] = { 0 };
	ULONG   pathLen = 0;
	ULONG   mode = MMAP_MODE_APC;
	__try
	{
		eproc   = p->Eprocess;
		pathLen = p->PathLen;
		mode    = p->Mode;
		if (pathLen == 0 || pathLen >= 260) { p->Status = MMAP_STATUS_BAD_PARAM; goto out; }
		RtlCopyMemory(path, p->DllPath, pathLen * sizeof(WCHAR));
		path[pathLen] = 0;
		p->Status        = MMAP_STATUS_OK;
		p->QueuedCount   = 0;
		p->ModuleBase    = 0;
		p->EntryPoint    = 0;
		p->SizeOfImage   = 0;
		p->FailedImportDll[0] = 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return; }

	if (eproc == 0 || !MmIsAddressValid((PVOID)eproc)) { p->Status = MMAP_STATUS_BAD_PROCESS; goto out; }
	if (PsGetProcessWow64Process((PEPROCESS)eproc) != NULL) { p->Status = MMAP_STATUS_X86_NOT_IMPL; goto out; }

	// Phase 1：读磁盘 DLL 到内核缓冲
	PUCHAR fileBuf = NULL;
	ULONG  fileSize = 0;
	if (!NT_SUCCESS(MmReadDllFile(path, &fileBuf, &fileSize))) { p->Status = MMAP_STATUS_FILE_FAIL; goto out; }

	// Phase 2：PE 解析（在内核缓冲里）
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)fileBuf;
	if (fileSize < sizeof(IMAGE_DOS_HEADER) || dos->e_magic != IMAGE_DOS_SIGNATURE)
	{ p->Status = MMAP_STATUS_BAD_PE; goto cleanup_file; }
	if ((ULONG)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > fileSize)
	{ p->Status = MMAP_STATUS_BAD_PE; goto cleanup_file; }
	PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(fileBuf + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{ p->Status = MMAP_STATUS_BAD_PE; goto cleanup_file; }

	ULONG sizeOfImage = nt->OptionalHeader.SizeOfImage;
	ULONG sizeOfHdrs  = nt->OptionalHeader.SizeOfHeaders;
	ULONG nSec        = nt->FileHeader.NumberOfSections;
	ULONG entryRva    = nt->OptionalHeader.AddressOfEntryPoint;   // DLL 的 DllMain (链接器入口) RVA
	ULONG64 origBase  = nt->OptionalHeader.ImageBase;
	if (sizeOfImage == 0 || sizeOfImage > 0x10000000) { p->Status = MMAP_STATUS_BAD_PE; goto cleanup_file; }
	if (entryRva == 0) { p->Status = MMAP_STATUS_BAD_PE; goto cleanup_file; }   // 必须有入口
	PIMAGE_SECTION_HEADER secHdr = (PIMAGE_SECTION_HEADER)((PUCHAR)&nt->OptionalHeader + nt->FileHeader.SizeOfOptionalHeader);

	// ★ 重要：attach 期间不能访问 p（用户 VA 在 caller 进程，attach 到目标后该 VA 不存在）。
	//    所有结果写到本地变量，detach 之后再回写 p。否则 __except 里碰 p → 二次 AV
	//    → 跳过 KeUnstackDetachProcess → APC_INDEX_MISMATCH (bugcheck 0x1)。
	ULONG    localStatus      = MMAP_STATUS_OK;
	ULONG64  localModBase     = 0;
	ULONG64  localEntry       = 0;
	ULONG    localSizeOfImage = 0;
	ULONG    localQueued      = 0;
	char     localFailedDll[64] = { 0 };
	ULONG    localThreadsSeen   = 0;
	ULONG    localThreadsSystem = 0;
	ULONG    localApcFails      = 0;
	ULONG64  localOriginalRip   = 0;

	// Phase 3 / 4：attach + 在目标 VAS 申请整块（先 PAGE_READWRITE 便于写入，最后再分节改保护）
	KAPC_STATE apcSt = { 0 };
	KeStackAttachProcess((PEPROCESS)eproc, &apcSt);

	PVOID   modBase   = NULL;
	SIZE_T  modSize   = sizeOfImage;
	NTSTATUS st = ZwAllocateVirtualMemory(NtCurrentProcess(), &modBase, 0, &modSize,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!NT_SUCCESS(st) || !modBase)
	{
		localStatus = MMAP_STATUS_ALLOC_FAIL;
		goto attached_done;
	}

	__try
	{
		ProbeForWrite(modBase, sizeOfImage, sizeof(ULONG64));
		RtlZeroMemory(modBase, sizeOfImage);

		// Phase 5：拷头 + 拷节
		RtlCopyMemory(modBase, fileBuf, sizeOfHdrs);
		for (ULONG i = 0; i < nSec; ++i)
		{
			ULONG vSize = secHdr[i].Misc.VirtualSize;
			ULONG rSize = secHdr[i].SizeOfRawData;
			ULONG copyLen = (rSize < vSize) ? rSize : vSize;
			if (copyLen == 0) continue;
			if ((SIZE_T)secHdr[i].VirtualAddress + copyLen > sizeOfImage) continue;
			if ((SIZE_T)secHdr[i].PointerToRawData + copyLen > fileSize) continue;
			RtlCopyMemory((PUCHAR)modBase + secHdr[i].VirtualAddress,
				fileBuf + secHdr[i].PointerToRawData, copyLen);
		}

		// Phase 6：base relocations
		LONG64 delta = (LONG64)((ULONG64)modBase - origBase);
		if (delta != 0)
		{
			ULONG relRva  = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
			ULONG relSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
			if (relRva && relSize && relRva + relSize <= sizeOfImage)
			{
				PUCHAR p0 = (PUCHAR)modBase + relRva;
				PUCHAR pEnd = p0 + relSize;
				while (p0 + sizeof(IMAGE_BASE_RELOCATION) <= pEnd)
				{
					PIMAGE_BASE_RELOCATION br = (PIMAGE_BASE_RELOCATION)p0;
					if (br->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION) || br->SizeOfBlock > (ULONG)(pEnd - p0)) break;
					ULONG nEntries = (br->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(USHORT);
					PUSHORT entries = (PUSHORT)(br + 1);
					for (ULONG i = 0; i < nEntries; ++i)
					{
						USHORT e = entries[i];
						USHORT type = (e >> 12) & 0xF;
						USHORT off  = e & 0xFFF;
						if (type == IMAGE_REL_BASED_DIR64)
						{
							ULONG64* tgt = (ULONG64*)((PUCHAR)modBase + br->VirtualAddress + off);
							*tgt += (ULONG64)delta;
						}
						// 其它类型（HIGHLOW=3 给 x86 用）此版不处理
					}
					p0 += br->SizeOfBlock;
				}
			}
		}

		// Phase 7：解 IAT
		BOOLEAN importFailed = FALSE;
		ULONG impRva  = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
		ULONG impSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
		if (impRva && impSize && impRva + impSize <= sizeOfImage)
		{
			PIMAGE_IMPORT_DESCRIPTOR desc = (PIMAGE_IMPORT_DESCRIPTOR)((PUCHAR)modBase + impRva);
			for (; desc->Name; ++desc)
			{
				const char* dllNameA = (const char*)((PUCHAR)modBase + desc->Name);
				ULONG_PTR depBase = MmFindModule64((PEPROCESS)eproc, dllNameA);
				if (!depBase)
				{
					localStatus = MMAP_STATUS_IMPORT_FAIL;
					for (int i = 0; i < 63 && dllNameA[i]; ++i) localFailedDll[i] = dllNameA[i];
					importFailed = TRUE;
					break;
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
						USHORT ord = (USHORT)(oft[i] & 0xFFFF);
						addr = MmResolveExport(depBase, NULL, ord, TRUE);
					}
					else
					{
						PIMAGE_IMPORT_BY_NAME ibn = (PIMAGE_IMPORT_BY_NAME)((PUCHAR)modBase + oft[i]);
						impName = (const char*)ibn->Name;
						addr = MmResolveExport(depBase, impName, 0, FALSE);
					}
					if (addr == 0)
					{
						DbgPrint("[MM] IMPORT FAIL: dll='%s' func='%s' ord=%u\n",
							dllNameA, impName ? impName : "(by-ord)",
							(impName ? 0 : (unsigned)(oft[i] & 0xFFFF)));
						localStatus = MMAP_STATUS_IMPORT_FAIL;
						for (int j = 0; j < 63 && dllNameA[j]; ++j) localFailedDll[j] = dllNameA[j];
						importFailed = TRUE;
						break;
					}
					ft[i] = (ULONG64)addr;
				}
			}
			if (importFailed) __leave;
		}

		// Phase 8：按节设保护
		for (ULONG i = 0; i < nSec; ++i)
		{
			PVOID secVa = (PUCHAR)modBase + secHdr[i].VirtualAddress;
			SIZE_T secSize = secHdr[i].Misc.VirtualSize;
			if (secSize == 0) continue;
			ULONG prot = MmSectionProtection(secHdr[i].Characteristics);
			ULONG old = 0;
			ZwProtectVirtualMemory(NtCurrentProcess(), &secVa, &secSize, prot, &old);
		}

		// Phase 9：申请一页 RW 放 shellcode（APC 模式用 35B 短码；Hijack 模式用 88B 长码）
		PVOID  scVa = NULL;
		SIZE_T scSz = 0x100;
		NTSTATUS scSt = ZwAllocateVirtualMemory(NtCurrentProcess(), &scVa, 0, &scSz,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!NT_SUCCESS(scSt) || !scVa) { localStatus = MMAP_STATUS_SHELLCODE_FAIL; __leave; }

		ULONG64 hMod    = (ULONG64)modBase;
		ULONG64 dllMain = (ULONG64)modBase + entryRva;
		localModBase     = hMod;
		localEntry       = dllMain;
		localSizeOfImage = sizeOfImage;

		if (mode == MMAP_MODE_APC)
		{
			// APC shellcode (~35B)：作为 user APC NormalRoutine 入口（约定 ret 给 dispatcher）
			//   48 B9 .. mov rcx, hModule
			//   BA 01 00 00 00              mov edx, 1
			//   4D 31 C0                    xor r8, r8
			//   48 83 EC 28                 sub rsp, 28h
			//   48 B8 ..                    mov rax, DllMain
			//   FF D0                       call rax
			//   48 83 C4 28                 add rsp, 28h
			//   C3                          ret
			UCHAR shellcode[] = {
				0x48,0xB9, 0,0,0,0, 0,0,0,0,
				0xBA, 0x01,0x00,0x00,0x00,
				0x4D,0x31,0xC0,
				0x48,0x83,0xEC,0x28,
				0x48,0xB8, 0,0,0,0, 0,0,0,0,
				0xFF,0xD0,
				0x48,0x83,0xC4,0x28,
				0xC3
			};
			RtlCopyMemory(shellcode + 2,  &hMod,    sizeof(ULONG64));
			RtlCopyMemory(shellcode + 24, &dllMain, sizeof(ULONG64));
			ProbeForWrite(scVa, sizeof(shellcode), 1);
			RtlCopyMemory(scVa, shellcode, sizeof(shellcode));

			ULONG oldProt = 0;
			SIZE_T scProtSz = sizeof(shellcode);
			ZwProtectVirtualMemory(NtCurrentProcess(), &scVa, &scProtSz, PAGE_EXECUTE_READ, &oldProt);

			// Phase 10-A：APC 投递到所有非系统线程
			PETHREAD prev = NULL, cur = NULL;
			for (ULONG i = 0; i < 256; ++i)
			{
				cur = MmNextThread((PEPROCESS)eproc, prev);
				if (prev) ObDereferenceObject(prev);
				prev = cur;
				if (!cur) break;
				++localThreadsSeen;
				if (PsIsSystemThread(cur)) { ++localThreadsSystem; continue; }
				if (MmQueueApc(cur, scVa)) ++localQueued;
				else                       ++localApcFails;
			}
			if (prev) ObDereferenceObject(prev);
			localStatus = localQueued ? MMAP_STATUS_OK : MMAP_STATUS_NO_THREAD;
		}
		else
		{
			// Phase 10-B：Blackbone 风格 force-APC（兼容 hardened processes）
			// 用与 APC 模式相同的 35B 短 shellcode，但每个非系统线程额外投一个 kernel APC
			// 强制把目标线程从 non-alertable wait 中唤醒，让 user APC 队列得以处理。
			UCHAR shellcode[] = {
				0x48,0xB9, 0,0,0,0, 0,0,0,0,
				0xBA, 0x01,0x00,0x00,0x00,
				0x4D,0x31,0xC0,
				0x48,0x83,0xEC,0x28,
				0x48,0xB8, 0,0,0,0, 0,0,0,0,
				0xFF,0xD0,
				0x48,0x83,0xC4,0x28,
				0xC3
			};
			RtlCopyMemory(shellcode + 2,  &hMod,    sizeof(ULONG64));
			RtlCopyMemory(shellcode + 24, &dllMain, sizeof(ULONG64));
			ProbeForWrite(scVa, sizeof(shellcode), 1);
			RtlCopyMemory(scVa, shellcode, sizeof(shellcode));

			ULONG oldProt = 0;
			SIZE_T scProtSz = sizeof(shellcode);
			ZwProtectVirtualMemory(NtCurrentProcess(), &scVa, &scProtSz, PAGE_EXECUTE_READ, &oldProt);

			PETHREAD prev = NULL, cur = NULL;
			for (ULONG i = 0; i < 256; ++i)
			{
				cur = MmNextThread((PEPROCESS)eproc, prev);
				if (prev) ObDereferenceObject(prev);
				prev = cur;
				if (!cur) break;
				++localThreadsSeen;
				if (PsIsSystemThread(cur)) { ++localThreadsSystem; continue; }
				if (localQueued > 0) continue;            // 只往第一个可用线程投，避免多线程并发跑 DllMain
				if (MmQueueApcForce(cur, scVa)) ++localQueued;
				else                            ++localApcFails;
			}
			if (prev) ObDereferenceObject(prev);

			localStatus = localQueued ? MMAP_STATUS_OK : MMAP_STATUS_NO_THREAD;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// 只动本地变量，绝不碰 p
		if (localStatus == MMAP_STATUS_OK) localStatus = MMAP_STATUS_BAD_PE;
	}

attached_done:
	KeUnstackDetachProcess(&apcSt);

	// ---- 现在回到调用方上下文，安全写回 p ----
	__try
	{
		p->Status         = localStatus;
		p->ModuleBase     = localModBase;
		p->EntryPoint     = localEntry;
		p->SizeOfImage    = localSizeOfImage;
		p->QueuedCount    = localQueued;
		p->ThreadsSeen    = localThreadsSeen;
		p->ThreadsSystem  = localThreadsSystem;
		p->ApcInsertFail  = localApcFails;
		p->OriginalRip    = localOriginalRip;
		for (int i = 0; i < 63; ++i)
		{
			p->FailedImportDll[i] = (WCHAR)localFailedDll[i];
			if (!localFailedDll[i]) break;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { /* R3 buf 失效，放弃 */ }

cleanup_file:
	if (fileBuf) ExFreePoolWithTag(fileBuf, MM_TAG);

out:
	if (MmIsAddressValid((PVOID)pRet))
	{
		__try { *(PULONG64)pRet = (ULONG64)p->Status; }
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}
}

