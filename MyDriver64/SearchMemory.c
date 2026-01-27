#include "DefineArea.h"
#include "KernelStruct.h"
#include "Head.h"
#include <ntimage.h>
#include "Define.h"

ULONG64 FindMoudleInMemoryAddrEx(ULONG64 MoudleAddr, SIZE_T MoudleSize, PVOID SitgCode, PCHAR Mask, PCHAR SectionName /*= ".text"*/)
{
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)MoudleAddr;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return NULL;
	}

	PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(MoudleAddr + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
	{
		return NULL;
	}

	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
	for (SIZE_T i = 0; i < nt->FileHeader.NumberOfSections; i++)
	{
		PIMAGE_SECTION_HEADER p = &section[i];

		if (strstr((PCHAR)p->Name, SectionName))
		{
			ULONG64 result = FindPattern(MoudleAddr + p->VirtualAddress, p->Misc.VirtualSize, SitgCode, Mask);
			if (result && MmIsAddressValid(result))
			{
				return result;
			}
		}
	}
	return NULL;
}

ULONG64 FindPattern(IN ULONG64 SectionAddr, IN SIZE_T nSize, IN PVOID SitgCode, IN PCHAR Mask)
{
	nSize = nSize - (SIZE_T)strlen(Mask);

	for (SIZE_T i = 0; i < nSize; i++)
	{
		if (PatternCheck(SectionAddr + i, SitgCode, Mask))
		{
			return SectionAddr + i;
		}
	}
	return NULL;
}

UCHAR PatternCheck(IN ULONG64 SectionAddr, IN PVOID SitgCode, IN PCHAR Mask)
{
	size_t m_len = strlen(Mask);

	for (size_t i = 0; i < m_len; i++)
	{
		if (((PCHAR)SectionAddr)[i] == ((PCHAR)SitgCode)[i] || ((PCHAR)Mask)[i] == '?')
		{
			continue;
		}
		else
		{
			return FALSE;
		}
	}
	return TRUE;
}

ULONG64 InitSystemPteBase()
{
	typedef PVOID(__fastcall* MMGETVIRTUALFORPHYSICALMAIN)(PHYSICAL_ADDRESS PhysicalAddress);//MmGetVirtualForPhysical

	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	UNICODE_STRING MmGetVirtualForPhysicalName = { 0 };
	PHYSICAL_ADDRESS pml4t = { 0 };
	PULONG64 pml4t_va = 0;
	ULONG index = 0;
	ULONG64 NtBase = 0, PteBase = 0, PdeBase = 0, PpeBase = 0, PxeBase = 0;
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//变量初始化区域
	RtlInitUnicodeString(&MmGetVirtualForPhysicalName, L"MmGetVirtualForPhysical");
	pml4t.QuadPart = __readcr3(); /*获取CR3寄存器*/
	MMGETVIRTUALFORPHYSICALMAIN MyMmGetVirtualForPhysical = (MMGETVIRTUALFORPHYSICALMAIN)MmGetSystemRoutineAddress(&MmGetVirtualForPhysicalName);

	if (!MyMmGetVirtualForPhysical)
	{
		return NULL;
	}

	/*映射CR3物理地址*/
	pml4t_va = (PULONG64)MyMmGetVirtualForPhysical(pml4t);

	//MyDbgPrintfEx("MmGetVirtualForPhysical:%I64X\n", pml4t_va);

	pml4t_va = (PULONG64)((ULONG64)pml4t_va & 0xFFFFFFFFFFFFF000);		//删除索引获取物理页帧
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//项目需求区域
	if (pml4t_va)
	{
		for (index = 0; index < 512; index++)		//一个页0x1000 //一条数据8字节 //一页有512条数据
		{
			if (((ULONG64)(pml4t_va[index]) & (ULONG64)0x0000FFFFFFFFF000) == (pml4t.QuadPart & 0x0000FFFFFFFFF000))
			{
				//获取当前系统物理地址的Base
				NtBase = (ULONG64)((ULONG64)(index + (ULONG64)0x1FFFE00) << 39);
				g_SystemAddrBase.PteBase = NtBase;
				g_SystemAddrBase.PdeBase = NtBase + ((ULONG64)index << 30);
				g_SystemAddrBase.PpeBase = NtBase + ((ULONG64)index << 30) + ((ULONG64)index << 21);
				g_SystemAddrBase.PxeBase = g_SystemAddrBase.PpeBase + ((ULONG64)index << 12);
				return index;
			}
		}
	}
	return NULL;
	////////////////////////////////////////////////////////////////////////
}

ULONG64 GetPageBaseLinearAddr(IN ULONG64 SrcLinearAddress, PCPageAddrInfo OutData)
{
	//验证参数是否正确
	if (MmIsAddressValid(SrcLinearAddress))
	{
		OutData->PxeBase = g_SystemAddrBase.PxeBase + ((SrcLinearAddress & 0xFF8000000000) >> 39) << 3;
		OutData->PpeBase = g_SystemAddrBase.PpeBase + ((SrcLinearAddress & 0xFFFFC0000000) >> 30) << 3;
		OutData->PdeBase = g_SystemAddrBase.PdeBase + ((SrcLinearAddress & 0xFFFFFFE00000) >> 21) << 3;
		OutData->PteBase = g_SystemAddrBase.PteBase + ((SrcLinearAddress & 0xFFFFFFFFF000) >> 12) << 3;
		return TRUE;
	}
	return FALSE;
}

ULONG64 MmIsAddressValidEx0(ULONG64 SrcLinearAddress)
{
	if (g_SystemAddrBase.PteBase == NULL)
	{
		return FALSE;
	}


#define Kernel_Addr	0xFFFF	/*内核模式地址*/
#define User_Addr	0x0000	/*用户模式地址*/

	ULONG64 Val = ((ULONG64)SrcLinearAddress >> 47) & 0xFFFF;
	//验证参数是否正确 //FFFF800000000000情况 和0000700000000000情况
	if ((Val == Kernel_Addr) || (Val == User_Addr))
	{
		//__debugbreak();
		//根据页目录表基址,获线性地址
		ULONG64 AddrInfo[4] = { 0 };
		AddrInfo[0] = ((SrcLinearAddress >> 9) & 0x7FFFFFFFF8i64) + g_SystemAddrBase.PteBase;
		AddrInfo[1] = ((AddrInfo[0] >> 9) & 0x7FFFFFFFF8i64) + g_SystemAddrBase.PteBase;
		AddrInfo[2] = ((AddrInfo[1] >> 9) & 0x7FFFFFFFF8i64) + g_SystemAddrBase.PteBase;
		AddrInfo[3] = ((AddrInfo[2] >> 9) & 0x7FFFFFFFF8i64) + g_SystemAddrBase.PteBase;

		for (int i = (sizeof(AddrInfo) / sizeof(ULONG64) - 1); i >= 0; i--)
		{
			//获取当前线程所属进程
#define GetCurThreadProcess() (*(PULONG64)((ULONG64)KeGetCurrentThread() + _KTHREAD_ApcState + _KAPC_STATE_Process))
			PULONG64 Addr = AddrInfo[i];
			ULONG64 Atttrubute = *Addr;
			UCHAR CurThreadAddressPolicy = *(PUCHAR)((ULONG64)GetCurThreadProcess() + _EPROCESS_AddressPolicy);
			if ((Addr >= g_SystemAddrBase.PxeBase) &&
				(Addr <= g_SystemAddrBase.PxeBase + 0x7F8) &&
				(*MiFlags & 0xC00000) != 0 &&
				(CurThreadAddressPolicy != 1))
			{
				//判断P位是否有效
				if (Atttrubute & 1 == 0)
				{
					return FALSE;
				}

				//0x20 A位表示是否访问了这个页 D位表示是否写入了这个页 U/S位为0则不允许用户层访问
				if ((Atttrubute & 0x20) == 0 || (Atttrubute & 0x42) == 0)
				{
					//获取EPROCESS中ShadowMapping
#define ShadowMappingVal (_EPROCESS_Vm+_MMSUPPORT_FULL_Shared+_MMSUPPORT_SHARED_ShadowMapping)
					PULONG64 ShadowMapping = *(PULONG64)((ULONG64)GetCurThreadProcess() + ShadowMappingVal);
					if (ShadowMapping)
					{
						ULONG64 PhysicalAddr = ShadowMapping[((ULONG64)Addr >> 3) & 0x1FF];
						ULONG Flags = Atttrubute | 0x20;//A位表示是否访问了这个页
						if ((PhysicalAddr & 0x20) == 0)
						{
							Flags = (UCHAR)Atttrubute;
						}

						*(PUCHAR)Atttrubute = Flags;

						if ((PhysicalAddr & 0x42) != 0)
						{
							*(PUCHAR)Atttrubute = Flags | 0x42;
						}
					}
				}
			}

			//判断P位是否是有效页
			if ((*Addr & 1) == 0)
			{
				return FALSE;
			}
			//判断页面大小 PS位
			if ((*Addr & 0x80) != 0)
			{
				break;
			}
			//当以上条件都不满足时返回真,且循环完
			if (!i)
			{
				return TRUE;
			}
		}

		if ((SrcLinearAddress < g_SystemAddrBase.PteBase) || (SrcLinearAddress > (g_SystemAddrBase.PteBase + 0x7FFFFFFFFF)))
		{
			return TRUE;
		}
	}
	return FALSE;
}

ULONG64 MmIsAddressValidEx1(ULONG64 SrcLinearAddress, ULONG64 nLen)
{
#define Kernel_Addr	0xFFFF	/*内核模式地址*/
#define User_Addr	0x0000	/*用户模式地址*/

	ULONG64 Val = ((ULONG64)SrcLinearAddress >> 47) & 0xFFFF;
	//验证参数是否正确 //FFFF800000000000情况 和0000700000000000情况
	if ((Val == Kernel_Addr) || (Val == User_Addr))
	{
		ULONG64 SrcStartAddr = SrcLinearAddress & ~0xFFF;				//或取开始地址的开始地址
		ULONG64 EndStartAddr = (SrcLinearAddress + nLen) & ~0xFFF;		//获取结束地址的开始地址
		ULONG64 PoolIndex = ((EndStartAddr - SrcStartAddr) >> 12/*右移12位  获取跨了几个页*/) + 1/*差值大于等于0x1000多+1*/;

		for (int i = 0; i < PoolIndex; i++, SrcLinearAddress += 0x1000 /*指向下一个页*/)
		{
			if (!MmIsAddressValidEx0(SrcLinearAddress))
			{
				return FALSE;
			}
		}
		return TRUE;
	}
	return FALSE;
}
