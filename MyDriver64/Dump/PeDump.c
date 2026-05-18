// MyDriver64/Dump/PeDump.c
// 从远程进程地址空间整块 dump 一个 PE 模块（EXE / DLL，PE32 / PE32+）。
//
// ★ 重要：KeStackAttachProcess 之后已经切换 CR3 到目标进程，此时 R3 缓冲
//   p->UserBuf（指向 MyPCHunter64.exe 调用方的进程地址空间）在目标进程里
//   不映射，直接写它会跑飞 → BSOD。
// 因此分两步：
//   ① attach 目标进程 → 用 SEH 把 SizeOfImage 字节按页读进 NonPagedPool 内核缓冲
//   ② detach → 在调用方进程上下文里 ProbeForWrite + RtlCopyMemory 到 UserBuf
//
// 容忍畸形 PE：
//   - MZ 不强求；只校验 e_lfanew 范围 + "PE\0\0" + OptionalHeader.Magic
//   - SizeOfImage 上限 256MB
//   - 单页读失败留 0，整体不中断

#include "PeDump.h"
#include "../Head.h"
#include <ntimage.h>

#define PEDUMP_TAG 'DpeP'

// 按页拷贝；某页 SEH 失败则跳过（buf 中保留 0）。返回实际成功的字节数。
// ★ 不要先 MmIsAddressValid：那只对"已在物理内存的页"为真，会把所有 demand-paged
//   的合法用户页（如 .rdata 里的 IAT 区）误判为不可读，导致 IAT 全 0 → 重建失败。
//   PASSIVE_LEVEL + attached 上下文下，让 RtlCopyMemory 自然触发 page-fault 由 MM
//   从磁盘 paging-in；真正不可读的 region（uncommitted / guard page / paged-out 失败）
//   由 SEH 接住即可。
static ULONG PeDump_CopyPagedKernel(PUCHAR dst, PUCHAR src, ULONG total)
{
	ULONG written = 0;
	ULONG done = 0;
	while (done < total)
	{
		ULONG_PTR pageOff = ((ULONG_PTR)(src + done)) & (PAGE_SIZE - 1);
		ULONG inPage = (ULONG)(PAGE_SIZE - pageOff);
		if (inPage > total - done) inPage = total - done;

		__try
		{
			RtlCopyMemory(dst + done, src + done, inPage);
			written += inPage;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			// 留 0
		}
		done += inPage;
	}
	return written;
}

VOID __vectorcall DumpProcessPEInfo(IN ULONG64 nCmd, IN ULONG64 pIndata, OUT ULONG64 pOutData, OUT ULONG64 pRet, IN OUT ULONG64 pParam)
{
	UNREFERENCED_PARAMETER(nCmd);
	UNREFERENCED_PARAMETER(pOutData);
	UNREFERENCED_PARAMETER(pParam);

	if (!MmIsAddressValid((PVOID)pIndata)) return;
	PCDumpPEInfo p = (PCDumpPEInfo)pIndata;

	// ---- PHASE 0：在调用方进程上下文里把所有输入字段 snapshot 到本地内核栈 ----
	// 之后 attach 到目标进程时，绝不再访问 p（user VA）：在目标 CR3 下 p 不映射，
	// 写 p 会 AV，AV 进入 __except；__except 里若再次写 p 又 AV → 编译器生成的
	// SEH 异常逃逸路径会跳过我们的 KeUnstackDetachProcess，导致携带 attach
	// 状态返回 R3 → APC_INDEX_MISMATCH (bugcheck 0x1)。
	ULONG64 eproc      = 0;
	ULONG64 imageBase  = 0;
	PVOID   userBuf    = NULL;
	ULONG   userBufLen = 0;
	__try
	{
		eproc      = p->Eprocess;
		imageBase  = p->ImageBase;
		userBuf    = p->UserBuf;
		userBufLen = p->UserBufLen;
		// 初始化输出字段（仍在调用方上下文，写 p 是安全的）
		p->Status       = DUMPPE_STATUS_OK;
		p->SizeOfImage  = 0;
		p->BytesWritten = 0;
		p->Machine      = 0;
		p->Is64         = 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// p 自己就不可读，直接返回；不写回 *pRet
		return;
	}

	if (eproc == 0 || imageBase == 0)
	{
		p->Status = DUMPPE_STATUS_BAD_PARAM;
		goto out_ret;
	}
	if (!MmIsAddressValid((PVOID)eproc))
	{
		p->Status = DUMPPE_STATUS_BAD_PROCESS;
		goto out_ret;
	}
	if ((ULONG64)imageBase >= 0x7FFFFFFF0000ULL)
	{
		p->Status = DUMPPE_STATUS_BAD_PARAM;
		goto out_ret;
	}

	// ---- PHASE A：attach 目标进程，把头 + .text 读进内核缓冲 ----
	// 这一段只能访问 imageBase、userBuf 这种已知的用户 VA（在目标 CR3 下有效）；
	// 绝对不要再碰 p。结果写到本地变量。
	ULONG   status     = DUMPPE_STATUS_OK;
	ULONG   sizeOfImage= 0;
	USHORT  machine    = 0;
	USHORT  is64       = 0;
	BOOLEAN headerOk   = FALSE;
	PUCHAR  kbuf       = NULL;
	ULONG   written    = 0;

	KAPC_STATE apc = { 0 };
	KeStackAttachProcess((PEPROCESS)eproc, &apc);

	__try
	{
		ProbeForRead((PVOID)imageBase, PAGE_SIZE, 1);
		PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)imageBase;
		LONG lfanew = dos->e_lfanew;
		if (lfanew < 0 || (ULONG)lfanew > (PAGE_SIZE - sizeof(IMAGE_NT_HEADERS32)))
		{
			status = DUMPPE_STATUS_BAD_HEADER;
			__leave;
		}
		PIMAGE_NT_HEADERS32 nt32 = (PIMAGE_NT_HEADERS32)((PUCHAR)imageBase + lfanew);
		if (nt32->Signature != IMAGE_NT_SIGNATURE)
		{
			status = DUMPPE_STATUS_BAD_HEADER;
			__leave;
		}
		machine = nt32->FileHeader.Machine;
		USHORT optMagic = nt32->OptionalHeader.Magic;
		if (optMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		{
			is64 = 1;
			sizeOfImage = ((PIMAGE_NT_HEADERS64)nt32)->OptionalHeader.SizeOfImage;
		}
		else if (optMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
		{
			is64 = 0;
			sizeOfImage = nt32->OptionalHeader.SizeOfImage;
		}
		else
		{
			status = DUMPPE_STATUS_BAD_HEADER;
			__leave;
		}
		if (sizeOfImage == 0 || sizeOfImage > 0x10000000)
		{
			status = DUMPPE_STATUS_BAD_HEADER;
			__leave;
		}
		headerOk = TRUE;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		status = DUMPPE_STATUS_BAD_HEADER;
	}

	// 头读到了，且 R3 给了 buf 才去读完整 image
	if (headerOk && userBuf != NULL && userBufLen >= sizeOfImage)
	{
		kbuf = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeOfImage, PEDUMP_TAG);
		if (kbuf != NULL)
		{
			RtlZeroMemory(kbuf, sizeOfImage);
			written = PeDump_CopyPagedKernel(kbuf, (PUCHAR)imageBase, sizeOfImage);
			if (written == 0) status = DUMPPE_STATUS_READ_FAILED;
		}
		else
		{
			status = DUMPPE_STATUS_READ_FAILED;
		}
	}
	else if (headerOk && userBuf != NULL && userBufLen < sizeOfImage)
	{
		status = DUMPPE_STATUS_BUF_TOO_SMALL;
	}
	// 探测阶段 (userBuf==NULL)：保留 status=OK，让 R3 看 SizeOfImage 后再调

	// ★ 无条件 detach（不在 __except 路径里，编译器无法跳过）
	KeUnstackDetachProcess(&apc);

	// ---- PHASE B：回到调用方进程上下文，写输出 + 拷给 userBuf ----
	__try
	{
		p->SizeOfImage = sizeOfImage;
		p->Machine     = machine;
		p->Is64        = is64;

		if (kbuf != NULL && written > 0)
		{
			ProbeForWrite(userBuf, sizeOfImage, 1);
			RtlCopyMemory(userBuf, kbuf, sizeOfImage);
			p->BytesWritten = written;
		}
		p->Status = status;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// R3 buf 失效；只能放弃写回
	}

	if (kbuf != NULL) ExFreePoolWithTag(kbuf, PEDUMP_TAG);

out_ret:
	if (MmIsAddressValid((PVOID)pRet))
	{
		__try
		{
			*(PULONG64)pRet = (ULONG64)p->BytesWritten;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			// 忽略；BytesWritten 已经在 PHASE B 写入 p
		}
	}
}
