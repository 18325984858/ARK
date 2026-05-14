#pragma once
#include "../DefineArea.h" /*公共头文件*/

#define ElemType ULONG64															/*数据类型*/

typedef struct CListNode CListNode, * PCListNode;
typedef struct CList CList, * PCList;

typedef DWORD(_fastcall* CMPFUNPTRCALLBACK)(ULONG64, ULONG64);

struct CListNode
{
	ElemType m_Data;																/*存储当前指针的数据*/
	CListNode* m_pFront;															/*存储当前指针的前一个指针*/
	CListNode* m_pNext;																/*存储当前指针的后一个指针*/
};

struct CList																		/*双向循环链表*/
{
	PCListNode m_pHead;																/*存储头结点*/
	PCListNode m_pTrail;															/*存储尾结点*/
	ULONG64 m_Size;																	/*存储表大小*/
};

PCListNode _ByListNode(ElemType data);												/*申请节点函数*/
int IsVerifyListNode(PCList plist, PCListNode pNode);								/*验证节点是否存在*/
int GetListSize(PCList plist);														/*返回当前链表的大小*/
void InitDoubleLoopList(PCList plist);												/*初始化双向循环链表*/
void InsertHeadDoubleLoopList(PCList plist, ElemType data);							/*头插双向循环链表*/
PCListNode InsertTrailDoubleLoopList(PCList plist, ElemType data);						/*尾插双向循环链表*/
void InsertPosFrontDoubleLoopList(PCList plist, PCListNode pNode, ElemType data);	/*按指定位置前面插入双向循环链表中*/
void InsertPosNextDoubleLoopList(PCList plist, PCListNode pNode, ElemType data);	/*按指定位置后面插入双向循环链表中*/

ElemType PopHeadDoubleLoopList(PCList plist);										/*返回头部的数据并删除头部数据*/
ElemType PopTrailDoubleLoopList(PCList plist);										/*返回尾部的数据并删除尾部数据*/

PCListNode FindListNode(PCList plist, ElemType data);								/*根据数据查找链表节点,返回节点*/

ElemType DeleteHeadDoubleLoopList(PCList plist);										/*头部删除,返回值将返回Data数据*/
ElemType DeleteTrailDoubleLoopList(PCList plist);									/*尾部删除,返回值将返回Data数据*/
ElemType DeletePosDoubleLoopList(PCList plist, PCListNode pNode);					/*按位置删除,返回值将返回Data数据*/

void PrintfDoubleLoopList(PCList plist);											/*打印循环链表*/

PCListNode FindListNodeEx(PCList plist, ULONG64 nAddrType);							/*根据数据查找链表节点,返回节点*/

/*回调函数须知 0表示两数等于,1表示大于,-1表示小于,其余表示无效*/
PCListNode FindListNodeEx1(PCList plist, ULONG64 nAddrType, CMPFUNPTRCALLBACK pfun);/*根据数据查找链表节点,返回节点*/

typedef VOID(__fastcall* pDestroyListCallBack)(PVOID64);


void DestroyList(PCList plist, pDestroyListCallBack pCall);														/*摧毁链表*/

//void ClearDoubleLoopList(PCList plist);											/*清空双向循环链表*/