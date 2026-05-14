#pragma once
#include "../Head.h"
#include "../DataStruct/CList.h"

////////////////////////////////////////////////////////////////////////
// ETW操作类型
typedef enum _trace_type
{
	start_trace = 1,
	stop_trace = 2,
	query_trace = 3,
	syscall_trace = 4,
	flush_trace = 5
}trace_type;
////////////////////////////////////////////////////////////////////////

typedef struct _WNODE_HEADER
{
	ULONG BufferSize;
	ULONG ProviderId;
	union {
		ULONG64 HistoricalContext;
		struct {
			ULONG Version;
			ULONG Linkage;
		};
	};
	union {
		HANDLE KernelHandle;
		LARGE_INTEGER TimeStamp;
	};
	GUID Guid;
	ULONG ClientContext;
	ULONG Flags;
} WNODE_HEADER, * PWNODE_HEADER;

typedef struct _EVENT_TRACE_PROPERTIES
{
	WNODE_HEADER Wnode;
	ULONG BufferSize;
	ULONG MinimumBuffers;
	ULONG MaximumBuffers;
	ULONG MaximumFileSize;
	ULONG LogFileMode;
	ULONG FlushTimer;
	ULONG EnableFlags;
	union {
		LONG AgeLimit;
		LONG FlushThreshold;
	} DUMMYUNIONNAME;
	ULONG NumberOfBuffers;
	ULONG FreeBuffers;
	ULONG EventsLost;
	ULONG BuffersWritten;
	ULONG LogBuffersLost;
	ULONG RealTimeBuffersLost;
	HANDLE LoggerThreadId;
	ULONG LogFileNameOffset;
	ULONG LoggerNameOffset;
} EVENT_TRACE_PROPERTIES, * PEVENT_TRACE_PROPERTIES;

typedef struct _CKCL_TRACE_PROPERIES
{
	EVENT_TRACE_PROPERTIES m_event;
	ULONG64 Unknown[3];
	UNICODE_STRING ProviderName;
} CKCL_TRACE_PROPERTIES, * PCKCL_TRACE_PROPERTIES;

//存储Hook链表中最大的值
#define ETW_HOOK_LEVEN_MAX_VALUE 0xFF
//存储Hook链表中头节点的值
#define ETW_HOOK_LEVEN_HEAD_VALUE 0x00

typedef struct _EtwHook
{
	PCListNode ParentListNode;				//存储父节点指针
	CList AttachList;						//附加的链表
	PCList pCurList;						//当前节点所在的链表
	union
	{
		ULONG64 nFlags;						//标志
		struct
		{
			ULONG64 nIsAttach : 1;			//存储是否有被Attach的数据 0代表没有 1代表有
			ULONG64 nInitAttachList : 1;	//存储是否初始化了Attach链表 0代表没有 1代表有
			ULONG64 nIsRootNode : 1;		//判断是否是根节点 这个节点只负责自己用 不能更改删除用来监控SSSDT表
			ULONG64 nIsValid : 1;			//判断是否有效 当为1时表示有效 当为0时表示无效
			ULONG64 nIndex : 8;				//表示这在链表中是第几个 最多0xFF个
			ULONG64 nIsMonitor : 1;			//是否开启了监控
			ULONG64 nLeven : 16;			//层级16位 高8为代表X 低8位代表Y
			ULONG64 nMaxLeven : 16;			//当前最大层级
		};
	};

	ULONG64 SrcAddr;						//记录要HOOK的原地址
	ULONG64 CallAddr;						//记录要Call的地址

}CEtwHook, * PCEtwHook;

//函数回调指针
typedef void(__fastcall* fptr_call_back)(unsigned long ssdt_index, void** ssdt_address);

EXTERN_C CList g_EtwHookList;												//存储修改的也内存属性地址
EXTERN_C ULONG g_IsOpenEtw;													//存储先前的Etw是否开启
EXTERN_C ULONG64 g_OldCpuClock;												//存储先前的GetCpuClock
EXTERN_C ULONG64 g_pCpuClock;												//存储GetCpuClock的指针
EXTERN_C ULONG64 g_SyscallTable;											//存储系统掉用首地址
EXTERN_C ULONG64 g_OldHalpPerformanceCounter;								//存储旧的HalpPerformanceCounter
//EXTERN_C ULONG64 HalPrivateDispatchTable;									//存储Ntoskrnl中HalPrivateDispatchTable全局变量
EXTERN_C ULONG64 HalpPerformanceCounter;									//存储HalpTimerQueryHostPerformanceCounter中使用的全局变量HalpPerformanceCounter
EXTERN_C fptr_call_back g_pEtwFunCallBack;									//存储要执行的函数

EXTERN_C ULONG64 g_MyHalpHvCounterQueryCounterAddr;							//存储要调用的函数指针
EXTERN_C ULONG64 g_OldHalpPerformanceCounter;								//存储旧的HalpPerformanceCounter
EXTERN_C ULONG64 g_circularKernelContextLogger;								//存储原Context结构体地址
EXTERN_C VOID checkLogger();												//ETW跳板函数

//地址比较函数,用于传递给链表回调 比较原函数和目标函数地址
DWORD EtwCmpSrcAndDstAddr(PCEtwHook SrcAddr, PCEtwHook DstAddr);

//地址比较函数,用于传递给链表回调 只比较源函数地址
DWORD EtwCmpSrcAddr(PCEtwHook SrcAddr, PCEtwHook DstAddr);

////////////////////////////////////////////////////////////////////////
//功能:初始化ETW相关信息
//参数一:要执行的函数地址
ULONG64 EtwInit(ULONG64 pFunCallBack);																								//初始化Etw
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//功能:运行ETWHOOK
ULONG64 EtwStart();																													//运行Etw
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//功能:停止运行ETWHOOK
ULONG64 EtwStop();																													//停止Etw
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
VOID keQueryPerformanceCounterHook(ULONG_PTR pStack);																				//自己的函数
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
ULONG64 self_get_cpu_clock();																										//获取调用函数地址位置
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
PVOID64 get_syscall_entry(ULONG64 ntoskrnl);																						//获取调用表
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
ULONG64 get_image_address(ULONG64 addr, PCHAR name, PULONG32 size);																	//获取模块基地址
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//功能:添加要Hook的地址								2024年8月24日19:23:08
//参数一:原函数地址
//参数二:调用的地址
//返回值:添加成功返回真,失败返回假
PCEtwHook HookAddr(ULONG64 SrcAddr/*原地址*/, ULONG64 CallAddt/*要替换的地址*/, USHORT SrcLeven/*要插入的层级 0xFF00代表X 0x00FF代表Y*/);
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//功能:卸载Hook										2024年8月24日19:23:26
//参数一:调用HookAddr函数的返回值
//返回值:成功返回真,失败返回FALSE
ULONG64 UnHookAddr(PCEtwHook SrcAddr/*要删除的地址,原函数地址*/);
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//功能:查找节点										2024年8月24日19:23:20
//参数一:要查找的数据结构体,此结构是在调用HookAddr函数返回的
//参数二:要对比方式,内部会调用此参数进行对比
//参数三:传出参数 查询到节点是返回节点所在的链表
//返回值:成功返回查询到的节点,失败返回NULL
PCListNode FindEtwHookData(PCEtwHook SrcData, CMPFUNPTRCALLBACK pfun, PCList* pOutList);
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//功能:查询要指定链表中可以插入的位置					2024年8月24日19:23:32
//参数一:要查询的链表
//参数二:要插入的层级
//参数三:要插入的数据
//参数四:传出参数 可为NULL 当查询到节点之后此参数将被赋值为当前节点链表
//参数五:传出参数 可为NULL 当要插入的数据为附加链表时会设置此参数
//返回值:成功返回要插入节点的前一个节点,失败返回NULL
PCListNode FindInsertDataNode(PCList plist, USHORT SrcLeven, PCEtwHook SrcData, PCList* pOutList, PCListNode* pParentNode);
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//功能:修复链表中Index的值确保是连续的					2024年8月24日19:23:40
//参数一:链表首地址
//返回值:无
void RepairEtwData(PCList pList);
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
//功能:返回EtwHook当前运行的状态						2024年9月1日000:444:40
//返回值:运行返回TRUE,停止返回FALSE
UCHAR IsEtwHookRun();
////////////////////////////////////////////////////////////////////////


//ZwTraceControl内核函数声明
EXTERN_C NTSYSCALLAPI NTSTATUS NTAPI ZwTraceControl(
	_In_ ULONG FunctionCode,
	_In_reads_bytes_opt_(InBufferLen) PVOID InBuffer,
	_In_ ULONG InBufferLen,
	_Out_writes_bytes_opt_(OutBufferLen) PVOID OutBuffer,
	_In_ ULONG OutBufferLen,
	_Out_ PULONG ReturnLength);