#pragma once
#include "../DefineArea.h"


#define ElemType ULONG64										/*数据类型*/
#define STACK_INIT_SIZE 0xF										/*栈的容量*/

typedef struct CStack											/*栈结构体*/
{
	ElemType* m_Data;											/*存储栈数据*/
	ULONG64 m_Capacity;											/*存储栈的最大容量*/
	ULONG64 m_Top;												/*存储指向当前栈的栈顶位置*/
}CStack, * PCStack;

NTSTATUS InitStack(PCStack MyStack);							/*初始化栈函数*/
BOOL GetStackIsFull(PCStack MyStack);							/*判断栈是否满*/
BOOL GetStackIsNull(PCStack MyStack);							/*判断栈是否空*/

BOOL PushStack(PCStack MyStack, ElemType Base);					/*插入数据*/
BOOL PopStack(PCStack MyStack);									/*出栈*/
ElemType GetStackTopData(PCStack MyStack, ElemType* OutData);	/*返回栈顶数据*/
VOID ShowStack(PCStack MyStack);								/*显示数据*/
ULONG64 GetStackLength(PCStack MyStack);						/*返回栈长度大小*/
ULONG64 GetStackSize(PCStack MyStack);							/*获取栈总空间*/
BOOL ClearStack(PCStack MyStack);								/*清扫栈*/
BOOL DestroyStack(PCStack MyStack);								/*摧毁栈*/
NTSTATUS IncStackSpace(PCStack MyStack);						/*空间不足时,向后延续申请*/