#include "hde64.h"
#include "EtwHook.h"
#include "../Define.h"
#include "../DataStruct/CStack.h"
#include <ntimage.h>
#include "../Ssdt.h"

CList g_EtwHookList = { 0 };
ULONG g_IsOpenEtw = 0;
ULONG64 g_OldCpuClock = 0;
ULONG64 g_pCpuClock = 0;
ULONG64 g_SyscallTable = 0;
//ULONG64 HalPrivateDispatchTable = 0;
ULONG64 HalpPerformanceCounter = 0;
fptr_call_back g_pEtwFunCallBack = 0;

ULONG64 g_MyHalpHvCounterQueryCounterAddr = keQueryPerformanceCounterHook;
ULONG64 g_OldHalpPerformanceCounter = NULL;
ULONG64 g_circularKernelContextLogger = NULL;

//比较地址 只处理等于
DWORD EtwCmpSrcAndDstAddr(PCEtwHook SrcAddr, PCEtwHook DstAddr)
{
	//比较原函数地址是否一样  //再比较要Call的地址是否一样  
	if (SrcAddr->SrcAddr == DstAddr->SrcAddr && SrcAddr->CallAddr == DstAddr->CallAddr)
	{
		return 0;
	}
	return -1;
}

//比较地址 只处理等于
DWORD EtwCmpSrcAddr(PCEtwHook SrcAddr, PCEtwHook DstAddr)
{
	//比较原函数地址是否一样  //再比较要Call的地址是否一样  
	if (SrcAddr->SrcAddr == DstAddr->SrcAddr)
	{
		return 0;
	}
	return -1;
}

//比较地址 只处理等于
DWORD EtwCmpDstAddr(PCEtwHook SrcAddr, PCEtwHook DstAddr)
{
	//比较目标函数地址是否一样  
	if (SrcAddr->CallAddr == DstAddr->CallAddr)
	{
		return 0;
	}
	return -1;
}

DWORD EtwCmpLeven(PCEtwHook SrcAddr, PCEtwHook DstAddr)
{
	//比较目标函数地址是否一样  
	if (SrcAddr->nLeven == DstAddr->nLeven)
	{
		return 0;
	}
	else if (SrcAddr->nLeven < DstAddr->nLeven)
	{
		return 1;
	}
	return -1;
}

//查找EtwHook的数据
PCListNode FindEtwHookData(PCEtwHook SrcData, CMPFUNPTRCALLBACK pfun, PCList* pOutList)
{
	PCListNode pRetAddr = 0xFFFFFFFF;
	PCList pMyOutList = NULL;
	//验证参数是否准确 和存储全局链表的地方
	if (MmIsAddressValid(SrcData)
		&& MmIsAddressValid(SrcData->SrcAddr)
		&& MmIsAddressValid(SrcData->CallAddr)
		&& MmIsAddressValid(&g_EtwHookList)
		&& GetListSize(&g_EtwHookList) > 0)
	{

		//__debugbreak();
		//首先比较g_EtwHookListl链表里的数据获取到根目录
		PCListNode p = FindListNodeEx1(&g_EtwHookList, SrcData, EtwCmpSrcAddr);

		PCListNode pCurNode = &g_EtwHookList.m_pHead;
		//遍历g_EtwHookList外层链表 查找到
		//do
		//{
		//	if (!MmIsAddressValid(pCurNode))
		//	{
		//		break;
		//	}
		//
		//
		//
		//} while (&g_EtwHookList.m_pHead);



		//初始化栈
		CStack Stack;
		if (!NT_SUCCESS(InitStack(&Stack)))
		{
			//失败返回
			return pRetAddr;
		}

		while (MmIsAddressValid(p) && MmIsAddressValid(p->m_Data))
		{
			//找到了
			PCEtwHook pHookNode = (PCEtwHook)p->m_Data;
			//首先判断当前的节点是否是要找的目标
			if (pHookNode->CallAddr == SrcData->CallAddr)
			{
				//找到了数据 结束循环
				pRetAddr = p;
				break;
			}


			//判断是否有被附加的数据
			if (pHookNode->nIsAttach)
			{	//
				//有被附加的数据在进入遍历链表的时候将前一个链表的地址放入到栈中
				//

				//如果下一个节点不是头结点就加入到栈中
				PCEtwHook pEtwHookNode = (PCEtwHook)p->m_pNext->m_Data;
				if (pEtwHookNode->nIndex != ETW_HOOK_LEVEN_HEAD_VALUE)
				{
					PushStack(&Stack, p->m_pNext);
				}

				//将附加链表赋值被d当前正在遍历的节点,下一个从这个开始遍历
				p = &pHookNode->AttachList;
			}
			else
			{
				//
				//没被附加的数据就继续遍历下一个
				//

				//
				//判断是要从当前链表中取下一个,还是从栈中拿上一层的链表
				//

				if (((PCEtwHook)p->m_pNext->m_Data)->nIndex == ETW_HOOK_LEVEN_HEAD_VALUE)
				{
					//如果成立就代表当前链表已经遍历完成,那么就是从栈中获取上层链表
					if (!GetStackIsNull(&Stack))
					{
						//栈空了,就代表遍历完成
						break;
					}

					//栈没空就弹出一个
					p = PopStack(&Stack);
				}
				else
				{
					//
					//否则就指向下一个
					// 

					p = p->m_pNext;
				}
			}
		}
		//清空栈数据
		DestroyStack(&Stack);
	}



	if (MmIsAddressValid(pOutList))
	{
		*pOutList = pMyOutList;
	}


	return pRetAddr;
}

//根据层级查找能插入的地方
PCListNode FindInsertDataNode(PCList plist, USHORT SrcLeven, PCEtwHook SrcData, PCList* pOutList, PCListNode* pParentNode)
{

	//
	// 1>此结构暂时只设计到两层链表
	// 2>也就是每个节点中也有一个链表,当要查找的数据重复时会加入到这个节点里面的链表也就是Attach
	// 3>如果遇到Y重复的值就不看层次了,就直接插入到链表最后面
	//

	PCListNode pRetParentNode = NULL;
	PCList OutLis = NULL;
	PCListNode pRetAddr = NULL;
	ULONG64 MainOutIndex = 0;

	do
	{
		//验证参数
		if (MmIsAddressValid(plist))
		{
			UCHAR dbX = MYHIWORD(SrcLeven);		//
			UCHAR dbY = MYLOWORD(SrcLeven);		//


			PCList pCurListLeven = plist;
			PCListNode pCurListLevenNode = (PCListNode)pCurListLeven->m_pHead;


			ULONG64 OutIndex = GetListSize(pCurListLeven);

			//当要插入的链表数量为1或者为0时直接返回头部
			if (OutIndex < 1)
			{
				if (dbY == 0)
				{
					OutLis = pCurListLeven;
					pRetAddr = pCurListLevenNode;
					pRetParentNode = NULL;
					break;
				}

				//当要设置的数据层级X大于0且链表中节点的数量小于2时生成一个空节点将原地址赋值上去,并设置位为FALSE代表无效节点
				else if (dbY > 0)
				{
					//申请一个Hook结构体
					PCEtwHook pAddr = ExAllocatePool(PagedPool, sizeof(CEtwHook));
					if (!MmIsAddressValid(pAddr))
					{
						break;
					}

					RtlZeroMemory(pAddr, sizeof(CEtwHook));

					//设置要call的地址为NULL
					pAddr->CallAddr = NULL;

					if (MmIsAddressValid(SrcData))
					{
						//设置原Call函数地址为SrcData的原函数地址
						pAddr->SrcAddr = SrcData->SrcAddr;
					}

					//设置是否有数据附加这个暂时设置为FALSE,由外层插入时在加入
					pAddr->nIsAttach = FALSE;

					{
						//设置初始化附加链表为真
						pAddr->nInitAttachList = TRUE;
						//初始化链表
						InitDoubleLoopList(&pAddr->AttachList);
					}

					//设置层级,Y基本为0
					pAddr->nLeven = SrcLeven & 0xFF00;

					//设置可用标志位假
					pAddr->nIsValid = FALSE;

					//设置当前节点所在链表
					pAddr->pCurList = pCurListLeven;

					//判断当要插入的链表wig_EtwHookList链表时将设置为根节点
					if (plist == &g_EtwHookList)
					{
						pAddr->nIsRootNode = TRUE;
					}

					//将地址当数据传送过去
					pRetParentNode = InsertTrailDoubleLoopList(plist, pAddr);

					//修复索引值
					RepairEtwData(plist);

					OutLis = &pAddr->AttachList;
					pRetAddr = pAddr->AttachList.m_pHead;

					break;
				}
			}

			OutIndex = 0;
			MainOutIndex = 0;

			//首先查找X层级
			do
			{
				//验证参数
				if (!MmIsAddressValid(pCurListLevenNode) || !MmIsAddressValid(pCurListLevenNode->m_Data))
				{
					break;
				}

				//获取下一个要对比的节点
				PCListNode pNextListLeven = (PCListNode)pCurListLevenNode->m_pNext;
				if (!MmIsAddressValid(pNextListLeven) || !MmIsAddressValid(pNextListLeven->m_Data))
				{
					break;
				}

				//当前的节点信息
				PCEtwHook pCurHookInfo = (PCEtwHook)pCurListLevenNode->m_Data;
				//当前的下一个节点信息
				PCEtwHook pNextHookInfo = (PCEtwHook)pNextListLeven->m_Data;


				//当要插入的条件为一个节点的里面时 ,就将这个节点加入到当前节点的链表里面去
				if (MYHIWORD(pCurHookInfo->nLeven) == dbX)
				{
					//相等说明已存在 
					//
					// 存在的话就先判断是否有效,有效就直接找找附加链表,根据附加的链表返回第几个 ,无效就直接比较原call的函数如果相等就直接赋值
					// 

					OutIndex = 0;

					//那么就开始找Y坐标的  
					if (!pCurHookInfo->nInitAttachList)
					{
						//
						//未初始化,但数据节点是存在的,那么就初始化后返回头结点
						// 

						//初始化链表
						InitDoubleLoopList(&pCurHookInfo->AttachList);

						//修改属性
						pCurHookInfo->nInitAttachList = TRUE;

						//将链表头返回回去
						pRetAddr = &pCurHookInfo->AttachList.m_pHead;

						//链表就为要附加的这个链表
						pCurListLeven = &pCurHookInfo->AttachList;

						//设置要插入的节点的位置
						OutIndex = 0;

						//设置父节点
						pRetParentNode = pCurListLevenNode;
					}
					else
					{
						//
						//初始化了,还有数据,这个时候就要遍历链表比较Y的值了
						// 
						//初始化过就从头开始加
						//
						OutIndex = 0;

						//获取附加链表
						PCList pAttachList = (PCList)&pCurHookInfo->AttachList;
						if (!MmIsAddressValid(pAttachList))
						{
							break;
						}

						//判断链表是有数据
						if (GetListSize(&pAttachList))
						{
							PCListNode pCurAttachListLevenNode = (PCListNode)pAttachList->m_pHead;
							//按照上面操作再找Y
							do
							{
								if (!MmIsAddressValid(pCurAttachListLevenNode) || !MmIsAddressValid(pCurAttachListLevenNode->m_Data))
								{
									break;
								}

								//获取下一个要对比的节点
								PCListNode pNextAttachListLeven = (PCListNode)pCurAttachListLevenNode->m_pNext;
								if (!MmIsAddressValid(pNextAttachListLeven) || !MmIsAddressValid(pNextAttachListLeven->m_Data))
								{
									break;
								}

								//当前的节点信息
								pCurHookInfo = (PCEtwHook)pCurAttachListLevenNode->m_Data;
								//当前的下一个节点信息
								pNextHookInfo = (PCEtwHook)pNextAttachListLeven->m_Data;

								if (MYLOWORD(pCurHookInfo->nLeven) == dbY)
								{
									//相等说明已存在 //那么就插入到他后面 //不附加到他里面的链表了
									pRetAddr = pCurAttachListLevenNode;

									//要返回的链表
									OutLis = pAttachList;

									break;
								}

								if (MYLOWORD(pCurHookInfo->nLeven) < dbY && (!(MYLOWORD(pNextHookInfo->nLeven) <= dbY) || pNextHookInfo->nIndex == ETW_HOOK_LEVEN_HEAD_VALUE))
								{
									//表示这个新插入的层级暂时没被使用 直接返回这个节点 Y层级直接没有
									pRetAddr = pCurAttachListLevenNode;

									//要返回的链表
									OutLis = pAttachList;

									break;
								}

								//指向下一个节点
								pCurAttachListLevenNode = pNextAttachListLeven;

								OutIndex++;
							} while (pCurAttachListLevenNode != pAttachList->m_pHead);
						}
						else
						{
							//当附加的链表没数据时
							pRetAddr = (PCListNode)pAttachList->m_pHead;

							//要返回的链表
							OutLis = pAttachList;

							//返回父节点
							pRetParentNode = pCurHookInfo;
						}
					}
					break;
				}

				//MyDbgPrintfEx("pCurHookInfoY:%d %I64X pNextHookInfoY:%d  %I64X dbX:%d\n", MYHIWORD(pCurHookInfo->nLeven), pCurHookInfo, MYHIWORD(pNextHookInfo->nLeven), pNextHookInfo, dbX);

				//当要插入的条件为两个值的中间时 或者是遍历到尾部依旧没比最后一个层级大那么就返回当前的节点
				if (MYHIWORD(pCurHookInfo->nLeven) < dbX && (!(MYHIWORD(pNextHookInfo->nLeven) <= dbX) || pNextHookInfo->nIndex == ETW_HOOK_LEVEN_HEAD_VALUE))
				{

					//判断高度是否是根节点
					if (dbY == FALSE)
					{
						//表示这个新插入的层级暂时没被使用 直接返回这个节点 Y层级直接没有
						pRetAddr = pCurListLevenNode;

						OutLis = pCurListLeven;

						//设置外层的索引
						OutIndex = MainOutIndex;
					}
					else
					{
						//不是根节点,且没有根节点的时候就需要生成一个临时节点将此节点挂在到附加链表上不然不好管理
						//申请一个Hook结构体
						PCEtwHook pAddr = ExAllocatePool(PagedPool, sizeof(CEtwHook));
						if (!MmIsAddressValid(pAddr))
						{
							break;
						}

						RtlZeroMemory(pAddr, sizeof(CEtwHook));

						//设置要call的地址为NULL
						pAddr->CallAddr = NULL;

						if (MmIsAddressValid(SrcData))
						{
							//设置原Call函数地址为SrcData的原函数地址
							pAddr->SrcAddr = SrcData->SrcAddr;
						}

						//设置是否有数据附加这个暂时设置为FALSE,由外层插入时在加入
						pAddr->nIsAttach = FALSE;

						{
							//设置初始化附加链表为真
							pAddr->nInitAttachList = TRUE;
							//初始化链表
							InitDoubleLoopList(&pAddr->AttachList);
						}

						//设置层级,Y基本为0
						pAddr->nLeven = SrcLeven & 0xFF00;

						//设置可用标志位假
						pAddr->nIsValid = FALSE;

						//判断当要插入的链表wig_EtwHookList链表时将设置为根节点
						if (plist == &g_EtwHookList)
						{
							pAddr->nIsRootNode = TRUE;
						}

						//设置当前节点所在链表
						pAddr->pCurList = pCurListLeven;

						//将地址当数据传送过去
						pRetParentNode = InsertTrailDoubleLoopList(plist, pAddr);

						//修复索引值
						RepairEtwData(plist);

						OutLis = &pAddr->AttachList;
						pRetAddr = pAddr->AttachList.m_pHead;
						break;

					}

					break;
				}

				//指向下一个
				pCurListLevenNode = pNextListLeven;
				MainOutIndex++;
			} while (pCurListLevenNode != (PCListNode)pCurListLeven->m_pHead);
			//如果循结束说明
		}
	} while (FALSE);

	//返回链表
	if (MmIsAddressValid(pOutList))
	{
		*pOutList = OutLis;
	}

	//返回父节点
	if (MmIsAddressValid(pParentNode))
	{
		*pParentNode = pRetParentNode;
	}
	return pRetAddr;
}

//修复数据
void RepairEtwData(PCList pList)
{
	if (MmIsAddressValid(pList))
	{
		if (!GetListSize(pList))
		{
			return;
		}

		ULONG32 i = ETW_HOOK_LEVEN_HEAD_VALUE;
		PCListNode pNextNode = (PCListNode)pList->m_pHead;
		do
		{
			if (!MmIsAddressValid(pNextNode))
			{
				break;
			}

			PCEtwHook pData = pNextNode->m_Data;
			if (MmIsAddressValid(pData))
			{
				//修复索引时顺便遍历当前节点是否是有效数据
				if (pData->nIsAttach == FALSE && pData->nIsValid == FALSE && pData->nInitAttachList == FALSE)
				{
					//释放资源
					PCEtwHook pDelData = DeletePosDoubleLoopList(pList, pNextNode);
					if (MmIsAddressValid(pDelData))
					{
						ExFreePool(pDelData);
					}

					//当链表为空时结束遍历
					if (!GetListSize(pList))
					{
						break;
					}

					//重头开始
					pNextNode = (PCListNode)pList->m_pHead;
					i = ETW_HOOK_LEVEN_HEAD_VALUE;
					continue;
				}
				else
				{
					pData->nIndex = i++;
				}
			}
		} while (pNextNode = pNextNode->m_pNext, pNextNode != pList->m_pHead);
	}
}

//Hook地址
PCEtwHook HookAddr(ULONG64 SrcAddr/*原地址*/, ULONG64 CallAddt/*要替换的地址*/, USHORT SrcLeven)
{
	//返回值
	PCEtwHook pAddr = NULL;

	//验证参数是否有效
	if (!MmIsAddressValid(SrcAddr) || !MmIsAddressValid(CallAddt))
	{
		return FALSE;
	}

	//申请一个Hook结构体
	pAddr = ExAllocatePool(PagedPool, sizeof(CEtwHook));
	if (!MmIsAddressValid(pAddr))
	{
		return FALSE;
	}

	RtlZeroMemory(pAddr, sizeof(CEtwHook));

	//初始化 要call的地址
	pAddr->CallAddr = CallAddt;

	//初始化原函数地址
	pAddr->SrcAddr = SrcAddr;

	//设置层级
	pAddr->nLeven = SrcLeven;

	//设置有效位
	pAddr->nIsValid = TRUE;

	//当Y为0时
	if (MYLOWORD(pAddr->nLeven) == 0)
	{
		//设置为根节点
		pAddr->nIsRootNode = TRUE;
	}

	//pAddr->nIndex = FindListNodeEx1(&g_EtwHookList, SrcAddr, EtwCmpSrcAddr);

	//存储要插入的链表
	//PCListNode pDstList = &g_EtwHookList;
	UCHAR nCurIndex = GetListSize(&g_EtwHookList);

	//	//												相等插入附加链表
	//	//													  /\
	//	//													  ||
	//	//判断要插入的层级 ,判断要插入的地方前后层级 低层级 < 要插入的层级 < ( (高层级) 因为这是个循环链表那么这个高层级X不能为0)
	//	//注:同级先注册的先运行,不同级的层级越高越后执行
	//	// 

	PCList pOutList = NULL;
	PCListNode pParentNode = NULL;
	//返回的节点是要插入到这个节点后面的一个数据
	PCListNode pInsertNode = FindInsertDataNode(&g_EtwHookList, SrcLeven, pAddr, &pOutList, &pParentNode);
	if (MmIsAddressValid(pInsertNode) && MmIsAddressValid(pOutList))
	{
		//判断链表的值是在同一个上面
		if (((PCEtwHook)pInsertNode->m_Data)->SrcAddr == SrcAddr)
		{
			if (((PCEtwHook)pInsertNode->m_Data)->nIsValid == FALSE && ((PCEtwHook)pInsertNode->m_Data)->nLeven == pAddr->nLeven)
			{
				//直接将数据拷贝到返回的节点上
				((PCEtwHook)pInsertNode->m_Data)->CallAddr = pAddr->CallAddr;
				((PCEtwHook)pInsertNode->m_Data)->nIsValid = TRUE;

				//释放原来申请的内存
				ExFreePool(pAddr);
			}
			else
			{
				//插入进链表中
				InsertPosNextDoubleLoopList(pOutList, pInsertNode, pAddr);

				//修复索引值
				RepairEtwData(pOutList);

				//设置当前要插入节点的父节点 要插入节点的前一个节点的父节点赋值给插入节点的父节点
				pAddr->ParentListNode = ((PCEtwHook)pInsertNode->m_Data)->ParentListNode;


				//设置父节点最大层级
				USHORT pParentMaxLeven = ((PCEtwHook)pInsertNode->m_Data)->nMaxLeven;
				if (MYLOWORD(pParentMaxLeven) < MYLOWORD(pAddr->nLeven))
				{
					//替换成新的
					((PCEtwHook)pInsertNode->m_Data)->nMaxLeven = pAddr->nLeven;

					//保存老的
					pAddr->nMaxLeven = pParentMaxLeven;
				}

			}
			//设置当前节点所在的链表
			pAddr->pCurList = pOutList;
		}
	}
	else
	{
		if (MmIsAddressValid(pOutList))
		{

			//
			// pOutList不为 NULL 的情况就是 链表的数量小于2
			// 

			//将地址当数据传送过去
			InsertTrailDoubleLoopList(pOutList, pAddr);
			//修复索引值
			RepairEtwData(pOutList);

			//设置当前要插入节点的父节点 //并设置链表的附加数据为真
			if (MmIsAddressValid(pParentNode))
			{
				pAddr->ParentListNode = pParentNode;
				((PCEtwHook)pParentNode->m_Data)->nIsAttach = TRUE;

				//设置父节点最大层级
				USHORT pParentMaxLeven = ((PCEtwHook)pInsertNode->m_Data)->nMaxLeven;
				if (MYLOWORD(pParentMaxLeven) < MYLOWORD(pAddr->nLeven))
				{
					//替换成新的
					((PCEtwHook)pInsertNode->m_Data)->nMaxLeven = pAddr->nLeven;
					//保存老的
					pAddr->nMaxLeven = pParentMaxLeven;
				}
			}
			pAddr->pCurList = pOutList;
		}
		else
		{
			//
			// 当输出链表为空时 插入到根链表 g_EtwHookList
			// 

			//将地址当数据传送过去
			InsertTrailDoubleLoopList(&g_EtwHookList, pAddr);
			//修复索引值
			RepairEtwData(&g_EtwHookList);

			pAddr->pCurList = &g_EtwHookList;
		}
	}

	return pAddr;
}

//卸载HOOK
ULONG64 UnHookAddr(PCEtwHook SrcAddr/*要删除的地址,原函数地址*/)
{
	//验证参数是否有效
	if (!MmIsAddressValid(SrcAddr))
	{
		return FALSE;
	}

	PCList pList = SrcAddr->pCurList;

	if (!GetListSize(pList))
	{
		return FALSE;
	}

	PCListNode pCurNode = pList->m_pHead;
	PCListNode p = NULL;
	do
	{
		if (!MmIsAddressValid(pCurNode))
		{
			break;
		}

		PCEtwHook pHookInfo = pCurNode->m_Data;
		if (!MmIsAddressValidEx(pHookInfo, sizeof(CEtwHook)))
		{
			break;
		}

		if ((pHookInfo->SrcAddr == SrcAddr->SrcAddr) && pHookInfo->nLeven == SrcAddr->nLeven)
		{
			//找到了
			p = pCurNode;
			break;
		}
		pCurNode = pCurNode->m_pNext;
	} while (pCurNode != pList->m_pHead);

	if (MmIsAddressValidEx(p, sizeof(CListNode)))
	{
		PCEtwHook pCurDelNode = (PCEtwHook)p->m_Data;

		//首先判断这个节点是否是根节点
		if (MmIsAddressValid(pCurDelNode))
		{
			//获取这个节点所在的链表
			PCList pCurDelList = pCurDelNode->pCurList;

			//判断是否是根节点
			if (pCurDelNode->nIsRootNode || pCurDelList == &g_EtwHookList)
			{
				//判断是否有附加的数据
				if (pCurDelNode->nIsAttach)
				{
					//当有附加链表时,根节点不删除数据只修改属性
					pCurDelNode->nIsValid = FALSE;			//设置成无效节点
					pCurDelNode->CallAddr = NULL;			//清空要Call的地址
					pCurDelNode->nInitAttachList = FALSE;	//
				}
				else
				{
					//删除节点
					PCEtwHook pData = DeletePosDoubleLoopList(pCurDelList, p);

					if (MmIsAddressValid(pData))
					{
						ExFreePool(pData);

						//修复索引
						RepairEtwData(pCurDelList);
					}
				}
			}
			else
			{
				//记录父节点
				PCListNode pParDelListNode = pCurDelNode->ParentListNode;

				//删除节点
				PCEtwHook pData = DeletePosDoubleLoopList(pCurDelList, p);

				if (MmIsAddressValid(pData))
				{
					//
					ExFreePool(pData);
					//修复父节点

					if (MmIsAddressValid(pParDelListNode))
					{
						PCEtwHook pPerData = (PCEtwHook)pParDelListNode->m_Data;

						if (!GetListSize(pCurDelList))
						{

							//当要删除的节点被挂在附加链表上时,且是最后一个节点时就检查父节点时候有效,无效一并删除,有效时清空nIsAttach和nInitAttachList标志就行
							if (!pPerData->nIsValid)
							{
								pData = DeletePosDoubleLoopList(pPerData->pCurList, pParDelListNode);
								if (MmIsAddressValid(pData))
								{
									pCurDelList = pData->pCurList;
									ExFreePool(pData);
								}
							}
							else
							{
								pPerData->nIsAttach = GetListSize(&pPerData->AttachList) > 0;
								pPerData->nInitAttachList = pPerData->nIsAttach;
							}
						}
						else
						{
							//修复最大 层级 nLeven
							pPerData->nMaxLeven = ((PCEtwHook)pCurDelList->m_pTrail->m_Data)->nLeven;
						}
					}
					//修复索引
					RepairEtwData(pCurDelList);
				}
			}
		}
		return TRUE;
	}
	return FALSE;
}

//开启ETW事件记录器,更改系统调用流程
NTSTATUS modify_trace_settings(trace_type type)
{
	GUID g_ckcl_session_guid = { 0x54dea73a, 0xed1f, 0x42a4, { 0xaf, 0x71, 0x3e, 0x63, 0xd0, 0x56, 0xf1, 0x74 } };

	const unsigned long tag = 'VMON';

	CKCL_TRACE_PROPERTIES* property = (CKCL_TRACE_PROPERTIES*)ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, tag);
	if (!property)
	{
		//DbgPrintEx(0, 0, "[%s] allocate ckcl trace propertice struct fail \n", __FUNCTION__);
		return STATUS_MEMORY_NOT_ALLOCATED;
	}

	wchar_t* provider_name = (wchar_t*)ExAllocatePoolWithTag(NonPagedPool, 256 * sizeof(wchar_t), tag);
	if (!provider_name)
	{
		//DbgPrintEx(0, 0, "[%s] allocate provider name fail \n", __FUNCTION__);
		ExFreePoolWithTag(property, tag);
		return STATUS_MEMORY_NOT_ALLOCATED;
	}

	RtlZeroMemory(property, PAGE_SIZE);
	RtlZeroMemory(provider_name, 256 * sizeof(wchar_t));

	RtlCopyMemory(provider_name, L"Circular Kernel Context Logger", sizeof(L"Circular Kernel Context Logger"));
	RtlInitUnicodeString(&property->ProviderName, (const wchar_t*)provider_name);

	property->m_event.Wnode.BufferSize = PAGE_SIZE;
	property->m_event.Wnode.Flags = 0x00020000;
	property->m_event.Wnode.Guid = g_ckcl_session_guid;
	property->m_event.Wnode.ClientContext = 3;
	property->m_event.BufferSize = sizeof(unsigned long);
	property->m_event.MinimumBuffers = 2;
	property->m_event.MaximumBuffers = 2;
	property->m_event.LogFileMode = 0x00000400;

	unsigned long length = 0;
	if (type == syscall_trace)
	{
		property->m_event.EnableFlags = 0x00000080;
	}
	NTSTATUS status = ZwTraceControl(type, property, PAGE_SIZE, property, PAGE_SIZE, &length);

	ExFreePoolWithTag(provider_name, tag);
	ExFreePoolWithTag(property, tag);

	return status;
}

//初始化要用到的变量
ULONG64 EtwInit(ULONG64 pFunCallBack)
{
	if (!MmIsAddressValid(PerfGlobalGroupMask))
	{
		return FALSE;
	}

	PULONG64 pArry = NULL; //存储数组首地址
	PULONG64 pCkclWmiLoggerContext = NULL;//存储_WMI_LOGGER_CONTEXT结构体指针


	if (MmIsAddressValid(EtwpDebuggerData))
	{
		//不知道为啥是加0x10,暂时无法确定EtwpDebuggerData的结构
		pArry = *(PULONG64)(EtwpDebuggerData + 0x10);
	}
	else if (MmIsAddressValid(EtwpHostSiloState))
	{
		//*EtwpHostSiloState +0x1c8 定位到数组首地值
		pArry = (*(PULONG64)EtwpHostSiloState + _ETW_SILODRIVERSTATE_EtwpLoggerContext);
	}
	//MyDbgPrintfEx("EtwpHostSiloState:%I64X\n", EtwpHostSiloState);
	//MyDbgPrintfEx("EtwpDebuggerData:%I64X\n", EtwpDebuggerData);
	//MyDbgPrintfEx("pArry:%I64X\n", pArry);

	//rcx, ds:0[r14 * 8];  r14不确定,但调试基本都是 r14=2
	pCkclWmiLoggerContext = pArry[2];
	if (!MmIsAddressValid(pCkclWmiLoggerContext))
	{
		MyDbgPrintfEx("[%s] pCkclWmiLoggerContext fail! \n", __FUNCTION__);
		return FALSE;
	}

	g_circularKernelContextLogger = pCkclWmiLoggerContext;

	//获取_WMI_LOGGER_CONTEXT中GetCpuClock成员地址 保存到全局变量中
	g_pCpuClock = (ULONG64)g_circularKernelContextLogger + _WMI_LOGGER_CONTEXT_GetCpuClock;

	//获取系统调用表
	g_SyscallTable = get_syscall_entry(g_NtoskrnlAddr);
	if (!MmIsAddressValid(g_SyscallTable))
	{
		MyDbgPrintfEx("[%s] g_SyscallTable fail! \n", __FUNCTION__);
		return FALSE;
	}

	//要HOOK的地址
	if (MmIsAddressValid(HalpPerformanceCounter))
	{
		//MyDbgPrintfEx("HalpPerformanceCounter:%I64X\n", HalpPerformanceCounter);
		HalpPerformanceCounter = *(PULONG64)HalpPerformanceCounter;
	}

	/*
	 * EtwpGetLoggerTimeStamp
	 * keQueryPerformanceCounter
	 *
	 nt!KeQueryPerformanceCounter+0x12:
	 fffff800`05efbed2 488b3dcf7e9500  mov     rdi,qword ptr [nt!HalpPerformanceCounter (fffff800`06853da8)]
	 nt!KeQueryPerformanceCounter+0xcc:
	 fffff800`05efbf8c 488b4770        mov     rax,qword ptr [rdi+70h]
	 nt!KeQueryPerformanceCounter+0xd0:
	 fffff800`05efbf90 e8bbbd1000      call    nt!guard_dispatch_icall (fffff800`06007d50)
	 nt!guard_dispatch_icall+0x71:
	 fffff800`06007dc1 ffe0            jmp     rax

	 * HalpPerformanceCounter
	 */

	if (MmIsAddressValid(pFunCallBack))
	{
		g_pEtwFunCallBack = pFunCallBack;
	}

	// 	MyDbgPrintfEx("HalpPerformanceCounter:%I64X\n", HalpPerformanceCounter);
	// 	MyDbgPrintfEx("pCkclWmiLoggerContext:%I64X\n", pCkclWmiLoggerContext);
	// 	MyDbgPrintfEx("g_pEtwFunCallBack:%I64X\n", g_pEtwFunCallBack);
	// 	MyDbgPrintfEx("g_pCpuClock:%I64X\n", g_pCpuClock);

	//初始化链表
	InitDoubleLoopList(&g_EtwHookList);
	return TRUE;
}

VOID keQueryPerformanceCounterHook(ULONG_PTR pStack)
{
	//__debugbreak();

	if (ExGetPreviousMode() == KernelMode)
	{
		return;
	}
	//MyDbgPrintfEx("[keQueryPerformanceCounterHook]\n");

	for (size_t i = 0; i < 30; i++)
	{
		uintptr_t Address = pStack + i * 8;
		if (!MmIsAddressValid((PVOID)Address))
		{
			break;
		}
		uintptr_t t = *(uintptr_t*)Address;
		if (t == g_circularKernelContextLogger)
		{
			//__debugbreak();
			self_get_cpu_clock();
			break;
		}
	}
}

//开始运行
ULONG64 EtwStart()
{
	if (!MmIsAddressValid(g_pCpuClock) || !MmIsAddressValid(HalpPerformanceCounter) || !MmIsAddressValid(g_pEtwFunCallBack))
	{
		return FALSE;
	}

	//修改属性开启ETW HOOK
	if (!NT_SUCCESS(modify_trace_settings(syscall_trace)))
	{
		if (!NT_SUCCESS(modify_trace_settings(start_trace)))
		{
			//MyDbgPrintfEx("[%s] start ckcl fail \n", __FUNCTION__);
			return FALSE;
		}

		if (!NT_SUCCESS(modify_trace_settings(syscall_trace)))
		{
			//MyDbgPrintfEx("[%s] syscall ckcl fail \n", __FUNCTION__);
			return FALSE;
		}
	}
	g_IsOpenEtw = TRUE;

	g_OldCpuClock = *(PULONG64)g_pCpuClock;											//保存原值
	*(PULONG64)g_pCpuClock = (PVOID64)1;											//修改执行流程
	g_OldHalpPerformanceCounter = *(PULONG64)(HalpPerformanceCounter + 0x70);		//保存原值
	*(PULONG64)(HalpPerformanceCounter + 0x70) = (ULONG64)checkLogger;				//自己函数地址

	return g_IsOpenEtw;
}

//停止运行
ULONG64 EtwStop()
{
	ULONG64 result = NULL;
	if (g_IsOpenEtw)
	{
		//CloseEtw(PerfGlobalGroupMask);
		result = NT_SUCCESS(modify_trace_settings(stop_trace)) && NT_SUCCESS(modify_trace_settings(start_trace));

		*(PULONG64)(HalpPerformanceCounter + 0x70) = (ULONG64)g_OldHalpPerformanceCounter;
		*(PULONG64)g_pCpuClock = (PVOID64)g_OldCpuClock;


		//释放链表
		DestroyList(&g_EtwHookList, ExFreePool);//摧毁链表

		g_IsOpenEtw = FALSE;
	}
	return result;
}

// 获取SSDT表地址
PVOID64 get_syscall_entry(ULONG64 ntoskrnl)
{
	if (!MmIsAddressValid(ntoskrnl))
	{
		return NULL;
	}

#define IA32_LSTAR_MSR 0xC0000082
	PVOID64 syscall_entry = (PVOID64)__readmsr(IA32_LSTAR_MSR);

	// 没有补丁过,直接返回KiSystemCall64
	ULONG section_size = 0;
	ULONG64 KVASCODE = get_image_address(ntoskrnl, "KVASCODE", &section_size);
	if (!KVASCODE)
	{
		return syscall_entry;
	}

	//KiSystemCall64还在区域内,返回
	if (!(syscall_entry >= (PVOID64)KVASCODE && syscall_entry < (PVOID64)(KVASCODE + section_size)))
	{
		return syscall_entry;
	}

	// 来到这一步代表KiSystemCall64Shadow,打补丁了
	CHde64 hde_info = { 0 };
	for (char* ki_system_service_user = (char*)syscall_entry; ; ki_system_service_user += hde_info.m_len)
	{
		//反汇编
		if (!Myhde64_disasm(ki_system_service_user, &hde_info)) break;

		// 我们要查找jmp
#define OPCODE_JMP_NEAR 0xE9
		if (hde_info.m_opcode != OPCODE_JMP_NEAR)
		{
			continue;
		}

		//忽略KVASCODE节中jmp指令
		PVOID64 possible_syscall_entry = (PVOID64)((LONG64)ki_system_service_user + (LONG)hde_info.m_len + (LONG)hde_info.CImm.m_imm32);
		if (possible_syscall_entry >= (PVOID64)KVASCODE && possible_syscall_entry < (PVOID64)((ULONG64)KVASCODE + section_size))
		{
			continue;
		}

		//发现KiSystemServiceUser
		syscall_entry = possible_syscall_entry;
		break;
	}

	return syscall_entry;
}

//替换函数
ULONG64 self_get_cpu_clock()
{

	if (ExGetPreviousMode() == KernelMode)
	{
		return __rdtsc();
	}

	PKTHREAD pCurThread = (PKTHREAD)__readgsqword(_KPCR_CurrentThread); //获取当前线程

	if (!MmIsAddressValid(pCurThread))
	{
		return FALSE;
	}

	ULONG32 nSystemCallNumber = 0;
	nSystemCallNumber = *(PULONG32)((ULONG64)pCurThread + _KTHREAD_SystemCallNumber); //获取SSSDT调用号 SystemCallNumber

	PVOID64* stack_max = (PVOID64*)__readgsqword(_KPCR_RspBase);		//获取当前线程堆栈
	PVOID64* stack_frame = (PVOID64*)_AddressOfReturnAddress();			//获取上层调用地址
	//当前栈位置      栈顶位置		//一直找到上层调用的堆栈出  
	for (PVOID64* stack_current = stack_max; stack_current > stack_frame; --stack_current)
	{
#define INFINITYHOOK_MAGIC_1 ((ULONG32)0x501802)
#define INFINITYHOOK_MAGIC_2 ((USHORT)0xF33)

		PULONG32 l_value = (PULONG32)stack_current;
		if (!MmIsAddressValid((PVOID)l_value))
		{
			break;
		}

		if (*l_value != INFINITYHOOK_MAGIC_1)
		{
			continue;
		}

		--stack_current;

		PUSHORT s_value = (PUSHORT)stack_current;
		if (!MmIsAddressValid((PVOID)s_value))
		{
			break;
		}

		if (*s_value != INFINITYHOOK_MAGIC_2)
		{
			continue;
		}

		//走到此处说明找了特征码位置,接下来拿到要调用的函数地址
		for (; stack_current < stack_max; ++stack_current)
		{

			PULONG64 ull_value = (PULONG64)stack_current;
			if (!MmIsAddressValid((PVOID)ull_value))
			{
				break;
			}

			if (!(PAGE_ALIGN(*ull_value) >= g_SyscallTable && PAGE_ALIGN(*ull_value) < (PVOID64)((ULONG64)g_SyscallTable + (PAGE_SIZE * 2))))
			{
				continue;
			}

			PVOID64* system_call_function = &stack_current[9];

			if (MmIsAddressValid(g_pEtwFunCallBack))
			{
				g_pEtwFunCallBack(nSystemCallNumber, system_call_function);
			}
			break;
		}
		break;
	}
	return __rdtsc();
}

// Get image address
ULONG64 get_image_address(ULONG64 addr, PCHAR name, PULONG32 size)
{
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)addr;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return NULL;
	}

	PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(addr + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
	{
		return NULL;
	}

	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
	for (USHORT i = 0; i < nt->FileHeader.NumberOfSections; i++)
	{
		PIMAGE_SECTION_HEADER p = &section[i];

		if (strstr((PCHAR)p->Name, name))
		{
			if (size) *size = p->SizeOfRawData;
			return (ULONG64)p + p->VirtualAddress;
		}
	}
	return NULL;
}

//返回Hook运行状态
UCHAR IsEtwHookRun()
{
	return g_IsOpenEtw;
}