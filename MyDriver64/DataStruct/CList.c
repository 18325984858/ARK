#include "CList.h"

PCListNode _ByListNode(ElemType data)
{
	PVOID p = NULL;															/*接收申请的地址*/
	ULONG64 nSpaceSize = sizeof(CListNode);									/*要申请空间的大小*/

	p = ExAllocatePool(PagedPool, nSpaceSize);

	if (!MmIsAddressValid(p))
	{
		return NULL;
	}

	RtlZeroMemory(p, sizeof(CListNode));
	((PCListNode)p)->m_Data = data;
	((PCListNode)p)->m_pFront = NULL;
	((PCListNode)p)->m_pNext = NULL;

	return (PCListNode)p;
}

void PrintfDoubleLoopList(PCList plist)
{
	PCListNode pCurrentNode = NULL;
	if (MmIsAddressValid(plist))
	{
		pCurrentNode = plist->m_pHead;
		do
		{
			DbgPrint("%d --> ", pCurrentNode->m_Data);
			pCurrentNode = pCurrentNode->m_pNext;
		} while (pCurrentNode != plist->m_pHead);
	}
	DbgPrint("Nul. ");
	return;
}

int IsVerifyListNode(PCList plist, PCListNode pNode)
{
	PCListNode pCurrentNode = NULL;
	if (pNode != NULL)
	{
		pCurrentNode = plist->m_pHead;
		do
		{
			if (pCurrentNode == pNode)
			{
				return 1;
			}
			pCurrentNode = pCurrentNode->m_pNext;
		} while (pCurrentNode != plist->m_pHead);
	}
	return 0;
}

void InitDoubleLoopList(PCList plist)
{
	RtlZeroMemory(plist, sizeof(CList));
}

int GetListSize(PCList plist)
{
	if (MmIsAddressValid(plist))
	{
		return plist->m_Size;
	}
	return 0;
}

void InsertHeadDoubleLoopList(PCList plist, ElemType data)		/*头插双向循环链表*/
{
	PCListNode pHead = NULL;
	PCListNode p = _ByListNode(data);

	if (p == NULL)
	{
		return;
	}

	if (plist->m_pHead == NULL && plist->m_pTrail == NULL)		/*当链表为空时*/
	{
		plist->m_pHead = p;										/*头结点指向新申请的节点*/
		plist->m_pTrail = p;									/*尾结点指向新申请的节点*/
		p->m_pFront = plist->m_pTrail;							/*新申请的节点的前驱为尾节点*/
		p->m_pNext = plist->m_pHead;							/*新申请的节点的后继为头节点*/
	}
	else
	{
		pHead = plist->m_pHead;									/*指向头结点*/
		if (MmIsAddressValid(pHead))
		{
			plist->m_pHead = p;										/*头结点指向新申请的节点*/
			p->m_pNext = pHead;										/*新申请节点的下一个节点为pHeadNext*/
			pHead->m_pFront = p;									/*pHeadNext的前驱为新节点*/
			p->m_pFront = plist->m_pTrail;							/*新申请节点的前驱为尾节点*/
			plist->m_pTrail->m_pNext = plist->m_pHead;				/*更改链表尾节点的下一个节点为*/
		}
	}
	plist->m_Size++;
	return;
}

PCListNode InsertTrailDoubleLoopList(PCList plist, ElemType data)		/*尾插双向循环链表*/
{
	PCListNode pTrail = NULL;
	PCListNode p = _ByListNode(data);

	if (p == NULL)
	{
		return p;
	}

	if (plist->m_pHead == NULL && plist->m_pTrail == NULL)		/*当链表为空时*/
	{
		plist->m_pHead = p;										/*头结点指向新申请的节点*/
		plist->m_pTrail = p;									/*尾结点指向新申请的节点*/
		p->m_pFront = plist->m_pTrail;							/*新申请的节点的前驱为尾节点*/
		p->m_pNext = plist->m_pHead;							/*新申请的节点的后继为头节点*/
	}
	else
	{
		pTrail = plist->m_pTrail;								/*获取当前尾节点*/
		if (MmIsAddressValid(pTrail))
		{
			pTrail->m_pNext = p;									/*pTrail的下一个节点为新节点*/
			p->m_pFront = pTrail;									/*新节点的前驱为pTrail*/
			plist->m_pTrail = p;									/*将尾节点指向新节点*/
			p->m_pNext = plist->m_pHead;							/*新节点的下一个节点为头节点*/
			plist->m_pHead->m_pFront = p;							/*更改链表头结点指向*/

		}
	}
	plist->m_Size++;
	return p;
}

void InsertPosFrontDoubleLoopList(PCList plist, PCListNode pNode, ElemType data)		/*按指定位置前面插入双向循环链表中*/
{
	PCListNode pCurrentFront = NULL;
	PCListNode pCurrentNext = NULL;
	PCListNode p = NULL;

	if ((plist->m_pHead == NULL && plist->m_pTrail == NULL) || pNode == NULL)
	{
		return;
	}
	else
	{

		if (!IsVerifyListNode(plist, pNode))				/*验证节点是否有效*/
		{
			return;
		}

		if (pNode == plist->m_pHead)
		{
			InsertHeadDoubleLoopList(plist, data);
			return;
		}
		p = _ByListNode(data);								/*申请新节点*/

		if (p == NULL)
		{
			return;
		}

		pCurrentFront = pNode->m_pFront;					/*获取要插入节点的前一个节点*/
		pCurrentNext = pNode;								/*获取要插入节点的后一个节点*/

		pCurrentFront->m_pNext = p;							/*更改pCurrentFront的后继节点为p*/
		p->m_pFront = pCurrentFront;						/*更改p的前驱为pCurrentFront*/
		pCurrentNext->m_pFront = p;							/*更改pCurrentNext的前驱节点为p*/
		p->m_pNext = pCurrentNext;							/*更改p的后继为pCurrentNext*/
	}
	plist->m_Size++;
	return;
}

void InsertPosNextDoubleLoopList(PCList plist, PCListNode pNode, ElemType data)		/*按指定位置后面插入双向循环链表中*/
{
	PCListNode pCurrentFront = NULL;
	PCListNode pCurrentNext = NULL;
	PCListNode p = NULL;

	if ((plist->m_pHead == NULL && plist->m_pTrail == NULL) || pNode == NULL)
	{
		return;
	}
	else
	{
		if (pNode == plist->m_pTrail)
		{
			InsertTrailDoubleLoopList(plist, data);
			return;
		}
		p = _ByListNode(data);								/*申请新节点*/

		if (p == NULL)
		{
			return;
		}

		pCurrentFront = pNode;								/*获取要插入节点的前一个节点*/
		pCurrentNext = pNode->m_pNext;						/*获取要插入节点的后一个节点*/

		pCurrentFront->m_pNext = p;							/*更改pCurrentFront的后继节点为p*/
		p->m_pFront = pCurrentFront;						/*更改p的前驱为pCurrentFront*/
		pCurrentNext->m_pFront = p;							/*更改pCurrentNext的前驱节点为p*/
		p->m_pNext = pCurrentNext;							/*更改p的后继为pCurrentNext*/
	}
	plist->m_Size++;
	return;
}

ElemType PopHeadDoubleLoopList(PCList plist)
{
	ElemType Data = NULL;
	//判断链表是否有数据 与 指针是否有用
	if (MmIsAddressValid(plist) && plist->m_Size > 0)
	{
		//取头部信息
		Data = plist->m_pHead->m_Data;
		DeleteHeadDoubleLoopList(plist);
	}
	return Data;
}

ElemType PopTrailDoubleLoopList(PCList plist)
{
	ElemType Data = NULL;
	//判断链表是否有数据 与 指针是否有用
	if (MmIsAddressValid(plist) && plist->m_Size > 0)
	{
		//取头部信息
		Data = plist->m_pTrail->m_Data;
		DeleteTrailDoubleLoopList(plist);
	}
	return Data;
}

PCListNode FindListNode(PCList plist, ElemType data)		/*根据数据查找链表节点,返回节点*/
{
	PCListNode pCurrent = NULL;
	if (plist->m_pHead == NULL && plist->m_pTrail == NULL)
	{
		return NULL;										/*链表为空*/
	}
	else
	{
		pCurrent = plist->m_pHead;							/*从头开始查找*/
		do
		{
			if (pCurrent->m_Data == data)					/*要查找的数据相等时*/
			{
				return pCurrent;							/*返回当前节点*/
			}
			pCurrent = pCurrent->m_pNext;					/*指向当前节点的一下个节点*/
		} while (pCurrent != plist->m_pHead);
	}
	return NULL;											/*数据不存在返回空*/
}

ElemType DeleteHeadDoubleLoopList(PCList plist)
{
	ULONG64 nFreeSpaceSize = 0;
	PCListNode pCurrentFront = NULL;
	PCListNode pCurrentNext = NULL;
	PCListNode pCurrent = NULL;
	ElemType pRetData = NULL;

	if (plist->m_pHead == NULL && plist->m_pTrail == NULL)
	{
		return pRetData;
	}
	else
	{
		nFreeSpaceSize = sizeof(CListNode);

		if (plist->m_pHead == plist->m_pTrail)				/*当只有一个节点时*/
		{
			pRetData = plist->m_pHead->m_Data;

			ExFreePool(plist->m_pHead);
			RtlZeroMemory(plist, sizeof(CList));
			return pRetData;
		}
		else
		{
			pCurrent = plist->m_pHead;						/*要释放的节点*/
			pCurrentFront = plist->m_pHead->m_pFront;		/*获取要释放节点的前驱*/
			pCurrentNext = plist->m_pHead->m_pNext;			/*获取要释放节点的后继*/

			pRetData = pCurrent->m_Data;					/*将存储的数据返回*/

			ExFreePool(pCurrent);

			plist->m_pHead = pCurrentNext;					/*重新更改链表头为pCurrentNext*/
			pCurrentFront->m_pNext = plist->m_pHead;		/*重新更改链表尾节点指向头*/
			plist->m_pHead->m_pFront = pCurrentFront;		/*重新更改链表头节点指向尾*/
		}
		plist->m_Size--;
	}
	return pRetData;
}

ElemType DeleteTrailDoubleLoopList(PCList plist)
{
	ULONG64 nFreeSpaceSize = 0;
	PCListNode pCurrentFront = NULL;
	PCListNode pCurrentNext = NULL;
	PCListNode pCurrent = NULL;
	ElemType pRetData = NULL;

	if (plist->m_pHead == NULL && plist->m_pTrail == NULL)
	{
		return;
	}
	else
	{
		nFreeSpaceSize = sizeof(CListNode);

		if (plist->m_pHead == plist->m_pTrail)				/*当只有一个节点时*/
		{
			pRetData = plist->m_pHead->m_Data;

			ExFreePool(plist->m_pHead);
			RtlZeroMemory(plist, sizeof(CList));
			return pRetData;
		}
		else
		{
			pCurrent = plist->m_pTrail;						/*要释放的节点*/
			pCurrentFront = plist->m_pTrail->m_pFront;		/*获取要释放节点的前驱*/
			pCurrentNext = plist->m_pTrail->m_pNext;		/*获取要释放节点的后继*/

			pRetData = pCurrent->m_Data;

			ExFreePool(pCurrent);

			plist->m_pTrail = pCurrentFront;				/*重新更改链表头为pCurrentNext*/
			pCurrentFront->m_pNext = plist->m_pHead;		/*重新更改链表尾节点指向头*/
			plist->m_pHead->m_pFront = plist->m_pTrail;		/*重新更改链表头节点指向尾*/
		}
	}
	plist->m_Size--;
	return pRetData;
}

ElemType DeletePosDoubleLoopList(PCList plist, PCListNode pNode)
{
	ULONG64 nFreeSpaceSize = 0;
	PCListNode pCurrentFront = NULL;
	PCListNode pCurrentNext = NULL;
	PCListNode pCurrent = NULL;
	ElemType pRetData = NULL;

	if ((plist->m_pHead == NULL && plist->m_pTrail == NULL) || pNode == NULL)
	{
		return pRetData;
	}
	else
	{
		nFreeSpaceSize = sizeof(CListNode);

		if (plist->m_pHead == plist->m_pTrail)				/*当只有一个节点时*/
		{
			if (plist->m_pHead == pNode)					/*直接比较头结点是否和要删除的节点相等*/
			{
				pRetData = plist->m_pHead->m_Data;
				ExFreePool(plist->m_pHead);
				RtlZeroMemory(plist, sizeof(CList));
			}
			return pRetData;
		}
		else
		{
			if (!IsVerifyListNode(plist, pNode))			/*验证节点是否有效*/
			{
				return NULL;
			}

			if (pNode == plist->m_pHead)					/*当要删除的节点是头结点时*/
			{
				pRetData = DeleteHeadDoubleLoopList(plist);
				return pRetData;
			}
			else if (pNode == plist->m_pTrail)				/*当要删除的节点是尾结点时*/
			{
				pRetData = DeleteTrailDoubleLoopList(plist);
				return pRetData;
			}
			else
			{
				pCurrentFront = pNode->m_pFront;			/*获取要删除节点的上一个节点*/
				pCurrentNext = pNode->m_pNext;				/*获取要删除节点的下一个节点*/
				pCurrent = pNode;							/*指向当前要删除的节点*/

				pRetData = pCurrent->m_Data;

				ExFreePool(pCurrent);

				pCurrentFront->m_pNext = pCurrentNext;		/*将删除节点前驱的后继指向删除节点的后继*/
				pCurrentNext->m_pFront = pCurrentFront;		/*将删除节点后继的前驱指向删除节点的前驱*/
			}
		}
	}
	plist->m_Size--;										/*链表大小减一*/
	return pRetData;
}

//特定查询
PCListNode FindListNodeEx1(PCList plist, ULONG64 nAddrType, CMPFUNPTRCALLBACK pfun)
{
	if (!MmIsAddressValid(plist))
	{
		return NULL;
	}
	//遍历链表

	PCListNode pCurrent = NULL;
	if (plist->m_pHead == NULL && plist->m_pTrail == NULL)
	{
		return NULL;										/*链表为空*/
	}
	else
	{
		pCurrent = plist->m_pHead;							/*从头开始查找*/
		do
		{
			if (!MmIsAddressValid(pCurrent->m_Data))
			{
				return NULL;
			}

			if (pfun(pCurrent->m_Data, nAddrType) == 0)
			{
				return pCurrent;							/*返回当前节点*/
			}
			pCurrent = pCurrent->m_pNext;					/*指向当前节点的一下个节点*/
		} while (pCurrent != plist->m_pHead);
	}
	return NULL;											/*数据不存在返回空*/
}

void DestroyList(PCList plist, pDestroyListCallBack pCall)													/*摧毁链表*/
{
	int nFlag = FALSE;
	if (MmIsAddressValid(pCall))
	{
		nFlag = TRUE;
	}

	ULONG64 p = NULL;

	while (p = PopHeadDoubleLoopList(plist), p != NULL)
	{
		if (nFlag)
		{
			pCall(p); //调用注册的释放资源函数
		}
	}
}