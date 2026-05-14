#include "LoadPE.h"
#include "../MyDriver64/Struct.h"

_LoadPEFile::_LoadPEFile()
{
	Pe = { 0 };
}

BOOL _LoadPEFile::LoadSystemDll(PWCHAR FilePath)
{
	HMODULE thMoudle = LoadLibraryW(FilePath);
	if (thMoudle != NULL)
	{
		Pe.hDllBaseAddr = thMoudle;
		Pe.IsMemoryMode = 1;
		InitPeDataStruct();
		return TRUE;
	}
	return FALSE;
}

BOOL _LoadPEFile::FreeSystemDll()
{
	if (Pe.IsMemoryMode == 1)
	{
		FreeLibrary(Pe.hDllBaseAddr);
		return TRUE;
	}
	else
	{
		return VirtualFree(Pe.buffer.Buffer, 0, MEM_DECOMMIT | MEM_RELEASE);
	}
	return FALSE;
}

BOOL _LoadPEFile::ReadUserDefineDllToBuffer(LPCWSTR FilePath)
{
	FILE* file = NULL;
	_wfopen_s(&file, FilePath, TEXT("rb"));
	if (file != NULL)
	{
		_fseeki64(file, 0, SEEK_END);
		Pe.buffer.BufferSize = ftell(file);
		fseek(file, 0, SEEK_SET);
		if (Pe.buffer.BufferSize > 0)
		{
			Pe.buffer.Buffer = (PVOID64)VirtualAlloc(NULL, Pe.buffer.BufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (Pe.buffer.Buffer != NULL)
			{
				if (fread_s(Pe.buffer.Buffer, Pe.buffer.BufferSize, 1, Pe.buffer.BufferSize, file))
				{
					fclose(file);
					if (InitPeDataStruct())
					{
						return TRUE;
					}
					return FALSE;
				}
			}
		}
	}
	fclose(file);
	return TRUE;
}

BOOL _LoadPEFile::FreeUserDefineDll()
{
	if (Pe.IsMemoryMode == 0)
	{
		return VirtualFree(Pe.buffer.Buffer, 0, MEM_DECOMMIT | MEM_RELEASE);
	}
	else
	{
		FreeLibrary(Pe.hDllBaseAddr);
		return TRUE;
	}
	return FALSE;
}

ULONG64 _LoadPEFile::IsX64PeFile()
{
	if (VerifyIsPESigna() == TRUE)//验证获取是否有MZ特征
	{
		PIMAGE_NT_HEADERS pNTHeader = (PIMAGE_NT_HEADERS)Pe.NtHeader;
		if (pNTHeader->Signature == IMAGE_NT_SIGNATURE)//验证PE特征
		{
			PIMAGE_OPTIONAL_HEADER pOptional = (PIMAGE_OPTIONAL_HEADER)(ULONG64)pNTHeader + sizeof(ULONG32);
			if (pOptional->Magic != 0x8664)//验证是否是X32程序
			{
				return FALSE;
			}
			Pe.IsX64 = 1;
			return TRUE;
		}
	}
	return (ULONG64)0xFFFFFFFF;
}

BOOL _LoadPEFile::VerifyIsPESigna()
{
	if (Pe.IsMemoryMode == 0x1)
	{
		Pe.PeDos = (PIMAGE_DOS_HEADER)Pe.hDllBaseAddr;
	}
	else
	{
		Pe.PeDos = (PIMAGE_DOS_HEADER)Pe.buffer.Buffer;
	}

	if (*(PSHORT)Pe.PeDos == IMAGE_DOS_SIGNATURE)
	{
		return TRUE;
	}
	return FALSE;
}

BOOL _LoadPEFile::InitPeDataStruct()
{
	if (VerifyIsPESigna())
	{
		Pe.NtHeader = (ULONG64)Pe.PeDos + Pe.PeDos->e_lfanew;
		Pe.PeFile = (PIMAGE_FILE_HEADER)((ULONG64)Pe.NtHeader + 4);
		Pe.OptionalHeader = (ULONG64)Pe.PeFile + IMAGE_SIZEOF_FILE_HEADER;

		if (IsX64PeFile() != (ULONG64)0xFFFFFFFF)
		{
			if (Pe.IsX64 == TRUE)
			{
				//X64
				Pe.PeDataDir = ((PIMAGE_OPTIONAL_HEADER64)Pe.PeOption64)->DataDirectory;
				Pe.PeSection = (PIMAGE_SECTION_HEADER)((ULONG64)Pe.PeOption64 + Pe.PeFile->SizeOfOptionalHeader);
			}
			else
			{
				//X32
				Pe.PeDataDir = ((PIMAGE_OPTIONAL_HEADER32)Pe.PeOption32)->DataDirectory;
				Pe.PeSection = (PIMAGE_SECTION_HEADER)((ULONG64)Pe.PeOption32 + Pe.PeFile->SizeOfOptionalHeader);
			}

			Pe.PeExportDir = (PIMAGE_EXPORT_DIRECTORY)((ULONG64)Pe.PeDos + RvaToFoa(Pe.PeDataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));//获取导出目录表
			Pe.PeImportDir = (PIMAGE_IMPORT_DESCRIPTOR)((ULONG64)Pe.PeDos + RvaToFoa(Pe.PeDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));//获取导入目录表
			Pe.PeResourceDir = (PIMAGE_RESOURCE_DIRECTORY)((ULONG64)Pe.PeDos + RvaToFoa(Pe.PeDataDir[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress));//获取资源目录表
			return TRUE;
		}
	}
	return 0;
}

VOID _LoadPEFile::EnumNtdllPeExportTable(PLONG64 Psssdt, ULONG64 Size)
{
	//获取导出表数据
#define MOV (0xb8)
	if (Pe.PeExportDir != NULL)
	{
		PULONG64 AddressOfNamesfoa = (PULONG64)((ULONG64)Pe.PeDos + RvaToFoa(Pe.PeExportDir->AddressOfNames));
		PULONG64 AddressOfFunctionsfoa = (PULONG64)((ULONG64)Pe.PeDos + RvaToFoa(Pe.PeExportDir->AddressOfFunctions));
		PULONG64 AddressOfNameOrdinalsfoa = (PULONG64)((ULONG64)Pe.PeDos + RvaToFoa(Pe.PeExportDir->AddressOfNameOrdinals));
		DWORD TableBaseOrdinal = Pe.PeExportDir->Base;
		HMODULE hNtdll = LoadLibraryW(L"ntdll.dll");

		if (hNtdll == NULL)
		{
			AfxMessageBox(TEXT("加载ntdll.dll动态库失败!"));
		}
		for (ULONG64 index = 0; index < Pe.PeExportDir->NumberOfNames; index++)
		{
			ULONG32 ServiceNumber = 0;
			USHORT AddressOfNameOrdinalsfoaindex = *(PUSHORT)((ULONG64)AddressOfNameOrdinalsfoa + (index * 0x2));//获取函数导出序号
			ULONG32 IndexBaseOrdinal = TableBaseOrdinal + AddressOfNameOrdinalsfoaindex;//Ordinals 真实序号
			ULONG32 AddressOfFunctionsfoaindex = *(PULONG32)((ULONG64)AddressOfFunctionsfoa + (AddressOfNameOrdinalsfoaindex * 0x4)); //真实序号函数地址
			PCHAR FunName = (PCHAR)((ULONG64)Pe.PeDos + RvaToFoa(*(PULONG32)((ULONG64)AddressOfNamesfoa + (index * 0x4))));//获取函数导出的名称

			if (FunName[0] == 'N' && FunName[1] == 't')
			{
				PUCHAR NtFunBaseAddr = (PUCHAR)((ULONG64)Pe.PeDos + RvaToFoa(AddressOfFunctionsfoaindex));//求出真实函数地址
				ServiceNumber = -1;

				//循环查找 MOV RAX,服务号 或 MOV EAX,服务号
				for (int i = 0; i < 10; i++)
				{
					if (*(NtFunBaseAddr + i) == MOV)
					{
						ServiceNumber = *(PULONG32)(NtFunBaseAddr + i + 1);//获取NT函数服务号
						break;
					}
				}

				ServiceNumber = ServiceNumber & 0xFFF;
				ULONG64 Addr = (ULONG64)GetProcAddress(hNtdll, FunName);
				if (Psssdt != NULL && ServiceNumber != -1)
				{

					PCLIST_ENTRY pList = &((PCSsdtInfo)Psssdt)->List.List;
					do
					{
						PCSsdtInfo pSsdtInfo = (PCSsdtInfo)pList;

						//比较序号是否和SSDT表索引一样
						if (pSsdtInfo->NumberOrder == ServiceNumber)
						{
							pSsdtInfo->ServiceNumber = ServiceNumber;//获取函数服务号
							pSsdtInfo->UsFunAddr = Addr;
							AsciitoUniCodeString(FunName, pSsdtInfo->FunName);//转换函数名Unicode
							break;
						}
						//下一项
						pList = pList->Blink;
					} while (pList != &((PCSsdtInfo)Psssdt)->List.List);
				}
			}
		}
		if (hNtdll != NULL)
		{
			FreeLibrary(hNtdll);
		}
	}
}


VOID _LoadPEFile::EnumWin32kPeExportTable(PLONG64 Psssdt, ULONG64 Size)
{

#define MOV (0xb8)
	if (Pe.PeExportDir != NULL)
	{
		PULONG64 AddressOfNamesfoa = (PULONG64)((ULONG64)Pe.PeDos + RvaToFoa(Pe.PeExportDir->AddressOfNames));
		PULONG64 AddressOfFunctionsfoa = (PULONG64)((ULONG64)Pe.PeDos + RvaToFoa(Pe.PeExportDir->AddressOfFunctions));
		PULONG64 AddressOfNameOrdinalsfoa = (PULONG64)((ULONG64)Pe.PeDos + RvaToFoa(Pe.PeExportDir->AddressOfNameOrdinals));
		DWORD TableBaseOrdinal = Pe.PeExportDir->Base;

		HMODULE hWin32tdll = LoadLibraryW(L"win32u.dll");

		if (hWin32tdll == NULL)
		{
			AfxMessageBox(TEXT("加载win32u.dll动态库失败!"));
		}

		for (ULONG64 index = 0; index < Pe.PeExportDir->NumberOfNames; index++)
		{
			ULONG32 ServiceNumber = 0;
			USHORT AddressOfNameOrdinalsfoaindex = *(PUSHORT)((ULONG64)AddressOfNameOrdinalsfoa + (index * 0x2));//获取函数导出序号
			ULONG32 IndexBaseOrdinal = TableBaseOrdinal + AddressOfNameOrdinalsfoaindex;//Ordinals 真实序号
			ULONG32 AddressOfFunctionsfoaindex = *(PULONG32)((ULONG64)AddressOfFunctionsfoa + (AddressOfNameOrdinalsfoaindex * 0x4)); //真实序号函数地址
			PCHAR SrcFunName = (PCHAR)((ULONG64)Pe.PeDos + RvaToFoa(*(PULONG32)((ULONG64)AddressOfNamesfoa + (index * 0x4))));//获取函数导出的名称
			PCHAR DstFunName = "nt";
			if (SrcFunName[0] == 'N' && SrcFunName[1] == 't')
			{
				PUCHAR NtFunBaseAddr = (PUCHAR)((ULONG64)Pe.PeDos + RvaToFoa(AddressOfFunctionsfoaindex));//求出真实函数地址
				ServiceNumber = -1;

				//循环查找 MOV RAX,服务号 或 MOV EAX,服务号
				for (int i = 0; i < 10; i++)
				{
					if (*(NtFunBaseAddr + i) == MOV)
					{
						ServiceNumber = *(PULONG32)(NtFunBaseAddr + i + 1);//获取NT函数服务号
						break;
					}
				}

				ServiceNumber = ServiceNumber & 0xFFF;
				ULONG64 Addr = (ULONG64)GetProcAddress(hWin32tdll, SrcFunName);
				if (Psssdt != NULL && ServiceNumber != -1)
				{

					PCLIST_ENTRY pList = &((PCSsdtInfo)Psssdt)->List.List;
					do
					{
						PCSsdtInfo pSsdtInfo = (PCSsdtInfo)pList;

						//比较序号是否和SSDT表索引一样
						if (pSsdtInfo->NumberOrder == ServiceNumber)
						{
							pSsdtInfo->ServiceNumber = ServiceNumber;//获取函数服务号
							pSsdtInfo->UsFunAddr = Addr;
							AsciitoUniCodeString(SrcFunName, pSsdtInfo->FunName);//转换函数名Unicode
							break;
						}
						//下一项
						pList = pList->Blink;
					} while (pList != &((PCSsdtInfo)Psssdt)->List.List);
				}
			}
		}
		if (hWin32tdll != NULL)
		{
			FreeLibrary(hWin32tdll);
		}
	}
	return;
}


ULONG64 _LoadPEFile::RvaToFoa(ULONG64 Rva)
{
	//1.如何文件(FOA)对齐和内存(RVA)对齐一样则不需要转换 
	//2.当内存(RVA)偏移 不在节(Section)段的时候不需要转换
	if (Pe.IsX64 == 1)
	{//X64
		if ((Pe.PeOption64->FileAlignment != Pe.PeOption64->SectionAlignment) && (Rva > Pe.PeOption64->SizeOfHeaders))
		{
			for (ULONG64 index = 0; index < Pe.PeFile->NumberOfSections; index++)
			{
				ULONG64 SectionAlignment = Align(Pe.PeOption64->SectionAlignment, Pe.PeSection[index].Misc.VirtualSize) + Pe.PeSection[index].VirtualAddress;
				if (Pe.PeSection[index].VirtualAddress <= Rva && Rva < SectionAlignment)
				{
					return Rva - Pe.PeSection[index].VirtualAddress + Pe.PeSection[index].PointerToRawData;
				}
			}
		}
		return Rva;
	}
	else
	{//X32
		if ((Pe.PeOption32->FileAlignment != Pe.PeOption32->SectionAlignment) && (Rva > Pe.PeOption32->SizeOfHeaders))
		{
			for (ULONG64 index = 0; index < Pe.PeFile->NumberOfSections; index++)
			{
				ULONG64 SectionAlignment = Align(Pe.PeOption32->SectionAlignment, Pe.PeSection[index].Misc.VirtualSize) + Pe.PeSection[index].VirtualAddress;
				if (Pe.PeSection[index].VirtualAddress <= Rva && Rva < SectionAlignment)
				{
					return Rva - Pe.PeSection[index].VirtualAddress + Pe.PeSection[index].PointerToRawData;
				}
			}
		}
		return Rva;
	}
	return 0;
}

ULONG64 _LoadPEFile::FoaToRva(ULONG64 Foa)
{
	//1.如何文件(FOA)对齐和内存(RVA)对齐一样则不需要转换 
	//2.当内存(RVA)偏移 不在节(Section)段的时候不需要转换
	if (Pe.IsX64 == 1)
	{//X64
		if ((Pe.PeOption64->FileAlignment != Pe.PeOption64->SectionAlignment) && (Foa > Pe.PeOption64->SizeOfHeaders))
		{
			for (ULONG64 index = 0; index < Pe.PeFile->NumberOfSections; index++)
			{
				if (Pe.PeSection[index].PointerToRawData <= Foa && Foa < (Pe.PeSection[index].SizeOfRawData + Pe.PeSection[index].PointerToRawData))
				{
					return Foa - Pe.PeSection[index].PointerToRawData + Pe.PeSection[index].VirtualAddress;
				}
			}
		}
		return Foa;
	}
	else
	{//X32
		if ((Pe.PeOption32->FileAlignment != Pe.PeOption32->SectionAlignment) && (Foa > Pe.PeOption32->SizeOfHeaders))
		{
			for (ULONG64 index = 0; index < Pe.PeFile->NumberOfSections; index++)
			{
				if (Pe.PeSection[index].PointerToRawData <= Foa && Foa < (Pe.PeSection[index].SizeOfRawData + Pe.PeSection[index].PointerToRawData))
				{
					return Foa - Pe.PeSection[index].PointerToRawData + Pe.PeSection[index].VirtualAddress;
				}
			}
		}
		return Foa;
	}
	return 0;
}

ULONG64 _LoadPEFile::Align(IN DWORD dwSectionAlignment, IN DWORD dwVirtualSize)
{
	//求虚拟内存节对齐后的大小
	DWORD dwAlignmentNumber = 0;
	if (dwSectionAlignment >= dwVirtualSize)
	{
		return dwSectionAlignment;
	}

	DWORD dwDivNumber = dwVirtualSize / dwSectionAlignment;
	if (dwVirtualSize % dwSectionAlignment)
	{
		dwDivNumber++;
	}
	dwAlignmentNumber = dwDivNumber * dwSectionAlignment;
	return dwAlignmentNumber;
}