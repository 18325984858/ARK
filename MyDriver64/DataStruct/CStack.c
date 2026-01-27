#include "CStack.h"

NTSTATUS InitStack(PCStack MyStack)
{
	NTSTATUS nStatus = 0;
	ULONG64 StackSize = sizeof(ElemType) * STACK_INIT_SIZE;
	PVOID64 Addr = 0;
	nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &MyStack->m_Data, NULL, &StackSize, MEM_COMMIT, PAGE_READWRITE);

	if (!NT_SUCCESS(nStatus))
	{
		//DbgPrint("栈空间初始化失败!.....nStatus : [%I64X]\n", nStatus);
		return nStatus;
	}

	RtlZeroMemory(MyStack->m_Data, StackSize);
	MyStack->m_Capacity = STACK_INIT_SIZE;
	MyStack->m_Top = 0;

	return nStatus;
}

BOOL GetStackIsFull(PCStack MyStack)
{
	if (MmIsAddressValid(MyStack) != TRUE)
	{
		return TRUE;
	}

	return MyStack->m_Top >= MyStack->m_Capacity ? TRUE : FALSE;
}

BOOL GetStackIsNull(PCStack MyStack)
{
	if (MmIsAddressValid(MyStack) != TRUE)
	{
		return TRUE;
	}

	return MyStack->m_Top == 0 ? TRUE : FALSE;
}

BOOL PushStack(PCStack MyStack, ElemType Base)
{
	if (MmIsAddressValid(MyStack) != TRUE)
	{
		return FALSE;
	}

	if (GetStackIsFull(MyStack))
	{
		if (IncStackSpace(MyStack))
		{
			DbgPrint("栈空间已满!....\n");
			return FALSE;
		}
	}
	MyStack->m_Data[MyStack->m_Top++] = Base;

	return TRUE;
}

BOOL PopStack(PCStack MyStack)
{
	if (MmIsAddressValid(MyStack))
	{
		if (GetStackIsNull(MyStack))
		{
			DbgPrint("栈空间以空!....\n");
			return TRUE;
		}

		MyStack->m_Top--;
		return TRUE;
	}
	return FALSE;
}

ElemType GetStackTopData(PCStack MyStack, ElemType* OutData)
{
	if (MmIsAddressValid(MyStack))
	{
		if (GetStackIsNull(MyStack) != TRUE)
		{
			if (MmIsAddressValid(OutData))
			{
				*OutData = MyStack->m_Data[MyStack->m_Top - 1];		/*返回栈顶-1的数据*/
			}
			return MyStack->m_Data[MyStack->m_Top - 1];
		}
	}
	return FALSE;
}

VOID ShowStack(PCStack MyStack)
{
	if (MmIsAddressValid(MyStack->m_Data))
	{
		DbgPrint("Stack--> ");
		for (int i = MyStack->m_Top - 1; i >= 0; i--)
		{
			DbgPrint(" [%I64X]-->", MyStack->m_Data[i]);
		}
		DbgPrint(" Nul.\n");
	}
}

ULONG64 GetStackLength(PCStack MyStack)
{
	if (MmIsAddressValid(MyStack))
	{
		return MyStack->m_Top;
	}
	return FALSE;
}

ULONG64 GetStackSize(PCStack MyStack)
{
	if (MmIsAddressValid(MyStack))
	{
		return MyStack->m_Capacity;
	}
	return FALSE;
}

BOOL ClearStack(PCStack MyStack)
{
	if (MmIsAddressValid(MyStack))
	{
		if (MmIsAddressValid(MyStack->m_Data))
		{
			RtlZeroMemory(MyStack->m_Data, sizeof(ElemType) * STACK_INIT_SIZE);
			MyStack->m_Top = 0;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL DestroyStack(PCStack MyStack)
{
	ULONG64 FreeSize = 0;
	if (MmIsAddressValid(MyStack))
	{
		if (MmIsAddressValid(MyStack->m_Data))
		{
			ZwFreeVirtualMemory(NtCurrentProcess(), &MyStack->m_Data, &FreeSize, MEM_RELEASE);	/*参数二,参数三不能直接写0,需要变量存储*/
			MyStack->m_Data = NULL;
			MyStack->m_Top = 0;
			MyStack->m_Capacity = 0;
			return TRUE;
		}
	}
	return FALSE;
}

NTSTATUS IncStackSpace(PCStack MyStack)
{
	ULONG64 SrcSize = 0;
	ULONG64 DstSize = 0;
	NTSTATUS nStatus = 0;
	PVOID64 DstAddr = 0;
	ULONG64 SrcFreeSize = 0;

	if (MmIsAddressValid(MyStack))
	{
		SrcSize = MyStack->m_Capacity * sizeof(ElemType);
		DstSize = SrcSize + (sizeof(ElemType) * STACK_INIT_SIZE);

		nStatus = ZwAllocateVirtualMemory(NtCurrentProcess(), &DstAddr, NULL, &DstSize, MEM_COMMIT, PAGE_READWRITE);
		if (!NT_SUCCESS(nStatus))
		{
			DbgPrint("栈空间申请失败,内存不足! : [%I64X].....\n", nStatus);
			return nStatus;
		}
		RtlZeroMemory(DstAddr, DstSize);
		memmove_s(DstAddr, DstSize, MyStack->m_Data, SrcSize);
		ZwFreeVirtualMemory(NtCurrentProcess(), &MyStack->m_Data, &SrcFreeSize, MEM_RELEASE);
		MyStack->m_Data = DstAddr;
		MyStack->m_Capacity += STACK_INIT_SIZE;
	}
	return nStatus;
}

