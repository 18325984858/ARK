#pragma once

#include "framework.h"


typedef struct _IMAGE_BUFFER
{
	PVOID64 Buffer;
	SIZE_T BufferSize;
}IMAGE_BUFFER, * PIMAGE_BUFFER;

typedef struct _IMAGE_PE
{
	HMODULE hDllBaseAddr;//存储模块的基地址
	IMAGE_BUFFER buffer;//存储读取到的文件Buffer

	union //标志位
	{
		ULONG64 Flags;
		struct
		{
			ULONG64 IsX64 : 1;			//1:64位 0:32位
			ULONG64 IsMemoryMode : 1;	//1:内存模式 0:文件模式
		};
	};

	PIMAGE_DOS_HEADER PeDos;//Dos头 [MZ]

	union //Nt头 [PE]
	{
		ULONG64 NtHeader;
		PIMAGE_NT_HEADERS32 PeNt32;
		PIMAGE_NT_HEADERS64 PeNt64;
	};

	PIMAGE_FILE_HEADER PeFile;//文件头

	union //可选PE头
	{
		ULONG64 OptionalHeader;
		PIMAGE_OPTIONAL_HEADER32 PeOption32;
		PIMAGE_OPTIONAL_HEADER64 PeOption64;
	};

	PIMAGE_DATA_DIRECTORY PeDataDir;//目录表
	struct
	{
		PIMAGE_EXPORT_DIRECTORY PeExportDir;//获取导出目录表
		PIMAGE_IMPORT_DESCRIPTOR PeImportDir;//获取导入目录表
		PIMAGE_RESOURCE_DIRECTORY PeResourceDir;//获取资源目录表

	};

	PIMAGE_SECTION_HEADER PeSection;//节目录

}IMAGE_PE, * PIMAGE_PE;//X32PE文件结构体

typedef class _LoadPEFile
{
public:
	_LoadPEFile();//初始化
public:

	BOOL _LoadPEFile::LoadSystemDll(PWCHAR FilePath);						//加载内存中的DLL
	BOOL _LoadPEFile::FreeSystemDll();										//释放内存中的DLL
	BOOL _LoadPEFile::ReadUserDefineDllToBuffer(LPCWSTR FilePath);//加载文件中DLL
	BOOL _LoadPEFile::FreeUserDefineDll();					//释放文件中DLL
private:
	BOOL _LoadPEFile::VerifyIsPESigna()						;//验证是否是PE文件
	ULONG64 _LoadPEFile::IsX64PeFile();//验证是否是X64平台程序
	BOOL _LoadPEFile::InitPeDataStruct();//初始化 PE结构体
public:
	VOID _LoadPEFile::EnumNtdllPeExportTable(PLONG64 Psssdt, ULONG64 Size);//遍历导出表
	VOID _LoadPEFile::EnumWin32kPeExportTable(PLONG64 Psssdt, ULONG64 Size);//遍历导出表
private:
	ULONG64 _LoadPEFile::Align(IN DWORD dwSectionAlignment, IN DWORD dwVirtualSize);//求内存节真实大小
	ULONG64 _LoadPEFile::RvaToFoa(ULONG64 Rva);//内存偏移转换文件偏移
	ULONG64 _LoadPEFile::FoaToRva(ULONG64 Foa);//文件偏移转换内存偏移
private:
	IMAGE_PE Pe;
};

using LoadPEFile = _LoadPEFile;
using PLoadPEFile = _LoadPEFile*;
