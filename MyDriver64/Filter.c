#include "Filter.h"
#include <ntimage.h>

FLT_POSTOP_CALLBACK_STATUS
MyFltCreatePostOperationCallBack(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	FLT_PREOP_CALLBACK_STATUS RetStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

	//获取文件信息
	PFLT_FILE_NAME_INFORMATION pFileInfo = NULL;
	NTSTATUS st = FltGetFileNameInformation(Data, FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_NORMALIZED, &pFileInfo);
	if (!NT_SUCCESS(st))
	{
		return RetStatus;
	}
	//分析内容
	st = FltParseFileNameInformation(pFileInfo);
	if (!NT_SUCCESS(st))
	{
		return RetStatus;
	}

	//获取文件对象
	PFILE_OBJECT TargetFileObject = Data->Iopb->TargetFileObject;
	if (TargetFileObject->Type != 5)//判断是否是文件
	{
		return RetStatus;
	}

#define EXE_FILE_INFO (L"*.EXE")
	UNICODE_STRING pDstFilePath = { 0 };
	RtlInitUnicodeString(&pDstFilePath, EXE_FILE_INFO);
#define EXE_FILE_INFO1 (L"*.TXT")
	UNICODE_STRING pDstFilePath1 = { 0 };
	RtlInitUnicodeString(&pDstFilePath1, EXE_FILE_INFO1);
#define EXE_FILE_INFO2 (L"*.DLL")
	UNICODE_STRING pDstFilePath2 = { 0 };
	RtlInitUnicodeString(&pDstFilePath2, EXE_FILE_INFO2);
#define EXE_FILE_INFO3 (L"*.SYS")
	UNICODE_STRING pDstFilePath3 = { 0 };
	RtlInitUnicodeString(&pDstFilePath3, EXE_FILE_INFO3);
	
	//比较文件后缀是否是要过滤的文件
	if (FsRtlIsNameInExpression(&pDstFilePath, &TargetFileObject->FileName, TRUE, NULL) == FALSE ||
		FsRtlIsNameInExpression(&pDstFilePath1, &TargetFileObject->FileName, TRUE, NULL) == FALSE ||
		FsRtlIsNameInExpression(&pDstFilePath2, &TargetFileObject->FileName, TRUE, NULL) == FALSE ||
		FsRtlIsNameInExpression(&pDstFilePath3, &TargetFileObject->FileName, TRUE, NULL) == FALSE)
	{
		ULONG64 safeToOpen = 0;

		//查找是否是PE文件
		ScannerpScanFileInUserMode(FltObjects->Instance,
			FltObjects->FileObject,
			&safeToOpen);

		if (!safeToOpen)
		{
			//是PE文件

			MyDbgPrintfEx("检测到加载了PE文件!\n");

		}

	}
	//释放掉文件信息
	FltReleaseFileNameInformation(pFileInfo);
	return RetStatus;
}


NTSTATUS
ScannerpScanFileInUserMode(
	__in PFLT_INSTANCE Instance,
	__in PFILE_OBJECT FileObject,
	__out PBOOLEAN SafeToOpen
)
{
#define SCANNER_READ_BUFFER_SIZE 1024

	if (!MmIsAddressValid(FileObject) || !MmIsAddressValid(SafeToOpen))
	{
		return FALSE;
	}
	*SafeToOpen = TRUE;

	PVOID buffer = NULL;
	PFLT_VOLUME volume = NULL;
	NTSTATUS status = STATUS_SUCCESS;

	try
	{
		//返回附加到过滤器卷的信息
		status = FltGetVolumeFromInstance(Instance, &volume);
		if (!NT_SUCCESS(status)) {

			leave;
		}
		//返回卷属性
		FLT_VOLUME_PROPERTIES volumeProps = { 0 };
		ULONG64 length = 0;
		status = FltGetVolumeProperties(volume,
			&volumeProps,
			sizeof(volumeProps),
			&length);

		if (NT_ERROR(status))
		{
			leave;
		}

		length = max(SCANNER_READ_BUFFER_SIZE, volumeProps.SectorSize);


		//申请空间 存放读取到的文件信息
		buffer = FltAllocatePoolAlignedWithTag(Instance,
			NonPagedPool,
			length,
			'nacS');

		if (NULL == buffer)
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
			leave;
		}
		RtlZeroMemory(buffer, length);

		//读取文件
		LARGE_INTEGER offset = { 0 };
		ULONG64 bytesRead = 0;
		status = FltReadFile(Instance,
			FileObject,
			&offset,
			length,
			buffer,
			FLTFL_IO_OPERATION_NON_CACHED |
			FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET,
			&bytesRead,
			NULL,
			NULL);

		//判断是否读取成功
		if (NT_SUCCESS(status) && (0 != bytesRead))
		{
			//判断文件的开头是否是MZ
			if (*(PUSHORT)buffer == 0x5A4D)
			{
				PULONG32 pNtHeader = ((ULONG64)buffer + ((PIMAGE_DOS_HEADER)buffer)->e_lfanew);
				if (*pNtHeader == 0x00004550)
				{
					status = 0;
					*SafeToOpen = FALSE;
				}
			}
		}
	}
	finally
	{
		//释放文件Buf
		if (buffer != NULL)
		{
			FltFreePoolAlignedWithTag(Instance, buffer, 'nacS');
		}

		//释放卷信息
		if (NULL != volume) {

			FltObjectDereference(volume);
		}
	}
	return status;
}
