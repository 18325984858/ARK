#include "DefineArea.h"
#include "KernelStruct.h"
#include <ntimage.h>    /*PE头文件*/
#include "FunctionPtr.h"

NTSTATUS SkillSetFileCompletion(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context)
{
	Irp->UserIosb->Status = Irp->IoStatus.Status;
	Irp->UserIosb->Information = Irp->IoStatus.Information;

	KeSetEvent(Irp->UserEvent, IO_NO_INCREMENT, FALSE);
	IoFreeIrp(Irp);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

ULONG64 MyDeleteRunFile(PUNICODE_STRING pFullName)
{
	NTSTATUS nStatus = STATUS_SUCCESS;
	HANDLE FileHandle = NULL;
	IO_STATUS_BLOCK IoStatusBlock = { 0 };
	OBJECT_ATTRIBUTES objAttribus = { 0 };

	InitializeObjectAttributes(&objAttribus, pFullName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	//打开文件获取文件句柄//属性为 读
	//NTSTATUS nStatus = ZwOpenFile(&FileHandle, GENERIC_READ, &objAttribus, &IoStatusBlock, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_NON_DIRECTORY_FILE);
	//__debugbreak();
	//打开文件
	IO_STATUS_BLOCK ioStatus = { 0 };
	nStatus = IoCreateFile(&FileHandle, FILE_READ_ATTRIBUTES, &objAttribus, &ioStatus, 0, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_DELETE, FILE_OPEN, 0, NULL, 0, 0, NULL, IO_NO_PARAMETER_CHECKING);
	if (!NT_SUCCESS(nStatus))
	{
		return nStatus;
	}

	//获取文件对象
	PFILE_OBJECT pFileObj = NULL;
	nStatus = ObReferenceObjectByHandle(FileHandle, DELETE, *IoFileObjectType, KernelMode, &pFileObj, NULL);
	if (!NT_SUCCESS(nStatus))
	{
		ZwClose(FileHandle);
		return nStatus;
	}

	//关闭文件句柄
	if (FileHandle)
	{
		ZwClose(FileHandle);
	}

	PDEVICE_OBJECT pFileDeviceObj = IoGetRelatedDeviceObject(pFileObj);
	if (MmIsAddressValid(pFileDeviceObj))
	{
		//	//去掉只读属性
		{
			//申请一个IRP请求
			PIRP pIrp = IoAllocateIrp(pFileDeviceObj->StackSize, TRUE);
			if (MmIsAddressValid(pIrp))
			{
				//初始化一个通知对象
				KEVENT nEvent = { 0 };
				KeInitializeEvent(&nEvent, SynchronizationEvent, FALSE);

				//填写IPR请求包
				FILE_BASIC_INFORMATION FileInformation = { 0 };
				FileInformation.FileAttributes = FILE_ATTRIBUTE_NORMAL; /*修改文件属性为 无属性*/
				pIrp->AssociatedIrp.SystemBuffer = &FileInformation;
				pIrp->UserEvent = &nEvent;
				pIrp->UserIosb = &nStatus;
				pIrp->Tail.Overlay.OriginalFileObject = pFileObj;
				pIrp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
				pIrp->RequestorMode = KernelMode;

				PIO_STACK_LOCATION irpSp = { 0 };
				irpSp = IoGetNextIrpStackLocation(pIrp);
				irpSp->MajorFunction = IRP_MJ_SET_INFORMATION;  /*要请求的消息*/
				irpSp->DeviceObject = pFileDeviceObj;
				irpSp->FileObject = pFileObj;
				irpSp->Parameters.SetFile.Length = sizeof(FILE_BASIC_INFORMATION);
				irpSp->Parameters.SetFile.FileInformationClass = FileBasicInformation;
				irpSp->Parameters.SetFile.FileObject = pFileObj;

				IoSetCompletionRoutine(pIrp, SkillSetFileCompletion, &nEvent, TRUE, TRUE, TRUE);

				IoCallDriver(pFileDeviceObj, pIrp);

				//等待处理完成
				KeWaitForSingleObject(&nEvent, Executive, KernelMode, TRUE, NULL);
			}
		}
	}
	/*
	//向文件驱动构建删除请求
	{
		KEVENT nEvent = { 0 };
		KeInitializeEvent(&nEvent, SynchronizationEvent, FALSE);

		FILE_DISPOSITION_INFORMATION_EX FileInformationEx;
		FileInformationEx.Flags = FILE_DISPOSITION_DELETE | FILE_DISPOSITION_IGNORE_READONLY_ATTRIBUTE;

		PIRP pIrp = IoAllocateIrp(pFileDeviceObj->StackSize, TRUE);
		if (MmIsAddressValid(pIrp))
		{
			pIrp->AssociatedIrp.SystemBuffer = &FileInformationEx;
			pIrp->UserEvent = &nEvent;
			pIrp->UserIosb = &ioStatus;
			pIrp->Tail.Overlay.OriginalFileObject = pFileObj;
			pIrp->Tail.Overlay.Thread = (PETHREAD)KeGetCurrentThread();
			pIrp->RequestorMode = KernelMode;

			PIO_STACK_LOCATION irpSp = { 0 };
			irpSp = IoGetNextIrpStackLocation(pIrp);
			irpSp->MajorFunction = IRP_MJ_SET_INFORMATION;
			irpSp->DeviceObject = pFileDeviceObj;
			irpSp->FileObject = pFileObj;
			irpSp->Parameters.SetFile.Length = sizeof(FILE_DISPOSITION_INFORMATION);
			irpSp->Parameters.SetFile.FileInformationClass = FileDispositionInformation;
			irpSp->Parameters.SetFile.FileObject = pFileObj;

			IoSetCompletionRoutine(pIrp, SkillSetFileCompletion, &nEvent, TRUE, TRUE, TRUE);

			//再加上下面这三行代码 ，MmFlushImageSection    函数通过这个结构来检查是否可以删除文件。
			//这个和Hook
			PULONG64 ImageSectionObject = NULL;
			PULONG64 DataSectionObject = NULL;
			PULONG64 SharedCacheMap = NULL;

			PSECTION_OBJECT_POINTERS pSectionObjectPointer = pFileObj->SectionObjectPointer;
			if (MmIsAddressValid(pSectionObjectPointer))
			{
				ImageSectionObject = pSectionObjectPointer->ImageSectionObject;			 // 备份之~~~
				pSectionObjectPointer->ImageSectionObject = NULL;						 //清零，准备删除

				DataSectionObject = pSectionObjectPointer->DataSectionObject;			//备份之
				pSectionObjectPointer->DataSectionObject = NULL;						//清零，准备删除

				SharedCacheMap = pSectionObjectPointer->SharedCacheMap;
				pSectionObjectPointer->SharedCacheMap = NULL;
			}

			//发irp删除
			IoCallDriver(pFileDeviceObj, pIrp);

			//等待操作完毕
			KeWaitForSingleObject(&nEvent, Executive, KernelMode, TRUE, NULL);

			//删除文件之后，从备份那里填充回来
			pSectionObjectPointer = pFileObj->SectionObjectPointer;

			if (MmIsAddressValid(pSectionObjectPointer))
			{
				//填充回来，不然蓝屏哦
				if (ImageSectionObject)
				{
					pSectionObjectPointer->ImageSectionObject = ImageSectionObject;
				}

				if (DataSectionObject)
				{
					pSectionObjectPointer->DataSectionObject = DataSectionObject;
				}

				if (SharedCacheMap)
				{
					pSectionObjectPointer->SharedCacheMap = SharedCacheMap;
				}
			}
		}
	}
	*/

	//调用API删除
	{
		//再加上下面这三行代码 ，MmFlushImageSection    函数通过这个结构来检查是否可以删除文件。
		//这个和Hook
		PULONG64 ImageSectionObject = NULL;
		PULONG64 DataSectionObject = NULL;
		PULONG64 SharedCacheMap = NULL;

		PSECTION_OBJECT_POINTERS pSectionObjectPointer = pFileObj->SectionObjectPointer;
		if (MmIsAddressValid(pSectionObjectPointer))
		{
			ImageSectionObject = pSectionObjectPointer->ImageSectionObject;			 // 备份之~~~
			pSectionObjectPointer->ImageSectionObject = NULL;						 //清零，准备删除

			DataSectionObject = pSectionObjectPointer->DataSectionObject;			//备份之
			pSectionObjectPointer->DataSectionObject = NULL;						//清零，准备删除

			SharedCacheMap = pSectionObjectPointer->SharedCacheMap;
			pSectionObjectPointer->SharedCacheMap = NULL;
		}

		//修改文件属性
		pFileObj->DeletePending = FALSE;
		pFileObj->DeleteAccess = TRUE;

		//刷新文件
		MmFlushImageSection(pFileObj->SectionObjectPointer, MmFlushForDelete);

		//修改属性之后再调用API删除
		nStatus = ZwDeleteFile(&objAttribus);

		//删除文件之后，从备份那里填充回来
		pSectionObjectPointer = pFileObj->SectionObjectPointer;

		if (MmIsAddressValid(pSectionObjectPointer))
		{
			//填充回来，不然蓝屏哦
			if (ImageSectionObject)
			{
				pSectionObjectPointer->ImageSectionObject = ImageSectionObject;
			}

			if (DataSectionObject)
			{
				pSectionObjectPointer->DataSectionObject = DataSectionObject;
			}

			if (SharedCacheMap)
			{
				pSectionObjectPointer->SharedCacheMap = SharedCacheMap;
			}
		}
	}

	//释放引用计数
	if (pFileObj)
	{
		ObDereferenceObject(pFileObj);
	}
	return nStatus;

}

ULONG64 MyReadFile(PVOID* pOutFileBuf, PULONG64 pOutFileSize, PUNICODE_STRING FilePath)
{
	HANDLE FileHandle = NULL;
	IO_STATUS_BLOCK IoStatusBlock = { 0 };
	OBJECT_ATTRIBUTES objAttribus = { 0 };
	InitializeObjectAttributes(&objAttribus, FilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

	//打开一个文件
	NTSTATUS nStatus = ZwOpenFile(&FileHandle, GENERIC_READ, &objAttribus, &IoStatusBlock, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE);
	if (!NT_SUCCESS(nStatus))
	{
		MyDbgPrintfEx("[%s] ZwOpenFile Error[%I64X]\n", __FUNCTION__, nStatus);
		return nStatus;
	}

	FILE_STANDARD_INFORMATION FileInfo = { 0 };
	nStatus = ZwQueryInformationFile(FileHandle,
		&IoStatusBlock,
		&FileInfo,
		sizeof(FileInfo),
		FileStandardInformation);

	if (!NT_SUCCESS(nStatus))
	{
		MyDbgPrintfEx("[%s] ZwQueryInformationFile Error[%I64X]\n", __FUNCTION__, nStatus);
		ZwClose(FileHandle);
		return nStatus;
	}

	//获取文件大小
	ULONG64 FileSize = FileInfo.EndOfFile.QuadPart;

	//申请空间
	PUCHAR pFileBuf = ExAllocatePool2(POOL_FLAG_NON_PAGED, FileSize, 'Tag');
	if (MmIsAddressValid(pFileBuf))
	{
		RtlZeroMemory(pFileBuf, FileSize);

		LARGE_INTEGER byteOffset = { 0 };
		nStatus = ZwReadFile(FileHandle, NULL, NULL, NULL, &IoStatusBlock, pFileBuf, FileSize, &byteOffset, NULL);
		if (!NT_SUCCESS(nStatus))
		{
			MyDbgPrintfEx("[%s] ZwReadFile Error[%I64X]\n", __FUNCTION__, nStatus);
			ZwClose(FileHandle);
			return nStatus;
		}
	}

	if (FileHandle != NULL)
	{
		ZwClose(FileHandle);
	}

	//返回文件大小
	if (MmIsAddressValid(pOutFileSize))
	{
		*pOutFileSize = FileSize;
	}

	//返回文件缓冲区
	if (MmIsAddressValid(pOutFileBuf))
	{
		*pOutFileBuf = pFileBuf;
	}

	return FileSize;
}

ULONG64 DllDeCodeFun(PUCHAR szDllBuf, ULONG64 DlllBufSize, PUCHAR* pOutBuf, PULONG64 pOutSize)
{
	//验证地址是否有效
	if (!MmIsAddressValid(szDllBuf) || !MmIsAddressValid(pOutBuf) || DlllBufSize <= 0)
	{
		return FALSE;
	}

	PUCHAR pVarOutBuf = ExAllocatePool(PagedPool, DlllBufSize);
	if (!MmIsAddressValid(pVarOutBuf))
	{
		return FALSE;
	}

	RtlZeroMemory(pVarOutBuf, DlllBufSize);

	int i = 0;
	for (i = 0; i < DlllBufSize; i++)
	{
		//解密 两次异或 防止一次解析规律
		pVarOutBuf[i] = (szDllBuf[i] ^ 0x46) ^ 0x89;
	}

	if (MmIsAddressValid(pOutBuf))
	{
		*pOutBuf = pVarOutBuf;
	}

	if (MmIsAddressValid(pOutSize))
	{
		*pOutSize = i;
	}
	return TRUE;
}

ULONG64 RepairBaseReloc(PCHAR pVirtualBuf, ULONG64 ImageBase)
{
	//验证参数
	if (!MmIsAddressValid(pVirtualBuf))
	{
		return FALSE;
	}

	//重定位表结构
	typedef struct BaseReloctionData
	{
		union
		{
			USHORT m_wData;
			struct
			{
				USHORT m_byOffset : 12;		/*偏移*/
				USHORT m_byType : 4;		/*类型*/
			};

		};

	}BaseReloctionData, * PBaseReloctionData;



	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pVirtualBuf;
	PIMAGE_NT_HEADERS64 pNt = (PIMAGE_NT_HEADERS64)(pVirtualBuf + pDos->e_lfanew);

	//重定位表地址
	PIMAGE_BASE_RELOCATION pRelocTable = (PIMAGE_BASE_RELOCATION)(pVirtualBuf + pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	//重定位表大小
	ULONG64 pRelocTableSize = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;


	if (pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == NULL ||
		pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size <= 0)
	{
		return -1;
	}

	//计算image差值				/*此处判断参数二是否为空 为空指定imagebase为当前缓冲区的首地址 否则为参数二*/
	ULONG64 dqImageDifference = (ImageBase ? ImageBase : pVirtualBuf) - pNt->OptionalHeader.ImageBase;

	ULONG64 CurSize = 0;

	while (pRelocTableSize > CurSize && pRelocTable->SizeOfBlock != NULL && pRelocTable->VirtualAddress != NULL)
	{
		//计算每个块的数量
		ULONG32 dwNumber = (pRelocTable->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(USHORT);
		PBaseReloctionData pBeginAddr = (PBaseReloctionData)((ULONG64)pRelocTable + sizeof(IMAGE_BASE_RELOCATION));

		for (int i = 0; i < dwNumber; i++)
		{
			PBaseReloctionData pCurinAddr = &pBeginAddr[i];

			USHORT Offset = pCurinAddr->m_byOffset;
			if (pCurinAddr->m_byType == IMAGE_REL_BASED_DIR64)			//修复64位地址
			{
				PULONG64 dwBaseAddr = (PULONG64)((ULONG64)pVirtualBuf + pRelocTable->VirtualAddress + Offset);
				*dwBaseAddr = *dwBaseAddr + dqImageDifference;
			}
			else if (pCurinAddr->m_byType == IMAGE_REL_BASED_HIGHLOW)	//修复32位地址
			{
				PULONG32 dwBaseAddr = (PULONG32)((ULONG64)pVirtualBuf + pRelocTable->VirtualAddress + Offset);
				*dwBaseAddr = *dwBaseAddr + dqImageDifference;
			}
		}
		CurSize = CurSize + pRelocTable->SizeOfBlock;
		pRelocTable = (PIMAGE_BASE_RELOCATION)((ULONG64)pRelocTable + pRelocTable->SizeOfBlock);
	}

	pNt->OptionalHeader.ImageBase = pVirtualBuf;

	return TRUE;
}

ULONG64 QuerySysModule(PUCHAR moduleName, ULONG_PTR* moduleSize)
{
	if (moduleName == NULL)
	{
		return 0;
	}

	RTL_PROCESS_MODULES rtlMoudles = { 0 };
	PRTL_PROCESS_MODULES SystemMoudles = &rtlMoudles;
	BOOLEAN isAllocate = FALSE;
	//测量长度
	ULONG* retLen = 0;
	NTSTATUS status = ZwQuerySystemInformation(SystemModuleInformation, SystemMoudles, sizeof(RTL_PROCESS_MODULES), &retLen);

	//分配实际长度内存
	if (status == STATUS_INFO_LENGTH_MISMATCH)
	{
		SystemMoudles = ExAllocatePool(PagedPool, retLen + sizeof(RTL_PROCESS_MODULES));
		if (!SystemMoudles) return 0;

		memset(SystemMoudles, 0, retLen + sizeof(RTL_PROCESS_MODULES));

		status = ZwQuerySystemInformation(SystemModuleInformation, SystemMoudles, retLen + sizeof(RTL_PROCESS_MODULES), &retLen);

		if (!NT_SUCCESS(status))
		{
			ExFreePool(SystemMoudles);
			return 0;
		}

		isAllocate = TRUE;
	}

	PUCHAR kernelModuleName = NULL;
	ULONG_PTR moudleBase = 0;

	do
	{
		if (_stricmp(moduleName, "ntoskrnl.exe") == 0 || _stricmp(moduleName, "ntkrnlpa.exe") == 0)
		{
			PRTL_PROCESS_MODULE_INFORMATION moudleInfo = &SystemMoudles->Modules[0];

			//获取模块基址
			moudleBase = moudleInfo->ImageBase;
			if (moduleSize)
			{
				*moduleSize = moudleInfo->ImageSize;
			}
			break;
		}

		kernelModuleName = ExAllocatePool(PagedPool, strlen(moduleName) + 1);
		memset(kernelModuleName, 0, strlen(moduleName) + 1);
		memcpy(kernelModuleName, moduleName, strlen(moduleName));
		_strupr(kernelModuleName);


		for (int i = 0; i < SystemMoudles->NumberOfModules; i++)
		{
			PRTL_PROCESS_MODULE_INFORMATION moudleInfo = &SystemMoudles->Modules[i];

			PUCHAR pathName = _strupr(moudleInfo->FullPathName);
			if (strstr(pathName, kernelModuleName))
			{
				moudleBase = moudleInfo->ImageBase;
				if (moduleSize)
				{
					*moduleSize = moudleInfo->ImageSize;
				}
				break;
			}

		}

	} while (0);


	if (kernelModuleName)
	{
		ExFreePool(kernelModuleName);
	}

	if (isAllocate)
	{
		ExFreePool(SystemMoudles);
	}

	return moudleBase;
}

ULONG64 ExportTableFuncByName(PUCHAR pModuleBaseAddr, PUCHAR pFuncName)
{
	//验证参数
	if (!MmIsAddressValid(pModuleBaseAddr) || !MmIsAddressValid(pFuncName))
	{
		return NULL;
	}

	ULONG64 dqFunAddr = NULL;


	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pModuleBaseAddr;
	PIMAGE_NT_HEADERS64 pNt = (PIMAGE_NT_HEADERS64)(pModuleBaseAddr + pDos->e_lfanew);
	PIMAGE_EXPORT_DIRECTORY pExportTable = (PIMAGE_EXPORT_DIRECTORY)(pModuleBaseAddr + pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	if (pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == NULL)
	{
		return -1;
	}

	for (int i = 0; i < pExportTable->NumberOfNames; i++)
	{
		PULONG funcAddress = pModuleBaseAddr + pExportTable->AddressOfFunctions;
		PULONG names = pModuleBaseAddr + pExportTable->AddressOfNames;
		PUSHORT fh = pModuleBaseAddr + pExportTable->AddressOfNameOrdinals;
		int index = -1;

		char* name = pModuleBaseAddr + names[i];
		if (strcmp(name, pFuncName) == 0)
		{
			index = fh[i];
		}

		if (index != -1)
		{
			dqFunAddr = (ULONG64)pModuleBaseAddr + funcAddress[index];
			break;
		}
	}
	return dqFunAddr;
}

ULONG64 RepairIatTable(PUCHAR pVirtualBuf)
{
	if (!MmIsAddressValid(pVirtualBuf))
	{
		return FALSE;
	}

	ULONG64 dqRet = TRUE;

	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pVirtualBuf;
	PIMAGE_NT_HEADERS64 pNt = (PIMAGE_NT_HEADERS64)(pVirtualBuf + pDos->e_lfanew);

	//没有导入表返回
	if (pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == NULL)
	{
		return -1;
	}


	//导入表地址
	PIMAGE_IMPORT_DESCRIPTOR pImportTable = (PIMAGE_IMPORT_DESCRIPTOR)(pVirtualBuf + pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	IMAGE_IMPORT_DESCRIPTOR ZeroImportTable = { 0 };

	while (memcmp(pImportTable, &ZeroImportTable, sizeof(IMAGE_IMPORT_DESCRIPTOR)) != 0)
	{
		if (pImportTable->FirstThunk != NULL)
		{
			PCHAR pModuleName = (PCHAR)((ULONG64)pVirtualBuf + pImportTable->Name);
			if (pModuleName != NULL)
			{
				//获取模块地址
				ULONG64 BaseModuleAddr = QuerySysModule(pModuleName, NULL);
				if (NULL < BaseModuleAddr)
				{
					//判断是否是
					PIMAGE_THUNK_DATA64 pFirstThunk = (PIMAGE_THUNK_DATA64)((ULONG64)pVirtualBuf + pImportTable->FirstThunk);
					PIMAGE_THUNK_DATA64 pThuckName = (PIMAGE_THUNK_DATA64)((ULONG64)pVirtualBuf + pImportTable->OriginalFirstThunk);

					//遍历名称表 
					for (; pThuckName->u1.ForwarderString != NULL; ++pFirstThunk, ++pThuckName)
					{
						PIMAGE_IMPORT_BY_NAME FuncName = (PIMAGE_IMPORT_BY_NAME)((ULONG64)pVirtualBuf + pThuckName->u1.AddressOfData);

						ULONG_PTR func = NULL;
						//判断是否是内核模块
						if (_stricmp(pModuleName, "hal.dll") == 0 ||
							_stricmp(pModuleName, "ntoskrnl.exe") == 0 ||
							_stricmp(pModuleName, "ntkrnlpa.exe") == 0)
						{
							STRING str = { 0 };
							UNICODE_STRING FunNameUStr = { 0 };
							RtlInitString(&str, FuncName->Name);
							RtlAnsiStringToUnicodeString(&FunNameUStr, &str, TRUE);

							func = MmGetSystemRoutineAddress(&FunNameUStr);

							RtlFreeUnicodeString(&FunNameUStr);
						}
						else
						{
							//遍历导出表获取API地址获取函数名  	//注:内核模块中基本没有用序号导出的
							func = ExportTableFuncByName((PUCHAR)BaseModuleAddr, FuncName->Name);
						}

						//有修复
						if (NULL < func)
						{
							pFirstThunk->u1.Function = (ULONG_PTR)func;
						}
						else
						{
							//未找到函数名
							dqRet = FALSE;
							break;
						}
					}
				}
				else
				{
					//未找到模块
					dqRet = FALSE;
					break;
				}
			}
		}
		//获取下一个导入表
		pImportTable = (PIMAGE_IMPORT_DESCRIPTOR)((ULONG64)pImportTable + sizeof(IMAGE_IMPORT_DESCRIPTOR));
	}

	return dqRet;
}

VOID UpdateCookie(PUCHAR imageBuffer)
{
	if (!MmIsAddressValid(imageBuffer))
	{
		return;
	}

	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)imageBuffer;
	PIMAGE_NT_HEADERS64 pNt = (PIMAGE_NT_HEADERS64)(imageBuffer + pDos->e_lfanew);
	PIMAGE_LOAD_CONFIG_DIRECTORY64 config = (PIMAGE_LOAD_CONFIG_DIRECTORY64)(imageBuffer + pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress);
	if (pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress == NULL || !MmIsAddressValid(config))
	{
		return;
	}
	PULONG64 p = config->SecurityCookie;

	if (MmIsAddressValid(p))
	{
		//可随机加个数值 只要 Cookie不等于 2B992DDFA232h 就行

		//INIT : 0000000140005000                               sub_140005000 proc near; CODE XREF : DriverEntry + 10↑p
		//INIT : 0000000140005000 48 8B 05 29 E0 FF FF          mov     rax, cs : __security_cookie
		//INIT : 0000000140005007 48 85 C0                      test    rax, rax
		//INIT : 000000014000500A 74 1B                         jz      short loc_140005027
		//INIT : 000000014000500A
		//INIT : 000000014000500C 48 B9 32 A2 DF 2D 99 2B 00 00 mov     rcx, 2B992DDFA232h
		//INIT : 0000000140005016 48 3B C1                      cmp     rax, rcx
		//INIT : 0000000140005019 74 0C                         jz      short loc_140005027
		//INIT : 0000000140005019
		//INIT : 000000014000501B 48 F7 D0                      not rax
		//INIT : 000000014000501E 48 89 05 13 E0 FF FF          mov     cs : qword_140003038, rax
		//INIT : 0000000140005025 C3                            retn
		*(PULONG64)(p) = *(PULONG64)(p)+10;
	}
}

ULONG64 LoadPE(PUCHAR* pDllBuf, ULONG64 dqNewImageBase)
{
	if (!MmIsAddressValid(pDllBuf) || !MmIsAddressValid(*pDllBuf))
	{
		return FALSE;
	}

	PUCHAR pDllBuf1 = *pDllBuf;

	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pDllBuf1;
	PIMAGE_NT_HEADERS64 pNt = (PIMAGE_NT_HEADERS64)((ULONG64)pDllBuf1 + pDos->e_lfanew);

	//获取内存大小
	ULONG SizeOfImage = pNt->OptionalHeader.SizeOfImage;

	//申请内存
	PUCHAR pDllBase = (PUCHAR)ExAllocatePool(NonPagedPool, SizeOfImage);
	if (NULL == pDllBase)
	{
		return FALSE;
	}
	memset(pDllBase, 0, SizeOfImage);

	//拷贝头
	memcpy_s(pDllBase, SizeOfImage, pDllBuf1, pNt->OptionalHeader.SizeOfHeaders);

	PIMAGE_SECTION_HEADER psection = (PIMAGE_SECTION_HEADER)((ULONG64)&pNt->OptionalHeader + pNt->FileHeader.SizeOfOptionalHeader);


	//拷贝节区
	for (int i = 0; i < pNt->FileHeader.NumberOfSections; i++)
	{
		PVOID64 pDstAddr = pDllBase + psection[i].VirtualAddress;
		PVOID64 pSrcAddr = pDllBuf1 + psection[i].PointerToRawData;

		ULONG64 DstSize = SizeOfImage - psection[i].Misc.VirtualSize - 0x1000;

		if (psection[i].PointerToRawData >= pNt->OptionalHeader.SizeOfHeaders && psection[i].SizeOfRawData > NULL)
		{
			memcpy_s(pDstAddr, DstSize, pSrcAddr, psection[i].SizeOfRawData);
		}
	}

	//修复重定位表
	if (RepairBaseReloc(pDllBase, dqNewImageBase) <= 0)
	{
		ExFreePool(pDllBase);
		return FALSE;
	}

	////修复IAT 未完成
	//if (RepairIatTable(pDllBase) <= 0)
	//{
	//	ExFreePool(pDllBase);
	//	return FALSE;
	//}

	//修复Cooick //这东西暂时有BUG
	//UpdateCookie(pDllBase);


	//if (MmIsAddressValid(pDllBuf))
	//{
	//	if (MmIsAddressValid(*pDllBuf))
	//	{
	//		ExFreePool(*pDllBuf);
	//	}
	//
	//	*pDllBuf = pDllBase;
	//}

	return TRUE;
}

NTSTATUS UnlockFile(PUNICODE_STRING NtPath, PULONG64 OutClosed)
{
	// 由 FindModuleData.c 提供：KTHREAD.PreviousMode 的偏移；用于把当前线程
	// 临时切到 KernelMode，以便 ZwClose 能够关闭受保护或来自其它进程的内核句柄。
	extern int g_Offset_KTHREAD_PreviousMode;

	if (OutClosed) *OutClosed = 0;
	if (NtPath == NULL || NtPath->Buffer == NULL || NtPath->Length == 0)
	{
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS Status = STATUS_SUCCESS;
	PSYSTEM_HANDLE_INFORMATION      Handles = NULL;
	PSYSTEM_HANDLE_TABLE_ENTRY_INFO HandleInfo = NULL;
	PVOID  Buffer = NULL;
	ULONG  BufferSize = 0x20000; // 句柄表很大，起步给大一点减少重试
	ULONG  ReturnLength = 0;
	ULONG64 closed = 0;

	POBJECT_NAME_INFORMATION ObjectNameInfo = ExAllocatePoolWithTag(NonPagedPool, 4096, '1234');
	if (!ObjectNameInfo)
	{
		return STATUS_NO_MEMORY;
	}

retry:
	Buffer = ExAllocatePoolWithTag(NonPagedPool, BufferSize, '1234');
	if (!Buffer)
	{
		ExFreePool(ObjectNameInfo);
		return STATUS_NO_MEMORY;
	}

	Status = ZwQuerySystemInformation(SystemHandleInformation, Buffer, BufferSize, &ReturnLength);
	if (Status == STATUS_INFO_LENGTH_MISMATCH)
	{
		ExFreePool(Buffer);
		BufferSize = ReturnLength + 0x4000;
		goto retry;
	}
	if (!NT_SUCCESS(Status))
	{
		ExFreePool(Buffer);
		ExFreePool(ObjectNameInfo);
		return Status;
	}

	Handles = (PSYSTEM_HANDLE_INFORMATION)Buffer;
	for (ULONG i = 0; i < Handles->NumberOfHandles; i++)
	{
		HandleInfo = &Handles->Handles[i];

		// 仅处理文件对象，先按指针引用过滤类型
		NTSTATUS refStatus = ObReferenceObjectByPointer(
			HandleInfo->Object,
			FILE_ALL_ACCESS,
			*IoFileObjectType,
			KernelMode);
		if (!NT_SUCCESS(refStatus))
			continue;

		ULONG nameLen = 0;
		NTSTATUS nameStatus = ObQueryNameString(HandleInfo->Object, ObjectNameInfo, 4096, &nameLen);
		if (NT_SUCCESS(nameStatus) && ObjectNameInfo->Name.Length > 0)
		{
			// 精确匹配（大小写不敏感）：避免 "*\X\Y" 跨盘符误伤
			if (RtlEqualUnicodeString(NtPath, &ObjectNameInfo->Name, TRUE))
			{
				PFILE_OBJECT pFile = (PFILE_OBJECT)HandleInfo->Object;

				// 先做 image / data section flush。
				// 这是真正"解占用"的关键：对加载为 DLL/EXE 的文件，OS 是通过
				// FILE_OBJECT->SectionObjectPointers 持有 image section 的，
				// 单纯 ZwClose 句柄并不会让 OS 释放 image。MmFlushImageSection
				// 在没有其它 mapping reference 时会清掉 image section，从而让
				// Explorer/Win32 之后能成功 DeleteFile。
				// 注意：对"当前正在运行的进程自己的 EXE" 这里返回 FALSE，
				//      OS 内核硬性保护，不可能在运行期间真删自己。
				BOOLEAN flushed = FALSE;
				if (MmIsAddressValid(pFile) &&
					pFile->SectionObjectPointer != NULL)
				{
					__try
					{
						flushed = MmFlushImageSection(
							pFile->SectionObjectPointer,
							MmFlushForDelete);
					}
					__except (EXCEPTION_EXECUTE_HANDLER)
					{
						flushed = FALSE;
					}
				}

				PEPROCESS Process = NULL;
				NTSTATUS pStatus = PsLookupProcessByProcessId(HandleInfo->UniqueProcessId, &Process);
				if (NT_SUCCESS(pStatus) && Process != NULL)
				{
					// 同步项目其它地方的 attach 安全约束
					if (Process != PsGetCurrentProcess() && IsProcessSafeToAttach((ULONG64)Process))
					{
						KAPC_STATE ApcState = { 0 };
						KeStackAttachProcess(Process, &ApcState);

						// 临时把 PreviousMode 切到 KernelMode，否则 ZwClose
						// 会用 R3 调用方的 PreviousMode 检查句柄保护属性而拒绝
						PCHAR pPrev = NULL;
						CHAR savedPrev = (CHAR)-1;
						if (g_Offset_KTHREAD_PreviousMode > 0)
						{
							pPrev = (PCHAR)((ULONG64)PsGetCurrentThread() + g_Offset_KTHREAD_PreviousMode);
							savedPrev = *pPrev;
							*pPrev = (CHAR)KernelMode;
						}

						NTSTATUS closeStatus = ZwClose(HandleInfo->HandleValue);

						if (pPrev) *pPrev = savedPrev;

						KeUnstackDetachProcess(&ApcState);

						if (NT_SUCCESS(closeStatus)) closed++;
					}
					ObDereferenceObject(Process);
				}

				// 即便没找到对应进程的句柄表（比如 system 进程持有的 image
				// section reference），只要我们成功 flush 了 image section，
				// 也算成功解了一份占用——统计入 closed，UI 才有反馈。
				if (flushed) closed++;
			}
		}

		ObDereferenceObject(HandleInfo->Object);
	}

	ExFreePool(Buffer);
	ExFreePool(ObjectNameInfo);

	if (OutClosed) *OutClosed = closed;
	return STATUS_SUCCESS;
}
