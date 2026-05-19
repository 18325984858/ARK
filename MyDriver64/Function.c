#include "Define.h"
#include "Head.h"
#include "DataStruct/CList.h"
#include "DataStruct/CStack.h"
#include "KernelStruct.h"
#include "Interface.h"
#include "Hook/hde64.h"
#include <ntstrsafe.h>
#include <ntimage.h>    /*PE头文件 — 给 EnumWdfFunction 用*/

// 这个导出未在 wdm.h 中声明（在 ntddk.h / ntifs.h 里），手动 extern
NTKERNELAPI UCHAR* NTAPI PsGetProcessImageFileName(__in PEPROCESS Process);

BOOLEAN ExtractDriverName(PUNICODE_STRING FullPath, PUNICODE_STRING OutputBuffer)
{
	USHORT i;
	USHORT lastBackslashIndex = -1;

	// 从后向前查找最后一个反斜杠字符
	for (i = FullPath->Length / sizeof(WCHAR); i > 0; i--)
	{
		if (FullPath->Buffer[i - 1] == L'\\')
		{
			lastBackslashIndex = i;
			break;
		}
	}

	if (lastBackslashIndex == -1 || lastBackslashIndex >= FullPath->Length / sizeof(WCHAR))
	{
		// 没有找到反斜杠或反斜杠在末尾，提取失败
		return FALSE;
	}

	// 计算剩余字符串长度
	USHORT nameLength = (FullPath->Length / sizeof(WCHAR) - lastBackslashIndex) * sizeof(WCHAR);

	// 初始化输出字符串
	OutputBuffer->Buffer = &FullPath->Buffer[lastBackslashIndex];
	OutputBuffer->Length = nameLength;
	OutputBuffer->MaximumLength = nameLength;

	return TRUE;
}

BOOLEAN MyQueryFileAndFileFolder(UNICODE_STRING Path, PCFileInfo* pFileInfo)
{
	// pFileInfo 由调用方提供，必须有效；*pFileInfo 是输出链表头，初始可为 NULL
	if (pFileInfo == NULL)
	{
		return FALSE;
	}
	UCHAR IsInit = (*pFileInfo != NULL);

	HANDLE hFile = NULL;
	OBJECT_ATTRIBUTES objectAttributes = { 0 };
	IO_STATUS_BLOCK iosb = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	InitializeObjectAttributes(&objectAttributes, &Path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

	status = ZwCreateFile(&hFile, FILE_LIST_DIRECTORY | SYNCHRONIZE | FILE_ANY_ACCESS,
		&objectAttributes, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
		NULL, 0);
	if (!NT_SUCCESS(status))
	{
		return FALSE;
	}

	// 单次查询的工作缓冲区：64KB 足够装下数百个目录项；不够就循环再查
	const ULONG ulLength = 64 * 1024;
	PFILE_BOTH_DIR_INFORMATION pBuffer = (PFILE_BOTH_DIR_INFORMATION)
		ExAllocatePool2(POOL_FLAG_PAGED, ulLength, 'FdrA');
	if (pBuffer == NULL)
	{
		ZwClose(hFile);
		return FALSE;
	}

	UNICODE_STRING ustrTemp;
	UNICODE_STRING ustrOne;
	UNICODE_STRING ustrTwo;
	RtlInitUnicodeString(&ustrOne, L".");
	RtlInitUnicodeString(&ustrTwo, L"..");

	SIZE_T AllSize = sizeof(CFileInfo);
	WCHAR wcFileName[MY_MAX_PATH] = { 0 };
	const USHORT cbFileNameMax = (USHORT)(sizeof(wcFileName) - sizeof(WCHAR)); // 保留 NUL

	// 外层：循环到 STATUS_NO_MORE_FILES，避免目录项过多时丢失
	BOOLEAN bRestart = TRUE;
	while (TRUE)
	{
		status = ZwQueryDirectoryFile(hFile, NULL, NULL, NULL, &iosb,
			pBuffer, ulLength, FileBothDirectoryInformation,
			FALSE, NULL, bRestart);
		bRestart = FALSE;

		if (status == STATUS_NO_MORE_FILES || !NT_SUCCESS(status))
		{
			break;
		}

		// 内层：遍历当前缓冲区里的所有目录项
		PFILE_BOTH_DIR_INFORMATION pDir = pBuffer;
		while (TRUE)
		{
			// 文件名长度校验：超出本地缓冲就跳过该项，避免栈溢出
			if (pDir->FileNameLength == 0 || pDir->FileNameLength > cbFileNameMax)
			{
				goto NEXT_ENTRY;
			}

			RtlZeroMemory(wcFileName, sizeof(wcFileName));
			RtlCopyMemory(wcFileName, pDir->FileName, pDir->FileNameLength);

			ustrTemp.Buffer = wcFileName;
			ustrTemp.Length = (USHORT)pDir->FileNameLength;
			ustrTemp.MaximumLength = cbFileNameMax;

			// 跳过 "." 和 ".."
			if (RtlCompareUnicodeString(&ustrTemp, &ustrOne, TRUE) == 0 ||
				RtlCompareUnicodeString(&ustrTemp, &ustrTwo, TRUE) == 0)
			{
				goto NEXT_ENTRY;
			}

			PCFileInfo pNewInfo = NULL;
			NTSTATUS nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &pNewInfo, 0,
				&AllSize, MEM_COMMIT, PAGE_READWRITE);
			if (!NT_SUCCESS(nStatus))
			{
				MyDbgPrintfEx("[%s] 申请空间失败!\n", __FUNCTION__);
				goto DONE;
			}

			RtlZeroMemory(pNewInfo, AllSize);
			if (IsInit && (*pFileInfo)->IsInitialize)
			{
				InsertHeadList(&(*pFileInfo)->List, &pNewInfo->List);
			}
			else
			{
				IsInit = TRUE;
				pNewInfo->IsInitialize = TRUE;
				*pFileInfo = pNewInfo;
				InitializeListHead(&pNewInfo->List);
			}

			pNewInfo->AllocationSize = pDir->AllocationSize.QuadPart;

			CTIME_FIELDS TimeFields = { 0 };
			RtlTimeToTimeFields(&pDir->CreationTime, &TimeFields);
			pNewInfo->CreationTime = TimeFields;

			RtlTimeToTimeFields(&pDir->ChangeTime, &TimeFields);
			pNewInfo->ChangeTime = TimeFields;

			pNewInfo->FileAttributes = pDir->FileAttributes;

			// 防越界：FileFullName 是 WCHAR[MY_MAX_PATH]
			ULONG copyBytes = pDir->FileNameLength;
			if (copyBytes > sizeof(pNewInfo->FileFullName) - sizeof(WCHAR))
			{
				copyBytes = sizeof(pNewInfo->FileFullName) - sizeof(WCHAR);
			}
			RtlCopyMemory(pNewInfo->FileFullName, pDir->FileName, copyBytes);

		NEXT_ENTRY:
			if (pDir->NextEntryOffset == 0)
			{
				break;
			}
			pDir = (PFILE_BOTH_DIR_INFORMATION)((PUCHAR)pDir + pDir->NextEntryOffset);
		}
	}

DONE:
	ExFreePoolWithTag(pBuffer, 'FdrA');
	ZwClose(hFile);
	return TRUE;
}

ULONG64 MyExAllocMemOry(SIZE_T Size, ULONG64 PageAttribut, MODE nMode)
{
	//验证参数 
	if (Size == 0)
	{
		return NULL;
	}

	PVOID64 pNewAddr = NULL;
	SIZE_T TypeSize = Size;
	//内核模式用 ExAllocatePool2 申请空间
	if (nMode == KernelMode)
	{
		// ExAllocatePool2 失败返回 NULL；不要用 MmIsAddressValid 判断分配结果，
		// paged pool 此刻可能被换出，会误判失败
		pNewAddr = ExAllocatePool2(PageAttribut, Size, 'Tag');
		if (pNewAddr == NULL)
		{
			return NULL;
		}
	}
	//用户模式 ZwAllocateVirtualMemory 申请空间
	else if (nMode == UserMode)
	{
		NTSTATUS nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &pNewAddr, 0, &TypeSize, MEM_COMMIT, PageAttribut);
		if (!NT_SUCCESS(nStatus))
		{
			return NULL;
		}
	}
	else
	{
		return NULL;
	}

	//初始化
	RtlZeroMemory(pNewAddr, Size);
	return pNewAddr;
}

VOID EnumRegistryKey(UNICODE_STRING RegUnicodeString, PCRegistryInfo* pRegistryInfo)
{
	//是否是空值
	UCHAR IsInit = MmIsAddressValid(pRegistryInfo) && *(PULONG64)pRegistryInfo;

	HANDLE hRegister;

	OBJECT_ATTRIBUTES objectAttributes;
	//初始化objectAttributes
	InitializeObjectAttributes(&objectAttributes, &RegUnicodeString, OBJ_CASE_INSENSITIVE /*对大小写敏感*/, NULL, NULL);

	//打开注册表
	NTSTATUS ntStatus = ZwOpenKey(&hRegister, KEY_ALL_ACCESS, &objectAttributes);

	if (NT_SUCCESS(ntStatus))
	{
		//MyDbgPrintfEx("Open register successfully %ws\n", RegUnicodeString.Buffer);
	}

	ULONG ulSize;
	//第一次调用ZwQueryKey,为了获取KEY_FULL_INFORMATION数据的长度
	ZwQueryKey(hRegister, KeyFullInformation, NULL, 0, &ulSize);

	PKEY_FULL_INFORMATION pfi = (PKEY_FULL_INFORMATION)ExAllocatePool(PagedPool, ulSize);

	//第二次调用ZwQueryKey,为了获取KEY_FULL_INFORMATION数据
	ZwQueryKey(hRegister, KeyFullInformation, pfi, ulSize, &ulSize);

	NTSTATUS nStatus = STATUS_SUCCESS;

	SIZE_T AllSize = sizeof(CRegistryInfo);
	for (ULONG i = 0; i < pfi->SubKeys; i++)
	{
		//第一次调用ZwEnumerateKey，为了获取KEY_BASIC_INFORMATION数据的长度
		ZwEnumerateKey(hRegister, i, KeyBasicInformation, NULL, 0, &ulSize);

		PKEY_BASIC_INFORMATION pbi = (PKEY_BASIC_INFORMATION)ExAllocatePool(PagedPool, ulSize);

		//第二次调用ZwEnumerateKey，为了获取KEY_BASIC_INFORMATION数据
		ZwEnumerateKey(hRegister, i, KeyBasicInformation, pbi, ulSize, &ulSize);

		UNICODE_STRING uniKeyName;
		uniKeyName.Length = uniKeyName.MaximumLength = (USHORT)pbi->NameLength;
		uniKeyName.Buffer = pbi->Name;

		//申请空间返回数据

		PCRegistryInfo pNewInfo = NULL;
		nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &pNewInfo, 0, &AllSize, MEM_COMMIT, PAGE_READWRITE);
		if (!NT_SUCCESS(nStatus))
		{
			MyDbgPrintfEx("[%s] 申请空间失败!\n", __FUNCTION__);
			goto TABLE_RET;
		}

		RtlZeroMemory(pNewInfo, AllSize);

		//更改类型
		pNewInfo->nType = 0;

		//拷贝数据
		RtlCopyMemory(pNewInfo->KeyName, uniKeyName.Buffer, uniKeyName.Length);

		if (IsInit == 0)
		{
			pNewInfo->IsInitialize = TRUE;					//已初始化
			InitializeListHead(&pNewInfo->List);			//初始化链表头
			*pRegistryInfo = pNewInfo;						//赋值
			IsInit = TRUE;
		}
		else
		{
			//插入链表里面
			InsertHeadList(&((PCRegistryInfo) * ((PULONG64)pRegistryInfo))->List, &pNewInfo->List);
		}

		//申请空间失败直接返回
	TABLE_RET:
		//回收内存
		if (pbi != NULL)
		{
			ExFreePool(pbi);
		}
	}

	//回收内存
	if (pfi != NULL)
	{
		ExFreePool(pfi);
	}
	//关闭句柄
	if (hRegister != NULL)
	{
		ZwClose(hRegister);
	}
}

VOID EnumRegistryValue(UNICODE_STRING RegUnicodeString, PCRegistryInfo* pRegistryInfo)
{
	UCHAR IsInit = MmIsAddressValid(pRegistryInfo) && *(PULONG64)pRegistryInfo;

	HANDLE hRegister;
	ULONG ulSize;
	NTSTATUS ntStatus;
	UNICODE_STRING uniKeyName;
	PKEY_VALUE_FULL_INFORMATION  pvbi;
	PKEY_FULL_INFORMATION pfi;
	ULONG i;
	OBJECT_ATTRIBUTES objectAttributes;
	//初始化UNICODE_STRING字符串


	//初始化objectAttributes
	InitializeObjectAttributes(&objectAttributes, &RegUnicodeString, OBJ_CASE_INSENSITIVE,/*对大小写敏感*/	NULL, NULL);

	//打开注册表
	ntStatus = ZwOpenKey(&hRegister, KEY_ALL_ACCESS, &objectAttributes);

	if (NT_SUCCESS(ntStatus))
	{
		//MyDbgPrintfEx("Open register successfully %ws\n", RegUnicodeString.Buffer);
	}

	ZwQueryKey(hRegister, KeyFullInformation, NULL, 0, &ulSize);

	pfi = (PKEY_FULL_INFORMATION)ExAllocatePool(PagedPool, ulSize);

	//查询注册表
	ZwQueryKey(hRegister, KeyFullInformation, pfi, ulSize, &ulSize);

	SIZE_T AllSize = sizeof(CRegistryInfo);

	NTSTATUS nStatus = STATUS_SUCCESS;
	//开始循环枚举注册表
	for (i = 0; i < pfi->Values; i++)
	{
		ZwEnumerateValueKey(hRegister, i, KeyValueFullInformation, NULL, 0, &ulSize);

		pvbi = (PKEY_VALUE_FULL_INFORMATION)ExAllocatePool(PagedPool, ulSize);

		ZwEnumerateValueKey(hRegister, i, KeyValueFullInformation, pvbi, ulSize, &ulSize);

		uniKeyName.Length = uniKeyName.MaximumLength = (USHORT)pvbi->NameLength;

		//名称
		uniKeyName.Buffer = pvbi->Name;

		//数据
		WCHAR szBuf[255] = { 0 };
		PWCHAR Data = (ULONG64)pvbi + pvbi->DataOffset;

		//申请空间
		PCRegistryInfo pNewInfo = NULL;
		nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &pNewInfo, 0, &AllSize, MEM_COMMIT, PAGE_READWRITE);
		if (!NT_SUCCESS(nStatus))
		{
			//MyDbgPrintfEx("[%s] 申请空间失败!\n", __FUNCTION__);
			goto TABLE_RET;
		}

		RtlZeroMemory(pNewInfo, AllSize);

		//更改类型
		pNewInfo->nType = 1;

		//拷贝数据
		RtlCopyMemory(pNewInfo->ValueName, uniKeyName.Buffer, uniKeyName.Length);

		//拷贝数据
		RtlCopyMemory(pNewInfo->ValueData, Data, pvbi->DataLength);


		//类型
		pNewInfo->ValueType = pvbi->Type;

		if (IsInit != 0 && (*pRegistryInfo)->IsInitialize)
		{
			//插入链表里面
			InsertHeadList(&(*(pRegistryInfo))->List, &pNewInfo->List);
		}
		else
		{
			pNewInfo->IsInitialize = TRUE;					//已初始化
			InitializeListHead(&pNewInfo->List);			//初始化链表头
			*(PULONG64)pRegistryInfo = pNewInfo;			//赋值
			IsInit = TRUE;
		}


	TABLE_RET:
		if (pvbi != NULL)
		{
			ExFreePool(pvbi);
		}
	}
	if (pfi != NULL)
	{
		ExFreePool(pfi);
	}
	if (hRegister != NULL)
	{
		ZwClose(hRegister);
	}
	return STATUS_SUCCESS;
}

VOID EnumGdtTable(PCGdtInfo* pGdtInfo)
{
	UCHAR IsInit = MmIsAddressValid(pGdtInfo) && *(PULONG64)pGdtInfo;
	SIZE_T AllSize = sizeof(CGdtInfo);

	//RTL_OSVERSIONINFOEXW version = { 0 };
	//RtlGetVersion(&version);

	if (!MmIsAddressValid(KiProcessorBlock))
	{
		return;
	}

	//循环遍历当前CPU结构体
	for (int i = 0; i < KeNumberProcessors; i++)
	{
		//获取
		PCGdt pGdtBase = *(PULONG64)(KiProcessorBlock[i] - 0x180 /**/ + _KPCR_GdtBase);

		//无效地址返回
		if (!MmIsAddressValid(pGdtBase))
		{
			break;
		}
		//RtlWalkFrameChain
		CGdt ZeroGdt[4] = { 0 };
		NTSTATUS nStatus = STATUS_SUCCESS;
		ULONG32 dwCurGdtIndex = 2; //指向当前GDT表的索引 前两项基本为0可以直接从第3项开始
		//遍历GDT 遇到四个为0的地方说明结束了
		while (RtlCompareMemory(&pGdtBase[dwCurGdtIndex], ZeroGdt, sizeof(ZeroGdt)) != sizeof(ZeroGdt))
		{
			//申请空间
			PCGdtInfo pNewInfo = NULL;
			nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &pNewInfo, 0, &AllSize, MEM_COMMIT, PAGE_READWRITE);
			if (!NT_SUCCESS(nStatus))
			{
				MyDbgPrintfEx("[%s] 申请空间失败!\n", __FUNCTION__);
				break;
			}
			//初始化
			RtlZeroMemory(pNewInfo, AllSize);

			//赋值
			pNewInfo->GdtBase = &pGdtBase[dwCurGdtIndex];

			RtlCopyMemory(&pNewInfo->GdtData, &pGdtBase[dwCurGdtIndex], sizeof(CGdt));

			pNewInfo->nCpuId = i;

			pNewInfo->nIndex = dwCurGdtIndex;

			//指向下一个
			dwCurGdtIndex++;

			//判断是否是64位段 //
			if (pNewInfo->GdtData.S == 0 && RtlCompareMemory(&pNewInfo->GdtData, ZeroGdt, sizeof(CGdt)) != sizeof(CGdt))
			{
				RtlCopyMemory(&pNewInfo->GdtData1, &pGdtBase[dwCurGdtIndex], sizeof(CGdt));

				pNewInfo->Is64Segment = TRUE;
				//指向下一个
				dwCurGdtIndex++;
			}


			//判断传进来的数据是否初始化过
			if (IsInit != 0 && (*pGdtInfo)->IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCGdtInfo) * ((PULONG64)pGdtInfo))->List, &pNewInfo->List);
			}
			else
			{
				pNewInfo->IsInitialize = TRUE;					//已初始化
				InitializeListHead(&pNewInfo->List);				//初始化链表头
				*pGdtInfo = pNewInfo;							//赋值
				IsInit = TRUE;
			}
		}
	}
}

VOID EnumIdtTable(PCIdtInfo* pIdtInfo)
{
	ULONG64 IsInit = MmIsAddressValid(pIdtInfo) && MmIsAddressValid(*pIdtInfo) && ((PCProcessVadInfo)(*pIdtInfo))->List.IsInitialize;

	//RTL_OSVERSIONINFOEXW version = { 0 };
	//RtlGetVersion(&version);


	//版本Win10 19045 版本
	//if (version.dwMajorVersion == 10 && version.dwBuildNumber == 19045)
	{
		PULONG64 KiProcessorBlock = GetKiProcessorBlock(); //获取存储CPU环境快的数组
		if (!MmIsAddressValid(KiProcessorBlock))
		{
			return;
		}

		//循环遍历当前CPU结构体
		for (int i = 0; i < KeNumberProcessors; i++)
		{
			//获取
			PCIdt pIdtBase = *(PULONG64)(KiProcessorBlock[i] - 0x180 /**/ + _KPCR_IdtBase);

			//无效地址返回
			if (!MmIsAddressValid(pIdtBase))
			{
				break;
			}
			//RtlWalkFrameChain
			NTSTATUS nStatus = STATUS_SUCCESS;

			CIdt ZeroIdt = { 0 };

			//遍历IDT表
			for (int CurIdtIndex = 0; CurIdtIndex < 0xFF; CurIdtIndex++)
			{
				if (!MmIsAddressValid(&pIdtBase[CurIdtIndex]))
				{
					break;
				}

				//比较当前的idt是否为空
				if (RtlCompareMemory(&pIdtBase[CurIdtIndex], &ZeroIdt, sizeof(CIdt)) == sizeof(CIdt))
				{
					continue;
				}

				//申请空间
				PCIdtInfo pNewInfo = MyExAllocMemOry(sizeof(CIdtInfo), PAGE_READWRITE, UserMode);
				if (pNewInfo == NULL)
				{
					break;
				}

				//拷贝16字节数据
				RtlCopyMemory(&pNewInfo->IdtData, &pIdtBase[CurIdtIndex], sizeof(CIdt));

				pNewInfo->nIndex = CurIdtIndex;
				pNewInfo->IdtBase = &pIdtBase[CurIdtIndex];
				pNewInfo->nCpuId = i;

				//获取函数地址
				ULONG64 FunAddr = pNewInfo->IdtData.Offset2;
				FunAddr = (((FunAddr << 16) | pNewInfo->IdtData.Offset1) << 16) | pNewInfo->IdtData.Offset0;

				CDriverInfo DriverInfo = { 0 };
				if (IsSysModuleEx(FunAddr, &DriverInfo))
				{
					//拷贝路径
					memcpy_s(pNewInfo->szPath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
				}

				//判断传进来的数据是否初始化过
				if (IsInit != 0 && (*pIdtInfo)->List.IsInitialize)
				{
					//插入链表里面
					InsertHeadList(&((PCIdtInfo) * ((PULONG64)pIdtInfo))->List, &pNewInfo->List);
				}
				else
				{
					pNewInfo->List.IsInitialize = TRUE;					//已初始化
					InitializeListHead(&pNewInfo->List.List);			//初始化链表头
					*pIdtInfo = pNewInfo;								//赋值
					IsInit = TRUE;
				}
			}
		}
	}
}

ULONG64 EnumGlobalHandleTable(UNICODE_STRING HandleType, PCHandleInfo* OutList, SIZE_T TypeSize)
{
	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	UCHAR TypeIndex = 0, TableIndex = 0;
	ULONG64 TableCode = 0, TableData = 0;
	ULONG64 Index = 0, Index1 = 0, Level1 = 0, Index2 = 0, Level2 = 0;
	PUNICODE_STRING64 TypeTempName = 0;
	ULONG64 Count = 0;
	ULONG64 ImageFilePointer = 0;
	UCHAR IsInit = NULL;
	NTSTATUS nStatus = NULL;
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//变量初始化区域 
	TableCode = *(PULONG64)((*(PULONG64)PspCidTable) + _HANDLE_TABLE_TableCode);
	IsInit = MmIsAddressValid(OutList) && MmIsAddressValid(*OutList) && ((PCHandleInfo)(*OutList))->List.IsInitialize;
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//项目需求区域
	if (MmIsAddressValid(TableCode))
	{
		switch (TableCode & 3)
		{
		case 0://一级句柄表
			TableCode &= 0xFFFFFFFFFFFFFFFC;
			for (Index = 0; Index < 256; Index++)
			{
				TableData = *(PULONG64)((ULONG64)TableCode + (Index * 0x10));
				if (MmIsAddressValid(TableData))
				{
					TableData = ((ULONG64)TableData >> 0x10) | 0xFFFF000000000000/*补齐符号位*/;
					if (MmIsAddressValid(TableData))
					{
						TypeIndex = *(PUCHAR)((ULONG64)TableData - _OBJECT_HEADER_TypeIndex);
						TableIndex = (unsigned __int8)((unsigned __int16)(TableData - _OBJECT_HEADER_PointerCount) >> 8);
						TypeIndex = TypeIndex ^ TableIndex;
						TypeIndex = TypeIndex ^ *ObHeaderCookie;
						if (TypeIndex != 0)
						{
							TypeTempName = (ULONG64)(*(PULONG64)((ObTypeIndexTable)[TypeIndex])) + 0x10;
							if (RtlCompareUnicodeString(&HandleType, TypeTempName, TRUE) == 0)
							{
								//申请内村
								PCHandleInfo pNewInfo = NULL;
								nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &pNewInfo, 0, &TypeSize, MEM_COMMIT, PAGE_READWRITE);
								if (!NT_SUCCESS(nStatus))
								{
									MyDbgPrintfEx("[%s] 申请空间失败!\n", __FUNCTION__);
									break;
								}

								pNewInfo->Object = TableData;

								//判断传进来的数据是否初始化过
								if (IsInit != 0 && ((PCHandleInfo)(*OutList)->List.IsInitialize))
								{
									//插入链表里面
									InsertHeadList(&((PCHandleInfo) * ((PULONG64)OutList))->List.List, &pNewInfo->List.List);
								}
								else
								{
									pNewInfo->List.IsInitialize = TRUE;							//已初始化
									InitializeListHead(&pNewInfo->List.List);					//初始化链表头
									*OutList = pNewInfo;										//赋值
									IsInit = TRUE;
								}
								Count++;
							}
						}
					}
				}
			}
			break;
		case 1://二级句柄表
			TableCode &= 0xFFFFFFFFFFFFFFFC;
			for (Index1 = 0; Index1 < 512; Index1++)
			{
				Level1 = *(PULONG64)((ULONG64)TableCode + (Index1 * _HANDLE_TABLE_TableCode));
				if (MmIsAddressValid(Level1))
				{
					for (Index2 = 0; Index2 < 256; Index2++)
					{
						TableData = *(PULONG64)((ULONG64)Level1 + (Index2 * 0x10));
						TableData = ((ULONG64)TableData >> 0x10) | 0xFFFF000000000000;
						if (MmIsAddressValid(TableData))
						{
							//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
							//ObGetObjectType
							//获取句柄类型
							TypeIndex = *(PUCHAR)((ULONG64)TableData - _OBJECT_HEADER_TypeIndex);
							TableIndex = (unsigned __int8)((unsigned __int16)(TableData - _OBJECT_HEADER_PointerCount) >> 8);
							TypeIndex = TypeIndex ^ TableIndex;
							TypeIndex = TypeIndex ^ *ObHeaderCookie;
							//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

							if (TypeIndex != 0)
							{
								//判断是否是进程句柄
								TypeTempName = (ULONG64)(*(PULONG64)(ObTypeIndexTable)[TypeIndex]) + 0x10;
								if (RtlCompareUnicodeString(&HandleType, TypeTempName, TRUE) == 0)
								{
									//申请内村
									PCHandleInfo pNewInfo = NULL;
									nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &pNewInfo, 0, &TypeSize, MEM_COMMIT, PAGE_READWRITE);
									if (!NT_SUCCESS(nStatus))
									{
										MyDbgPrintfEx("[%s] 申请空间失败!\n", __FUNCTION__);
										break;
									}

									pNewInfo->Object = TableData;

									//判断传进来的数据是否初始化过
									if (IsInit != 0 && ((PCHandleInfo)(*OutList)->List.IsInitialize))
									{
										//插入链表里面
										InsertHeadList(&((PCHandleInfo) * ((PULONG64)OutList))->List.List, &pNewInfo->List.List);
									}
									else
									{
										pNewInfo->List.IsInitialize = TRUE;						//已初始化
										InitializeListHead(&pNewInfo->List.List);				//初始化链表头
										*OutList = pNewInfo;									//赋值
										IsInit = TRUE;
									}
									Count++;
								}
							}
						}
					}
				}
			}
			break;
		case 2://三级句柄表
			TableCode &= 0xFFFFFFFFFFFFFFFC;
			for (Index = 0; Index < 512; Index++)
			{
				Level1 = *(PULONG64)((ULONG64)TableCode + (Index * _HANDLE_TABLE_TableCode));
				if (MmIsAddressValid(Level1))
				{
					for (Index1 = 0; Index1 < 512; Index1++)
					{
						Level2 = *(PULONG64)((PULONG64)Level1 + (Index1 * _HANDLE_TABLE_TableCode));
						if (MmIsAddressValid(Level2))
						{
							for (Index2 = 0; Index2 < 256; Index2++)
							{
								TableData = *(PULONG64)((ULONG64)Level2 + (Index2 * 0x10));
								TableData = ((ULONG64)TableData >> 0x10) | 0xFFFF000000000000;
								if (MmIsAddressValid(TableData))
								{
									TypeIndex = *(PUCHAR)((ULONG64)TableData - _OBJECT_HEADER_TypeIndex);
									TableIndex = (unsigned __int8)((unsigned __int16)(TableData - _OBJECT_HEADER_PointerCount) >> 8);
									TypeIndex = TypeIndex ^ TableIndex;
									TypeIndex = TypeIndex ^ *ObHeaderCookie;
									if (TypeIndex != 0)
									{
										TypeTempName = (ULONG64)(*(PULONG64)((ObTypeIndexTable)[TypeIndex])) + 0x10;
										if (RtlCompareUnicodeString(&HandleType, TypeTempName, TRUE) == 0)
										{
											//申请内村
											PCHandleInfo pNewInfo = NULL;
											nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &pNewInfo, 0, &TypeSize, MEM_COMMIT, PAGE_READWRITE);
											if (!NT_SUCCESS(nStatus))
											{
												MyDbgPrintfEx("[%s] 申请空间失败!\n", __FUNCTION__);
												break;
											}

											pNewInfo->Object = TableData;

											//判断传进来的数据是否初始化过
											if (IsInit != 0 && ((PCHandleInfo)(*OutList)->List.IsInitialize))
											{
												//插入链表里面
												InsertHeadList(&((PCHandleInfo) * ((PULONG64)OutList))->List.List, &pNewInfo->List.List);
											}
											else
											{
												pNewInfo->List.IsInitialize = TRUE;						//已初始化
												InitializeListHead(&pNewInfo->List.List);				//初始化链表头
												*OutList = pNewInfo;									//赋值
												IsInit = TRUE;
											}
											Count++;
										}
									}
								}
							}
						}
					}
				}
			}
			break;
		}
	}
	////////////////////////////////////////////////////////////////////////
	return Count;
}

ULONG64 PsLookUpProcessByProcessId(HANDLE Pid)
{
	//验证全局句柄表是否正确
	if (!MmIsAddressValid(PspCidTable)) //_HANDLE_TABLE
	{
		return NULL;
	}

	//处理句柄
	ULONG64 Index = (ULONG64)Pid & ~0x3;

	//判断是否超过了当前页大小
	if (Index >= (*PspCidTable + _HANDLE_TABLE_NextHandleNeedingPool))
	{
		return NULL;
	}


	ULONG64 ProcessObject = NULL;
	//获取表地址
	PULONG64 TableCode = *(PULONG64)(*PspCidTable + _HANDLE_TABLE_TableCode) & ~0x3;
	//获取几级表
	ULONG64 nFlags = *(PULONG64)(*PspCidTable + _HANDLE_TABLE_TableCode) & 0x3;

	if (nFlags == 1)//二级表
	{
		ProcessObject = (*(PULONG64) & (((PULONG32)(TableCode[Index >> 10]))[(Index & 0x3FF)]) >> 16) | 0xFFFF000000000000;
	}
	else if (nFlags == 3)//三级表
	{
		ProcessObject = (*(PULONG64) & (((PULONG32)((PULONG64)(TableCode[Index >> 19]))[(Index >> 10) & 0x1FF])[Index & 0x3FF]) >> 16) | 0xFFFF000000000000;
	}
	else //一级表
	{
		ProcessObject = (*(PULONG64) & ((PULONG32)TableCode)[Index] >> 16) | 0xFFFF000000000000;
	}


	return ProcessObject;
}

BOOLEAN IsProcessSafeToAttach(ULONG64 pEprocess)
{
	if (!MmIsAddressValid((PVOID)pEprocess))
	{
		return FALSE;
	}
	//attach 到当前进程会触发 bugcheck 0x5 INVALID_PROCESS_ATTACH_ATTEMPT
	if ((PEPROCESS)pEprocess == PsGetCurrentProcess())
	{
		return FALSE;
	}
	//正在退出/已销毁的僵尸进程：ObjectTable 被清零，attach 同样会触发 bugcheck 0x5
	if (_EPROCESS_ObjectTable > 0 &&
		*(PULONG64)((ULONG64)pEprocess + _EPROCESS_ObjectTable) == 0)
	{
		return FALSE;
	}
	return TRUE;
}

VOID WriteBufferToProcessStructEx(PCProcessInfo OutProcess, ULONG64 pEprocess)
{
	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	ULONG64 ImageBaseName = 0;
	ULONG64 ImageFilePointer = 0;
	PUNICODE_STRING64 FileName = 0;
	ULONG64 DeviceObject = 0;
	UNICODE_STRING64 DeviceObjectName = { 0 };
	ULONG64 SeAuditProcessCreationInfo = 0;
	ULONG64 Session = 0;
	PLARGE_INTEGER CreateTime = 0;
	LARGE_INTEGER CurrentSystemTime = { 0 }, CurrentLoadTime = { 0 };
	CTIME_FIELDS TimeFields = { 0 }, LoadTimeFields = { 0 };
	CSHORT Hour = 0;
	UCHAR IsSystemProcess = 0;
	PWCHAR ProcessUserName = 0;
	ULONG64 Token, LogonSession = 0;
	PUNICODE_STRING AccountName = 0;
	////////////////////////////////////////////////////////////////////////

	if (MmIsAddressValid(OutProcess) && MmIsAddressValid(pEprocess))
	{
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		OutProcess->ProcessId = *(PLONG64)((ULONG64)pEprocess + _EPROCESS_UniqueProcessId);//获取自进程ID
		OutProcess->ParentPId = *(PLONG64)((ULONG64)pEprocess + _EPROCESS_InheritedFromUniqueProcessId);//获取父进程ID
		OutProcess->DebugPort = PsGetProcessInDebugPort(pEprocess);//获取调试对象
		OutProcess->Eprocess = (ULONG64)pEprocess;//获取EPROCESS
		OutProcess->Peb = *(PULONG64)((ULONG64)pEprocess + _EPROCESS_Peb);//获取PEB
		OutProcess->Is64Process = *(PULONG64)((ULONG64)pEprocess + _EPROCESS_WoW64Process) == NULL ? TRUE : FALSE;//判读是否是x64位进程

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//获取是否是受保护的进程
		UCHAR ucIsUserVisit = PsGetProcessProtection(pEprocess);
		OutProcess->IsUserVisit = ucIsUserVisit & 7 ? TRUE : FALSE;		//判读是否是系统进程 PPL PP
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//获取文件名
		ImageBaseName = ((ULONG64)pEprocess + _EPROCESS_ImageFileName); //获取进程名
		memset(OutProcess->ImageBaseName, 0, MAX_BASE_FILE_NAME);
		memcpy_s(OutProcess->ImageBaseName, MAX_BASE_FILE_NAME, ImageBaseName, 15);
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//获取进程会话ID
		Session = PsGetSessionIdEx(pEprocess);
		if (Session != (ULONG64)0xFFFFFFFF)
		{
			OutProcess->Session = Session;//获取进程会话ID
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//获取进程用户名
		Token = *(PULONG64)((ULONG64)pEprocess + _EPROCESS_Token) & ~0xF; //将_EPROCESS_Token.Object & ~0xF 清空最后一位
		if (MmIsAddressValid(Token))
		{
			LogonSession = *(PULONG64)((ULONG64)Token + _TOKEN_LogonSession);//
			AccountName = (ULONG64)LogonSession + _SEP_LOGON_SESSION_REFERENCES_AccountName;
			memcpy_s(OutProcess->UserName, MAX_BASE_FILE_NAME, AccountName->Buffer, AccountName->Length);
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//获取进程命令行参数
		GetProcessPebMsgEx(pEprocess, OutProcess);
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//进程获取路径
		ImageFilePointer = *(PULONG64)((ULONG64)pEprocess + _EPROCESS_ImageFilePointer);
		SeAuditProcessCreationInfo = *(PULONG64)((ULONG64)pEprocess + _EPROCESS_SeAuditProcessCreationInfo);
		memset(OutProcess->FullFileName, 0, MY_MAX_PATH);
		if (MmIsAddressValid(ImageFilePointer))
		{
			DeviceObject = *(PULONG64)((ULONG64)ImageFilePointer + _FILE_OBJECT_DeviceObject);
			IoVolumeDeviceToDosName(DeviceObject, &DeviceObjectName);//获取文件盘符
			FileName = (PUNICODE_STRING64)((ULONG64)ImageFilePointer + _FILE_OBJECT_FileName);
			OutProcess->FullFileNameLength = FileName->Length;
			memcpy_s(OutProcess->FullFileName, MY_MAX_PATH, DeviceObjectName.Buffer, DeviceObjectName.Length);
			wcscat_s(OutProcess->FullFileName, MY_MAX_PATH, FileName->Buffer);
			ExFreePool(DeviceObjectName.Buffer);/*释放IoVolumeDeviceToDosName分配的内存*/
		}
		else if (MmIsAddressValid(SeAuditProcessCreationInfo))
		{
			FileName = (PUNICODE_STRING64)SeAuditProcessCreationInfo;
			OutProcess->FullFileNameDrviceLength = FileName->Length;
			memcpy_s(OutProcess->FullFileNameDrvice, MY_MAX_PATH, FileName->Buffer, FileName->Length);
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//获取创建时间
		CreateTime = (PLARGE_INTEGER)((ULONG64)pEprocess + _EPROCESS_CreateTime);//CreateTime = PsGetProcessCreateTimeQuadPart((PEPROCESS)TableData);
		if (MmIsAddressValid(CreateTime))
		{
			// EPROCESS.CreateTime 是 UTC。用 ExSystemTimeToLocalTime 按系统时区+DST 转成本地时间，
			// 代替原来硬编码的 +8。
			LARGE_INTEGER LocalTime = { 0 };
			ExSystemTimeToLocalTime(CreateTime, &LocalTime);
			RtlTimeToTimeFields(&LocalTime, &TimeFields);
			OutProcess->CreateTime = TimeFields;
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	}
}

UCHAR PsGetProcessProtection(ULONG64 pEprocess)
{
	if (!MmIsAddressValid(pEprocess))
	{
		return 0xFF;
	}
	return *(PUCHAR)(pEprocess + _EPROCESS_Protection);
}

ULONG64 PsGetProcessInDebugPort(ULONG64 pEprocess)
{
	ULONG64 isDebug = 0;
	isDebug = *(PULONG64)(pEprocess + _EPROCESS_DebugPort);
	if (isDebug != NULL)
	{
		return isDebug;
	}
	return NULL;
}

ULONG64 PsGetSessionIdEx(ULONG64 pEprocess)
{
	ULONG64 SessionId = 0;

	SessionId = *(PULONG64)(pEprocess + _EPROCESS_Session);
	if (!SessionId || (*(PULONG64)(pEprocess + _EPROCESS_AddressPolicyFrozen) & 0x1000) != 0)
	{
		return (ULONG64)0xFFFFFFFF;
	}
	return *(PULONG64)(SessionId + _MM_SESSION_SPACE_SessionId);
}

VOID GetProcessPebMsgEx(ULONG64 pEprocess, PCProcessInfo OutProcess)
{
	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	ULONG64 Peb = 0;
	KAPC_STATE ApcState = { 0 };
	ULONG64 ProcessParameters = 0, ImageBase = 0;
	UNICODE_STRING CommandLineBuffer = { 0 }, tCommandLineBuffer = { 0 };
	PWCHAR CommandOutBuffer = 0;
	UCHAR BeingDebugged = 0;
	////////////////////////////////////////////////////////////////////////

	// PEB / RTL_USER_PROCESS_PARAMETERS 是 ntdll 的结构，ntoskrnl.exe 的 PDB
	// 不一定包含它们 —— 此时 R3 端 ToUserSendGetStructInfoMessgae 返回 -1，
	// 偏移就废了。这里用 Win10/11 x64 的固定值兜底，保证"参数"列在新 VM
	// 上也能填出来。
	ULONG OffPebBeingDebugged = (g_Offset_PEB_BeingDebugged > 0) ? g_Offset_PEB_BeingDebugged : 0x02;
	ULONG OffPebProcessParameters = (g_Offset_PEB_ProcessParameters > 0) ? g_Offset_PEB_ProcessParameters : 0x20;
	ULONG OffPebImageBaseAddress = (g_Offset_PEB_ImageBaseAddress > 0) ? g_Offset_PEB_ImageBaseAddress : 0x10;
	ULONG OffParamsCommandLine = (g_Offset_RTL_USER_PROCESS_PARAMETERS_CommandLine > 0)
		? g_Offset_RTL_USER_PROCESS_PARAMETERS_CommandLine : 0x70;
	ULONG SizeOfPeb = (g_Size_PEB > 0) ? g_Size_PEB : 0x800;

	////////////////////////////////////////////////////////////////////////
	//项目需求区域
	if (MmIsAddressValid(pEprocess) && MmIsAddressValid(OutProcess))
	{
		if (!IsProcessSafeToAttach(pEprocess))
		{
			return;
		}
		Peb = *(PULONG64)((ULONG64)pEprocess + _EPROCESS_Peb);//获取PEB
		if (Peb != NULL)
		{
			KeStackAttachProcess(pEprocess, &ApcState);
			try
			{
				ProbeForRead(Peb, SizeOfPeb, 8);//测试用户层数据是否可读
				RtlCopyMemory(&BeingDebugged, Peb + OffPebBeingDebugged, 1);
				RtlCopyMemory(&ProcessParameters, Peb + OffPebProcessParameters, 8);
				RtlCopyMemory(&ImageBase, Peb + OffPebImageBaseAddress, 8);
				RtlCopyMemory(&CommandLineBuffer, ProcessParameters + OffParamsCommandLine, sizeof(UNICODE_STRING));

				CommandOutBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, CommandLineBuffer.MaximumLength, L"PCommandLine");//申请非分页内存
				if (MmIsAddressValid(CommandOutBuffer))
				{
					RtlCopyMemory(CommandOutBuffer, CommandLineBuffer.Buffer, CommandLineBuffer.Length);

					KeUnstackDetachProcess(&ApcState);
					memcpy_s(OutProcess->CommandLine, 0x500, CommandOutBuffer, CommandLineBuffer.Length < 0x500 ? CommandLineBuffer.Length : 0x500);//拷贝数据
					ExFreePool(CommandOutBuffer);//释放内存
				}
				OutProcess->DebugPort |= BeingDebugged;//判断是否调试状态
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				KeUnstackDetachProcess(&ApcState);//卸载
				return;
			}
		}
	}
	////////////////////////////////////////////////////////////////////////
	return;
}

VOID FsVolumeDeviceToDosNameEx(ULONG64 FileObject, PWCHAR DstFilePath)
{
	ULONG64 DeviceObject = 0;
	UNICODE_STRING DeviceObjectName = { 0 };
	PUNICODE_STRING FileName = { 0 };
	WCHAR wTstr[MAX_PATH] = { 0 };
	if (MmIsAddressValid(FileObject))
	{
		DeviceObject = *(PULONG64)(FileObject + _FILE_OBJECT_DeviceObject);
		if (MmIsAddressValid(DeviceObject))
		{
			IoVolumeDeviceToDosName(DeviceObject, &DeviceObjectName);
			if (MmIsAddressValid(DstFilePath) && DeviceObjectName.Length >= 0)
			{
				memcpy_s(DstFilePath, MAX_PATH, DeviceObjectName.Buffer, DeviceObjectName.Length);
				ExFreePool(DeviceObjectName.Buffer);/*释放IoVolumeDeviceToDosName分配的内存*/
			}

			FileName = (PUNICODE_STRING)((ULONG64)FileObject + _FILE_OBJECT_FileName);
			if (MmIsAddressValid(FileName))
			{
				memcpy_s(wTstr, MAX_PATH * 2, FileName->Buffer, FileName->Length);
				wcscat_s(DstFilePath, MAX_PATH, wTstr);
			}
		}
	}
}

ULONG64 EnumProcessVad(ULONG64 pEprocess, PCProcessVadInfo* OutData)
{
	ULONG64 RootVad = 0;
	ULONG64 VadCount = 0;
	if (MmIsAddressValid(pEprocess))
	{
		RootVad = *(PULONG64)(pEprocess + _EPROCESS_VadRoot);				/*获取Vad根节点*/
		VadCount = *(PULONG64)(pEprocess + _EPROCESS_VadCount);			/*获取Vad节点数量*/
		if (MmIsAddressValid(RootVad) && MmIsAddressValid(OutData))
		{
			return EnumVad(RootVad, OutData);								/*前序遍历树*/
		}
	}
	return VadCount;
}

ULONG64 EnumVad(ULONG64 VadNode, PCProcessVadInfo* OutData)
{
	PRTL_BALANCED_NODE Node = { 0 };
	CStack stack = { 0 };
	ElemType Data = 0;

	ULONG64 Subsection = 0;
	ULONG64 ControlArea = 0;
	PEX_FAST_REF FilePointer = { 0 };
	PFILE_OBJECT FileObject = { 0 };
	ULONG32 CommitCharge = 0;
	ULONG64 StartingVpn = 0;
	ULONG64 EndingVpn = 0;
	UCHAR StartingVpnHigh = 0;
	UCHAR EndingVpnHigh = 0;
	ULONG32 PrivateMemory = 0;
	ULONG32 Protection = 0;
	WCHAR wstr[MAX_PATH] = { 0 };
	ULONG64 nIndex = 0;

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCProcessVadInfo)(*OutData))->List.IsInitialize;

	SIZE_T TypeSize = sizeof(CProcessVadInfo);
	NTSTATUS nStatus = STATUS_SUCCESS;

	if (MmIsAddressValid(VadNode))
	{
		Node = VadNode;
		InitStack(&stack);
		PushStack(&stack, VadNode);
		while (GetStackIsNull(&stack) != TRUE)
		{
			Data = 0;
			GetStackTopData(&stack, &Data);
			PopStack(&stack);
			if (MmIsAddressValid(Data))
			{
				//申请空间
				PCProcessVadInfo pNewInfo = NULL;
				nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &pNewInfo, 0, &TypeSize, MEM_COMMIT, PAGE_READWRITE);
				if (!NT_SUCCESS(nStatus))
				{
					MyDbgPrintfEx("[%s] 申请空间失败!\n", __FUNCTION__);
					break;
				}

				RtlZeroMemory(pNewInfo, TypeSize);

				/*---------------------------数据获取区域-----------------------------------*/

				PrivateMemory = ((PMMVAD_SHORT)(Data + _MMVAD_SHORT_u))->PrivateMemory;
				Protection = ((PMMVAD_SHORT)(Data + _MMVAD_SHORT_u))->Protection;

				CommitCharge = ((PMMVAD_SHORT)(Data + _MMVAD_SHORT_u1))->CommitCharge;
				StartingVpn = *(PULONG64)(Data + _MMVAD_SHORT_StartingVpn);
				EndingVpn = *(PULONG64)(Data + _MMVAD_SHORT_EndingVpn);

				StartingVpnHigh = *(PULONG64)(Data + _MMVAD_SHORT_StartingVpnHigh);
				EndingVpnHigh = *(PULONG64)(Data + _MMVAD_SHORT_EndingVpnHigh);

				*((PULONG32)&StartingVpn + 1) = StartingVpnHigh;
				*((PULONG32)&EndingVpn + 1) = EndingVpnHigh;


				Subsection = *(PULONG64)(Data + _MMVAD_Subsection);
				if (MmIsAddressValid(Subsection))
				{
					ControlArea = *(PULONG64)(Subsection + _SUBSECTION_ControlArea);
					if (MmIsAddressValid(ControlArea))
					{
						FilePointer = (PEX_FAST_REF)(ControlArea + _CONTROL_AREA_FilePointer);
						if (MmIsAddressValid(FilePointer))
						{
							FileObject = (FilePointer->Value & (~0xF));			/*清楚后四位*/
							if (MmIsAddressValid(FileObject))
							{
								if (MmIsAddressValid(&OutData[nIndex]))
								{
									FsVolumeDeviceToDosNameEx(FileObject, pNewInfo->ExeFilePath);
								}
							}
						}
					}
				}
				/*---------------------------数据写入区域-----------------------------------*/
				pNewInfo->VadNode = Data;
				pNewInfo->Protection = Protection;
				pNewInfo->PrivateMemory = PrivateMemory;
				pNewInfo->CommitCharge = CommitCharge;
				pNewInfo->StartingVpn = StartingVpn << 12;
				pNewInfo->EndingVpn = EndingVpn << 12;

				if (IsInit != 0 && ((PCProcessVadInfo)(*OutData))->List.IsInitialize)
				{
					//插入链表里面
					InsertHeadList(&((PCProcessVadInfo)(*OutData))->List.List, &pNewInfo->List.List);
				}
				else
				{
					pNewInfo->List.IsInitialize = TRUE;						//已初始化
					InitializeListHead(&pNewInfo->List.List);				//初始化链表头
					*OutData = pNewInfo;									//赋值
					IsInit = TRUE;
				}

				/*-------------------------------------------------------------------------*/
				if (((PRTL_BALANCED_NODE)Data)->Right != NULL)
				{
					PushStack(&stack, ((PRTL_BALANCED_NODE)Data)->Right);
				}
				if (((PRTL_BALANCED_NODE)Data)->Left != NULL)
				{
					PushStack(&stack, ((PRTL_BALANCED_NODE)Data)->Left);
				}
				nIndex++;
			}
		}
		DestroyStack(&stack);
	}
	return nIndex;
}

ULONG64 MiReadVirtualMemory(ULONG64 pEprocess, ULONG64 pDstAddr, ULONG64 dqSize, PUCHAR* pOutBuf)
{
	if (!MmIsAddressValid(pEprocess))
	{
		return FALSE;
	}



	PUCHAR pSrcMem = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, dqSize, 'Tag');
	if (!MmIsAddressValid(pSrcMem))
	{
		return FALSE;
	}

	RtlZeroMemory(pSrcMem, dqSize);

	if (!IsProcessSafeToAttach(pEprocess))
	{
		ExFreePoolWithTag(pSrcMem, 'Tag');
		return FALSE;
	}

	KAPC_STATE ApcState = { 0 };
	//挂靠到指定进程
	KeStackAttachProcess(pEprocess, &ApcState);								//切换目标CR3


	//用户地址才能读,禁止读内核内存,判断地址是否有效,
	if (pDstAddr + dqSize > 0x7FFFFFFF0000i64)//|| //!MmIsAddressValidEx(pDstAddr, dqSize))
	{
		KeUnstackDetachProcess(&ApcState);//卸载
		ExFreePool(pSrcMem);
		return FALSE;
	}

	UCHAR nReadFlags = FALSE;
	int i = 0;
	try
	{
		//映射内存
		PHYSICAL_ADDRESS pHysicalAddr = MmGetPhysicalAddress(pDstAddr);
		PUCHAR pMapAddr = (PUCHAR)MmMapIoSpace(pHysicalAddr, dqSize, MmNonCached);
		if (pMapAddr != NULL)
		{
			//RtlCopyMemory(pSrcMem, pMapAddr, dqSize);

			//
			//四个字节四个字节拷贝
			//当出现拷贝到无效地址时,丢失后面数据,返回拷贝到的大小
			// 
			for (i = 0; i < dqSize; i++)
			{
				pSrcMem[i] = pMapAddr[i];
			}

			nReadFlags = TRUE;
			MmUnmapIoSpace(pMapAddr, dqSize);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		KeUnstackDetachProcess(&ApcState);//卸载
		//ExFreePool(pSrcMem);

		if (!MmIsAddressValid(pOutBuf) || !nReadFlags)
		{
			ExFreePool(pSrcMem);
			return FALSE;
		}

		*pOutBuf = pSrcMem;
		return i;
	}

	KeUnstackDetachProcess(&ApcState);//卸载

	if (!MmIsAddressValid(pOutBuf) || !nReadFlags)
	{
		ExFreePool(pSrcMem);
		return FALSE;
	}

	*pOutBuf = pSrcMem;
	return dqSize;
}

ULONG64 MiWriteVirtualMemory(ULONG64 pEprocess, ULONG64 pDstAddr, ULONG64 dqSize, PUCHAR pInBuf)
{
	//__debugbreak();

	//验证参数大小,缓冲区是否正确
	if (!MmIsAddressValid(pInBuf) || dqSize == 0)
	{
		return FALSE;
	}

	//验证要写入的地址是否是内核,是内核地址返回
	if (pDstAddr + dqSize > 0x7FFFFFFF0000i64)
	{
		return FALSE;
	}


	PUCHAR pBuf = pInBuf;
	//验证缓冲区地址是否是用户层的,如果是申请空间存储
	if ((ULONG64)pInBuf < (ULONG64)0x7FFFFFFF0000i64)
	{
		pBuf = (PUCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, dqSize, 'Tag');
		if (!MmIsAddressValid(pBuf))
		{
			return FALSE;
		}
		RtlZeroMemory(pBuf, dqSize);
		RtlCopyMemory(pBuf, pInBuf, dqSize);
	}

	if (!IsProcessSafeToAttach(pEprocess))
	{
		if (pBuf != pInBuf)
		{
			ExFreePoolWithTag(pBuf, 'Tag');
		}
		return FALSE;
	}

	//挂靠到目标进程
	KAPC_STATE ApcState = { 0 };
	KeStackAttachProcess(pEprocess, &ApcState);

	try
	{
		//获取虚拟地址物理地址
		PHYSICAL_ADDRESS pHysicalAddr = MmGetPhysicalAddress(pDstAddr);
		//映射物理地址
		PVOID pMapAddr = MmMapIoSpace(pHysicalAddr, dqSize, MmNonCached);
		if (pMapAddr != NULL)
		{
			RtlCopyMemory(pMapAddr, pBuf, dqSize);
			MmUnmapIoSpace(pMapAddr, dqSize);
		}
	}
	__except (1)
	{

		KeUnstackDetachProcess(&ApcState);
		//释放内存
		ExFreePoolWithTag(pBuf, 'Tag');
		return FALSE;
	}


	KeUnstackDetachProcess(&ApcState);

	//释放内存
	ExFreePoolWithTag(pBuf, 'Tag');

	return TRUE;
}

ULONG64 IsProcessInModule(ULONG64 pEprocess, ULONG64 dqDstAddr, PCProcessModuleInfo outData)
{
	//验证参数是大于用户地址
	if (dqDstAddr >= 0xFFFFF80000000000i64)
	{
		return NULL;
	}

	KAPC_STATE ApcState = { 0 };
	ULONG64 nRet = 0;

	//初始化链表
	CList list;
	InitDoubleLoopList(&list);															//初始化链表
	ULONG64 nFlags = MmIsAddressValid(outData);

	if (MmIsAddressValid(pEprocess))
	{
		if (!IsProcessSafeToAttach(pEprocess))
		{
			return nRet;
		}
		ULONG64 Peb = *(PULONG64)((ULONG64)pEprocess + _EPROCESS_Peb);					//获取PEB
		if (Peb != NULL)
		{
			try
			{
				PCProcessModuleInfo tmpData = NULL;
				KeStackAttachProcess(pEprocess, &ApcState);								//切换目标CR3
				ProbeForRead(Peb, _SIZE_PEB, 1);										//测试用户层数据是否可读
				ULONG64 ldr = *(PULONG64)(Peb + _PEB_Ldr);								//获取ldR
				PLIST_ENTRY64 pModuleList = ldr + _PEB_LDR_DATA_InLoadOrderModuleList;	//获取模块链表

				PLIST_ENTRY64 StartAddr = pModuleList->Flink; //指向头部
				do
				{
					PUNICODE_STRING pFullDllName = ((PUNICODE_STRING)((ULONG64)StartAddr + _LDR_DATA_TABLE_ENTRY_FullDllName));
					PUNICODE_STRING pModuleName = ((PUNICODE_STRING)((ULONG64)StartAddr + _LDR_DATA_TABLE_ENTRY_BaseDllName));

					tmpData = (PCProcessModuleInfo)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(CProcessModuleInfo), 'Tag');//申请非分页内存;

					if (MmIsAddressValid(tmpData))
					{
						memset(tmpData, 0, sizeof(CProcessModuleInfo));

						//获取想要的数据
						RtlCopyMemory(tmpData->ModuleFullPath, pFullDllName->Buffer, pFullDllName->MaximumLength);
						RtlCopyMemory(tmpData->ModuleName, pModuleName->Buffer, pModuleName->MaximumLength);
						tmpData->ModuleBaseAddr = *(PULONG64)((ULONG64)StartAddr + _LDR_DATA_TABLE_ENTRY_DllBase);
						tmpData->ModuleSize = *(PULONG64)((ULONG64)StartAddr + _LDR_DATA_TABLE_ENTRY_SizeOfImage);

						//插入链表
						InsertTrailDoubleLoopList(&list, tmpData);
					}

					StartAddr = StartAddr->Flink; //指向下一个
				} while (StartAddr != pModuleList);
				KeUnstackDetachProcess(&ApcState);//卸载


				int index = 0;
				PCProcessModuleInfo tmp = PopHeadDoubleLoopList(&list);
				while (tmp != NULL)
				{
					//判断是否是要获取的模块地址
					if (tmp->ModuleBaseAddr <= dqDstAddr && dqDstAddr < tmp->ModuleBaseAddr + tmp->ModuleSize)
					{
						if (nFlags)
						{
							RtlMoveMemory(outData, tmp, sizeof(CProcessModuleInfo));
							nRet = TRUE;
						}
					}

					ExFreePool(tmp);//释放内存
					tmp = PopHeadDoubleLoopList(&list);
				}

			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				MyDbgPrintfEx("[%ws] 触发了异常\n", __FUNCDNAME__);
				KeUnstackDetachProcess(&ApcState);//卸载

				//出现异常检测链表是否有数据 //释放资源
				int index = 0;
				PCProcessModuleInfo tmp = PopHeadDoubleLoopList(&list);
				while (tmp != NULL)
				{
					//判断是否是要获取的模块地址
					if (tmp->ModuleBaseAddr <= dqDstAddr && dqDstAddr < tmp->ModuleBaseAddr + tmp->ModuleSize)
					{
						if (nFlags)
						{
							RtlMoveMemory(outData, tmp, sizeof(CProcessModuleInfo));
						}
					}

					ExFreePool(tmp);//释放内存
					tmp = PopHeadDoubleLoopList(&list);
				}
				return 0;
			}
		}
	}
	////////////////////////////////////////////////////////////////////////
	return nRet;
}

ULONG64 EnumProcessModule(ULONG64 pEprocess, PCProcessModuleInfo* outData)
{
	ULONG64 IsInit = MmIsAddressValid(outData) && MmIsAddressValid(*outData) && ((PCProcessVadInfo)(*outData))->List.IsInitialize;

	KAPC_STATE ApcState = { 0 };
	ULONG64 nRet = 0;

	//初始化链表
	CList list;
	InitDoubleLoopList(&list);															//初始化链表
	ULONG64 nFlags = MmIsAddressValid(outData);

	if (MmIsAddressValid(pEprocess))
	{
		if (!IsProcessSafeToAttach(pEprocess))
		{
			return nRet;
		}
		ULONG64 Peb = *(PULONG64)((ULONG64)pEprocess + _EPROCESS_Peb);					//获取PEB
		if (Peb != NULL)
		{
			try
			{
				PCProcessModuleInfo tmpData = NULL;
				KeStackAttachProcess(pEprocess, &ApcState);								//切换目标CR3
				ProbeForRead(Peb, _SIZE_PEB, 1);										//测试用户层数据是否可读
				ULONG64 ldr = *(PULONG64)(Peb + _PEB_Ldr);								//获取ldR
				PLIST_ENTRY64 pModuleList = ldr + _PEB_LDR_DATA_InLoadOrderModuleList;	//获取模块链表

				PLIST_ENTRY64 StartAddr = pModuleList->Flink; //指向头部
				do
				{
					PUNICODE_STRING pFullDllName = ((PUNICODE_STRING)((ULONG64)StartAddr + _LDR_DATA_TABLE_ENTRY_FullDllName));
					PUNICODE_STRING pModuleName = ((PUNICODE_STRING)((ULONG64)StartAddr + _LDR_DATA_TABLE_ENTRY_BaseDllName));

					tmpData = (PCProcessModuleInfo)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(CProcessModuleInfo), 'Tag');//申请非分页内存;

					if (MmIsAddressValid(tmpData))
					{
						memset(tmpData, 0, sizeof(CProcessModuleInfo));

						//获取想要的数据
						RtlCopyMemory(tmpData->ModuleFullPath, pFullDllName->Buffer, pFullDllName->MaximumLength);
						RtlCopyMemory(tmpData->ModuleName, pModuleName->Buffer, pModuleName->MaximumLength);
						tmpData->ModuleBaseAddr = *(PULONG64)((ULONG64)StartAddr + _LDR_DATA_TABLE_ENTRY_DllBase);
						tmpData->ModuleSize = *(PULONG64)((ULONG64)StartAddr + _LDR_DATA_TABLE_ENTRY_SizeOfImage);

						//插入链表
						InsertTrailDoubleLoopList(&list, tmpData);
					}

					StartAddr = StartAddr->Flink; //指向下一个
				} while (StartAddr != pModuleList);
				KeUnstackDetachProcess(&ApcState);//卸载


				int index = 0;
				PCProcessModuleInfo tmp = PopHeadDoubleLoopList(&list);
				while (tmp != NULL)
				{
					//申请内存
					PCProcessModuleInfo pNewInfo = MyExAllocMemOry(sizeof(CProcessModuleInfo), PAGE_READWRITE, UserMode);
					if (!MmIsAddressValid(pNewInfo))
					{
						break;
					}

					//拷贝数据
					RtlMoveMemory(pNewInfo, tmp, sizeof(CProcessModuleInfo));


					if (IsInit != 0 && ((PCProcessModuleInfo)(*outData))->List.IsInitialize)
					{
						//插入链表里面
						InsertHeadList(&((PCProcessModuleInfo)(*outData))->List.List, &pNewInfo->List.List);
					}
					else
					{
						pNewInfo->List.IsInitialize = TRUE;						//已初始化
						InitializeListHead(&pNewInfo->List.List);				//初始化链表头
						*outData = pNewInfo;									//赋值
						IsInit = TRUE;
					}

					nRet++;

					ExFreePool(tmp);//释放内存
					tmp = PopHeadDoubleLoopList(&list);
				}

			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				MyDbgPrintfEx("[%ws] 触发了异常\n", __FUNCDNAME__);
				KeUnstackDetachProcess(&ApcState);//卸载

				//出现异常检测链表是否有数据 //释放资源
				int index = 0;
				PCProcessModuleInfo tmp = PopHeadDoubleLoopList(&list);
				while (tmp != NULL)
				{
					//申请内存
					PCProcessModuleInfo pNewInfo = MyExAllocMemOry(sizeof(CProcessModuleInfo), PAGE_READWRITE, UserMode);
					if (!MmIsAddressValid(pNewInfo))
					{
						break;
					}

					//拷贝数据
					RtlMoveMemory(pNewInfo, tmp, sizeof(CProcessModuleInfo));


					if (IsInit != 0 && ((PCProcessModuleInfo)(*outData))->List.IsInitialize)
					{
						//插入链表里面
						InsertHeadList(&((PCProcessModuleInfo)(*outData))->List.List, &pNewInfo->List.List);
					}
					else
					{
						pNewInfo->List.IsInitialize = TRUE;						//已初始化
						InitializeListHead(&pNewInfo->List.List);				//初始化链表头
						*outData = pNewInfo;									//赋值
						IsInit = TRUE;
					}

					nRet++;

					ExFreePool(tmp);//释放内存
					tmp = PopHeadDoubleLoopList(&list);
				}
				return 0;
			}
		}
	}
	////////////////////////////////////////////////////////////////////////
	return nRet;
}

ULONG64 EnumThread(ULONG64 pEprocess, PCProcessThreadInfo* OutData)
{
	PLIST_ENTRY CurrentList = 0, NextList = 0;
	ULONG64 ThreadObject = 0;
	ULONG64 Pid = 0, TPid = 0, Tid = 0;
	ULONG64 CountThread = 0;//存储线程数量
	PLARGE_INTEGER CreateTime = 0;
	CTIME_FIELDS TimeFields = { 0 };
	CSHORT Hour = 0;

	UCHAR IsGuiThreadFlag = FALSE;
	UCHAR IsRestrictedGuiThreadFlag = FALSE;
	UCHAR ThreaFlags = 0;

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCProcessVadInfo)(*OutData))->List.IsInitialize;

	ULONG64 dqMoubleCount = 0;
	ULONG64 dqSpaceSize = 0;
	NTSTATUS status = 0;
	if (MmIsAddressValid(pEprocess))
	{
		Pid = *(PULONG64)(pEprocess + _EPROCESS_UniqueProcessId);
		NextList = CurrentList = pEprocess + _EPROCESS_ThreadListHead;
		do
		{
			if (NextList == NULL)
			{
				break;
			}

			//申请新的内存
			PCProcessThreadInfo pNewInfo = MyExAllocMemOry(sizeof(CProcessThreadInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			ThreadObject = (ULONG64)NextList - _ETHREAD_ThreadListEntry;
			TPid = *(PULONG64)(ThreadObject + _KTHREAD_UniqueProcess);
			Tid = *(PULONG64)(ThreadObject + _KTHREAD_UniqueThread);

			//跳过已经被终止还挂在 ThreadListHead 上的僵尸线程（KTHREAD.State == 4 Terminated）
			//否则杀掉线程后用户在 UI 上还会看到它残留
			UCHAR ThreadState = *(PUCHAR)(ThreadObject + _KTHREAD_State);
			if (ThreadState == 4 /*Terminated*/)
			{
				NextList = NextList->Flink;
				continue;
			}

			//判断线程所属PID 是否是当前进程的PID
			if (TPid == Pid)
			{
				if (MmIsAddressValid(OutData))
				{
					pNewInfo->Ethread = ThreadObject;
					pNewInfo->UniqueThread = Tid;
					pNewInfo->Priority = *(PULONG64)(ThreadObject + _KTHREAD_Priority);
					pNewInfo->StartAddress = *(PULONG64)(ThreadObject + _ETHREAD_Win32StartAddress);
					pNewInfo->Teb = *(PULONG64)(ThreadObject + _KTHREAD_Teb);
					pNewInfo->ContextSwitches = *(PULONG64)(ThreadObject + _KTHREAD_ContextSwitches);
					pNewInfo->State = *(PULONG64)(ThreadObject + _KTHREAD_State);

					IsGuiThreadFlag = IsGuiThread(ThreadObject);
					IsRestrictedGuiThreadFlag = IsRestrictedGuiThread(ThreadObject);
					ThreaFlags = 0;

					if (IsGuiThreadFlag == TRUE)
					{
						ThreaFlags |= 0x1;
					}

					if (IsRestrictedGuiThreadFlag == TRUE)
					{
						ThreaFlags |= 0x2;
					}
					pNewInfo->ThreadTypeFlag = ThreaFlags; /*获取是否是GUI线程还是受限线程或普通线程*/

					CreateTime = (PLARGE_INTEGER)(ThreadObject + _ETHREAD_CreateTime);
					if (MmIsAddressValid(CreateTime))
					{
						LARGE_INTEGER LocalTime = { 0 };
						ExSystemTimeToLocalTime(CreateTime, &LocalTime);
						RtlTimeToTimeFields(&LocalTime, &TimeFields);
						pNewInfo->CreateTime = TimeFields;
					}

					CProcessModuleInfo pModuleInfo = { 0 };
					//获取所属当前进程模块
					if (IsProcessInModule(pEprocess, pNewInfo->StartAddress, &pModuleInfo))
					{
						RtlMoveMemory(pNewInfo->MoudleName, pModuleInfo.ModuleName, MY_MAX_PATH);
					}

					//插入链表
					if (IsInit != 0 && ((PCProcessThreadInfo)(*OutData))->List.IsInitialize)
					{
						//插入链表里面
						InsertHeadList(&((PCProcessThreadInfo)(*OutData))->List.List, &pNewInfo->List.List);
					}
					else
					{
						pNewInfo->List.IsInitialize = TRUE;						//已初始化
						InitializeListHead(&pNewInfo->List.List);				//初始化链表头
						*OutData = pNewInfo;									//赋值
						IsInit = TRUE;
					}
				}
				CountThread++;
			}
			NextList = NextList->Flink;
		} while (CurrentList != NextList);

		return CountThread;//进程获取正在运行的线程数量
	}
	return 0;
}

ULONG64 IsGuiThread(ULONG64 pEthread)
{
	PULONG32 cGuiThreadFlag = NULL;
	if (!MmIsAddressValid(pEthread))
	{
		return MYERROR;
	}

	cGuiThreadFlag = pEthread + _KTHREAD_ThreadFlagsSpare_ThreadFlags;

	if ((*cGuiThreadFlag) & _ThreadFlagsSpare_GuiThread) /*判断是否是GUI线程*/
	{
		return TRUE;
	}

	return FALSE;
}

ULONG64 IsRestrictedGuiThread(ULONG64 pEthread)
{
	PULONG32 cGuiThreadFlag = NULL;
	if (!MmIsAddressValid(pEthread))
	{
		return MYERROR;
	}

	cGuiThreadFlag = pEthread + _KTHREAD_ThreadFlagsSpare_ThreadFlags;

	if ((*cGuiThreadFlag) & _ThreadFlagsSpare_RestrictedGuiThread) /*判断是否是受限GUI线程*/
	{
		return TRUE;
	}

	return FALSE;
}

ULONG64 ObGetObjectType(ULONG64 Object)
{
	if (!MmIsAddressValid(Object))
	{
		MyDbgPrintfEx(" OBJECT:[%I64X] 无效对象地址!.....\n", Object);
		return NULL;
	}
	return (ULONG64)((PULONG64)ObTypeIndexTable)[*ObHeaderCookie ^ *(PUCHAR)(Object - _OBJECT_HEADER_TypeIndex) ^ (ULONG64)(UCHAR)((USHORT)(Object - _OBJECT_HEADER_PointerCount) >> 8)];
}

PWCHAR obGetObjectName(ULONG64 Object, PUNICODE_STRING TypeName)/*根据对象获取对象名*/
{
	/*功能实现有问题.............................................*/
	PWCHAR HandleName = NULL;
	PUNICODE_STRING LinkTarget = NULL;
	PUNICODE_STRING FileName = NULL;
	PDEVICE_OBJECT DeviceObject = NULL;
	PDEVOBJ_EXTENSION DeviceObjectExtension = NULL;

	HandleName = ExAllocatePool2(POOL_FLAG_PAGED, MY_MAX_PATH, L"ObjFlag");
	memset(HandleName, 0, MY_MAX_PATH);

	if (MmIsAddressValid(Object) && MmIsAddressValid(HandleName))
	{
		if (RtlCompareUnicodeString(TypeName, &g_RootProcessName, TRUE) == 0)
		{
			FsVolumeDeviceToDosNameEx(*(PULONG64)(Object + _EPROCESS_ImageFilePointer), HandleName);
		}
		else if (RtlCompareUnicodeString(TypeName, &g_RootDirectoryName, TRUE) == 0)
		{
		}
		else if (RtlCompareUnicodeString(TypeName, &g_RootSymbolicLinkName, TRUE) == 0)
		{
			LinkTarget = Object + _OBJECT_SYMBOLIC_LINK_LinkTarget;
			if (MmIsAddressValid(LinkTarget))
			{
				memcpy_s(HandleName, MY_MAX_PATH, LinkTarget->Buffer, LinkTarget->Length);
			}
		}
		else if (RtlCompareUnicodeString(TypeName, &g_RootFileName, TRUE) == 0)
		{
			FileName = (PUNICODE_STRING64)((ULONG64)Object + _FILE_OBJECT_FileName);
			if (MmIsAddressValid(FileName))
			{
				memcpy_s(HandleName, MY_MAX_PATH, FileName->Buffer, FileName->Length);
			}
		}
		else
		{
			POBJECT_HEADER_NAME_INFO NameAddr = (POBJECT_HEADER_NAME_INFO)ObQueryNameInfo(Object);
			if (MmIsAddressValid(NameAddr))
			{
				PUNICODE_STRING ObjectName = &NameAddr->Name;
				if (MmIsAddressValid(ObjectName) && ObjectName->Length != 0)
				{
					memcpy_s(HandleName, MY_MAX_PATH, ObjectName, ObjectName->MaximumLength);
				}
			}
		}
	}
	return HandleName;
}

ULONG64 EnumProcessHandleTable(ULONG64 eProcess, PCProcessHandleInfo* OutData)//遍历进程里的句柄表
{
	ULONG64 TableCode = 0;
	ULONG64 ObjectAddr = 0;
	ULONG64 Leve1 = 0;
	ULONG64 Count = 0;
	ULONG64 Index = 0;
	PPROCESS_HANDLE_STRUCT Base = 0;
	ULONG64 nIndex = 1;

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCProcessVadInfo)(*OutData))->List.IsInitialize;


	if (MmIsAddressValid(eProcess))
	{
		TableCode = eProcess + _EPROCESS_ObjectTable;
		if (MmIsAddressValid(TableCode))
		{
			TableCode = *(PULONG64)TableCode + _HANDLE_TABLE_TableCode;
			if (MmIsAddressValid(TableCode))
			{
				TableCode = *(PULONG64)TableCode;
				if (MmIsAddressValid(TableCode))
				{
					switch (TableCode & 3)
					{
					case 0:
						TableCode &= 0xFFFFFFFFFFFFFFFC;
						Base = TableCode;
						for (int i = 0; i < 256; i++)
						{
							ObjectAddr = ((Base[i].TableAddr >> 0x10) | 0xFFFF000000000000) & 0xFFFFFFFFFFFFFFF0;
							if (MmIsAddressValid(ObjectAddr))
							{
								//申请空间
								PCProcessHandleInfo pNewInfo = MyExAllocMemOry(sizeof(CProcessHandleInfo), PAGE_READWRITE, UserMode);
								if (pNewInfo == NULL)
								{
									break;
								}

								/*__debugbreak();*/
								Index = Count;
								pNewInfo->Power = Base[i].Power;
								pNewInfo->Handle = nIndex * 4;
								WriteBufferToProcessHandleStruct(pNewInfo, ObjectAddr);

								//插入链表
								if (IsInit != 0 && ((PCProcessVadInfo)(*OutData))->List.IsInitialize)
								{
									//插入链表里面
									InsertHeadList(&((PCProcessVadInfo)(*OutData))->List.List, &pNewInfo->List.List);
								}
								else
								{
									pNewInfo->List.IsInitialize = TRUE;						//已初始化
									InitializeListHead(&pNewInfo->List.List);				//初始化链表头
									*OutData = pNewInfo;									//赋值
									IsInit = TRUE;
								}

								nIndex++;
								Count++;
							}
						}
						break;
					case 1:
						TableCode &= 0xFFFFFFFFFFFFFFFC;
						for (int i = 0; i < 512; i++)
						{
							Base = ((PULONG64)TableCode)[i];
							if (MmIsAddressValid(Base))
							{
								for (int j = 0; j < 256; j++)
								{
									ObjectAddr = ((Base[j].TableAddr >> 0x10) | 0xFFFF000000000000) & 0xFFFFFFFFFFFFFFF0;

									if (MmIsAddressValid(ObjectAddr))
									{

										//申请空间
										PCProcessHandleInfo pNewInfo = MyExAllocMemOry(sizeof(CProcessHandleInfo), PAGE_READWRITE, UserMode);
										if (pNewInfo == NULL)
										{
											break;
										}
										/*__debugbreak();*/
										Index = Count;
										pNewInfo->Power = Base[j].Power;
										pNewInfo->Handle = nIndex * 4;
										WriteBufferToProcessHandleStruct(pNewInfo, ObjectAddr);

										//插入链表
										if (IsInit != 0 && ((PCProcessVadInfo)(*OutData))->List.IsInitialize)
										{
											//插入链表里面
											InsertHeadList(&((PCProcessVadInfo)(*OutData))->List.List, &pNewInfo->List.List);
										}
										else
										{
											pNewInfo->List.IsInitialize = TRUE;						//已初始化
											InitializeListHead(&pNewInfo->List.List);				//初始化链表头
											*OutData = pNewInfo;									//赋值
											IsInit = TRUE;
										}


										nIndex++;
										Count++;
									}
								}
							}
						}
						break;
					case 2:
						TableCode &= 0xFFFFFFFFFFFFFFFC;
						for (int i = 0; i < 512; i++)
						{
							Leve1 = ((PULONG64)TableCode)[i];
							if (MmIsAddressValid(Leve1))
							{
								for (int j = 0; j < 512; j++)
								{
									Base = ((PULONG64)Leve1)[j];
									if (MmIsAddressValid(Base))
									{
										for (int k = 0; k < 256; k++)
										{
											ObjectAddr = ((Base[k].TableAddr >> 0x10) | 0xFFFF000000000000) & 0xFFFFFFFFFFFFFFF0;
											if (MmIsAddressValid(ObjectAddr))
											{

												//申请空间
												PCProcessHandleInfo pNewInfo = MyExAllocMemOry(sizeof(CProcessHandleInfo), PAGE_READWRITE, UserMode);
												if (pNewInfo == NULL)
												{
													break;
												}
												/*__debugbreak();*/
												Index = Count;
												pNewInfo->Power = Base[k].Power;
												pNewInfo->Handle = nIndex * 4;
												WriteBufferToProcessHandleStruct(pNewInfo, ObjectAddr);

												//插入链表
												if (IsInit != 0 && ((PCProcessVadInfo)(*OutData))->List.IsInitialize)
												{
													//插入链表里面
													InsertHeadList(&((PCProcessVadInfo)(*OutData))->List.List, &pNewInfo->List.List);
												}
												else
												{
													pNewInfo->List.IsInitialize = TRUE;						//已初始化
													InitializeListHead(&pNewInfo->List.List);				//初始化链表头
													*OutData = pNewInfo;									//赋值
													IsInit = TRUE;
												}

												nIndex++;
												Count++;
											}
										}
									}
								}
							}
						}
						break;
					}
				}
			}
		}
	}
	return Count;//返回句柄的数量
}

VOID WriteBufferToProcessHandleStruct(PCProcessHandleInfo OutProcessTable, ULONG64 ObjectAddr)							//获取句柄的信息写入结构体
{
	POBJECT_TYPE ObjectType = 0;
	PWCHAR ObjectName = NULL;
	ObjectType = ObGetObjectType(ObjectAddr + _OBJECT_HEADER_SIZE);	//获取对象的类型


	if (MmIsAddressValid(OutProcessTable))
	{

		OutProcessTable->HandleObject = ObjectAddr + _OBJECT_HEADER_SIZE;

		OutProcessTable->Quote = *(PULONG64)ObjectAddr;

		if (MmIsAddressValid(ObjectType))
		{
			memcpy_s(OutProcessTable->HandleType, MAX_BASE_FILE_NAME, ObjectType->Name.Buffer, ObjectType->Name.MaximumLength);
			OutProcessTable->Index = ObjectType->Index;

			NTSTATUS status;
			ULONG size = 0;
			POBJECT_NAME_INFORMATION nameInfo = NULL;

			// 先调用一次获取所需缓冲区大小
			status = ObQueryNameString(OutProcessTable->HandleObject, NULL, 0, &size);
			if (status != STATUS_INFO_LENGTH_MISMATCH) {
				return status;
			}

			nameInfo = (POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, size, 'namT');
			if (nameInfo)
			{
				status = ObQueryNameString(OutProcessTable->HandleObject, nameInfo, size, &size);
				if (NT_SUCCESS(status) && nameInfo->Name.Length > 0)
				{
					memcpy_s(OutProcessTable->HandleName, MY_MAX_PATH, nameInfo->Name.Buffer, nameInfo->Name.Length);
				}
				ExFreePoolWithTag(nameInfo, 'namT');
			}

			// ObQueryNameString 拿不到名字的常见类型，按类型 peek 内部字段提供语义化名称
			if (OutProcessTable->HandleName[0] == L'\0' && ObjectType->Name.Buffer != NULL)
			{
				const PVOID Object = (PVOID)OutProcessTable->HandleObject;
				const WCHAR* tn = ObjectType->Name.Buffer;
				WCHAR* dst = OutProcessTable->HandleName;
				SIZE_T cap = MY_MAX_PATH;

				if (_wcsicmp(tn, L"Process") == 0)
				{
					HANDLE pid = PsGetProcessId((PEPROCESS)Object);
					UCHAR* img = PsGetProcessImageFileName((PEPROCESS)Object);
					if (img)
					{
						RtlStringCbPrintfW(dst, cap * sizeof(WCHAR),
							L"%hs (PID:%llu)", (char*)img, (ULONG64)pid);
					}
					else
					{
						RtlStringCbPrintfW(dst, cap * sizeof(WCHAR), L"PID:%llu", (ULONG64)pid);
					}
				}
				else if (_wcsicmp(tn, L"Thread") == 0)
				{
					HANDLE tid = PsGetThreadId((PETHREAD)Object);
					HANDLE pid = PsGetThreadProcessId((PETHREAD)Object);
					PEPROCESS owner = NULL;
					UCHAR* img = NULL;
					if (NT_SUCCESS(PsLookupProcessByProcessId(pid, &owner)) && owner)
					{
						img = PsGetProcessImageFileName(owner);
					}
					if (img)
					{
						RtlStringCbPrintfW(dst, cap * sizeof(WCHAR),
							L"TID:%llu  %hs(PID:%llu)", (ULONG64)tid, (char*)img, (ULONG64)pid);
					}
					else
					{
						RtlStringCbPrintfW(dst, cap * sizeof(WCHAR),
							L"TID:%llu PID:%llu", (ULONG64)tid, (ULONG64)pid);
					}
					if (owner) ObDereferenceObject(owner);
				}
				else if (_wcsicmp(tn, L"Event") == 0)
				{
					wcsncpy_s(dst, cap, L"<unnamed Event>", _TRUNCATE);
				}
				else if (_wcsicmp(tn, L"IoCompletion") == 0)
				{
					wcsncpy_s(dst, cap, L"<unnamed IoCompletion>", _TRUNCATE);
				}
				else if (_wcsicmp(tn, L"Mutant") == 0)
				{
					wcsncpy_s(dst, cap, L"<unnamed Mutant>", _TRUNCATE);
				}
				else if (_wcsicmp(tn, L"Semaphore") == 0)
				{
					wcsncpy_s(dst, cap, L"<unnamed Semaphore>", _TRUNCATE);
				}
				else if (_wcsicmp(tn, L"Section") == 0)
				{
					wcsncpy_s(dst, cap, L"<anonymous Section>", _TRUNCATE);
				}
			}
		}
	}
}

ULONG64 ObQueryNameInfo(ULONG64 Object)
{

	UCHAR InfoMask = 0;
	ULONG64 HeaderAddr = 0;
	if (!MmIsAddressValid(Object))
	{
		return NULL;
	}

	HeaderAddr = (ULONG64)Object - _OBJECT_HEADER_SIZE;

	InfoMask = *(PUCHAR)((ULONG64)HeaderAddr + _OBJECT_HEADER_InfoMask);

	if ((InfoMask & 2) != 0)
	{
		//字符串地址
		PULONG64 StrAddr = (ULONG64)(HeaderAddr)-(UCHAR)((PUCHAR)ObpInfoMaskToOffset)[InfoMask & 3];
		if (StrAddr == HeaderAddr)
		{
			StrAddr = NULL;
		}
		return StrAddr;
	}
	return NULL;
}

ULONG64 obQueryRootDirectoryTable(PUNICODE_STRING RootName)//根据名字获取对象目录表
{
	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	ULONG Index = 0;
	POBJECT_DIRECTORY RootDirectory = 0;
	POBJECT_DIRECTORY_ENTRY CurrectRootDirectoryEntry = 0, NextRootDirectoryEntry = 0;
	POBJECT_HEADER_NAME_INFO ObjectNameInfo = 0;
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//项目需求区域
	RootDirectory = *(PULONG64)ObpRootDirectoryObject;//获取全局变量ObpRootDirectoryObject存储的值 _OBJECT_DIRECTORY_ENTRY
	if (!MmIsAddressValid(RootDirectory))
	{
		//MyDbgPrintfEx("无效的值[ObpRootDirectoryObject : %I64X]\n", RootDirectory);
		return NULL;
	}

	for (Index = 0; Index < NUMBER_HASH_BUCKETS; Index++)
	{
		CurrectRootDirectoryEntry = RootDirectory->HashBuckets[Index];
		if (CurrectRootDirectoryEntry != NULL)
		{
			do
			{
				ObjectNameInfo = (POBJECT_HEADER_NAME_INFO)ObQueryNameInfo(CurrectRootDirectoryEntry->Object);
				if (MmIsAddressValid(ObjectNameInfo))
				{
					if (RtlCompareUnicodeString(RootName, &ObjectNameInfo->Name, TRUE) == 0)
					{
						return CurrectRootDirectoryEntry->Object; //返回Object地址
					}
				}
				CurrectRootDirectoryEntry = CurrectRootDirectoryEntry->ChainLink; //
			} while (CurrectRootDirectoryEntry != NULL);
		}
	}
	////////////////////////////////////////////////////////////////////////
	return NULL;
}

POBJECT_NAME_INFORMATION obGetObjectNameEx(PVOID pObject)
{
	if (MmIsAddressValid(pObject))
	{
		ULONG64 size = 0;
		// 先调用一次获取所需缓冲区大小
		NTSTATUS status = ObQueryNameString(pObject, NULL, 0, &size);
		if (status != STATUS_INFO_LENGTH_MISMATCH)
		{
			return  FALSE;
		}

		POBJECT_NAME_INFORMATION nameInfo = (POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, size, 'namT');
		if (nameInfo)
		{
			status = ObQueryNameString(pObject, nameInfo, size, &size);
			if (NT_SUCCESS(status) && nameInfo->Name.Length > 0)
			{

				return nameInfo;

			}
			ExFreePoolWithTag(nameInfo, 'namT');
		}
	}
	return FALSE;
}

ULONG64 obQueryDirectoryTable(POBJECT_DIRECTORY pDirectroy, PUNICODE_STRING RootName)
{
	int Index = 0;
	POBJECT_DIRECTORY_ENTRY CurrectRootDirectoryEntry = 0, NextRootDirectoryEntry = 0;

	if (MmIsAddressValid(pDirectroy) && MmIsAddressValid(RootName))
	{

		//POBJECT_HEADER_NAME_INFO pDir = (POBJECT_HEADER_NAME_INFO)ObQueryNameInfo(pDirectroy);

		//判断是否是目录对象
		//if (RtlCompareUnicodeString(&g_RootDirectoryName, &pDir->Name, TRUE) == 0)
		{

			for (Index = 0; Index < NUMBER_HASH_BUCKETS; Index++)
			{
				CurrectRootDirectoryEntry = pDirectroy->HashBuckets[Index];

				if (MmIsAddressValid(CurrectRootDirectoryEntry))
				{
					do
					{

						ULONG64 size = 0;
						PVOID pObject = CurrectRootDirectoryEntry->Object;

						POBJECT_NAME_INFORMATION pObjectName = obGetObjectNameEx(pObject);



						UNICODE_STRING Name = { 0 };
						PUNICODE_STRING pName = &pObjectName->Name;

						if (ExtractDriverName(&pObjectName->Name, &Name))
						{
							pName = &Name;
						}

						if (pObjectName && RtlCompareUnicodeString(RootName, pName, TRUE) == 0)
						{
							ExFreePoolWithTag(pObjectName, 'namT');

							return CurrectRootDirectoryEntry->Object; //返回Object地址
						}

						CurrectRootDirectoryEntry = CurrectRootDirectoryEntry->ChainLink; //
					} while (CurrectRootDirectoryEntry != NULL);
				}
			}
		}
	}
	return NULL;
}

ULONG64 EnumDriverObject(PCDriverInfo* OutData)//遍历根目录对象
{
	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	POBJECT_DIRECTORY RootDriverObject = 0;
	POBJECT_DIRECTORY RootFileSystemObject = 0;
	POBJECT_DIRECTORY_ENTRY CurrentDriverObjectEntry = 0;
	POBJECT_DIRECTORY_ENTRY CurrentFileSystemObjectEntry = 0;
	POBJECT_HEADER_NAME_INFO ObjectNameInfo = 0;
	PDRIVER_OBJECT CurrectDriverObject = 0;
	PDRIVER_EXTENSION CurrentDriverExtension = 0;
	ULONG index = 0;
	ULONG SysIndex = 0;
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//变量初始化区域
	RootDriverObject = (POBJECT_DIRECTORY)obQueryRootDirectoryTable(&g_RootDriverName);//获取Driver目录对象
	RootFileSystemObject = (POBJECT_DIRECTORY)obQueryRootDirectoryTable(&g_RootFileSystemName);//获取FileSystem目录对象

	//MyDbgPrintfEx("RootDriverObject : %I64X RootFileSystemObject : %I64X\n", RootDriverObject, RootFileSystemObject);

	////////////////////////////////////////////////////////////////////////

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCDriverInfo)(*OutData))->List.IsInitialize;

	////////////////////////////////////////////////////////////////////////
	//项目需求区域
	if (RootDriverObject != NULL || RootFileSystemObject != NULL)
	{
		for (index = 0; index < NUMBER_HASH_BUCKETS; index++)
		{
			///ExAcquirePushLockShared(&RootDriverObject->Lock);
			if (!MmIsAddressValid(&RootDriverObject->HashBuckets[index]) || !MmIsAddressValid(&RootFileSystemObject->HashBuckets[index]))
			{
				break;
			}

			CurrentDriverObjectEntry = RootDriverObject->HashBuckets[index];
			//ExReleasePushLockShared(&RootDriverObject->Lock);

			//ExAcquirePushLockShared(&RootFileSystemObject->Lock);
			CurrentFileSystemObjectEntry = RootFileSystemObject->HashBuckets[index];
			//ExReleasePushLockShared(&RootFileSystemObject->Lock);


			if (MmIsAddressValid(CurrentDriverObjectEntry))
			{
				//遍历Driver表
				do
				{
					if (MmIsAddressValid(CurrentDriverObjectEntry->Object))
					{
						CurrectDriverObject = (PDRIVER_OBJECT)CurrentDriverObjectEntry->Object;
						CurrentDriverExtension = CurrectDriverObject->DriverExtension;
						if (MmIsAddressValid(CurrectDriverObject))
						{
							//申请空间
							PCDriverInfo pNewInfo = MyExAllocMemOry(sizeof(CDriverInfo), PAGE_READWRITE, UserMode);
							if (pNewInfo == NULL)
							{
								break;
							}

							//拷贝数据
							pNewInfo->DriverObject = CurrectDriverObject;
							pNewInfo->DriverStart = CurrectDriverObject->DriverStart;
							memcpy_s(pNewInfo->ServerName, MY_MAX_PATH, CurrectDriverObject->DriverName.Buffer, CurrectDriverObject->DriverName.Length);
							if (MmIsAddressValid(CurrentDriverExtension))
							{
								UNICODE_STRING CurrentDriverExtensionName = { 0 };
								if (ExtractDriverName(&CurrentDriverExtension->ServiceKeyName, &CurrentDriverExtensionName))
								{
									memcpy_s(pNewInfo->DriverName, MY_MAX_PATH, CurrentDriverExtensionName.Buffer, CurrentDriverExtensionName.Length);
								}
								else
								{
									memcpy_s(pNewInfo->DriverName, MY_MAX_PATH, CurrentDriverExtension->ServiceKeyName.Buffer, CurrentDriverExtension->ServiceKeyName.Length);
								}
							}

							//插入链表
							if (IsInit != 0 && ((PCDriverInfo)(*OutData))->List.IsInitialize)
							{
								//插入链表里面
								InsertHeadList(&((PCDriverInfo)(*OutData))->List.List, &pNewInfo->List.List);
							}
							else
							{
								pNewInfo->List.IsInitialize = TRUE;						//已初始化
								InitializeListHead(&pNewInfo->List.List);				//初始化链表头
								*OutData = pNewInfo;									//赋值
								IsInit = TRUE;
							}


							SysIndex++;
						}
					}
					CurrentDriverObjectEntry = CurrentDriverObjectEntry->ChainLink;
				} while (CurrentDriverObjectEntry != NULL);
			}


			if (MmIsAddressValid(CurrentFileSystemObjectEntry))
			{
				do
				{
					if (MmIsAddressValid(CurrentFileSystemObjectEntry->Object))
					{
						CurrectDriverObject = (PDRIVER_OBJECT)CurrentFileSystemObjectEntry->Object;
						CurrentDriverExtension = CurrectDriverObject->DriverExtension;
						if (CurrectDriverObject != NULL)
						{
							//申请空间
							PCDriverInfo pNewInfo = MyExAllocMemOry(sizeof(CDriverInfo), PAGE_READWRITE, UserMode);
							if (pNewInfo == NULL)
							{
								break;
							}

							//拷贝数据
							pNewInfo->DriverObject = CurrectDriverObject;
							pNewInfo->DriverStart = CurrectDriverObject->DriverStart;

							memcpy_s(pNewInfo->ServerName, MY_MAX_PATH, CurrectDriverObject->DriverName.Buffer, CurrectDriverObject->DriverName.Length);
							if (MmIsAddressValid(CurrentDriverExtension))
							{
								memcpy_s(pNewInfo->DriverName, MY_MAX_PATH, CurrentDriverExtension->ServiceKeyName.Buffer, CurrentDriverExtension->ServiceKeyName.Length);
							}

							//插入链表
							if (IsInit != 0 && ((PCDriverInfo)(*OutData))->List.IsInitialize)
							{
								//插入链表里面
								InsertHeadList(&((PCDriverInfo)(*OutData))->List.List, &pNewInfo->List.List);
							}
							else
							{
								pNewInfo->List.IsInitialize = TRUE;						//已初始化
								InitializeListHead(&pNewInfo->List.List);				//初始化链表头
								*OutData = pNewInfo;									//赋值
								IsInit = TRUE;
							}

							SysIndex++;
						}
					}
					CurrentFileSystemObjectEntry = CurrentFileSystemObjectEntry->ChainLink;
				} while (CurrentFileSystemObjectEntry != NULL);
			}
		}
	}
	////////////////////////////////////////////////////////////////////////
	return SysIndex;
}

ULONG64 EnumSysModule(PDRIVER_OBJECT pDriver, PCDriverInfo* OutData)//遍历.Sys对象
{
	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	PLDR_DATA_TABLE_ENTRY pLdr = 0;
	PLIST_ENTRY CurrentListEntry = 0;
	PLIST_ENTRY NextListEntry = 0;
	ULONG RetIndex = 0;
	////////////////////////////////////////////////////////////////////////

	if (!MmIsAddressValid(PsLoadedModuleList))
	{
		return 0; //如果PsLoadedModuleList无效则返回0
	}

	////////////////////////////////////////////////////////////////////////
	//变量初始化区域
	//pLdr = (PLDR_DATA_TABLE_ENTRY)pDriver->DriverSection;
	CurrentListEntry = PsLoadedModuleList->Flink;//(PLIST_ENTRY)pLdr->InLoadOrderLinks.Flink;
	NextListEntry = CurrentListEntry;
	////////////////////////////////////////////////////////////////////////


	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCDriverInfo)(*OutData))->List.IsInitialize;

	////////////////////////////////////////////////////////////////////////
	//项目需求区域
	do
	{
		pLdr = CONTAINING_RECORD(NextListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
		if (pLdr->DllBase != 0 && MmIsAddressValid(pLdr) == TRUE)
		{
			//申请空间
			PCDriverInfo pNewInfo = MyExAllocMemOry(sizeof(CDriverInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			//拷贝数据
			memcpy_s(pNewInfo->ImageBaseName, MAX_BASE_FILE_NAME, pLdr->BaseDllName.Buffer, pLdr->BaseDllName.Length);
			memcpy_s(pNewInfo->ImageFullBaseName, MY_MAX_PATH, pLdr->FullDllName.Buffer, pLdr->FullDllName.Length);
			pNewInfo->ImageFullBaseNameLength = pLdr->FullDllName.Length;
			pNewInfo->ImageBaseAddr = pLdr->DllBase;
			pNewInfo->Size = pLdr->SizeOfImage;
			//获取驱动加载顺序
			pNewInfo->LoadOrder = RetIndex;

			//插入链表
			if (IsInit != 0 && ((PCDriverInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCDriverInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);		//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}

			RetIndex++;
		}
		NextListEntry = NextListEntry->Flink;
	} while (NextListEntry != CurrentListEntry);
	////////////////////////////////////////////////////////////////////////
	return RetIndex;
}

ULONG64 KeGroundIndexGetSsdtTableFunAddr(IN ULONG64 KiServiceTableBaseAddr, IN ULONG64 dwIndex)
{
	LONG32 dwOffset = 0;
	ULONG64 FunBaseAddr = 0;

	if (MmIsAddressValid(KiServiceTableBaseAddr))
	{
		dwOffset = ((PULONG32)(KiServiceTableBaseAddr))[dwIndex];						//将索引以次乘以4
		FunBaseAddr = ((ULONG64)KiServiceTableBaseAddr + (dwOffset >> 0x4));
		return FunBaseAddr;
	}
	return NULL;
}

ULONG64 EnumSsdtTable(PCSsdtInfo* OutData)
{
	ULONG64 dqCount = 0;
	ULONG64 dqIndex = 0;
	ULONG64 FunAddr = 0;
	ULONG64 TableBaseAddr = 0;
	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCProcessVadInfo)(*OutData))->List.IsInitialize;

	if (MmIsAddressValid(KeServiceDescriptorTable))
	{
		dqCount = ((PKSYSTEM_SERVICE_TABLE)KeServiceDescriptorTable)->NumberOfService;			//获取SSDT表数量
		TableBaseAddr = ((PKSYSTEM_SERVICE_TABLE)KeServiceDescriptorTable)->ServiceTableBase;	//获取SSDT函数基地址

		for (dqIndex = 0; dqIndex < dqCount; dqIndex++)
		{
			FunAddr = KeGroundIndexGetSsdtTableFunAddr(TableBaseAddr, dqIndex);				//计算出真正的函数地址

			//申请空间
			PCSsdtInfo pNewInfo = MyExAllocMemOry(sizeof(CSsdtInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			pNewInfo->NtFunAddr = FunAddr;																//函数地址
			pNewInfo->NumberOrder = dqIndex;															//函数地址所在表的位置
			pNewInfo->SrcNtFunAddr = TableBaseAddr + g_NewKiServiceTable[dqIndex];


			//获取函数地址所在模块
			CDriverInfo DriverInfo = { 0 };
			if (IsSysModuleEx(FunAddr, &DriverInfo))
			{
				memcpy_s(pNewInfo->Path, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
			}

			if (IsInit != 0 && ((PCSsdtInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCSsdtInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}
		}
	}
	return dqCount;
}

ULONG64 EnumSsdtShadowTable(PCSsdtInfo* OutData)
{
	ULONG64 dqCount = 0;
	ULONG64 dqIndex = 0;
	ULONG64 FunAddr = 0;
	ULONG64 TableBaseAddr = 0;
	PCSsdtInfo tmpData = NULL;
	ULONG64 WinLogeProcess = ByNameGetProcessObject(L"winlogon.exe");

	if (!MmIsAddressValid(WinLogeProcess))
	{
		return FALSE;
	}

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCProcessVadInfo)(*OutData))->List.IsInitialize;


	//初始化链表
	CList list;
	InitDoubleLoopList(&list);								//初始化链表

	if (!IsProcessSafeToAttach(WinLogeProcess))
	{
		return FALSE;
	}

	//附加到winlogon.exe进程
	KAPC_STATE apcState;
	KeStackAttachProcess(WinLogeProcess, &apcState);
	ULONG64 nServiceTableShadow = (ULONG64)KeServiceDescriptorTableShadow + sizeof(KSYSTEM_SERVICE_TABLE);
	if (MmIsAddressValid(nServiceTableShadow))
	{
		dqCount = ((PKSYSTEM_SERVICE_TABLE)nServiceTableShadow)->NumberOfService;							//获取SSDT表数量
		TableBaseAddr = ((PKSYSTEM_SERVICE_TABLE)nServiceTableShadow)->ServiceTableBase;					//获取SSDT函数基地址


		for (dqIndex = 0; dqIndex < dqCount; dqIndex++)
		{
			FunAddr = KeGroundIndexGetSsdtTableFunAddr(TableBaseAddr, dqIndex);								//计算出真正的函数地址
			tmpData = (PCSsdtInfo)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(CSsdtInfo), L"tmpData");		//申请非分页内存;
			if (!MmIsAddressValid(tmpData))
			{
				continue;
			}

			memset(tmpData, 0, sizeof(CSsdtInfo));

			tmpData->NtFunAddr = FunAddr;
			tmpData->NumberOrder = dqIndex;
			//tmpData->SrcNtFunAddr = KeGroundIndexGetSsdtTableFunAddr(g_NewW32pServiceTable, dqIndex);	//获取原函数地址
			tmpData->SrcNtFunAddr = TableBaseAddr + ((LONG32)g_NewW32pServiceTable[dqIndex] >> 4);

			//获取函数所在模块路径
			CDriverInfo DriverInfo = { 0 };
			if (IsSysModuleEx(FunAddr, &DriverInfo))
			{
				memcpy_s(tmpData->Path, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
			}


			//插入链表
			InsertTrailDoubleLoopList(&list, tmpData);
		}

	}
	KeUnstackDetachProcess(&apcState);


	PCSsdtInfo tmp = PopHeadDoubleLoopList(&list);
	while (tmp != NULL)
	{
		//申请空间
		PCSsdtInfo pNewInfo = MyExAllocMemOry(sizeof(CSsdtInfo), PAGE_READWRITE, UserMode);
		if (pNewInfo == NULL)
		{
			break;
		}

		//拷贝数据
		pNewInfo->NtFunAddr = tmp->NtFunAddr;
		pNewInfo->NumberOrder = tmp->NumberOrder;
		pNewInfo->SrcNtFunAddr = tmp->SrcNtFunAddr;
		memcpy_s(pNewInfo->Path, MY_MAX_PATH, tmp->Path, MY_MAX_PATH);



		if (IsInit != 0 && ((PCSsdtInfo)(*OutData))->List.IsInitialize)
		{
			//插入链表里面
			InsertHeadList(&((PCSsdtInfo)(*OutData))->List.List, &pNewInfo->List.List);
		}
		else
		{
			pNewInfo->List.IsInitialize = TRUE;						//已初始化
			InitializeListHead(&pNewInfo->List.List);				//初始化链表头
			*OutData = pNewInfo;									//赋值
			IsInit = TRUE;
		}

		ExFreePool(tmp);//释放内存
		tmp = PopHeadDoubleLoopList(&list);
	}

	return dqCount;
}

ULONG64 ByNameGetProcessObject(PWCHAR ProcessName)
{
	if (!MmIsAddressValid(ProcessName) && !MmIsAddressValid(PspCidTable) && !MmIsAddressValid(ObTypeIndexTable))
	{
		return NULL;
	}
	//遍历全局句柄表

	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	UCHAR TypeIndex = 0, TableIndex = 0;
	ULONG64 TableCode = 0, TableData = 0;
	ULONG64 Index = 0, Index1 = 0, Level1 = 0, Index2 = 0, Level2 = 0;
	PUNICODE_STRING64 TypeTempName = 0;
	ULONG64 IndexProcess = 0;
	ULONG64 ImageFilePointer = 0;
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//变量初始化区域 
	TableCode = *(PULONG64)((*(PULONG64)PspCidTable) + _HANDLE_TABLE_TableCode);
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//项目需求区域
	if (MmIsAddressValid(TableCode))
	{
		switch (TableCode & 3)
		{
		case 0://一级句柄表
			TableCode &= 0xFFFFFFFFFFFFFFFC;
			for (Index = 0; Index < 256; Index++)
			{
				TableData = *(PULONG64)((ULONG64)TableCode + (Index * 0x10));
				if (MmIsAddressValid(TableData))
				{
					TableData = ((ULONG64)TableData >> 0x10) | 0xFFFF000000000000;
					if (MmIsAddressValid(TableData))
					{
						TypeIndex = *(PUCHAR)((ULONG64)TableData - _OBJECT_HEADER_TypeIndex);
						TableIndex = (unsigned __int8)((unsigned __int16)(TableData - _OBJECT_HEADER_PointerCount) >> 8);
						TypeIndex = TypeIndex ^ TableIndex;
						TypeIndex = TypeIndex ^ *ObHeaderCookie;
						if (TypeIndex != 0)
						{
							TypeTempName = (ULONG64)(*(PULONG64)(((PULONG64)ObTypeIndexTable)[TypeIndex])) + 0x10;
							if (RtlCompareUnicodeString(&g_RootProcessName, TypeTempName, TRUE) == 0)
							{
								CProcessInfo ProcessInfo = { 0 };

								WriteBufferToProcessStructEx(&ProcessInfo, TableData);//将进程信息写入结构体

								UNICODE_STRING pSrcName = { 0 };
								UNICODE_STRING pDstName = { 0 };
								WCHAR pszBuf[MAX_PATH * 2] = { 0 };
								swprintf(pszBuf, L"*%ws", ProcessName);

								RtlInitUnicodeString(&pSrcName, ProcessInfo.FullFileName);
								RtlInitUnicodeString(&pDstName, pszBuf);

								//正则表达式查找路径名中的exe名称
								if (FsRtlIsNameInExpression(&pDstName, &pSrcName, FALSE, NULL) == TRUE)
								{
									return TableData; //返回EPROCESS
								}
							}
						}
					}
				}
			}
			break;
		case 1://二级句柄表
			TableCode &= 0xFFFFFFFFFFFFFFFC;
			for (Index1 = 0; Index1 < 512; Index1++)
			{
				Level1 = *(PULONG64)((ULONG64)TableCode + (Index1 * _HANDLE_TABLE_TableCode));
				if (MmIsAddressValid(Level1))
				{
					for (Index2 = 0; Index2 < 256; Index2++)
					{
						TableData = *(PULONG64)((ULONG64)Level1 + (Index2 * 0x10));
						TableData = ((ULONG64)TableData >> 0x10) | 0xFFFF000000000000;
						if (MmIsAddressValid(TableData))
						{
							//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
							//ObGetObjectType
							TypeIndex = *(PUCHAR)((ULONG64)TableData - _OBJECT_HEADER_TypeIndex);
							TableIndex = (unsigned __int8)((unsigned __int16)(TableData - _OBJECT_HEADER_PointerCount) >> 8);
							TypeIndex = TypeIndex ^ TableIndex;
							TypeIndex = TypeIndex ^ *ObHeaderCookie;
							//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

							if (TypeIndex != 0)
							{
								TypeTempName = (ULONG64)(*(PULONG64)(((PULONG64)ObTypeIndexTable)[TypeIndex])) + 0x10;
								if (RtlCompareUnicodeString(&g_RootProcessName, TypeTempName, TRUE) == 0)
								{
									CProcessInfo ProcessInfo = { 0 };

									WriteBufferToProcessStructEx(&ProcessInfo, TableData);//将进程信息写入结构体

									UNICODE_STRING pSrcName = { 0 };
									UNICODE_STRING pDstName = { 0 };
									WCHAR pszBuf[MAX_PATH * 2] = { 0 };
									swprintf(pszBuf, L"*%ws", ProcessName);

									RtlInitUnicodeString(&pSrcName, ProcessInfo.FullFileName);
									RtlInitUnicodeString(&pDstName, pszBuf);

									//正则表达式查找路径名中的exe名称
									if (FsRtlIsNameInExpression(&pDstName, &pSrcName, FALSE, NULL) == TRUE)
									{
										return TableData; //返回EPROCESS
									}
								}
							}
						}
					}
				}
			}
			break;
		case 2://三级句柄表
			TableCode &= 0xFFFFFFFFFFFFFFFC;
			for (Index = 0; Index < 512; Index++)
			{
				Level1 = *(PULONG64)((ULONG64)TableCode + (Index * _HANDLE_TABLE_TableCode));
				if (MmIsAddressValid(Level1))
				{
					for (Index1 = 0; Index1 < 512; Index1++)
					{
						Level2 = *(PULONG64)((PULONG64)Level1 + (Index1 * _HANDLE_TABLE_TableCode));
						if (MmIsAddressValid(Level2))
						{
							for (Index2 = 0; Index2 < 256; Index2++)
							{
								TableData = *(PULONG64)((ULONG64)Level2 + (Index2 * 0x10));
								TableData = ((ULONG64)TableData >> 0x10) | 0xFFFF000000000000;
								if (MmIsAddressValid(TableData))
								{
									TypeIndex = *(PUCHAR)((ULONG64)TableData - _OBJECT_HEADER_TypeIndex);
									TableIndex = (unsigned __int8)((unsigned __int16)(TableData - _OBJECT_HEADER_PointerCount) >> 8);
									TypeIndex = TypeIndex ^ TableIndex;
									TypeIndex = TypeIndex ^ *ObHeaderCookie;
									if (TypeIndex != 0)
									{
										TypeTempName = (ULONG64)(*(PULONG64)(((PULONG64)ObTypeIndexTable)[TypeIndex])) + 0x10;
										if (RtlCompareUnicodeString(&g_RootProcessName, TypeTempName, TRUE) == 0)
										{
											CProcessInfo ProcessInfo = { 0 };

											WriteBufferToProcessStructEx(&ProcessInfo, TableData);//将进程信息写入结构体

											UNICODE_STRING pSrcName = { 0 };
											UNICODE_STRING pDstName = { 0 };
											WCHAR pszBuf[MAX_PATH * 2] = { 0 };
											swprintf(pszBuf, L"*%ws", ProcessName);

											RtlInitUnicodeString(&pSrcName, ProcessInfo.FullFileName);
											RtlInitUnicodeString(&pDstName, pszBuf);

											//正则表达式查找路径名中的exe名称
											if (FsRtlIsNameInExpression(&pDstName, &pSrcName, FALSE, NULL) == TRUE)
											{
												return TableData; //返回EPROCESS
											}
										}
									}
								}
							}
						}
					}
				}
			}
			break;
		}
	}
	////////////////////////////////////////////////////////////////////////
	return IndexProcess;
}

ULONG64 IsSysMoudle(ULONG64 pAddr)
{
	if (!MmIsAddressValid(pAddr))
	{
		return FALSE;
	}

	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	POBJECT_DIRECTORY RootDriverObject = 0;
	POBJECT_DIRECTORY RootFileSystemObject = 0;
	POBJECT_DIRECTORY_ENTRY CurrentDriverObjectEntry = 0;
	POBJECT_DIRECTORY_ENTRY CurrentFileSystemObjectEntry = 0;
	POBJECT_HEADER_NAME_INFO ObjectNameInfo = 0;
	PDRIVER_OBJECT CurrectDriverObject = 0;
	PDRIVER_EXTENSION CurrentDriverExtension = 0;
	ULONG index = 0;
	ULONG SysIndex = 0;
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//变量初始化区域
	RootDriverObject = (POBJECT_DIRECTORY)obQueryRootDirectoryTable(&g_RootDriverName);			//获取Driver目录对象
	RootFileSystemObject = (POBJECT_DIRECTORY)obQueryRootDirectoryTable(&g_RootFileSystemName);	//获取FileSystem目录对象
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//项目需求区域

	//遍历对象管理器中 Driver目录
	if (MmIsAddressValid(RootDriverObject) && MmIsAddressValid(RootFileSystemObject))
	{
		//最大数量 0-36
		for (index = 0; index < NUMBER_HASH_BUCKETS; index++)
		{

			CurrentDriverObjectEntry = RootDriverObject->HashBuckets[index];
			CurrentFileSystemObjectEntry = RootFileSystemObject->HashBuckets[index];
			do
			{
				if (MmIsAddressValid(CurrentDriverObjectEntry))
				{
					CurrectDriverObject = (PDRIVER_OBJECT)CurrentDriverObjectEntry->Object;
					if (MmIsAddressValid(CurrectDriverObject))
					{
						ULONG64 StartAddr = CurrectDriverObject->DriverStart;
						ULONG64 EndStart = (ULONG64)CurrectDriverObject->DriverStart + CurrectDriverObject->DriverSize;
						if (StartAddr <= pAddr && pAddr <= EndStart)
						{
							return CurrectDriverObject;
						}
						SysIndex++;
					}
					CurrentDriverObjectEntry = CurrentDriverObjectEntry->ChainLink;
				}
			} while (CurrentDriverObjectEntry != NULL);
		}

		//遍历对象管理器中 FileSystem目录
		do
		{
			if (MmIsAddressValid(CurrentFileSystemObjectEntry))
			{
				CurrectDriverObject = (PDRIVER_OBJECT)CurrentFileSystemObjectEntry->Object;
				if (MmIsAddressValid(CurrectDriverObject))
				{
					//判断参数二的地址是否在此模块内
					ULONG64 StartAddr = CurrectDriverObject->DriverStart;
					ULONG64 EndStart = (ULONG64)CurrectDriverObject->DriverStart + CurrectDriverObject->DriverSize;
					if (StartAddr < pAddr && pAddr <= EndStart)
					{
						return CurrectDriverObject;
					}
					SysIndex++;
				}
				CurrentFileSystemObjectEntry = CurrentFileSystemObjectEntry->ChainLink;
			}
		} while (CurrentFileSystemObjectEntry != NULL);
	}
	////////////////////////////////////////////////////////////////////////
	return NULL;
}

ULONG64 IsSysMoudle1(PDRIVER_OBJECT pDriver, ULONG64 pAddr)//遍历.Sys对象
{
	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	PLDR_DATA_TABLE_ENTRY pLdr = 0;
	PLIST_ENTRY CurrentListEntry = 0;
	PLIST_ENTRY NextListEntry = 0;
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//变量初始化区域
	pLdr = (PLDR_DATA_TABLE_ENTRY)pDriver->DriverSection;
	CurrentListEntry = (PLIST_ENTRY)pLdr->InLoadOrderLinks.Flink;
	NextListEntry = CurrentListEntry;
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//项目需求区域
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}
	do
	{
		pLdr = CONTAINING_RECORD(NextListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
		if (pLdr->DllBase != 0 && MmIsAddressValid(pLdr) == TRUE)
		{
			ULONG64 StartAddr = pLdr->DllBase;
			ULONG64 EndAddr = pLdr->DllBase + pLdr->SizeOfImage;
			if (StartAddr <= pAddr && pAddr <= EndAddr)
			{
				return pLdr;
			}
		}
		NextListEntry = NextListEntry->Flink;
	} while (NextListEntry != CurrentListEntry);
	////////////////////////////////////////////////////////////////////////
	return NULL;
}

ULONG64 IsSysModuleEx(ULONG64 pAddr, PCDriverInfo OutData)
{
	//验证参数
	if (!MmIsAddressValid(OutData))
	{
		return FALSE;
	}

	ULONG64 dqRet = FALSE;
	PCDriverInfo pDriverInfo = NULL;
	EnumDriverInfo(NULL, NULL, &pDriverInfo, NULL, NULL);

	PCLIST_ENTRY pCurList = &pDriverInfo->List.List;

	do
	{
		if (pCurList == NULL)
		{
			break;
		}

		PCDriverInfo pCurDriverObjectInfo = (PCDriverInfo)pCurList;

		ULONG64 Start = pCurDriverObjectInfo->ImageBaseAddr ? pCurDriverObjectInfo->ImageBaseAddr : pCurDriverObjectInfo->DriverStart;
		ULONG64 StartAddr = Start;
		ULONG64 EndAddr = Start + pCurDriverObjectInfo->Size;

		//判断地址是否在这个模块的起始地址和结束地址范围内
		if (StartAddr <= pAddr && pAddr <= EndAddr)
		{
			memcpy_s(OutData, sizeof(CDriverInfo), pCurDriverObjectInfo, sizeof(CDriverInfo));
			dqRet = TRUE;
		}

		//指向下一个
		pCurList = pCurList->Flink;

		//释放资源
		SIZE_T AllocFreeSize = 0;
		ZwFreeVirtualMemory(NtCurrentProcess(), &pCurDriverObjectInfo, &AllocFreeSize, MEM_RELEASE);
	} while (pCurList != &pDriverInfo->List.List);

	return dqRet;
}


VOID ToUpperUnicodeString(PUNICODE_STRING UnicodeString)
{
	USHORT i;
	for (i = 0; i < UnicodeString->Length / sizeof(WCHAR); i++)
	{
		WCHAR ch = UnicodeString->Buffer[i];
		UnicodeString->Buffer[i] = RtlUpcaseUnicodeChar(ch);
	}
}


ULONG64 LookUpDriverObjectByName(PUNICODE_STRING pDriverName, PCDriverInfo pDriverInfoParagma)
{
	if (!MmIsAddressValid(pDriverName))
	{
		return FALSE;
	}


	ToUpperUnicodeString(pDriverName);

	ULONG64 dqRet = FALSE;
	PCDriverInfo pDriverInfo = NULL;
	EnumDriverInfo(NULL, NULL, &pDriverInfo, NULL, NULL);

	PCLIST_ENTRY pCurList = &pDriverInfo->List.List;

	do
	{
		if (pCurList == NULL)
		{
			break;
		}

		PCDriverInfo pCurDriverObjectInfo = (PCDriverInfo)pCurList;

		if (dqRet == FALSE)
		{
			UNICODE_STRING pDstStr = { 0 };
			RtlInitUnicodeString(&pDstStr, pCurDriverObjectInfo->ImageBaseName);

			ToUpperUnicodeString(&pDstStr);

			if (FsRtlIsNameInExpression(pDriverName, &pDstStr, TRUE, NULL))
			{
				//相等返回对象
				dqRet = pCurDriverObjectInfo->DriverObject;

				if (MmIsAddressValid(pDriverInfoParagma))
				{
					RtlMoveMemory(pDriverInfoParagma, pCurDriverObjectInfo, sizeof(CDriverInfo));
				}

				break;
			}
		}

		//指向下一个
		pCurList = pCurList->Blink;

		//释放资源
		SIZE_T AllocFreeSize = 0;
		ZwFreeVirtualMemory(NtCurrentProcess(), &pCurDriverObjectInfo, &AllocFreeSize, MEM_RELEASE);
	} while (pCurList != &pDriverInfo->List.List);

	return dqRet;
}

// 在一个已加载内核模块的导出表里按名字找符号地址。
static PVOID FindKernelExport(ULONG64 ModuleBase, const char* Name)
{
	if (!MmIsAddressValid((PVOID)ModuleBase) || Name == NULL) return NULL;

	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)ModuleBase;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
	PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(ModuleBase + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;

	IMAGE_DATA_DIRECTORY ed = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	if (ed.Size == 0 || ed.VirtualAddress == 0) return NULL;

	PIMAGE_EXPORT_DIRECTORY exp = (PIMAGE_EXPORT_DIRECTORY)(ModuleBase + ed.VirtualAddress);
	PULONG names = (PULONG)(ModuleBase + exp->AddressOfNames);
	PUSHORT ords = (PUSHORT)(ModuleBase + exp->AddressOfNameOrdinals);
	PULONG funcs = (PULONG)(ModuleBase + exp->AddressOfFunctions);

	for (ULONG i = 0; i < exp->NumberOfNames; i++)
	{
		const char* fname = (const char*)(ModuleBase + names[i]);
		if (strcmp(fname, Name) == 0)
		{
			USHORT ord = ords[i];
			return (PVOID)(ModuleBase + funcs[ord]);
		}
	}
	return NULL;
}

// 枚举 classpnp.sys 客户驱动注册的 CLASS_INIT_DATA 回调表。
// 算法：
//  1) classpnp.sys 模块基址；导出表里取 ClassInitialize 地址 (= 客户扩展 ID)
//  2) 遍历所有已加载驱动；对每个 DRIVER_OBJECT 调 IoGetDriverObjectExtension(drvObj, ClassInitialize)
//  3) 非 NULL 表示该驱动是 classpnp 客户。CLASS_DRIVER_EXTENSION 公开 ABI:
//       +0x00 UNICODE_STRING  RegistryPath   (16 字节)
//       +0x10 CLASS_INIT_DATA InitData       (16 字节头 + 12 个 8 字节函数指针)
// CallBackType 存 slot 索引，R3 端按数组查名字。
ULONG64 EnumClassInitDataCallback(PCKernelCallBackInfo* OutData)
{
	if (!MmIsAddressValid(OutData)) return 0;

	ULONG_PTR classpnpSize = 0;
	ULONG64 classpnpBase = QuerySysModule((PUCHAR)"CLASSPNP.SYS", &classpnpSize);
	if (classpnpBase == 0)
		classpnpBase = QuerySysModule((PUCHAR)"classpnp.sys", &classpnpSize);
	if (classpnpBase == 0) return 0;

	PVOID classInitId = FindKernelExport(classpnpBase, "ClassInitialize");
	if (classInitId == NULL) return 0;

	// CLASS_INIT_DATA 内 12 个函数指针的偏移（从 CLASS_DRIVER_EXTENSION 起点算）
	// = 0x10（跳 RegistryPath UNICODE_STRING）+ 0x10（跳 InitData 4个 ULONG 头）+ N*8
	static const ULONG slotOffs[] = {
		0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78
	};
	const ULONG slotCount = sizeof(slotOffs) / sizeof(slotOffs[0]);

	ULONG64 IsInit = MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;
	ULONG64 Count = 0;

	PCDriverInfo pDriverInfo = NULL;
	EnumDriverInfo(NULL, NULL, &pDriverInfo, NULL, NULL);
	if (pDriverInfo == NULL) return 0;

	PCLIST_ENTRY pCur = &pDriverInfo->List.List;
	do
	{
		if (!MmIsAddressValid(pCur)) break;
		PCDriverInfo pDi = (PCDriverInfo)pCur;

		ULONG64 drvObj = pDi->DriverObject;
		if (MmIsAddressValid((PVOID)drvObj))
		{
			PVOID ext = IoGetDriverObjectExtension((PDRIVER_OBJECT)drvObj, classInitId);
			if (MmIsAddressValid(ext))
			{
				for (ULONG s = 0; s < slotCount; s++)
				{
					ULONG64 slotAddr = (ULONG64)ext + slotOffs[s];
					if (!MmIsAddressValid((PVOID)slotAddr)) break;

					ULONG64 fn = *(volatile ULONG64*)slotAddr;
					if (fn == 0 || !MmIsAddressValid((PVOID)fn)) continue;

					PCKernelCallBackInfo n = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
					if (!n) break;

					n->CallBackType = (ULONG64)s;
					n->CallBackAddr = fn;
					n->Descr        = drvObj;
					n->ModuleOffset = 0;

					CDriverInfo di = { 0 };
					if (IsSysModuleEx(fn, &di))
					{
						n->ModuleOffset = (di.ImageBaseAddr != 0) ? (fn - di.ImageBaseAddr) : 0;
						memcpy_s(n->ModulePath, sizeof(n->ModulePath),
							di.ImageFullBaseName, sizeof(n->ModulePath));
					}

					if (IsInit && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
					{
						InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &n->List.List);
					}
					else
					{
						n->List.IsInitialize = TRUE;
						InitializeListHead(&n->List.List);
						*OutData = n;
						IsInit = TRUE;
					}
					Count++;
				}
			}
		}

		pCur = pCur->Flink;
		SIZE_T freeSize = 0;
		ZwFreeVirtualMemory(NtCurrentProcess(), &pDi, &freeSize, MEM_RELEASE);
	} while (pCur != &pDriverInfo->List.List);

	return Count;
}

// 枚举传统文件系统过滤驱动 (Sfilter) 回调。
// 这些回调通过 nt!IoRegisterFsRegistrationChange / IoRegisterFsRegistrationChangeMountAware
// 注册到 nt!IopFsNotifyChangeQueueHead / IopFsNotifyChangeQueueHeadMountAware 链表里。
// 每个表项是 (LIST_ENTRY + DriverObject + NotificationRoutine)。
// CallBackType:  0 = 常规, 1 = MountAware；R3 端据此显示。
ULONG64 EnumSfilterCallback(PCKernelCallBackInfo* OutData)
{
	if (!MmIsAddressValid(OutData)) return 0;

	typedef struct _NOTIFICATION_PACKET {
		LIST_ENTRY     ListEntry;
		PDRIVER_OBJECT DriverObject;
		PVOID          NotificationRoutine;
	} NOTIFICATION_PACKET, *PNOTIFICATION_PACKET;

	ULONG64 Heads[2] = {
		(ULONG64)IopFsNotifyChangeQueueHead,
		(ULONG64)IopFsNotifyChangeQueueHeadMountAware
	};

	ULONG64 IsInit = MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;
	ULONG64 Count = 0;

	for (int hi = 0; hi < 2; hi++)
	{
		PLIST_ENTRY head = (PLIST_ENTRY)Heads[hi];
		if (!MmIsAddressValid(head)) continue;
		if (!MmIsAddressValid(head->Flink)) continue;

		for (PLIST_ENTRY cur = head->Flink;
			MmIsAddressValid(cur) && cur != head;
			cur = cur->Flink)
		{
			PNOTIFICATION_PACKET np = (PNOTIFICATION_PACKET)cur;
			if (!MmIsAddressValid(np)) break;

			ULONG64 fn = (ULONG64)np->NotificationRoutine;
			if (fn == 0 || !MmIsAddressValid((PVOID)fn)) continue;

			PCKernelCallBackInfo p = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
			if (!p) break;

			p->CallBackType = (ULONG64)hi;
			p->CallBackAddr = fn;
			p->Descr        = (ULONG64)np->DriverObject;
			p->ModuleOffset = 0;

			CDriverInfo di = { 0 };
			if (IsSysModuleEx(fn, &di))
			{
				p->ModuleOffset = (di.ImageBaseAddr != 0) ? (fn - di.ImageBaseAddr) : 0;
				memcpy_s(p->ModulePath, sizeof(p->ModulePath),
					di.ImageFullBaseName, sizeof(p->ModulePath));
			}

			if (IsInit && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
			{
				InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &p->List.List);
			}
			else
			{
				p->List.IsInitialize = TRUE;
				InitializeListHead(&p->List.List);
				*OutData = p;
				IsInit = TRUE;
			}
			Count++;
		}
	}
	return Count;
}

ULONG64 EnumShutdownCallBack(PCKernelCallBackInfo* OutData)
{
	if (!MmIsAddressValid(IopNotifyShutdownQueueHead))
	{
		return FALSE;
	}

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;

	ULONG64 dqRet = 0;
	//标志
#define _SHOTDOWN_FLAGS 0x800u

	//ShutDownCallBack 回调结构体
	typedef struct _ShutDownCallBack
	{
		CLIST_ENTRY List;
		PDEVICE_OBJECT pDeviceObject;
	}CShutDownCallBack, * PCShutDownCallBack;


	PLIST_ENTRY pCurList = ((PLIST_ENTRY)IopNotifyShutdownQueueHead)->Blink;


	while (pCurList != IopNotifyShutdownQueueHead)
	{
		PCShutDownCallBack pCallBack = (PCShutDownCallBack)pCurList;
		if (pCallBack == NULL)
		{
			break;
		}

		PDEVICE_OBJECT pDeviceObject = pCallBack->pDeviceObject;
		if (MmIsAddressValid(pDeviceObject) && (pDeviceObject->Flags & _SHOTDOWN_FLAGS)/*判断标志*/)
		{
			//获取驱动对象
			PDRIVER_OBJECT pDriverObject = pDeviceObject->DriverObject;
			if (MmIsAddressValid(pDriverObject))
			{
				//申请空间
				PCKernelCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
				if (pNewInfo == NULL)
				{
					break;
				}


				//拷贝数据
				pNewInfo->CallBackAddr = pDriverObject->MajorFunction[IRP_MJ_SHUTDOWN];
				pNewInfo->Descr = pDeviceObject;
				pNewInfo->CallBackType = um_KernalCallBackType_ShutDown;

				CDriverInfo DriverInfo = { 0 };
				if (IsSysModuleEx(pNewInfo->CallBackAddr, &DriverInfo))
				{
					//偏移
					pNewInfo->ModuleOffset = pNewInfo->CallBackAddr - DriverInfo.ImageBaseAddr;
					//路径
					memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
				}

				//插入链表
				if (IsInit != 0 && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
				{
					//插入链表里面
					InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
				}
				else
				{
					pNewInfo->List.IsInitialize = TRUE;						//已初始化
					InitializeListHead(&pNewInfo->List.List);				//初始化链表头
					*OutData = pNewInfo;									//赋值
					IsInit = TRUE;
				}
				dqRet++;
			}
		}
		//指向下一个
		pCurList = pCurList->Blink;
	}
	return dqRet;
}

ULONG64 EnumBugCheckCallback(PCKernelCallBackInfo* OutData)
{
	if (!MmIsAddressValid(KeBugCheckCallbackListHead))
	{
		return FALSE;
	}

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;

	ULONG64 dqRet = 0;
	// BugCheck回调类型结构 PKBUGCHECK_CALLBACK_RECORD

	PLIST_ENTRY pCurList = ((PLIST_ENTRY)KeBugCheckCallbackListHead)->Blink;

	while (pCurList != KeBugCheckCallbackListHead)
	{
		PKBUGCHECK_CALLBACK_RECORD pCallBack = pCurList;
		if (pCallBack == NULL)
		{
			break;
		}

		//申请空间
		PCKernelCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
		if (pNewInfo == NULL)
		{
			break;
		}

		//拷贝数据
		pNewInfo->CallBackAddr = pCallBack->CallbackRoutine;
		pNewInfo->Descr = pCallBack;
		pNewInfo->CallBackType = um_KernalCallBackType_DbgCheck;

		CDriverInfo DriverInfo = { 0 };
		if (IsSysModuleEx(pNewInfo->CallBackAddr, &DriverInfo))
		{
			//偏移
			pNewInfo->ModuleOffset = pNewInfo->CallBackAddr - DriverInfo.ImageBaseAddr;
			//路径
			memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
		}

		//插入链表
		if (IsInit != 0 && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
		{
			//插入链表里面
			InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
		}
		else
		{
			pNewInfo->List.IsInitialize = TRUE;						//已初始化
			InitializeListHead(&pNewInfo->List.List);				//初始化链表头
			*OutData = pNewInfo;									//赋值
			IsInit = TRUE;
		}

		dqRet++;

		//指向下一个
		pCurList = pCurList->Blink;
	}
	return dqRet;
}

ULONG64 EnumPnpCallBack(PCKernelCallBackInfo* OutData)
{
	//PnpDeviceClassNotifyList 回调结构
	typedef struct _PnpCallBackInfo
	{
		LIST_ENTRY List;        								//双向链表
		ULONG32 ReserveFlags;   								//1 2 3
		ULONG32 SessionId;      								//MmGetSessionIdEx((__int64)KeGetCurrentThread()->ApcState.Process);
		ULONG64 v16;            								//ZwOpenSession(&v16, 0i64, &v18);    
		PDRIVER_NOTIFICATION_CALLBACK_ROUTINE CallbackRoutine;	//回调函数地址
		PVOID Context;
		PDRIVER_OBJECT DriverObject;							//驱动对象结构体
		USHORT Count;           								//1   计数器
		UCHAR ReserveFlags2;    								//2
		UCHAR Reserve[5];       								//对齐
		PULONG64 Lock;          								//&PnpTargetDeviceNotifyLock
		PERESOURCE Resource;    								//
		ULONG64 EventCategoryData;								//IoRegisterPlugPlayNotification 参数三
		ULONG64 Reserve1;
	}CPnpCallBackInfo, * PCPnpCallBackInfo;

	//注:PnpDeviceClassNotifyList 链表为13个,所以需要遍历13次

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;

	ULONG64 dqRet = 0;

	//遍历PnpDeviceClassNotifyList链表
	do
	{
		//验证链表是否是有效地址
		if (!MmIsAddressValid(PnpDeviceClassNotifyList))
		{
			break;
		}

		PCLIST_ENTRY pListArry = (PCLIST_ENTRY)PnpDeviceClassNotifyList;
		for (int i = 0; i < 0xD; i++)
		{
			PCLIST_ENTRY pCurList = pListArry[i].Blink;

			while (pCurList != &pListArry[i])
			{
				PCPnpCallBackInfo pCallBackInfo = (PCPnpCallBackInfo)pCurList;
				if (!MmIsAddressValid(pCallBackInfo))
				{
					break;
				}

				//申请空间
				PCKernelCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
				if (pNewInfo == NULL)
				{
					break;
				}

				pNewInfo->CallBackAddr = pCallBackInfo->CallbackRoutine;
				pNewInfo->CallBackType = um_KernalCallBackType_Pnp;
				pNewInfo->Descr = pCallBackInfo;


				CDriverInfo DriverInfo = { 0 };
				if (IsSysModuleEx(pNewInfo->CallBackAddr, &DriverInfo))
				{
					//偏移
					pNewInfo->ModuleOffset = pNewInfo->CallBackAddr - DriverInfo.ImageBaseAddr;
					//路径
					memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
				}

				//插入链表
				if (IsInit != 0 && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
				{
					//插入链表里面
					InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
				}
				else
				{
					pNewInfo->List.IsInitialize = TRUE;						//已初始化
					InitializeListHead(&pNewInfo->List.List);				//初始化链表头
					*OutData = pNewInfo;									//赋值
					IsInit = TRUE;
				}

				dqRet++;
				//指向下一个
				pCurList = pCurList->Blink;
			}
		}
	} while (0);

	//遍历PnpDeferredRegistrationList链表
	do
	{
		//验证链表是否是有效地址
		if (!MmIsAddressValid(PnpDeferredRegistrationList))
		{
			break;
		}

		PCLIST_ENTRY pCurList = ((PCLIST_ENTRY)PnpDeferredRegistrationList)->Blink;

		while (pCurList != PnpDeferredRegistrationList)
		{
			PCPnpCallBackInfo pCallBackInfo = (PCPnpCallBackInfo)pCurList;
			if (!MmIsAddressValid(pCallBackInfo))
			{
				break;
			}

			//申请空间
			PCKernelCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			pNewInfo->CallBackAddr = pCallBackInfo->CallbackRoutine;
			pNewInfo->CallBackType = um_KernalCallBackType_Pnp;
			pNewInfo->Descr = pCallBackInfo;


			CDriverInfo DriverInfo = { 0 };
			if (IsSysModuleEx(pNewInfo->CallBackAddr, &DriverInfo))
			{
				//偏移
				pNewInfo->ModuleOffset = pNewInfo->CallBackAddr - DriverInfo.ImageBaseAddr;
				//路径
				memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
			}

			//插入链表
			if (IsInit != 0 && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}


			dqRet++;
			//指向下一个
			pCurList = pCurList->Blink;
		}
	} while (0);

	//遍历PnpProfileNotifyList链表
/*
	do
	{
		//验证链表是否是有效地址
		if (!MmIsAddressValid(PnpProfileNotifyList))
		{
			break;
		}

		PCLIST_ENTRY pCurList = ((PCLIST_ENTRY)PnpProfileNotifyList)->Blink;

		while (pCurList != PnpProfileNotifyList)
		{
			PCPnpCallBackInfo pCallBackInfo = (PCPnpCallBackInfo)pCurList;
			if (!MmIsAddressValid(pCallBackInfo))
			{
				break;
			}

			//申请空间
			PCKernelCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			pNewInfo->CallBackAddr = pCallBackInfo->CallbackRoutine;
			pNewInfo->CallBackType = um_KernalCallBackType_Pnp;
			pNewInfo->Descr = pCallBackInfo;


			CDriverInfo DriverInfo = { 0 };
			if (IsSysModuleEx(pNewInfo->CallBackAddr, &DriverInfo))
			{
				//偏移
				pNewInfo->ModuleOffset = pNewInfo->CallBackAddr - DriverInfo.ImageBaseAddr;
				//路径
				memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
			}

			MyDbgPrintfEx("PnpProfileNotifyList Type:%I64X ModulePath:%ws Offset:%I64X Descr:%I64X CallBackAddr:%I64X\n", pNewInfo->CallBackType, pNewInfo->ModulePath, pNewInfo->ModuleOffset, pNewInfo->Descr, pNewInfo->CallBackAddr);


			//插入链表
			if (IsInit != 0 && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}

			dqRet++;
			//指向下一个
			pCurList = pCurList->Blink;
		}
	} while (0);*/

	return dqRet;
}

ULONG64 EnumLoadImageCallBack(PCKernelCallBackInfo* OutData)
{
	if (!MmIsAddressValid(PspLoadImageNotifyRoutine))
	{
		return FALSE;
	}

	if (!MmIsAddressValid(PspLoadImageNotifyRoutineCount))
	{
		return FALSE;
	}

	typedef struct _LoadImageCallBackInfo
	{
		ULONG64 Reserve;
		ULONG64 CallBackPtr;
	}CLoadImageCallBackInfo, * PCLoadImageCallBackInfo;

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;


	ULONG64 dqRet = *PspLoadImageNotifyRoutineCount;

	//遍历PspLoadImageNotifyRoutine 类型是8字节数组 PspLoadImageNotifyRoutineCount存储的是 LoadImage回调数量

	for (int i = 0; i < *PspLoadImageNotifyRoutineCount; i++)
	{
		PCLoadImageCallBackInfo pCallBack = (PCLoadImageCallBackInfo)(PspLoadImageNotifyRoutine[i] & ~0xF);
		if (MmIsAddressValid(pCallBack))
		{

			//申请空间
			PCKernelCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			pNewInfo->CallBackAddr = pCallBack->CallBackPtr; //清除最后4位
			pNewInfo->CallBackType = um_KernalCallBackType_LoadImage;
			pNewInfo->Descr = pCallBack;


			CDriverInfo DriverInfo = { 0 };
			if (IsSysModuleEx(pNewInfo->CallBackAddr, &DriverInfo))
			{
				//偏移
				pNewInfo->ModuleOffset = pNewInfo->CallBackAddr - DriverInfo.ImageBaseAddr;
				//路径
				memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
			}

			//插入链表
			if (IsInit != 0 && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}
		}
	}
	return dqRet;
}

ULONG64 EnumCreateProcessCallBack(PCKernelCallBackInfo* OutData)
{
	if (!MmIsAddressValid(PspCreateProcessNotifyRoutine) ||
		!MmIsAddressValid(PspCreateProcessNotifyRoutineExCount) ||
		!MmIsAddressValid(PspCreateProcessNotifyRoutineCount))
	{
		return FALSE;
	}

	//回调结构
	typedef struct _LoadImageCallBackInfo
	{
		ULONG64 Reserve;
		ULONG64 CallBackPtr;
	}CLoadImageCallBackInfo, * PCLoadImageCallBackInfo;

	ULONG64 dqRet = *PspCreateProcessNotifyRoutineExCount + *PspCreateProcessNotifyRoutineCount;

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;

	for (int i = 0; i < dqRet; i++)
	{
		PCLoadImageCallBackInfo pCallBack = (PCLoadImageCallBackInfo)(PspCreateProcessNotifyRoutine[i] & ~0xF);
		if (MmIsAddressValid(pCallBack))
		{
			//申请空间
			PCKernelCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			pNewInfo->CallBackAddr = pCallBack->CallBackPtr; //清除最后4位
			pNewInfo->CallBackType = um_KernalCallBackType_CreateProcess;
			pNewInfo->Descr = pCallBack;


			CDriverInfo DriverInfo = { 0 };
			if (IsSysModuleEx(pNewInfo->CallBackAddr, &DriverInfo))
			{
				//偏移
				pNewInfo->ModuleOffset = pNewInfo->CallBackAddr - DriverInfo.ImageBaseAddr;
				//路径
				memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
			}

			//插入链表
			if (IsInit != 0 && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}
		}
	}
	return dqRet;
}

ULONG64 EnumCreateThreadCallBack(PCKernelCallBackInfo* OutData)
{
	if (!MmIsAddressValid(PspCreateThreadNotifyRoutine) ||
		!MmIsAddressValid(PspCreateThreadNotifyRoutineNonSystemCount) ||
		!MmIsAddressValid(PspCreateThreadNotifyRoutineCount))
	{
		return FALSE;
	}


	//回调结构
	typedef struct _LoadImageCallBackInfo
	{
		ULONG64 Reserve;
		ULONG64 CallBackPtr;
	}CLoadImageCallBackInfo, * PCLoadImageCallBackInfo;

	ULONG64 dqRet = *PspCreateThreadNotifyRoutineNonSystemCount + *PspCreateThreadNotifyRoutineCount;

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;

	for (int i = 0; i < dqRet; i++)
	{
		PCLoadImageCallBackInfo pCallBack = (PCLoadImageCallBackInfo)(PspCreateThreadNotifyRoutine[i] & ~0xF);
		if (MmIsAddressValid(pCallBack))
		{
			//申请空间
			PCKernelCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			pNewInfo->CallBackAddr = pCallBack->CallBackPtr; //清除最后4位
			pNewInfo->CallBackType = um_KernalCallBackType_CreateThread;
			pNewInfo->Descr = pCallBack;


			CDriverInfo DriverInfo = { 0 };
			if (IsSysModuleEx(pNewInfo->CallBackAddr, &DriverInfo))
			{
				//偏移
				pNewInfo->ModuleOffset = pNewInfo->CallBackAddr - DriverInfo.ImageBaseAddr;
				//路径
				memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
			}

			//插入链表
			if (IsInit != 0 && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}
		}
	}
	return dqRet;
}

ULONG64 EnumRegistryCallBack(PCKernelCallBackInfo* OutData)
{
	if (!MmIsAddressValid(CallbackListHead))
	{
		return FALSE;
	}

	typedef struct _RegisterCallBackInfo
	{
		LIST_ENTRY List;				//
		ULONG32 ReverseFlags0;			// 0
		ULONG32 ReverseFlags1;			//(*((_DWORD *)v11 + 5) ^ a5)^v12 & 1
		ULONG64 Reverse0;
		ULONG64 Context;
		ULONG64 CallBackPtr;			//函数指针 
		USHORT Altitude0;				//&CmLegacyAltitude
		USHORT Altitude1;				//&CmLegacyAltitude
		ULONG32 Reverse;				//对齐
		ULONG64 NewAllocBuf2;			//ExAllocatePoolWithTag(PagedPool, *(unsigned __int16 *)a3, 0x61634D43u)
		ULONG64 NewAllocBuf0;			//ExAllocatePoolWithTag(PagedPool, 0x50ui64, 0x62634D43u)+8
		ULONG64 NewAllocBuf1;			//ExAllocatePoolWithTag(PagedPool, 0x50ui64, 0x62634D43u)+8
	}CRegisterCallBackInfo, * PCRegisterCallBackInfo;

	ULONG64 dqRet = 0;

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;


	PLIST_ENTRY pCurList = ((PLIST_ENTRY)CallbackListHead)->Blink;


	while (pCurList != CallbackListHead)
	{
		PCRegisterCallBackInfo pCallBack = (PCRegisterCallBackInfo)pCurList;
		if (!MmIsAddressValid(pCallBack))
		{
			break;
		}

		//申请空间
		PCKernelCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
		if (pNewInfo == NULL)
		{
			break;
		}

		pNewInfo->CallBackAddr = pCallBack->CallBackPtr; //清除最后4位
		pNewInfo->CallBackType = um_KernalCallBackType_CreateRegistry;
		pNewInfo->Descr = pCallBack;

		CDriverInfo DriverInfo = { 0 };
		if (IsSysModuleEx(pNewInfo->CallBackAddr, &DriverInfo))
		{
			//偏移
			pNewInfo->ModuleOffset = pNewInfo->CallBackAddr - DriverInfo.ImageBaseAddr;
			//路径
			memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
		}

		//插入链表
		if (IsInit != 0 && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
		{
			//插入链表里面
			InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
		}
		else
		{
			pNewInfo->List.IsInitialize = TRUE;						//已初始化
			InitializeListHead(&pNewInfo->List.List);				//初始化链表头
			*OutData = pNewInfo;									//赋值
			IsInit = TRUE;
		}

		dqRet++;
		//指向下一个
		pCurList = pCurList->Blink;
	}
	return dqRet;
}

ULONG64 EnumIoTimer(PCKernelCallBackInfo* OutData)
{
	//回调结构体
	typedef struct _IO_TIMER
	{
		INT16        Type;
		INT16        TimerFlag;
		LONG32       Unknown;
		LIST_ENTRY   TimerList;
		PVOID        TimerRoutine;
		PVOID        Context;
		PVOID        DeviceObject;
	}IO_TIMER, * PIO_TIMER;

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;

	if (!MmIsAddressValid(IopTimerQueueHead))
	{
		return FALSE;
	}

	ULONG64 nCount = 0;

	PLIST_ENTRY pListStart = IopTimerQueueHead->Flink;
	do
	{
		PIO_TIMER Timer = CONTAINING_RECORD(pListStart, IO_TIMER, TimerList);
		if (Timer && MmIsAddressValid(Timer))
		{
			//申请空间
			PCKernelCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CKernelCallBackInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			pNewInfo->CallBackAddr = Timer->TimerRoutine;
			pNewInfo->CallBackType = um_KernalCallBackType_IoTime;
			pNewInfo->Descr = Timer;

			CDriverInfo DriverInfo = { 0 };
			if (IsSysModuleEx(pNewInfo->CallBackAddr, &DriverInfo))
			{
				//偏移
				pNewInfo->ModuleOffset = pNewInfo->CallBackAddr - DriverInfo.ImageBaseAddr;
				//路径
				memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
			}

			//插入链表
			if (IsInit != 0 && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCKernelCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}
			//数量加1
			nCount++;
		}

		//指向下一个结构
		pListStart = pListStart->Blink;
	} while (pListStart != IopTimerQueueHead);


	return nCount;
}

ULONG64 EnumObjectTypeCallBack(ULONG64 pObjectType, PCObjectTypeCallBackInfo* OutData)
{
	if (!MmIsAddressValid(pObjectType))
	{
		return FALSE;
	}

	//回调结构
	typedef struct _ObjectCallBackInfoExtend
	{
		LIST_ENTRY List;                            //链表CallbackList 
		OB_OPERATION Operations;                    //标志
		ULONG64 pSelf;								 //指向自己头部 PCObjectCallBackInfo
		POBJECT_TYPE ObjectType;                    //对象类型结构体
		POB_PRE_OPERATION_CALLBACK PreOperation;    //回调函数
		POB_POST_OPERATION_CALLBACK PostOperation;  //回调函数
		unsigned long long int Reserve2;
	}CObjectCallBackInfoExtend, * PCObjectCallBackInfoExtend;

	typedef struct _ObjectCallBackInfo
	{
		unsigned short Version;                     //版本
		unsigned long long int RegistrationContext; //传递的参数
		UNICODE_STRING Altitude;                    //海拔高度
		ULONG64 ObjCallBackInfoExtend;              //扩展结构 _ObjectCallBackInfoExtend
	}CObjectCallBackInfo, * PCObjectCallBackInfo;


	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;


	//用于返回数据
	ULONG64 nRetIndex = 0;

	//_OBJECT_TYPE 首地址
	ULONG64 pObject = *(PULONG64)pObjectType;
	if (!MmIsAddressValid(pObject))
	{
		return nRetIndex;
	}

	//判断SupportsObjectCallbacks位是否支持回调注册   [+0x002 ( 6: 6)] SupportsObjectCallbacks : 0x1 [Type: unsigned char]
	if (!*(PUSHORT)(pObject + _OBJECT_TYPE_OBJECT_TYPE_INITIALIZER_ObjectTypeFlags) & _ObjectTypeFlags_SupportsObjectCallbacks_byte)
	{
		return nRetIndex;
	}

	//获取对象类型名称
	PUNICODE_STRING pObjectTypeName = (PUNICODE_STRING)(pObject + _OBJECT_TYPE_Name);
	if (!MmIsAddressValid(pObjectTypeName))
	{
		return nRetIndex;
	}

	//获取对象回调链表
	PLIST_ENTRY pCallbackList = (PUNICODE_STRING)(pObject + _OBJECT_TYPE_CallbackList);
	if (!MmIsAddressValid(pCallbackList))
	{
		return nRetIndex;
	}

	//开始遍历
	PLIST_ENTRY pStart = pCallbackList->Flink;
	do
	{
		PCObjectCallBackInfoExtend	pObjectInfoEx = (PCObjectCallBackInfoExtend)pStart;
		if (!MmIsAddressValid(pObjectInfoEx))
		{
			return nRetIndex;
		}
		PCObjectCallBackInfo pObjectInfo = (PCObjectCallBackInfo)pObjectInfoEx->pSelf;
		if (!MmIsAddressValid(pObjectInfo))
		{
			return nRetIndex;
		}

		{
			//申请空间
			PCObjectTypeCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CObjectTypeCallBackInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			memcpy_s(pNewInfo->szObjectTypeName, MAX_BASE_FILE_NAME, pObjectTypeName->Buffer, pObjectTypeName->Length);
			//海拔
			memcpy_s(pNewInfo->Altitude, MAX_BASE_FILE_NAME, pObjectInfo->Altitude.Buffer, pObjectInfo->Altitude.Length);
			//句柄
			pNewInfo->pHandle = pObjectInfoEx->pSelf;
			//回调函数PreOperation
			pNewInfo->PreOperation = pObjectInfoEx->PreOperation;
			//回调函数PostOperation
			pNewInfo->PostOperation = pObjectInfoEx->PostOperation;

			//获取所在模块路径
			ULONG64 Addr = pNewInfo->PreOperation;
			if (Addr == NULL)
			{
				Addr = pNewInfo->PostOperation;
			}

			CDriverInfo pDriverInfo = { 0 };
			if (IsSysModuleEx(Addr, &pDriverInfo))
			{
				memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, pDriverInfo.ImageFullBaseName, MY_MAX_PATH);
			}

			//插入链表
			if (IsInit != 0 && ((PCObjectTypeCallBackInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCObjectTypeCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}
		}

		pStart = pStart->Flink;
		nRetIndex++;//
	} while (pStart != pCallbackList->Flink);
	return nRetIndex;
}

ULONG64 EnumObjectTypeCallBackEx(ULONG64 pObjectType, PCObjectTypeCallBackExInfo* OutData)
{
	//验证参数是否有效
	if (!MmIsAddressValid(pObjectType))
	{
		return FALSE;
	}

	ULONG64 TypeInfo = pObjectType + _OBJECT_TYPE_TypeInfo;
	if (!MmIsAddressValid(TypeInfo))
	{
		return FALSE;
	}

	//申请空间
	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;


	ULONG64 DumpProcedure = *(PULONG64)(TypeInfo + _OBJECT_TYPE_INITIALIZER_DumpProcedure);
	ULONG64 OpenProcedure = *(PULONG64)(TypeInfo + _OBJECT_TYPE_INITIALIZER_OpenProcedure);
	ULONG64 CloseProcedure = *(PULONG64)(TypeInfo + _OBJECT_TYPE_INITIALIZER_CloseProcedure);
	ULONG64 DeleteProcedure = *(PULONG64)(TypeInfo + _OBJECT_TYPE_INITIALIZER_DeleteProcedure);
	ULONG64 ParseProcedure = *(PULONG64)(TypeInfo + _OBJECT_TYPE_INITIALIZER_ParseProcedure);
	ULONG64 SecurityProcedure = *(PULONG64)(TypeInfo + _OBJECT_TYPE_INITIALIZER_SecurityProcedure);
	ULONG64 QueryNameProcedure = *(PULONG64)(TypeInfo + _OBJECT_TYPE_INITIALIZER_QueryNameProcedure);
	ULONG64 OkayToCloseProcedure = *(PULONG64)(TypeInfo + _OBJECT_TYPE_INITIALIZER_OkayToCloseProcedure);

	PUNICODE_STRING pTypeName = (pObjectType + _OBJECT_TYPE_Name);

	do
	{
		//申请空间
		PCObjectTypeCallBackExInfo pNewInfo = MyExAllocMemOry(sizeof(CObjectTypeCallBackExInfo), PAGE_READWRITE, UserMode);
		if (pNewInfo == NULL)
		{
			break;
		}

		//赋值
		//ValidAccessMask值
		pNewInfo->ValidAccessMask = *(PULONG32)(TypeInfo + _OBJECT_TYPE_INITIALIZER_ValidAccessMask);
		//对象
		pNewInfo->Object = pObjectType;
		//拷贝类型名
		memcpy_s(pNewInfo->TypeName, MAX_BASE_FILE_NAME, pTypeName->Buffer, pTypeName->Length < MAX_BASE_FILE_NAME ? pTypeName->Length : MAX_BASE_FILE_NAME);

		//指向回调起始地址
		PULONG64 CallBackFunAddr = (TypeInfo + _OBJECT_TYPE_INITIALIZER_DumpProcedure);
		//8个回调
		for (int i = 0; i < OBJECT_TYPE_CALLBACK_MAX_NUMBER; i++)
		{
			//赋值函数地址
			pNewInfo->FunAddr[i].FunAddr = CallBackFunAddr[i];
			//回调类型
			pNewInfo->FunAddr[i].FunType = i;
			//回调所在模块
			CDriverInfo pInfo = { 0 };
			if (pNewInfo->FunAddr[i].FunAddr != 0 && IsSysModuleEx(pNewInfo->FunAddr[i].FunAddr, &pInfo))
			{
				memcpy_s(pNewInfo->FunAddr[i].ModulePath, MY_MAX_PATH, pInfo.ImageFullBaseName, MY_MAX_PATH);
			}
		}

		//插入链表
		if (IsInit != 0 && ((PCObjectTypeCallBackExInfo)(*OutData))->List.IsInitialize)
		{
			//插入链表里面
			InsertHeadList(&((PCObjectTypeCallBackExInfo)(*OutData))->List.List, &pNewInfo->List.List);
		}
		else
		{
			pNewInfo->List.IsInitialize = TRUE;						//已初始化
			InitializeListHead(&pNewInfo->List.List);				//初始化链表头
			*OutData = pNewInfo;									//赋值
			IsInit = TRUE;
		}

	} while (0);

	return TRUE;
}

ULONG64 EnumCallBack(PCCallBackInfo* OutData)
{
	//记录数量
	ULONG64 nCount = 0;

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;

	POBJECT_DIRECTORY RootCallBackDir = (POBJECT_DIRECTORY)obQueryRootDirectoryTable(&g_RootCallbackName);			//获取CallBack目录对象

	for (int index = 0; index < NUMBER_HASH_BUCKETS; index++)
	{
		POBJECT_DIRECTORY_ENTRY pRootCallBackDirEntry = RootCallBackDir->HashBuckets[index];
		if (MmIsAddressValid(pRootCallBackDirEntry))
		{
			do
			{
				//获取对象
				PMY_CALLBACK_OBJECT pCallBackObject = pRootCallBackDirEntry->Object;
				if (MmIsAddressValid(pCallBackObject))
				{
					//获取类型名  // /CallBack/xxxxxx
					POBJECT_HEADER_NAME_INFO ObjectNameInfo = (POBJECT_HEADER_NAME_INFO)ObQueryNameInfo(pCallBackObject);


					PLIST_ENTRY pRegCallBackList = &pCallBackObject->RegisteredCallbacks;

					//筛选链表为空的数据
					if (pRegCallBackList == pCallBackObject->RegisteredCallbacks.Flink == pCallBackObject->RegisteredCallbacks.Blink)
					{
						goto While_END;
					}

					do
					{
						if (!MmIsAddressValid(pRegCallBackList))
						{
							goto While_END;
						}

						PMY_CALLBACK_REGISTRATION pCallbackObjectAddr = pRegCallBackList;
						PCALLBACK_FUNCTION pCallbackFunction = pCallbackObjectAddr->CallbackFunction;

						if (MmIsAddressValid(pCallbackFunction) && ((ULONG64)pCallbackFunction & 0xF) == 0 /*筛选无效的地址*/)
						{
							//申请空间
							PCCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CObjectTypeCallBackInfo), PAGE_READWRITE, UserMode);
							if (pNewInfo == NULL)
							{
								break;
							}
							if (ObjectNameInfo)
								//拷贝类型名
								swprintf(pNewInfo->szTypeName, L"/CallBack/%ws", ObjectNameInfo->Name.Buffer);
							//函数地址
							pNewInfo->dqFunctionAddr = pCallbackFunction;
							//对象地址
							pNewInfo->dqObjectAddr = pCallbackObjectAddr;
							//所在模块
							CDriverInfo DiverMsg = { 0 };
							IsSysModuleEx(pCallbackFunction, &DiverMsg);
							swprintf(pNewInfo->szPath, L"%ws", DiverMsg.ImageFullBaseName);

							//插入链表
							if (IsInit != 0 && ((PCCallBackInfo)(*OutData))->List.IsInitialize)
							{
								//插入链表里面
								InsertHeadList(&((PCCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
							}
							else
							{
								pNewInfo->List.IsInitialize = TRUE;						//已初始化
								InitializeListHead(&pNewInfo->List.List);				//初始化链表头
								*OutData = pNewInfo;									//赋值
								IsInit = TRUE;
							}
							//数量加1
							nCount++;
						}
						pRegCallBackList = pRegCallBackList->Blink;//获取下一个
					} while (pRegCallBackList != &pCallBackObject->RegisteredCallbacks);
				While_END:
					//遍历子项
					pRootCallBackDirEntry = pRootCallBackDirEntry->ChainLink;
				}
			} while (pRootCallBackDirEntry != NULL);
		}
	}
	return nCount;
}

ULONG64 GetDriverMsg(ULONG64 DriverObject, PCDriverInfo pOutData)
{
	//验证地址有效性
	if (!MmIsAddressValid(DriverObject) || !MmIsAddressValid(pOutData))
	{
		return FALSE;
	}

	PCDriverInfo pDriverOutData = (PCDriverInfo)pOutData;

	ULONG64 DriverSection = *(PULONG64)(DriverObject + _DRIVER_OBJECT_DriverSection);
	ULONG64 DriverStart = *(PULONG64)(DriverObject + _DRIVER_OBJECT_DriverStart);
	ULONG64 DriverSize = *(PULONG64)(DriverObject + _DRIVER_OBJECT_DriverSize);
	ULONG64 DriverExtension = *(PULONG64)(DriverObject + _DRIVER_OBJECT_DriverExtension);

	//赋值驱动对象地址
	pDriverOutData->DriverObject = DriverObject;

	//赋值驱动加载基址
	pDriverOutData->ImageBaseAddr = DriverStart;

	//赋值驱动大小
	pDriverOutData->ImageBaseAddr = DriverSize;

	if (MmIsAddressValid(DriverSection))
	{
		//拷贝路径
		PUNICODE_STRING FullDllName = DriverSection + _LDR_DATA_TABLE_ENTRY_FullDllName;
		wcscpy_s(pDriverOutData->ImageFullBaseName, MAX_PATH * sizeof(WCHAR), FullDllName->Buffer);
		pDriverOutData->ImageFullBaseNameLength = FullDllName->MaximumLength;

		//拷贝驱动名称
		PUNICODE_STRING BaseDllName = DriverSection + _LDR_DATA_TABLE_ENTRY_BaseDllName;
		wcscpy_s(pDriverOutData->DriverName, MAX_PATH * sizeof(WCHAR), BaseDllName->Buffer);
	}

	if (MmIsAddressValid(DriverExtension))
	{
		//拷贝服务名称
		PUNICODE_STRING ServiceKeyName = DriverExtension + _DRIVER_EXTENSION_ServiceKeyName;
		wcscpy_s(pDriverOutData->ServerName, MAX_PATH * sizeof(WCHAR), ServiceKeyName->Buffer);
	}

	return TRUE;
}

ULONG64 EnumMiniFilter(ULONG64 Filter, PCMiniFilterCallBackInfo* OutData, PULONG64 pNumber)
{
	if (!MmIsAddressValid(Filter))
	{
		MyDbgPrintfEx("[%ws] 无效的Filter\n", __FUNCTION__);
		return -1;
	}

	//定位当前对象
	ULONG64 FilterObject = Filter;
	ULONG64 FilterObjectEnd = ((ULONG64)(((PLIST_ENTRY)(FilterObject + _FLT_OBJECT_PrimaryLink))->Blink) - _FLT_OBJECT_PrimaryLink);
	//PUNICODE_STRING pName = NULL;
	PUNICODE_STRING pAltitude = NULL;
	ULONG64 pCallBack = NULL;

	ULONG64 nIndex = 0;

	ULONG64 DriverObject = 0;

	//回调类型名称和类型
	typedef  struct _CMiniFilterFunCallBackType
	{
		UCHAR cType;							//类型
		WCHAR pTypeName[MAX_BASE_FILE_NAME];	//类型名称
	}CMiniFilterFunCallBackType, * PCMiniFilterFunCallBackType;

	//存储回调类型名称了
	CMiniFilterFunCallBackType szFilterType[] = {
	{IRP_MJ_CREATE,L"IRP_MJ_CREATE"},
	{IRP_MJ_CREATE_NAMED_PIPE,L"IRP_MJ_CREATE_NAMED_PIPE"},
	{IRP_MJ_CLOSE,L"IRP_MJ_CLOSE"},
	{IRP_MJ_READ,L"IRP_MJ_READ"},
	{IRP_MJ_WRITE,L"IRP_MJ_WRITE"},
	{IRP_MJ_QUERY_INFORMATION,L"IRP_MJ_QUERY_INFORMATION"},
	{IRP_MJ_SET_INFORMATION,L"IRP_MJ_SET_INFORMATION"},
	{IRP_MJ_QUERY_EA,L"IRP_MJ_QUERY_EA"},
	{IRP_MJ_SET_EA,L"IRP_MJ_SET_EA"},
	{IRP_MJ_FLUSH_BUFFERS,L"IRP_MJ_FLUSH_BUFFERS"},
	{IRP_MJ_QUERY_VOLUME_INFORMATION,L"IRP_MJ_QUERY_VOLUME_INFORMATION"},
	{IRP_MJ_SET_VOLUME_INFORMATION,L"IRP_MJ_SET_VOLUME_INFORMATION"},
	{IRP_MJ_DIRECTORY_CONTROL,L"IRP_MJ_DIRECTORY_CONTROL"},
	{IRP_MJ_FILE_SYSTEM_CONTROL,L"IRP_MJ_FILE_SYSTEM_CONTROL"},
	{IRP_MJ_DEVICE_CONTROL,L"IRP_MJ_DEVICE_CONTROL"},
	{IRP_MJ_INTERNAL_DEVICE_CONTROL,L"IRP_MJ_INTERNAL_DEVICE_CONTROL"},
	{IRP_MJ_SHUTDOWN,L"IRP_MJ_SHUTDOWN"},
	{IRP_MJ_LOCK_CONTROL,L"IRP_MJ_LOCK_CONTROL"},
	{IRP_MJ_CLEANUP,L"IRP_MJ_CLEANUP"},
	{IRP_MJ_CREATE_MAILSLOT,L"IRP_MJ_CREATE_MAILSLOT"},
	{IRP_MJ_QUERY_SECURITY,L"IRP_MJ_QUERY_SECURITY"},
	{IRP_MJ_SET_SECURITY,L"IRP_MJ_SET_SECURITY"},
	{IRP_MJ_POWER,L"IRP_MJ_POWER"},
	{IRP_MJ_SYSTEM_CONTROL,L"IRP_MJ_SYSTEM_CONTROL"},
	{IRP_MJ_DEVICE_CHANGE,L"IRP_MJ_DEVICE_CHANGE"},
	{IRP_MJ_QUERY_QUOTA,L"IRP_MJ_QUERY_QUOTA"},
	{IRP_MJ_SET_QUOTA,L"IRP_MJ_SET_QUOTA"},
	{IRP_MJ_PNP,L"IRP_MJ_PNP"},
	{IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION,L"IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION"},
	{IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION,L"IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION"},
	{IRP_MJ_ACQUIRE_FOR_MOD_WRITE,L"IRP_MJ_ACQUIRE_FOR_MOD_WRITE"},
	{IRP_MJ_RELEASE_FOR_MOD_WRITE,L"IRP_MJ_RELEASE_FOR_MOD_WRITE"},
	{IRP_MJ_ACQUIRE_FOR_CC_FLUSH,L"IRP_MJ_ACQUIRE_FOR_CC_FLUSH"},
	{IRP_MJ_RELEASE_FOR_CC_FLUSH,L"IRP_MJ_RELEASE_FOR_CC_FLUSH"},
	{IRP_MJ_QUERY_OPEN,L"IRP_MJ_QUERY_OPEN"},
	{IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE,L"IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE"},
	{IRP_MJ_NETWORK_QUERY_OPEN,L"IRP_MJ_NETWORK_QUERY_OPEN"},
	{IRP_MJ_MDL_READ,L"IRP_MJ_MDL_READ"},
	{IRP_MJ_MDL_READ_COMPLETE,L"IRP_MJ_MDL_READ_COMPLETE"},
	{IRP_MJ_PREPARE_MDL_WRITE,L"IRP_MJ_PREPARE_MDL_WRITE"},
	{IRP_MJ_MDL_WRITE_COMPLETE,L"IRP_MJ_MDL_WRITE_COMPLETE"},
	{IRP_MJ_VOLUME_MOUNT,L"IRP_MJ_VOLUME_MOUNT"},
	{IRP_MJ_VOLUME_DISMOUNT,L"IRP_MJ_VOLUME_DISMOUNT" } };

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;


	do
	{
		if (!MmIsAddressValid(FilterObject))
		{
			goto TABLE_RET;
		}

		//定位名字
		//pName = (FilterObject + _FLT_FILTER_Name);
		//获取回调地址
		pCallBack = *(PULONG64)(FilterObject + _FLT_FILTER_Operations);

		//定位海拔
		pAltitude = (FilterObject + _FLT_FILTER_DefaultAltitude);

		//定位驱动对象
		DriverObject = *(PULONG64)(FilterObject + _FLT_FILTER_DriverObject);

		while (*(PUCHAR)pCallBack != IRP_MJ_OPERATION_END/*循环结束标志0x80*/)
		{
			//获取注册的结构体
			PFLT_OPERATION_REGISTRATION pOperation = pCallBack;
			if (!MmIsAddressValid(pOperation))
			{
				goto TABLE_1;
			}

			{
				//申请空间
				PCMiniFilterCallBackInfo pNewInfo = MyExAllocMemOry(sizeof(CMiniFilterCallBackInfo), PAGE_READWRITE, UserMode);
				if (pNewInfo == NULL)
				{
					break;
				}


				//拷贝名称
				int Typeindex = 0;
				for (Typeindex = 0; Typeindex < sizeof(szFilterType) / sizeof(szFilterType[0]); Typeindex++)
				{
					if (pOperation->MajorFunction == szFilterType[Typeindex].cType)
					{
						wcscpy_s(pNewInfo->szFilterTypeName, MAX_BASE_FILE_NAME, szFilterType[Typeindex].pTypeName);
						break;
					}
				}

				if (Typeindex >= sizeof(szFilterType) / sizeof(szFilterType[0]))
				{
					swprintf(pNewInfo->szFilterTypeName, L"Unknown(%X)", pOperation->MajorFunction);
				}

				//拷贝PostOperation
				pNewInfo->PostOperation = pOperation->PostOperation;

				//拷贝PreOperation
				pNewInfo->PreOperation = pOperation->PreOperation;

				//拷贝过滤器地址
				pNewInfo->pFilterAddr = FilterObject;

				//拷贝海拔
				PWCHAR Altitude = pNewInfo->Altitude;
				memcpy_s(Altitude, MAX_BASE_FILE_NAME, pAltitude->Buffer, pAltitude->MaximumLength);

				CDriverInfo DriverInfo = { 0 };

				if (GetDriverMsg(DriverObject, &DriverInfo))
				{
					wcscpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName);
				}

				//插入链表
				if (IsInit != 0 && ((PCCallBackInfo)(*OutData))->List.IsInitialize)
				{
					//插入链表里面
					InsertHeadList(&((PCCallBackInfo)(*OutData))->List.List, &pNewInfo->List.List);
				}
				else
				{
					pNewInfo->List.IsInitialize = TRUE;						//已初始化
					InitializeListHead(&pNewInfo->List.List);				//初始化链表头
					*OutData = pNewInfo;									//赋值
					IsInit = TRUE;
				}
				//数量佳佳
				nIndex++;
			}
			//指向下一个回调
			pCallBack = pCallBack + sizeof(FLT_OPERATION_REGISTRATION);
		}

	TABLE_1:

		//向前遍历获取下一个对象
		FilterObject = ((ULONG64)(((PLIST_ENTRY)((ULONG64)FilterObject + _FLT_OBJECT_PrimaryLink))->Flink) - _FLT_OBJECT_PrimaryLink);
	} while (FilterObject != FilterObjectEnd);


TABLE_RET:

	if (MmIsAddressValid(pNumber))
	{
		*(PULONG64)pNumber = nIndex;
	}

	return STATUS_SUCCESS;
}

ULONG64 EnumSysObjectMajorFunction(ULONG64 DriverObject, PCSysMajorFunctionInfo* OutData)
{
	//验证参数
	if (!MmIsAddressValid(DriverObject))
	{
		return FALSE;
	}

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;

	//遍历IRP派遣函数
	PULONG64 pMajorFunctionAddr = (DriverObject + _DRIVER_OBJECT_MajorFunction);

	for (int i = 0; i < MAJORFUNCTION_MAX_NUMBER; i++)
	{
		if (pMajorFunctionAddr[i] == 0)
		{
			continue;
		}

		//申请空间
		PCSysMajorFunctionInfo pNewInfo = MyExAllocMemOry(sizeof(CSysMajorFunctionInfo), PAGE_READWRITE, UserMode);
		if (pNewInfo == NULL)
		{
			break;
		}

		//函数地址
		pNewInfo->FunAddr = pMajorFunctionAddr[i];
		//序号
		pNewInfo->Ord = i;
		//类型
		pNewInfo->Type = i;
		pNewInfo->ModuleBase = 0;

		//拷贝路径
		CDriverInfo DriverInfo = { 0 };
		if (IsSysModuleEx(pNewInfo->FunAddr, &DriverInfo))
		{
			memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
			pNewInfo->ModuleBase = DriverInfo.ImageBaseAddr;
		}

		//插入链表
		if (IsInit != 0 && ((PCSysMajorFunctionInfo)(*OutData))->List.IsInitialize)
		{
			//插入链表里面
			InsertHeadList(&((PCSysMajorFunctionInfo)(*OutData))->List.List, &pNewInfo->List.List);
		}
		else
		{
			pNewInfo->List.IsInitialize = TRUE;						//已初始化
			InitializeListHead(&pNewInfo->List.List);				//初始化链表头
			*OutData = pNewInfo;									//赋值
			IsInit = TRUE;
		}
	}

	//遍历快速IRP派遣函数
	PULONG64 pFastMajorFunctionAddr = *(PULONG64)(DriverObject + _DRIVER_OBJECT_FastIoDispatch);
	if (MmIsAddressValid(pFastMajorFunctionAddr))
	{
		for (int i = 1 /*从一开始*/; i < (sizeof(FAST_IO_DISPATCH) / sizeof(ULONG64)); i++)
		{
			if (pFastMajorFunctionAddr[i] == 0)
			{
				continue;
			}

			//申请空间
			PCSysMajorFunctionInfo pNewInfo = MyExAllocMemOry(sizeof(CSysMajorFunctionInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			//函数地址
			pNewInfo->FunAddr = pFastMajorFunctionAddr[i];
			//序号
			pNewInfo->Ord = i;
			//类型
			pNewInfo->Type = i + IRP_MJ_MAXIMUM_FUNCTION;
			pNewInfo->ModuleBase = 0;

			//拷贝路径
			CDriverInfo DriverInfo = { 0 };
			if (IsSysModuleEx(pNewInfo->FunAddr, &DriverInfo))
			{
				memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
				pNewInfo->ModuleBase = DriverInfo.ImageBaseAddr;
			}

			//插入链表
			if (IsInit != 0 && ((PCSysMajorFunctionInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCSysMajorFunctionInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}
		}
	}


	return TRUE;
}

ULONG64 MyIoThreadToProcess(ULONG64 Thread)
{
	if (MmIsAddressValid(Thread))
	{
		return *(PULONG64)(Thread + _KTHREAD_Process);
	}
	return NULL;
}

ULONG64 EnumDpcTimer(PCDPcInfo* OutData)
{
	//验证内核中所使用的变量地址是否有效
	if (!MmIsAddressValid(KiProcessorBlock) || !MmIsAddressValid(KiWaitNever) || !MmIsAddressValid(KiWaitAlways))
	{
		return FALSE;
	}

	//验证链表是否初始化
	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCKernelCallBackInfo)(*OutData))->List.IsInitialize;

	//存储数量
	ULONG64 Index = 0;

	for (int i = 0; i < KeNumberProcessors; i++)
	{
		//获取CPU环境快
		PUCHAR pKprcb = KiProcessorBlock[i];
		if (MmIsAddressValid(pKprcb))
		{
			//获取存储DPC的表
			PKTIMER_TABLE TimerTable = (PKTIMER_TABLE)(pKprcb + _KPRCB_TimerTable);

			//MyDbgPrintfEx("TimerTable:%I64X\n", TimerTable);

			if (MmIsAddressValid(TimerTable))
			{
				PKTIMER_TABLE_ENTRY TimerEntries = &TimerTable->TimerEntries[0][0];
				//MyDbgPrintfEx("TimerEntries:%I64X\n", TimerEntries);

				// +0x200 TimerEntries     : [2] [256] _KTIMER_TABLE_ENTRY
				//二维数组 
				for (int j = 0; j < (TIMERENTRIES_MAX_NUMBER_UP * TIMERENTRIES_MAX_NUMBER_DOWN); j++)
				{
					//获取链表
					PLIST_ENTRY pListEntryHeader = (PLIST_ENTRY)&TimerEntries[j].Entry;

					if (MmIsAddressValid(pListEntryHeader))
					{
						//循环遍历链表
						for (PLIST_ENTRY pListEntry = pListEntryHeader->Blink; pListEntry != pListEntryHeader; pListEntry = pListEntry->Blink)
						{
							//指向KTIMER结构头
							PKTIMER pTimer = CONTAINING_RECORD(pListEntry, KTIMER, TimerListEntry);
							//MyDbgPrintfEx("pTimer:%I64X\n", pTimer);
							if (MmIsAddressValid(pTimer))
							{
								//获取DPC
								ULONG64 Dpc = pTimer->Dpc;

								//系统加密函数
								//Dpc = (KiWaitNever ^ _rotr64(Timer ^ _byteswap_uint64(Dpc ^ KiWaitAlways),KiWaitNever));

								//解密
								Dpc = Dpc ^ *KiWaitNever;								// 异或
								Dpc = _rotl64(Dpc, *KiWaitNever);						// 循环左移
								Dpc = Dpc ^ (ULONG64)pTimer;							// 异或
								Dpc = _byteswap_uint64(Dpc);							// 颠倒顺序
								Dpc = Dpc ^ *KiWaitAlways;								// 异或

								//验证DPC对象是否正确
								if (MmIsAddressValid(Dpc))
								{
									PKDPC pDpc = (PKDPC)Dpc;

									//申请空间
									PCDPcInfo pNewInfo = MyExAllocMemOry(sizeof(CDPcInfo), PAGE_READWRITE, UserMode);
									if (pNewInfo == NULL)
									{
										break;
									}

									pNewInfo->DpcObject = pDpc;	//Dpc对象
									pNewInfo->TimeObject = pTimer;//Time对象
									pNewInfo->FunCtionStartAddr = pDpc->DeferredRoutine;//函数地址
									pNewInfo->TriggerCycle = pTimer->Period;	//触发周期

									//拷贝路径
									CDriverInfo DriverInfo = { 0 };
									if (IsSysModuleEx(pNewInfo->FunCtionStartAddr, &DriverInfo))
									{
										memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
									}

									//插入链表
									if (IsInit != 0 && ((PCDPcInfo)(*OutData))->List.IsInitialize)
									{
										//插入链表里面
										InsertHeadList(&((PCDPcInfo)(*OutData))->List.List, &pNewInfo->List.List);
									}
									else
									{
										pNewInfo->List.IsInitialize = TRUE;						//已初始化
										InitializeListHead(&pNewInfo->List.List);		//初始化链表头
										*OutData = pNewInfo;									//赋值
										IsInit = TRUE;
									}
									Index++;
								}
							}
						}
					}
				}
			}
		}
	}
	return Index;
}

ULONG64 AlterPspNotifyEnableMask(ULONG32 Value)
{
	if (MmIsAddressValid(g_PspNotifyEnableMaskInfo.pSrcMask) && (*(PULONG32)g_PspNotifyEnableMaskInfo.pSrcMask != Value))
	{
		//访问标志为真
		g_PspNotifyEnableMaskInfo.nVisit = TRUE;
		//访问次数++
		g_PspNotifyEnableMaskInfo.nVisitCount++;
		//保存旧的
		g_PspNotifyEnableMaskInfo.pOldMask = *(PULONG32)g_PspNotifyEnableMaskInfo.pSrcMask;
		//替换成新的
		*(PULONG32)g_PspNotifyEnableMaskInfo.pSrcMask = Value;
		return TRUE;
	}
	return FALSE;
}

ULONG64 EnumWorkThreadStack()
{
	//__debugbreak();
	//验证全局变量
	if (!MmIsAddressValid(PspSystemPartition))
	{
		return FALSE;
	}
	//存储数量
	ULONG64 dqRet = 0;
	//  if ( !(unsigned __int8)ExpQueueWorkItem(*((_QWORD *)PspSystemPartition + 2), WorkItem, v4, 0xFFFFFFFF, 0) )

	//加2的位置
	ULONG64 ExPartition = *(PULONG64)((ULONG64)*PspSystemPartition + _EPARTITION_ExPartition);
	if (!MmIsAddressValid(ExPartition))
	{
		return FALSE;
	}

	//v20 = a1->WorkQueues[v15->NodeNumber];      // _EX_WORK_QUEUE
	//我们是遍历所以只需要获取WorkQueues就行了,然后遍历每一项

	ULONG64 WorkQueues = *(PULONG64)(ExPartition + _EX_PARTITION_WorkQueues);
	//这是个三级指针    +0x008 WorkQueues       : 0xffff9b04`f0c9f330  -> 0xffff9b04`f0c73060  -> 0xffff9b04`f0c90c60 _EX_WORK_QUEUE
	if (!MmIsAddressValid(WorkQueues))
	{
		return FALSE;
	}
	//再取两次
	for (int i = 0; i < 2; i++)
	{
		WorkQueues = *(PULONG64)WorkQueues;
		if (!MmIsAddressValid(WorkQueues))
		{
			break;
		}
	}

	//p_WaitListHead = &v21->WorkPriQueue.Header.WaitListHead;// _EX_WORK_QUEUE_MANAGER + 8
	PLIST_ENTRY WaitListHead = (PLIST_ENTRY) * (PULONG64)((ULONG64)WorkQueues + _EX_WORK_QUEUE_WorkPriQueue + _KPRIQUEUE_Header);
	if (!MmIsAddressValid(WaitListHead))
	{
		return FALSE;
	}

	//开始遍历
	for (PLIST_ENTRY pCurList = WaitListHead->Flink; pCurList != WaitListHead; pCurList = pCurList->Flink, dqRet++/*数量加加*/)
	{
		// 


	}



	return dqRet;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 枚举内核工作线程队列：实质就是 System 进程(PID=4)中的所有线程。
// 这些线程的 StartAddress 落在 ntoskrnl 或第三方驱动里，复用 IsSysModuleEx 解析所属模块，
// 字段沿用 CProcessThreadInfo，避免引入新的传输结构和资源文件改动。
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ULONG64 EnumWorkerThread(PCProcessThreadInfo* OutData)
{
	if (!MmIsAddressValid(OutData))
	{
		return 0;
	}

	// System 进程 EPROCESS（PID=4）
	ULONG64 SystemEprocess = PsLookUpProcessByProcessId((HANDLE)4);
	if (!MmIsAddressValid(SystemEprocess))
	{
		return 0;
	}

	ULONG64 IsInit = MmIsAddressValid(*OutData) && ((PCProcessThreadInfo)(*OutData))->List.IsInitialize;
	ULONG64 Count = 0;
	ULONG64 SystemPid = *(PULONG64)(SystemEprocess + _EPROCESS_UniqueProcessId);
	PLIST_ENTRY ListHead = (PLIST_ENTRY)(SystemEprocess + _EPROCESS_ThreadListHead);

	for (PLIST_ENTRY Cur = ListHead->Flink; MmIsAddressValid(Cur) && Cur != ListHead; Cur = Cur->Flink)
	{
		ULONG64 Thread = (ULONG64)Cur - _ETHREAD_ThreadListEntry;
		if (!MmIsAddressValid((PVOID)Thread))
		{
			break;
		}

		ULONG64 TPid = *(PULONG64)(Thread + _KTHREAD_UniqueProcess);
		if (TPid != SystemPid)
		{
			continue;
		}

		PCProcessThreadInfo pNewInfo = MyExAllocMemOry(sizeof(CProcessThreadInfo), PAGE_READWRITE, UserMode);
		if (pNewInfo == NULL)
		{
			break;
		}

		pNewInfo->Ethread = Thread;
		pNewInfo->UniqueThread = *(PULONG64)(Thread + _KTHREAD_UniqueThread);
		pNewInfo->Priority = *(PCHAR)(Thread + _KTHREAD_Priority);
		pNewInfo->State = *(PUCHAR)(Thread + _KTHREAD_State);
		pNewInfo->ContextSwitches = *(PULONG32)(Thread + _KTHREAD_ContextSwitches);
		pNewInfo->Teb = *(PULONG64)(Thread + _KTHREAD_Teb);
		// System 进程线程的入口在内核态：用 ETHREAD.StartAddress（内核 routine），
		// 不是 Win32StartAddress（用户态线程才用得到）。
		pNewInfo->StartAddress = *(PULONG64)(Thread + _ETHREAD_StartAddress);
		pNewInfo->ThreadTypeFlag = 0;

		PLARGE_INTEGER CreateTime = (PLARGE_INTEGER)(Thread + _ETHREAD_CreateTime);
		if (MmIsAddressValid(CreateTime))
		{
			LARGE_INTEGER LocalTime = { 0 };
			ExSystemTimeToLocalTime(CreateTime, &LocalTime);
			CTIME_FIELDS Tf = { 0 };
			RtlTimeToTimeFields(&LocalTime, &Tf);
			pNewInfo->CreateTime = Tf;
		}

		// 解析 StartAddress 所在内核模块
		CDriverInfo DriverInfo = { 0 };
		if (pNewInfo->StartAddress && IsSysModuleEx(pNewInfo->StartAddress, &DriverInfo))
		{
			memcpy_s(pNewInfo->MoudleName, sizeof(pNewInfo->MoudleName), DriverInfo.ImageFullBaseName, sizeof(pNewInfo->MoudleName));
		}

		//插入链表
		if (IsInit != 0 && ((PCProcessThreadInfo)(*OutData))->List.IsInitialize)
		{
			InsertHeadList(&((PCProcessThreadInfo)(*OutData))->List.List, &pNewInfo->List.List);
		}
		else
		{
			pNewInfo->List.IsInitialize = TRUE;
			InitializeListHead(&pNewInfo->List.List);
			*OutData = pNewInfo;
			IsInit = TRUE;
		}

		Count++;
	}

	return Count;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 枚举 Wdf01000.sys 内部的 WdfFunctions 函数表。
// 思路：
//   1. 通过 PsLoadedModuleList 找 Wdf01000.sys 模块基址 + 大小 + 完整路径。
//   2. 取它的导出 WdfVersionBind。该导出会把 BindInfo->FuncTable = &WdfFunctions 写进调用方
//      结构，编译出来一定包含 `lea r?, [rip+disp32]` 指向 WdfFunctions 全局。
//   3. 用 HDE64 反汇编 WdfVersionBind 前 ~512 字节，枚举每个 lea-rip 目标，计算该目标处
//      连续的合法内核指针条目数，选最长的就是 WdfFunctions 表。
//   4. 遍历表项；指针不在 Wdf01000.sys 模块范围内即标 Hook，并尽量用 IsSysModuleEx 反查
//      实际所属模块路径（帮助定位 hook 来源）。
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 把单条诊断字符串作为一个 CWdfInfo 项插进 *OutData 链表里——这样不开 DbgView 也能立刻
// 在 R3 列表里看到 EnumWdfFunction 失败发生在哪个分支。
static VOID WdfPushDiag(PCWdfInfo* OutData, const char* AsciiMsg)
{
	if (!MmIsAddressValid(OutData)) return;
	PCWdfInfo p = MyExAllocMemOry(sizeof(CWdfInfo), PAGE_READWRITE, UserMode);
	if (!p) return;
	p->pFunOrder = 0;
	p->pFunAddr = 0;
	p->pSrcFunAddr = 0;
	p->HookType = 0;
	// ModulePath 是 WCHAR[]；把 ASCII 直接展宽进去
	WCHAR* dst = p->ModulePath;
	SIZE_T cap = sizeof(p->ModulePath) / sizeof(WCHAR);
	SIZE_T i = 0;
	for (; AsciiMsg[i] && i + 1 < cap; i++) dst[i] = (WCHAR)(UCHAR)AsciiMsg[i];
	dst[i] = 0;
	if (MmIsAddressValid(*OutData) && ((PCWdfInfo)(*OutData))->List.IsInitialize)
	{
		InsertHeadList(&((PCWdfInfo)(*OutData))->List.List, &p->List.List);
	}
	else
	{
		p->List.IsInitialize = TRUE;
		InitializeListHead(&p->List.List);
		*OutData = p;
	}
}

// 在 Wdf01000.sys 的 .data 节里特征码定位 FxLibraryGlobals。
// 失败返回 0；若 OutDataForDiag != NULL 会顺手往链表里塞一条 DIAG。
//
// 流程（版本无关）：
//   1) 在 .data 找一对相邻的 nt!IoConnectInterruptEx / nt!IoDisconnectInterruptEx 指针。
//   2) 若 PDB 提供了 IoConnectInterruptEx 在 FxLibraryGlobals 内的偏移 → 直接 cur - off 拿基址。
//   3) 否则从命中点向前 8 字节步进 (≤0x80) 找一个指向有效 _DRIVER_OBJECT (Type==4)
//      的 qword —— 即 FxLibraryGlobals.+0x000 (DriverObject)。
static ULONG64 LocateFxLibraryGlobals(ULONG64 Wdf01000Base, ULONG_PTR Wdf01000Size, PCWdfInfo* OutDataForDiag)
{
	UNREFERENCED_PARAMETER(Wdf01000Size);
	UNICODE_STRING UsIoConnect    = RTL_CONSTANT_STRING(L"IoConnectInterruptEx");
	UNICODE_STRING UsIoDisconnect = RTL_CONSTANT_STRING(L"IoDisconnectInterruptEx");
	ULONG64 Sig1 = (ULONG64)MmGetSystemRoutineAddress(&UsIoConnect);
	ULONG64 Sig2 = (ULONG64)MmGetSystemRoutineAddress(&UsIoDisconnect);
	if (Sig1 == 0 || Sig2 == 0)
	{
		if (OutDataForDiag) WdfPushDiag(OutDataForDiag, "DIAG: MmGetSystemRoutineAddress failed");
		return 0;
	}

	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)Wdf01000Base;
	if (!MmIsAddressValid(pDos) || pDos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		if (OutDataForDiag) WdfPushDiag(OutDataForDiag, "DIAG: bad DOS header");
		return 0;
	}
	PIMAGE_NT_HEADERS64 pNt = (PIMAGE_NT_HEADERS64)(Wdf01000Base + pDos->e_lfanew);
	if (!MmIsAddressValid(pNt) || pNt->Signature != IMAGE_NT_SIGNATURE)
	{
		if (OutDataForDiag) WdfPushDiag(OutDataForDiag, "DIAG: bad NT header");
		return 0;
	}

	ULONG64 DataStart = 0;
	ULONG   DataSize  = 0;
	PIMAGE_SECTION_HEADER pSec = IMAGE_FIRST_SECTION(pNt);
	for (USHORT i = 0; i < pNt->FileHeader.NumberOfSections; i++)
	{
		if (memcmp(pSec[i].Name, ".data", 6) == 0)
		{
			DataStart = Wdf01000Base + pSec[i].VirtualAddress;
			DataSize  = pSec[i].Misc.VirtualSize;
			break;
		}
	}
	if (DataStart == 0 || DataSize < 0x200)
	{
		if (OutDataForDiag) WdfPushDiag(OutDataForDiag, "DIAG: .data section not found");
		return 0;
	}

	// 步骤 1：在 .data 里找 (IoConnectInterruptEx, IoDisconnectInterruptEx) 相邻对
	ULONG64 HitAddr = 0;
	ULONG64 ScanEnd = DataStart + DataSize;
	for (ULONG64 cur = DataStart; cur + 0x10 <= ScanEnd; cur += sizeof(ULONG64))
	{
		if (*(volatile ULONG64*)cur != Sig1) continue;
		if (*(volatile ULONG64*)(cur + 8) != Sig2) continue;
		HitAddr = cur;
		break;
	}
	if (HitAddr == 0)
	{
		if (OutDataForDiag)
		{
			char buf[160];
			RtlStringCbPrintfA(buf, sizeof(buf),
				"DIAG: IoConnectInterruptEx signature not found in .data (%I64X size=%X)",
				DataStart, DataSize);
			WdfPushDiag(OutDataForDiag, buf);
		}
		return 0;
	}

	// 步骤 2/3：用 PDB 偏移直接回推；没有就反向找 DriverObject (Type==4) 自动发现
	ULONG64 FxLibraryGlobals = 0;
	if (g_Offset_FxLibraryGlobalsType_IoConnectInterruptEx > 0 &&
		HitAddr >= DataStart + (ULONG)g_Offset_FxLibraryGlobalsType_IoConnectInterruptEx)
	{
		FxLibraryGlobals = HitAddr - (ULONG)g_Offset_FxLibraryGlobalsType_IoConnectInterruptEx;
	}
	else
	{
		// 向前最多 0x80 字节，找 qword == 指向 _DRIVER_OBJECT 的指针
		ULONG64 Lo = (HitAddr >= DataStart + 0x80) ? (HitAddr - 0x80) : DataStart;
		for (ULONG64 p = HitAddr - sizeof(ULONG64); p >= Lo; p -= sizeof(ULONG64))
		{
			ULONG64 q = *(volatile ULONG64*)p;
			if ((q >> 48) != 0xFFFF) { if (p == Lo) break; continue; }
			if (!MmIsAddressValid((PVOID)q)) { if (p == Lo) break; continue; }
			CSHORT Type = *(volatile CSHORT*)q;
			if (Type == 4 /* IO_TYPE_DRIVER */)
			{
				FxLibraryGlobals = p;
				break;
			}
			if (p == Lo) break;
		}
	}

	MyDbgPrintfEx("[LocateFxLibraryGlobals] Wdf01000=%I64X .data=%I64X size=%X Hit=%I64X FxLibraryGlobals=%I64X\n",
		Wdf01000Base, DataStart, DataSize, HitAddr, FxLibraryGlobals);

	if (FxLibraryGlobals == 0 && OutDataForDiag)
	{
		char buf[160];
		RtlStringCbPrintfA(buf, sizeof(buf),
			"DIAG: FxLibraryGlobals base not derivable (Hit=%I64X)", HitAddr);
		WdfPushDiag(OutDataForDiag, buf);
	}
	return FxLibraryGlobals;
}

// 在 FxLibraryGlobals 结构体内**特征发现** FxDriverGlobalsList (LIST_ENTRY) 的偏移。
// 自洽 LIST_ENTRY 判据：
//   (a) 空表：Flink == Blink == &head；或
//   (b) 非空：Flink/Blink 都是内核指针，且 Flink->Blink == &head。
// 搜索窗口 [base+0x10, base+0x800)；返回偏移 0 表示失败。
static ULONG DiscoverFxDriverGlobalsListOff(ULONG64 FxLibraryGlobals)
{
	for (ULONG off = 0x10; off + 0x10 <= 0x800; off += sizeof(ULONG64))
	{
		ULONG64 head = FxLibraryGlobals + off;
		ULONG64 flink = *(volatile ULONG64*)head;
		ULONG64 blink = *(volatile ULONG64*)(head + 8);
		// 空表
		if (flink == head && blink == head)
		{
			return off;
		}
		// 非空表
		if ((flink >> 48) != 0xFFFF) continue;
		if ((blink >> 48) != 0xFFFF) continue;
		if (!MmIsAddressValid((PVOID)flink)) continue;
		ULONG64 fl_bk = *(volatile ULONG64*)(flink + 8);
		if (fl_bk == head)
		{
			return off;
		}
	}
	return 0;
}

// 在一个 FX_DRIVER_GLOBALS 节点内**特征发现** WdfBindInfo 字段的偏移。
// _WDF_BIND_INFO 的判据：[+0]=0x30 (Size)，[+8]=Component 指针，目标宽字符串前 4 个 wchar 为 L"Kmdf"。
// 搜索窗口 [+0x10, +0x300)；返回偏移 0 表示失败。
static ULONG DiscoverWdfBindInfoOff(ULONG64 NodeAddr)
{
	for (ULONG off = 0x10; off < 0x300; off += sizeof(ULONG64))
	{
		ULONG64 candidate = *(volatile ULONG64*)(NodeAddr + off);
		if ((candidate >> 48) != 0xFFFF) continue;
		if (!MmIsAddressValid((PVOID)candidate)) continue;
		ULONG size = *(volatile ULONG*)candidate;
		if (size != 0x30) continue;
		ULONG64 comp = *(volatile ULONG64*)(candidate + 8);
		if (!MmIsAddressValid((PVOID)comp)) continue;
		const WCHAR* s = (const WCHAR*)comp;
		if (s[0] != L'K' || s[1] != L'm' || s[2] != L'd' || s[3] != L'f') continue;
		return off;
	}
	return 0;
}

ULONG64 EnumWdfFunction(PCWdfInfo* OutData)
{
	if (!MmIsAddressValid(OutData))
	{
		return 0;
	}

	// 1. 找 Wdf01000.sys 基址 + 大小
	ULONG_PTR Wdf01000Size = 0;
	ULONG64 Wdf01000Base = QuerySysModule((PUCHAR)"Wdf01000.sys", &Wdf01000Size);
	if (Wdf01000Base == 0 || Wdf01000Size == 0)
	{
		MyDbgPrintfEx("[%s] Wdf01000.sys not loaded.\n", __FUNCTION__);
		WdfPushDiag(OutData, "DIAG: Wdf01000.sys NOT LOADED");
		return 1;
	}
	ULONG64 Wdf01000End = Wdf01000Base + Wdf01000Size;

	// 2. 取它的完整路径（用于回填 ModulePath）
	WCHAR Wdf01000Path[MY_MAX_PATH] = { 0 };
	BOOLEAN GotPath = FALSE;
	if (MmIsAddressValid(PsLoadedModuleList))
	{
		PLIST_ENTRY Head = PsLoadedModuleList;
		for (PLIST_ENTRY Cur = Head->Flink; MmIsAddressValid(Cur) && Cur != Head; Cur = Cur->Flink)
		{
			PLDR_DATA_TABLE_ENTRY Ldr = CONTAINING_RECORD(Cur, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
			if (!MmIsAddressValid(Ldr) || !MmIsAddressValid(Ldr->FullDllName.Buffer))
			{
				continue;
			}
			if ((ULONG64)Ldr->DllBase == Wdf01000Base)
			{
				USHORT cb = Ldr->FullDllName.Length;
				if (cb > sizeof(Wdf01000Path) - sizeof(WCHAR)) cb = sizeof(Wdf01000Path) - sizeof(WCHAR);
				memcpy(Wdf01000Path, Ldr->FullDllName.Buffer, cb);
				GotPath = TRUE;
				break;
			}
		}
	}

	// 3. 用 nt!IoConnectInterruptEx / nt!IoDisconnectInterruptEx 作为特征码，
	//    在 Wdf01000.sys 的 .data 节里定位 FxLibraryGlobals。
	//    FxLibraryGlobals 结构 (WinDbg 实测):
	//      +0x000 DriverObject              (PDRIVER_OBJECT of Wdf01000)
	//      +0x008 LibraryDeviceObject
	//      +0x010 IoConnectInterruptEx      <— 特征 1
	//      +0x018 IoDisconnectInterruptEx   <— 特征 2
	//      +0x020 KeQueryActiveProcessors
	//      ...
	//      +0x1E0 FxDriverGlobalsList       (LIST_ENTRY 链表头, 串起所有 KMDF 客户驱动)
	ULONG64 FxLibraryGlobals = LocateFxLibraryGlobals(Wdf01000Base, Wdf01000Size, OutData);
	if (FxLibraryGlobals == 0)
	{
		return 1;
	}

	// 4. 走 FxDriverGlobalsList 链表，找第一个有 WdfBindInfo 的客户驱动
	//    偏移优先级：PDB > 内存特征发现 > 失败
	//    FuncCount/FuncTable 是 WDF_BIND_INFO 公开 ABI（wdf.h），所有版本固定 0x1C/0x20。
	ULONG OffList = (g_Offset_FxLibraryGlobalsType_FxDriverGlobalsList > 0)
		? (ULONG)g_Offset_FxLibraryGlobalsType_FxDriverGlobalsList
		: DiscoverFxDriverGlobalsListOff(FxLibraryGlobals);
	if (OffList == 0)
	{
		WdfPushDiag(OutData, "DIAG: FxDriverGlobalsList offset not discoverable");
		return 1;
	}
	ULONG OffBind = (g_Offset_FX_DRIVER_GLOBALS_WdfBindInfo > 0)
		? (ULONG)g_Offset_FX_DRIVER_GLOBALS_WdfBindInfo : 0; // 0 = 待发现
	const ULONG OffFnCount = (g_Offset_WDF_BIND_INFO_FuncCount > 0)
		? (ULONG)g_Offset_WDF_BIND_INFO_FuncCount : 0x1C;
	const ULONG OffFnTable = (g_Offset_WDF_BIND_INFO_FuncTable > 0)
		? (ULONG)g_Offset_WDF_BIND_INFO_FuncTable : 0x20;

	MyDbgPrintfEx("[EnumWdfFunction] offsets: List=%X Bind=%X FnCount=%X FnTable=%X\n",
		OffList, OffBind, OffFnCount, OffFnTable);

	PLIST_ENTRY ListHead = (PLIST_ENTRY)(FxLibraryGlobals + OffList);
	if (!MmIsAddressValid(ListHead) || !MmIsAddressValid(ListHead->Flink))
	{
		WdfPushDiag(OutData, "DIAG: FxDriverGlobalsList head invalid");
		return 1;
	}

	ULONG64 FuncTableBase = 0;
	ULONG   FuncCount     = 0;
	ULONG   Clients       = 0;
	for (PLIST_ENTRY Cur = ListHead->Flink;
		MmIsAddressValid(Cur) && Cur != ListHead && Clients < 256;
		Cur = Cur->Flink, Clients++)
	{
		// FX_DRIVER_GLOBALS
		ULONG64 FxGlobals = (ULONG64)Cur;
		// 第一个客户驱动时按需特征发现 WdfBindInfo 偏移并缓存
		if (OffBind == 0)
		{
			OffBind = DiscoverWdfBindInfoOff(FxGlobals);
			MyDbgPrintfEx("[EnumWdfFunction] discovered OffBind=%X via node=%I64X\n",
				OffBind, FxGlobals);
			if (OffBind == 0) continue;
		}
		ULONG64 BindInfo  = *(volatile ULONG64*)(FxGlobals + OffBind);
		if (!MmIsAddressValid((PVOID)BindInfo)) continue;

		// _WDF_BIND_INFO
		ULONG   Cnt   = *(volatile ULONG*)(BindInfo + OffFnCount);
		ULONG64 PTbl  = *(volatile ULONG64*)(BindInfo + OffFnTable);
		if (Cnt == 0 || Cnt > 2048) continue;
		if (!MmIsAddressValid((PVOID)PTbl)) continue;
		ULONG64 TblBase = *(volatile ULONG64*)PTbl;
		if (!MmIsAddressValid((PVOID)TblBase)) continue;
		if (TblBase < Wdf01000Base || TblBase >= Wdf01000End) continue;

		FuncTableBase = TblBase;
		FuncCount     = Cnt;
		break;
	}

	MyDbgPrintfEx("[EnumWdfFunction] clients=%u FuncTable=%I64X FuncCount=%u\n",
		Clients, FuncTableBase, FuncCount);

	if (FuncTableBase == 0 || FuncCount == 0)
	{
		char buf[160];
		RtlStringCbPrintfA(buf, sizeof(buf),
			"DIAG: no KMDF client (clients walked=%u)", Clients);
		WdfPushDiag(OutData, buf);
		return 1;
	}

	// 5. 输出函数表的每一项
	ULONG64* WdfFunctions = (ULONG64*)FuncTableBase;
	ULONG64  IsInit = MmIsAddressValid(*OutData) && ((PCWdfInfo)(*OutData))->List.IsInitialize;
	ULONG64  Count = 0;
	for (ULONG i = 0; i < FuncCount; i++)
	{
		if (((ULONG64)&WdfFunctions[i] & 0xFFF) == 0 && !MmIsAddressValid(&WdfFunctions[i]))
		{
			break;
		}
		ULONG64 FunAddr = WdfFunctions[i];

		PCWdfInfo pNewInfo = MyExAllocMemOry(sizeof(CWdfInfo), PAGE_READWRITE, UserMode);
		if (pNewInfo == NULL) break;

		pNewInfo->pFunOrder   = i;
		pNewInfo->pFunAddr    = FunAddr;
		pNewInfo->pSrcFunAddr = 0; // 暂无原始基线
		pNewInfo->HookType    = (FunAddr >= Wdf01000Base && FunAddr < Wdf01000End) ? 0 : 1;
		pNewInfo->ModuleBase  = 0;

		// 反查实际模块（被 hook 时显示 hook 模块）
		CDriverInfo DI = { 0 };
		if (IsSysModuleEx(FunAddr, &DI))
		{
			memcpy_s(pNewInfo->ModulePath, sizeof(pNewInfo->ModulePath),
				DI.ImageFullBaseName, sizeof(pNewInfo->ModulePath));
			pNewInfo->ModuleBase = DI.ImageBaseAddr;
		}
		else if (GotPath)
		{
			memcpy_s(pNewInfo->ModulePath, sizeof(pNewInfo->ModulePath),
				Wdf01000Path, sizeof(pNewInfo->ModulePath));
			pNewInfo->ModuleBase = Wdf01000Base;
		}

		if (IsInit != 0 && ((PCWdfInfo)(*OutData))->List.IsInitialize)
		{
			InsertHeadList(&((PCWdfInfo)(*OutData))->List.List, &pNewInfo->List.List);
		}
		else
		{
			pNewInfo->List.IsInitialize = TRUE;
			InitializeListHead(&pNewInfo->List.List);
			*OutData = pNewInfo;
			IsInit = TRUE;
		}
		Count++;
	}

	return Count;
}

// 枚举 Wdf01000.sys 的 DRIVER_OBJECT.MajorFunction[28]。
// FxLibraryGlobals 首字段 (+0x000) 即 Wdf01000 自己的 PDRIVER_OBJECT。
ULONG64 EnumWdf01000Maj(PCWdfInfo* OutData)
{
	if (!MmIsAddressValid(OutData))
	{
		return 0;
	}

	ULONG_PTR Wdf01000Size = 0;
	ULONG64 Wdf01000Base = QuerySysModule((PUCHAR)"Wdf01000.sys", &Wdf01000Size);
	if (Wdf01000Base == 0 || Wdf01000Size == 0)
	{
		WdfPushDiag(OutData, "DIAG: Wdf01000.sys NOT LOADED");
		return 1;
	}
	ULONG64 Wdf01000End = Wdf01000Base + Wdf01000Size;

	// 路径
	WCHAR Wdf01000Path[MY_MAX_PATH] = { 0 };
	BOOLEAN GotPath = FALSE;
	if (MmIsAddressValid(PsLoadedModuleList))
	{
		PLIST_ENTRY Head = PsLoadedModuleList;
		for (PLIST_ENTRY Cur = Head->Flink; MmIsAddressValid(Cur) && Cur != Head; Cur = Cur->Flink)
		{
			PLDR_DATA_TABLE_ENTRY Ldr = CONTAINING_RECORD(Cur, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
			if (!MmIsAddressValid(Ldr) || !MmIsAddressValid(Ldr->FullDllName.Buffer)) continue;
			if ((ULONG64)Ldr->DllBase == Wdf01000Base)
			{
				USHORT cb = Ldr->FullDllName.Length;
				if (cb > sizeof(Wdf01000Path) - sizeof(WCHAR)) cb = sizeof(Wdf01000Path) - sizeof(WCHAR);
				memcpy(Wdf01000Path, Ldr->FullDllName.Buffer, cb);
				GotPath = TRUE;
				break;
			}
		}
	}

	ULONG64 FxLibraryGlobals = LocateFxLibraryGlobals(Wdf01000Base, Wdf01000Size, OutData);
	if (FxLibraryGlobals == 0) return 1;

	PDRIVER_OBJECT DrvObj = *(PDRIVER_OBJECT*)FxLibraryGlobals;
	if (!MmIsAddressValid(DrvObj))
	{
		WdfPushDiag(OutData, "DIAG: Wdf01000 DRIVER_OBJECT invalid");
		return 1;
	}
	// _DRIVER_OBJECT.Type == IO_TYPE_DRIVER == 4
	if (DrvObj->Type != 4)
	{
		char buf[128];
		RtlStringCbPrintfA(buf, sizeof(buf),
			"DIAG: DRIVER_OBJECT type=%d (expected 4)", (int)DrvObj->Type);
		WdfPushDiag(OutData, buf);
		return 1;
	}

	MyDbgPrintfEx("[EnumWdf01000Maj] DRIVER_OBJECT=%p Size=%d\n", DrvObj, (int)DrvObj->Size);

	ULONG64 IsInit = MmIsAddressValid(*OutData) && ((PCWdfInfo)(*OutData))->List.IsInitialize;
	ULONG64 Count = 0;
	for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		ULONG64 FunAddr = (ULONG64)DrvObj->MajorFunction[i];

		PCWdfInfo pNewInfo = MyExAllocMemOry(sizeof(CWdfInfo), PAGE_READWRITE, UserMode);
		if (pNewInfo == NULL) break;

		pNewInfo->pFunOrder   = i;
		pNewInfo->pFunAddr    = FunAddr;
		pNewInfo->pSrcFunAddr = 0;
		pNewInfo->HookType    = (FunAddr >= Wdf01000Base && FunAddr < Wdf01000End) ? 0 : 1;
		pNewInfo->ModuleBase  = 0;

		CDriverInfo DI = { 0 };
		if (FunAddr != 0 && IsSysModuleEx(FunAddr, &DI))
		{
			memcpy_s(pNewInfo->ModulePath, sizeof(pNewInfo->ModulePath),
				DI.ImageFullBaseName, sizeof(pNewInfo->ModulePath));
			pNewInfo->ModuleBase = DI.ImageBaseAddr;
		}
		else if (GotPath)
		{
			memcpy_s(pNewInfo->ModulePath, sizeof(pNewInfo->ModulePath),
				Wdf01000Path, sizeof(pNewInfo->ModulePath));
			pNewInfo->ModuleBase = Wdf01000Base;
		}

		if (IsInit != 0 && ((PCWdfInfo)(*OutData))->List.IsInitialize)
		{
			InsertHeadList(&((PCWdfInfo)(*OutData))->List.List, &pNewInfo->List.List);
		}
		else
		{
			pNewInfo->List.IsInitialize = TRUE;
			InitializeListHead(&pNewInfo->List.List);
			*OutData = pNewInfo;
			IsInit = TRUE;
		}
		Count++;
	}

	return Count;
}

// ---- 旧的启发式扫描（已废弃，保留供历史参考；不会被编译） ----
#if 0
	// 3. 定位 Wdf01000.sys 内的 "PAGEWdfV" 节 —— KMDF 把函数表和绑定信息(WDF_BIND_INFO)
	//    都放在这个特定节里。新版本 Wdf01000.sys 已经没有导出表，但节名一直没变。
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)Wdf01000Base;
	if (!MmIsAddressValid(pDos) || pDos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		MyDbgPrintfEx("[%s] bad DOS header.\n", __FUNCTION__);
		WdfPushDiag(OutData, "DIAG: bad DOS header");
		return 1;
	}
	PIMAGE_NT_HEADERS64 pNt = (PIMAGE_NT_HEADERS64)(Wdf01000Base + pDos->e_lfanew);
	if (!MmIsAddressValid(pNt) || pNt->Signature != IMAGE_NT_SIGNATURE)
	{
		MyDbgPrintfEx("[%s] bad NT header.\n", __FUNCTION__);
		WdfPushDiag(OutData, "DIAG: bad NT header");
		return 1;
	}

	ULONG64 ScanStart = 0;
	ULONG   ScanBytes = 0;
	PIMAGE_SECTION_HEADER pSec = IMAGE_FIRST_SECTION(pNt);
	MyDbgPrintfEx("[EnumWdfFunction] Wdf01000 base=%I64X size=%I64X sections=%u\n",
		Wdf01000Base, (ULONG64)Wdf01000Size, (ULONG)pNt->FileHeader.NumberOfSections);
	for (USHORT i = 0; i < pNt->FileHeader.NumberOfSections; i++)
	{
		char nm[9] = { 0 };
		memcpy(nm, pSec[i].Name, 8);
		MyDbgPrintfEx("[EnumWdfFunction] sect[%u] name=%-8s rva=%08X vsize=%08X chr=%08X\n",
			(ULONG)i, nm, pSec[i].VirtualAddress, pSec[i].Misc.VirtualSize,
			pSec[i].Characteristics);
	}

	// 4. PAGEWdfV 存的是 WDF_BIND_INFO，FuncTable 指向真函数指针数组。
	//    但有的 KMDF 构建里 FuncTable 直接落在别的 PAGEWdf* 节里 / 或 .data 节。
	//    所以这里**遍历 Wdf01000.sys 的所有可写/分页节**做扫描。
	ULONG64 BestTable = 0;
	ULONG   BestCount = 0;
	const ULONG MaxEntries = 2048;
	ULONG   CandidateSeen = 0;
	ULONG   CandidateProbed = 0;
	ULONG   SectionsScanned = 0;

	for (USHORT si = 0; si < pNt->FileHeader.NumberOfSections; si++)
	{
		// 只扫描可能含数据的节：包含 "PAGE" / ".data" / ".rdata" 前缀，
		// 或显式带 INITIALIZED_DATA 特征位。
		BOOLEAN IsData =
			(memcmp(pSec[si].Name, "PAGE", 4) == 0) ||
			(memcmp(pSec[si].Name, ".data", 5) == 0) ||
			(memcmp(pSec[si].Name, ".rdata", 6) == 0) ||
			((pSec[si].Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) &&
			 !(pSec[si].Characteristics & IMAGE_SCN_MEM_EXECUTE));
		if (!IsData) continue;

		ULONG64 SecStart = Wdf01000Base + pSec[si].VirtualAddress;
		ULONG64 SecEnd = SecStart + pSec[si].Misc.VirtualSize;
		if (pSec[si].Misc.VirtualSize < sizeof(ULONG64) * 8) continue;
		SectionsScanned++;

		ScanStart = SecStart;
		ScanBytes = pSec[si].Misc.VirtualSize;
		ULONG64 ScanEnd = SecEnd;

		for (ULONG64 cur = ScanStart; cur + sizeof(ULONG64) <= ScanEnd; cur += sizeof(ULONG64))
		{
			ULONG64 candidate = 0;
			__try { candidate = *(PULONG64)cur; }
			__except (EXCEPTION_EXECUTE_HANDLER) { continue; }

			if ((candidate >> 48) != 0xFFFF) continue;
			if (candidate & 7) continue;
			if (!MmIsAddressValid((PVOID)candidate)) continue;
			CandidateSeen++;

			ULONG64 first = 0;
			__try { first = *(PULONG64)candidate; }
			__except (EXCEPTION_EXECUTE_HANDLER) { continue; }
			if (first < Wdf01000Base || first >= Wdf01000End) continue;

			ULONG  Count = 0;
			ULONG64* Tbl = (ULONG64*)candidate;
			__try
			{
				while (Count < MaxEntries)
				{
					if ((((ULONG64)&Tbl[Count]) & 0xFFF) == 0 &&
						!MmIsAddressValid(&Tbl[Count]))
					{
						break;
					}
					ULONG64 ptr = Tbl[Count];
					if (ptr == 0) break;
					if ((ptr >> 48) != 0xFFFF) break;
					if (ptr < Wdf01000Base || ptr >= Wdf01000End) break;
					Count++;
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {}

			if (Count >= 4) CandidateProbed++;
			if (Count > 4)
			{
				MyDbgPrintfEx("[EnumWdfFunction]   sect=%u slot=%I64X cand=%I64X cnt=%u\n",
					(ULONG)si, cur, candidate, Count);
			}

			if (Count > BestCount)
			{
				BestCount = Count;
				BestTable = candidate;
			}
		}
	} // end section loop
	MyDbgPrintfEx("[EnumWdfFunction] scan done: scannedSects=%u kernelPtrs=%u probedOk=%u best=%I64X cnt=%u\n",
		SectionsScanned, CandidateSeen, CandidateProbed, BestTable, BestCount);

	if (BestTable == 0 || BestCount == 0)
	{
		MyDbgPrintfEx("[%s] WdfFunctions table not located.\n", __FUNCTION__);
		char buf[160];
		RtlStringCbPrintfA(buf, sizeof(buf),
			"DIAG: not located sects=%u kPtrs=%u probed=%u base=%I64X",
			SectionsScanned, CandidateSeen, CandidateProbed, Wdf01000Base);
		WdfPushDiag(OutData, buf);
		return 1;
	}

	MyDbgPrintfEx("[%s] WdfFunctions @%I64X count=%u\n", __FUNCTION__, BestTable, BestCount);

	// 5. 输出每个表项
	ULONG64* WdfFunctions = (ULONG64*)BestTable;
	ULONG64 IsInit = MmIsAddressValid(*OutData) && ((PCWdfInfo)(*OutData))->List.IsInitialize;
	ULONG64 Count = 0;

	for (ULONG i = 0; i < BestCount; i++)
	{
		ULONG64 FunAddr = WdfFunctions[i];

		PCWdfInfo pNewInfo = MyExAllocMemOry(sizeof(CWdfInfo), PAGE_READWRITE, UserMode);
		if (pNewInfo == NULL)
		{
			break;
		}

		pNewInfo->pFunOrder = i;
		pNewInfo->pFunAddr = FunAddr;
		// 没有历史基线表，原始地址置 0 由 R3 显示 "--"
		pNewInfo->pSrcFunAddr = 0;
		pNewInfo->HookType = (FunAddr >= Wdf01000Base && FunAddr < Wdf01000End) ? 0 : 1;

		// 反查当前函数实际所属模块（被 hook 时这里就是 hook 模块）
		CDriverInfo DI = { 0 };
		if (IsSysModuleEx(FunAddr, &DI))
		{
			memcpy_s(pNewInfo->ModulePath, sizeof(pNewInfo->ModulePath), DI.ImageFullBaseName, sizeof(pNewInfo->ModulePath));
		}
		else if (GotPath)
		{
			memcpy_s(pNewInfo->ModulePath, sizeof(pNewInfo->ModulePath), Wdf01000Path, sizeof(pNewInfo->ModulePath));
		}

		if (IsInit != 0 && ((PCWdfInfo)(*OutData))->List.IsInitialize)
		{
			InsertHeadList(&((PCWdfInfo)(*OutData))->List.List, &pNewInfo->List.List);
		}
		else
		{
			pNewInfo->List.IsInitialize = TRUE;
			InitializeListHead(&pNewInfo->List.List);
			*OutData = pNewInfo;
			IsInit = TRUE;
		}
		Count++;
	}

	return Count;
#endif

ULONG64 EnumHalDispatchTable(ULONG64 HalTable, ULONG64 TableSize, PCHalFunTableInfo* OutData)
{
	//验证参数一是否有效 
	if (!MmIsAddressValid(HalTable))
	{
		return FALSE;
	}

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCHalFunTableInfo)(*OutData))->List.IsInitialize;


	//循环遍历HalDispatchTable表中的数据 大小为 HAL_DISPATCH 结构体-8 
	int i = 0;
	for (i = 1; i < TableSize; i++)
	{
		//申请空间
		PCHalFunTableInfo pNewInfo = MyExAllocMemOry(sizeof(CHalFunTableInfo), PAGE_READWRITE, UserMode);
		if (pNewInfo == NULL)
		{
			break;
		}

		pNewInfo->pFunOrder = i - 1;								//序号
		pNewInfo->pFunAddr = ((PULONG64)HalTable)[i];				//函数地址

		//拷贝路径
		CDriverInfo DriverInfo = { 0 };
		if (IsSysModuleEx(pNewInfo->pFunAddr, &DriverInfo))
		{
			memcpy_s(pNewInfo->ModulePath, MY_MAX_PATH, DriverInfo.ImageFullBaseName, MY_MAX_PATH);
		}

		//插入链表
		if (IsInit != 0 && ((PCHalFunTableInfo)(*OutData))->List.IsInitialize)
		{
			//插入链表里面
			InsertHeadList(&((PCHalFunTableInfo)(*OutData))->List.List, &pNewInfo->List.List);
		}
		else
		{
			pNewInfo->List.IsInitialize = TRUE;						//已初始化
			InitializeListHead(&pNewInfo->List.List);				//初始化链表头
			*OutData = pNewInfo;									//赋值
			IsInit = TRUE;
		}
	}
	return i;
}

ULONG64 EnumFileSystemDevice(ULONG64 pListEntry, ULONG64 nType, PCFileSystemDeviceInfo* OutData)
{
	if (!MmIsAddressValid(pListEntry))
	{
		return NULL;
	}
	ULONG64 dqRet = 0;

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCFileSystemDeviceInfo)(*OutData))->List.IsInitialize;


	PLIST_ENTRY pList = (PLIST_ENTRY)pListEntry;

	for (PLIST_ENTRY pCurList = pList->Flink; pCurList != pList; pCurList = pCurList->Flink)
	{
		//获取设备对象
		PDEVICE_OBJECT pDeviceObj = (PDEVICE_OBJECT)((ULONG64)pCurList - _DEVICE_OBJECT_Queue);
		if (MmIsAddressValid(pDeviceObj))
		{
			//申请空间
			PCFileSystemDeviceInfo pNewInfo = MyExAllocMemOry(sizeof(CFileSystemDeviceInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}
			RtlZeroMemory(pNewInfo, sizeof(CFileSystemDeviceInfo));
			//类型
			pNewInfo->nType = nType;

			//设备对象
			pNewInfo->DeviceObject = (ULONG64)pDeviceObj;

			//驱动对象
			pNewInfo->DriverObject = (ULONG64)pDeviceObj->DriverObject;
			pNewInfo->DeviceType = pDeviceObj->DeviceType;
			pNewInfo->Characteristics = pDeviceObj->Characteristics;
			pNewInfo->DeviceFlags = pDeviceObj->Flags;
			pNewInfo->AttachedDevice = (ULONG64)pDeviceObj->AttachedDevice;
			pNewInfo->NextDevice = (ULONG64)pDeviceObj->NextDevice;

			//拷贝驱动对象名称
			if (MmIsAddressValid(pNewInfo->DriverObject))
			{
				PUNICODE_STRING pDriverName = (PUNICODE_STRING) & ((PDRIVER_OBJECT)pNewInfo->DriverObject)->DriverName;
				if (MmIsAddressValid(pDriverName) && MmIsAddressValid(pDriverName->Buffer))
				{
					ULONG64 NameLen = pDriverName->Length;
					if (NameLen > sizeof(pNewInfo->DriverName) - sizeof(WCHAR))
					{
						NameLen = sizeof(pNewInfo->DriverName) - sizeof(WCHAR);
					}
					memcpy_s(pNewInfo->DriverName, sizeof(pNewInfo->DriverName), pDriverName->Buffer, NameLen);
					pNewInfo->DriverName[NameLen / sizeof(WCHAR)] = L'\0';
				}
			}

			//拷贝设备名称
			POBJECT_NAME_INFORMATION pDeviceName = obGetObjectNameEx(pDeviceObj);
			if (pDeviceName)
			{
				if (MmIsAddressValid(pDeviceName->Name.Buffer))
				{
					ULONG64 NameLen = pDeviceName->Name.Length;
					if (NameLen > sizeof(pNewInfo->DeviceName) - sizeof(WCHAR))
					{
						NameLen = sizeof(pNewInfo->DeviceName) - sizeof(WCHAR);
					}
					memcpy_s(pNewInfo->DeviceName, sizeof(pNewInfo->DeviceName), pDeviceName->Name.Buffer, NameLen);
					pNewInfo->DeviceName[NameLen / sizeof(WCHAR)] = L'\0';
				}
				ExFreePoolWithTag(pDeviceName, 'namT');
			}

			//插入链表
			if (IsInit != 0 && ((PCFileSystemDeviceInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCFileSystemDeviceInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;								//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;											//赋值
				IsInit = TRUE;
			}
			dqRet++;
		}
	}
	return dqRet;
}

ULONG64 SSDTTable()
{
	//获取KiServiceTable地址偏移

	if (!MmIsAddressValid(KeServiceDescriptorTable))
	{
		return FALSE;
	}

	//__debugbreak();

	//
	// 获取旧的KiServiceTable地址表
	// 然后根据旧的KiServiceTable地址-旧的ImageBase 获取偏移
	// 根据KiServiceTable偏移+新的ImageBase 获取的新的KiServiceTable地址
	// 
	ULONG64 OldKiServiceTable = ((PKSYSTEM_SERVICE_TABLE)KeServiceDescriptorTable)->ServiceTableBase;
	ULONG64 FunctionCount = ((PKSYSTEM_SERVICE_TABLE)KeServiceDescriptorTable)->NumberOfService;
	ULONG64 OldKiServiceTableOffset = OldKiServiceTable - g_NtoskrnlAddr;

	PULONG32 NewKiServiceTable = OldKiServiceTableOffset + g_NewNtoskrnlAddr;

	//
	// 重新映射一份KiServiceTable表 获取KiServiceTable中的内容
	//
	if (g_NewKiServiceTable == NULL)
	{
		g_NewKiServiceTable = MyExAllocMemOry(FunctionCount * sizeof(ULONG32), POOL_FLAG_PAGED, KernelMode);
		if (MmIsAddressValid(g_NewKiServiceTable))
		{
			RtlZeroMemory(g_NewKiServiceTable, FunctionCount * sizeof(ULONG32));

			//
			// 计算KiServiceTable表中的数据,保存到g_NewKiServiceTable中
			//

			ULONG32 KiServiceTableBase = (ULONG32)NewKiServiceTable;
			ULONG32 NewImage = (ULONG32)g_NewNtoskrnlAddr;
			for (int i = 0; i < FunctionCount; i++)
			{

				//
				//	修复算法 :
				//					根据文件中 KiServiceTable 表的数据 - (ULONG32)KiServiceTable地址 + (ULONG32)新的Ntoskrnel模块基址
				//					使用是 新的KiServiceTable基址 + 修复后的数据
				//

				g_NewKiServiceTable[i] = NewKiServiceTable[i] - KiServiceTableBase + NewImage;
			}
		}
	}

	//MyDbgPrintfEx("NewKiServiceTable : %I64X\n", g_NewKiServiceTable);

	return TRUE;
}

ULONG64 LoadNtosKrnel()
{
	typedef NTSTATUS
	(__fastcall* PfnMmLoadSystemImage)(
		IN PUNICODE_STRING ImageFileName,
		IN PUNICODE_STRING NamePrefix OPTIONAL,
		IN PUNICODE_STRING LoadedBaseName OPTIONAL,
		IN ULONG LoadFlags,
		OUT PVOID* ImageHandle,
		OUT PVOID* ImageBaseAddress
		);

	UNICODE_STRING MmLoadSystemImageStr = { 0 };
	RtlInitUnicodeString(&MmLoadSystemImageStr, L"MmLoadSystemImage");
	PfnMmLoadSystemImage MmLoadSystemImage = MmGetSystemRoutineAddress(&MmLoadSystemImageStr);

	UNICODE_STRING NtoskrnelPath = { 0 };
	RtlInitUnicodeString(&NtoskrnelPath, L"\\??\\C:\\Windows\\system32\\ntoskrnl.exe");

#define MM_LOAD_IMAGE_IN_SESSION    0x1
#define MM_LOAD_IMAGE_AND_LOCKDOWN  0x2
	ULONG64 ImageDll = NULL;

	//__debugbreak();
	if (MmIsAddressValid(MmLoadSystemImage))
	{
		ImageDll = MmLoadSystemImage(&NtoskrnelPath, NULL, NULL, MM_LOAD_IMAGE_AND_LOCKDOWN, &g_NewNtoskrnlHandle, &g_NewNtoskrnlAddr);

		//修复SSDT
		SSDTTable();
	}
	return TRUE;
}

ULONG64 UnLoadKernelModule()
{
	typedef NTSTATUS
	(__fastcall* PfnMmUnloadSystemImage)(
		IN PVOID Section
		);

	UNICODE_STRING MmUnloadSystemImageStr = { 0 };
	RtlInitUnicodeString(&MmUnloadSystemImageStr, L"MmUnloadSystemImage");
	PfnMmUnloadSystemImage MmUnloadSystemImage = MmGetSystemRoutineAddress(&MmUnloadSystemImageStr);
	if (MmIsAddressValid(MmUnloadSystemImage) && MmIsAddressValid(g_NewNtoskrnlAddr))
	{
		//MyDbgPrintfEx("MmUnloadSystemImage:%I64X g_NewNtoskrnlAddr:%I64X\n", MmUnloadSystemImage, g_NewNtoskrnlAddr);


		//NtClose(g_NewNtoskrnlHandle);

		ULONG64 nStatus = MmUnloadSystemImage(g_NewNtoskrnlHandle);
		//MyDbgPrintfEx("MmUnloadSystemImage:%I64X g_NewNtoskrnlAddr:%I64X nStatus:%I64X\n", MmUnloadSystemImage, g_NewNtoskrnlAddr, nStatus);
	}

	if (MmIsAddressValid(g_NewKiServiceTable))
	{
		ExFreePool(g_NewKiServiceTable);
	}
	return TRUE;
}

ULONG64 SSDTTShadowable()
{
	//获取KiServiceTable地址偏移

	if (!MmIsAddressValid(KeServiceDescriptorTable))
	{
		return FALSE;
	}

	//
	// 获取旧的KiServiceTable地址表
	// 然后根据旧的KiServiceTable地址-旧的ImageBase 获取偏移
	// 根据KiServiceTable偏移+新的ImageBase 获取的新的KiServiceTable地址
	// 
	ULONG64 nServiceTableShadow = (ULONG64)KeServiceDescriptorTableShadow + sizeof(KSYSTEM_SERVICE_TABLE);
	if (MmIsAddressValid(nServiceTableShadow))
	{
		ULONG64 FunctionCount = ((PKSYSTEM_SERVICE_TABLE)nServiceTableShadow)->NumberOfService;							//获取SSDT表数量
		ULONG64 OldW32pServiceTable = ((PKSYSTEM_SERVICE_TABLE)nServiceTableShadow)->ServiceTableBase;					//获取SSDT函数基地址

		ULONG64 OldW32pServiceTableOffset = OldW32pServiceTable - g_Win32kAddr;

		PULONG32 NewW32pServiceTable = OldW32pServiceTableOffset + g_NewWin32kAddr;

		//
		// 重新映射一份KiServiceTable表 获取KiServiceTable中的内容
		//
		if (g_NewW32pServiceTable == NULL)
		{
			g_NewW32pServiceTable = MyExAllocMemOry(FunctionCount * sizeof(ULONG32), POOL_FLAG_PAGED, KernelMode);
			if (MmIsAddressValid(g_NewW32pServiceTable))
			{
				RtlZeroMemory(g_NewW32pServiceTable, FunctionCount * sizeof(ULONG32));

				//
				// 计算KiServiceTable表中的数据,保存到g_NewKiServiceTable中
				//

				ULONG32 KiServiceTableBase = (ULONG32)NewW32pServiceTable;
				ULONG32 NewImage = (ULONG32)g_NewWin32kAddr;
				for (int i = 0; i < FunctionCount; i++)
				{

					//
					//	修复算法 :
					//					根据文件中 KiServiceTable 表的数据 - (ULONG32)KiServiceTable地址 + (ULONG32)新的Ntoskrnel模块基址
					//					使用是 新的KiServiceTable基址 + 修复后的数据
					//

					g_NewW32pServiceTable[i] = (ULONG32)(NewW32pServiceTable[i] - KiServiceTableBase + NewImage) << 4;
				}
			}
		}
		//MyDbgPrintfEx("NewKiServiceTable : %I64X NewW32pServiceTable:%I64X\n", g_NewW32pServiceTable, NewW32pServiceTable);
	}
	return TRUE;
}

ULONG64 LoadWin32k()
{
	typedef NTSTATUS
	(__fastcall* PfnMmLoadSystemImage)(
		IN PUNICODE_STRING ImageFileName,
		IN PUNICODE_STRING NamePrefix OPTIONAL,
		IN PUNICODE_STRING LoadedBaseName OPTIONAL,
		IN ULONG LoadFlags,
		OUT PVOID* ImageHandle,
		OUT PVOID* ImageBaseAddress
		);

	UNICODE_STRING MmLoadSystemImageStr = { 0 };
	RtlInitUnicodeString(&MmLoadSystemImageStr, L"MmLoadSystemImage");
	PfnMmLoadSystemImage MmLoadSystemImage = MmGetSystemRoutineAddress(&MmLoadSystemImageStr);

	UNICODE_STRING Win32kPath = { 0 };
	RtlInitUnicodeString(&Win32kPath, L"\\??\\C:\\Windows\\system32\\Win32k.sys");

#define MM_LOAD_IMAGE_IN_SESSION    0x1
#define MM_LOAD_IMAGE_AND_LOCKDOWN  0x2
	ULONG64 ImageDll = NULL;

	if (MmIsAddressValid(MmLoadSystemImage))
	{
		ImageDll = MmLoadSystemImage(&Win32kPath, NULL, NULL, MM_LOAD_IMAGE_AND_LOCKDOWN, &g_NewWin32kHandle, &g_NewWin32kAddr);
		//MyDbgPrintfEx("NewImageBase:%I64X ImageHandle:%I64X\n", g_NewWin32kAddr, g_NewWin32kHandle);

		//修复SSDTShadow
		SSDTTShadowable();
	}
	return TRUE;
}

ULONG64 UnLoadWin32k()
{
	typedef NTSTATUS
	(__fastcall* PfnMmUnloadSystemImage)(
		IN PVOID Section
		);

	UNICODE_STRING MmUnloadSystemImageStr = { 0 };
	RtlInitUnicodeString(&MmUnloadSystemImageStr, L"MmUnloadSystemImage");
	PfnMmUnloadSystemImage MmUnloadSystemImage = MmGetSystemRoutineAddress(&MmUnloadSystemImageStr);
	if (MmIsAddressValid(MmUnloadSystemImage) && MmIsAddressValid(g_NewWin32kAddr))
	{

		//MmUnloadSystemImage(g_NewNtoskrnlAddr);
		//NtClose(g_NewWin32kHandle);

		ULONG64 nStatus = MmUnloadSystemImage(g_NewWin32kHandle);

		//MyDbgPrintfEx("MmUnloadSystemImage:%I64X g_NewNtoskrnlAddr:%I64X nStatus:%I64X\n", MmUnloadSystemImage, g_NewWin32kAddr, nStatus);

	}

	//释放资源
	if (MmIsAddressValid(g_NewW32pServiceTable))
	{
		ExFreePool(g_NewW32pServiceTable);
	}

	return TRUE;
}

ULONG64 ReturnSsdt(nIndex)
{

}

ULONG64 ReturnSsdtShadow(nIndex)
{

}

ULONG64 GetDeviceObject(ULONG64 pDriverObject)
{
	if (!MmIsAddressValid(pDriverObject))
	{
		return NULL;
	}
	return (ULONG64)((PDRIVER_OBJECT)pDriverObject)->DeviceObject;
}

ULONG64 GetDriverObject(ULONG64 pDeviceObject)
{
	if (!MmIsAddressValid(pDeviceObject))
	{
		return NULL;
	}
	return (ULONG64)((PDEVICE_OBJECT)pDeviceObject)->DriverObject;
}

ULONG64 GetNextDeviceObject(ULONG64 pDeviceObject)
{
	if (!MmIsAddressValid(pDeviceObject))
	{
		return NULL;
	}
	return (ULONG64)((PDEVICE_OBJECT)pDeviceObject)->NextDevice;
}

ULONG64 GetDeviceAttachTo(ULONG64 DeviceObject)
{
	if (!MmIsAddressValid(DeviceObject))
	{
		return NULL;
	}

	PDEVOBJ_EXTENSION DeviceObjectExtension = ((PDEVICE_OBJECT)DeviceObject)->DeviceObjectExtension;

	if (!MmIsAddressValid(DeviceObjectExtension))
	{
		return NULL;
	}

	//返回附加的对象
	return DeviceObjectExtension->AttachedTo;
}

ULONG64 GetRootAtttachTo(ULONG64 DriverObject, PCFilterDeviceInfo* OutData)
{
	if (!MmIsAddressValid(DriverObject))
	{
		return NULL;
	}

	ULONG64 IsInit = MmIsAddressValid(OutData) && MmIsAddressValid(*OutData) && ((PCProcessVadInfo)(*OutData))->List.IsInitialize;

	int nIndex = 0;
	PDEVICE_OBJECT pCurDevice = ((PDRIVER_OBJECT)DriverObject)->DeviceObject;

	while (MmIsAddressValid(pCurDevice))
	{
		PDEVICE_OBJECT pAttachedDevice = pCurDevice->AttachedDevice;

		//判断是否有附加的设备
		if (MmIsAddressValid(pAttachedDevice))
		{
			//											//申请空间
			PCFilterDeviceInfo pNewInfo = MyExAllocMemOry(sizeof(CFilterDeviceInfo), PAGE_READWRITE, UserMode);
			if (pNewInfo == NULL)
			{
				break;
			}

			pNewInfo->FilterDeviceObject = pAttachedDevice; //附加的设备

			POBJECT_NAME_INFORMATION pFilterDeviceName = obGetObjectNameEx(pAttachedDevice);
			if (pFilterDeviceName)
			{
				wcscpy_s(pNewInfo->FilterDeviceName, FILTER_DEVICE_NAME_LEN, pFilterDeviceName->Name.Buffer);
				ExFreePoolWithTag(pFilterDeviceName, 'namT');
			}

			PUNICODE_STRING pCurDriverName = &pAttachedDevice->DriverObject->DriverName;
			UNICODE_STRING CurDriverName = { 0 };
			WCHAR szBuf[MY_MAX_PATH] = { 0 };

			if (ExtractDriverName(&pAttachedDevice->DriverObject->DriverName, &CurDriverName))
			{
				pCurDriverName = &CurDriverName;
			}

			memcpy_s(szBuf, MY_MAX_PATH, pCurDriverName->Buffer, pCurDriverName->Length);

			WCHAR szBuf1[MY_MAX_PATH] = { 0 };

			swprintf(szBuf1, L"%ws*", szBuf);
			RtlInitUnicodeString(&CurDriverName, szBuf1);

			//获取过滤驱动信息
			CDriverInfo FilterDriverInfo = { 0 };
			if (LookUpDriverObjectByName(&CurDriverName, &FilterDriverInfo))
			{
				wcscpy_s(pNewInfo->FilterDriverName, MY_MAX_PATH, FilterDriverInfo.DriverName);
				wcscpy_s(pNewInfo->FilterDriverPath, MY_MAX_PATH, FilterDriverInfo.ImageFullBaseName);
			}

			pCurDriverName = &pCurDevice->DriverObject->DriverName;
			wcscpy_s(pNewInfo->SrcDriverName, MY_MAX_PATH, pCurDriverName->Buffer);

			UNICODE_STRING pDriverName = { 0 };
			RtlInitUnicodeString(&pDriverName, pNewInfo->SrcDriverName);

			PUNICODE_STRING ppDriverName = &pDriverName;

			UNICODE_STRING pExtractDriverName = { 0 };
			if (ExtractDriverName(&pDriverName, &pExtractDriverName))
			{
				ppDriverName = &pExtractDriverName;
			}

			wcscpy_s(pNewInfo->TypeName, FILTER_DEVICE_TYPENAME_LEN, ppDriverName->Buffer);

			//插入链表
			if (IsInit != 0 && ((PCFilterDeviceInfo)(*OutData))->List.IsInitialize)
			{
				//插入链表里面
				InsertHeadList(&((PCFilterDeviceInfo)(*OutData))->List.List, &pNewInfo->List.List);
			}
			else
			{
				pNewInfo->List.IsInitialize = TRUE;						//已初始化
				InitializeListHead(&pNewInfo->List.List);				//初始化链表头
				*OutData = pNewInfo;									//赋值
				IsInit = TRUE;
			}

			nIndex++;
		}
		//获取下一个设备
		pCurDevice = pCurDevice->NextDevice;
	}
	//返回附加的对象
	return nIndex;
}

ULONG64 EnumFilterDriver(PCFilterDeviceInfo* pFilterDevice)
{
	int nIndex = 0;
	POBJECT_DIRECTORY pFileSystemDirectroy = (POBJECT_DIRECTORY)obQueryRootDirectoryTable(&g_RootFileSystemName);
	POBJECT_DIRECTORY pDriverDirectroy = (POBJECT_DIRECTORY)obQueryRootDirectoryTable(&g_RootDriverName);
	if (!MmIsAddressValid(pFileSystemDirectroy) || !MmIsAddressValid(pDriverDirectroy))
	{
		return nIndex;
	}

	UNICODE_STRING pStrRAW = { 0 };
	UNICODE_STRING pStrKbdClass = { 0 };
	UNICODE_STRING pStri8042prt = { 0 };
	UNICODE_STRING pStrTcpip = { 0 };
	UNICODE_STRING pStrnsiproxy = { 0 };
	UNICODE_STRING pStrtdx = { 0 };
	UNICODE_STRING pStrMouclass = { 0 };
	UNICODE_STRING pStrNDIS = { 0 };
	UNICODE_STRING pStrPnpManager = { 0 };
	UNICODE_STRING pStrVolume = { 0 };
	UNICODE_STRING pStrDisk = { 0 };

	RtlInitUnicodeString(&pStrRAW, L"RAW");
	RtlInitUnicodeString(&pStrKbdClass, L"KbdClass");
	RtlInitUnicodeString(&pStri8042prt, L"i8042prt");
	RtlInitUnicodeString(&pStrTcpip, L"Tcpip");
	RtlInitUnicodeString(&pStrnsiproxy, L"nsiproxy");
	RtlInitUnicodeString(&pStrtdx, L"tdx");
	RtlInitUnicodeString(&pStrNDIS, L"NDIS");
	RtlInitUnicodeString(&pStrPnpManager, L"PnpManager");
	RtlInitUnicodeString(&pStrVolume, L"volume");
	RtlInitUnicodeString(&pStrDisk, L"Disk");

	PDRIVER_OBJECT pRawDriver = obQueryDirectoryTable(pFileSystemDirectroy, &pStrRAW);
	PDRIVER_OBJECT pKbdClassDriver = obQueryDirectoryTable(pDriverDirectroy, &pStrKbdClass);
	PDRIVER_OBJECT pI8042prtDriver = obQueryDirectoryTable(pDriverDirectroy, &pStri8042prt);
	PDRIVER_OBJECT pTcpipDriver = obQueryDirectoryTable(pDriverDirectroy, &pStrTcpip);
	PDRIVER_OBJECT pNsiproxyDriver = obQueryDirectoryTable(pDriverDirectroy, &pStrnsiproxy);
	PDRIVER_OBJECT pTdxDriver = obQueryDirectoryTable(pDriverDirectroy, &pStrtdx);
	PDRIVER_OBJECT pNdisDriver = obQueryDirectoryTable(pDriverDirectroy, &pStrNDIS);
	PDRIVER_OBJECT pPnpManagerDriver = obQueryDirectoryTable(pDriverDirectroy, &pStrPnpManager);
	PDRIVER_OBJECT pVolume = obQueryDirectoryTable(pDriverDirectroy, &pStrVolume);
	PDRIVER_OBJECT pDisk = obQueryDirectoryTable(pDriverDirectroy, &pStrDisk);

	{
		int nIndexVal = 0;
		if (MmIsAddressValid(pRawDriver))
		{
			nIndexVal = GetRootAtttachTo(pRawDriver, pFilterDevice);
			MyDbgPrintfEx("pRawDriver当前过滤驱动数量为：%08X\n", nIndexVal);
			nIndex += nIndexVal;
		}

		if (MmIsAddressValid(pKbdClassDriver))
		{
			nIndexVal = GetRootAtttachTo(pKbdClassDriver, pFilterDevice);
			MyDbgPrintfEx("pRawDriver当前过滤驱动数量为：%08X\n", nIndexVal);
			nIndex += nIndexVal;
		}

		if (MmIsAddressValid(pI8042prtDriver))
		{
			nIndexVal = GetRootAtttachTo(pI8042prtDriver, pFilterDevice);
			MyDbgPrintfEx("pI8042prtDriver当前过滤驱动数量为：%08X\n", nIndexVal);
			nIndex += nIndexVal;
		}

		if (MmIsAddressValid(pTcpipDriver))
		{
			nIndexVal = GetRootAtttachTo(pTcpipDriver, pFilterDevice);
			MyDbgPrintfEx("pTcpipDriver当前过滤驱动数量为：%08X\n", nIndexVal);
			nIndex += nIndexVal;
		}

		if (MmIsAddressValid(pNsiproxyDriver))
		{
			nIndexVal = GetRootAtttachTo(pNsiproxyDriver, pFilterDevice);
			MyDbgPrintfEx("pNsiproxyDriver当前过滤驱动数量为：%08X\n", nIndexVal);
			nIndex += nIndexVal;
		}

		if (MmIsAddressValid(pTdxDriver))
		{
			nIndexVal = GetRootAtttachTo(pTdxDriver, pFilterDevice);
			MyDbgPrintfEx("pTdxDriver当前过滤驱动数量为：%08X\n", nIndexVal);
			nIndex += nIndexVal;
		}

		if (MmIsAddressValid(pNdisDriver))
		{
			nIndexVal = GetRootAtttachTo(pNdisDriver, pFilterDevice);
			MyDbgPrintfEx("pNdisDriver当前过滤驱动数量为：%08X\n", nIndexVal);
			nIndex += nIndexVal;
		}

		if (MmIsAddressValid(pPnpManagerDriver))
		{
			nIndexVal = GetRootAtttachTo(pPnpManagerDriver, pFilterDevice);
			MyDbgPrintfEx("pPnpManagerDriver当前过滤驱动数量为：%08X\n", nIndexVal);
			nIndex += nIndexVal;
		}

		if (MmIsAddressValid(pVolume))
		{
			nIndexVal = GetRootAtttachTo(pVolume, pFilterDevice);
			MyDbgPrintfEx("pVolume当前过滤驱动数量为：%08X\n", nIndexVal);
			nIndex += nIndexVal;
		}

		if (MmIsAddressValid(pDisk))
		{
			nIndexVal = GetRootAtttachTo(pDisk, pFilterDevice);
			MyDbgPrintfEx("pDisk当前过滤驱动数量为：%08X\n", nIndexVal);
			nIndex += nIndexVal;
		}
	}

	MyDbgPrintfEx("当前过滤驱动数量为：%08X\n", nIndex);

	return nIndex;
}

UCHAR PsSetProcessProtection(ULONG64 pEprocess, UCHAR SrcProtectionValue)
{
	if (!MmIsAddressValid(pEprocess))
	{
		return 0;
	}

	UCHAR SrcProtectione = *(PUCHAR)(pEprocess + g_Offset_EPROCESS_Protection);

	*(PUCHAR)(pEprocess + g_Offset_EPROCESS_Protection) = SrcProtectionValue;

	return SrcProtectione;
}

ULONG64 kdDebugFlags(PCDebugFlagInfo pDebugInfo)
{
	if (!pDebugInfo)
	{
		return FALSE;
	}

	if (pDebugInfo->UserOperate == USER_GET_DEBUG_FLAG)
	{
		pDebugInfo->KdPitchDebugger = *KdPitchDebugger;
		pDebugInfo->kdDebuggerEnable = *KdDebuggerEnabled;
		pDebugInfo->KdDebuggerNotPresent = *KdDebuggerNotPresent;
		pDebugInfo->SharedDataKdDebuggerEnabled = ((PKUSER_SHARED_DATA)KI_USER_SHARED_DATA)->KdDebuggerEnabled;
	}
	else
	{
		*KdPitchDebugger = pDebugInfo->KdPitchDebugger;
		*KdDebuggerEnabled = pDebugInfo->kdDebuggerEnable;
		*KdDebuggerNotPresent = pDebugInfo->KdDebuggerNotPresent;
		((PKUSER_SHARED_DATA)KI_USER_SHARED_DATA)->KdDebuggerEnabled = pDebugInfo->SharedDataKdDebuggerEnabled;
	}


	MyDbgPrintfEx("kdDebugFlags UserOperate:%d kdDebuggerEnable:%d KdDebuggerNotPresent:%d SharedDataKdDebuggerEnabled:%d KdPitchDebugger:%d\n",
		pDebugInfo->UserOperate, pDebugInfo->kdDebuggerEnable, pDebugInfo->KdDebuggerNotPresent, pDebugInfo->SharedDataKdDebuggerEnabled, pDebugInfo->KdPitchDebugger);

	return TRUE;
}

BOOL IsDebug()
{
#define KI_USER_SHARED_DATA     0xFFFFF78000000000ULL

	BOOL nIsDebug = FALSE;
	PKUSER_SHARED_DATA pSharedData = (PKUSER_SHARED_DATA)KI_USER_SHARED_DATA;


	if (MmIsAddressValid(KdDebuggerNotPresent))
	{
		nIsDebug |= *KdDebuggerNotPresent;
	}

	if (MmIsAddressValid(KdDebuggerEnabled))
	{
		nIsDebug |= *KdDebuggerEnabled;
	}

	if (MmIsAddressValid(KdEnteredDebugger))
	{
		nIsDebug |= *KdEnteredDebugger;
	}

	if (MmIsAddressValid(pSharedData) && pSharedData->KdDebuggerEnabled)
	{
		nIsDebug |= TRUE;
	}

	if (MmIsAddressValid(KdPitchDebugger))
	{
		nIsDebug |= *KdPitchDebugger;
	}

	return nIsDebug;
}