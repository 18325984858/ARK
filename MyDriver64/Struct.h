#pragma once


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define MY_MAX_PATH						0xFF
#define MAX_BASE_FILE_NAME				0x50
#define MYERROR							0xFFFFFFFFFFFF
#define MYLOWORD(y)						((UCHAR)(((USHORT)(y)) & 0xff))						/*Y*/
#define MYHIWORD(x)						((UCHAR)((((USHORT)(x)) >> 8) & 0xff))				/*X*/
#define BUILDETWHOOKLEVEL(x,y)			(((USHORT)(((USHORT)(x))<< 8) & 0xFF00)|((USHORT)(y)))
#define SSDT_MAX_NUMBER 0x255
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct _CLIST_ENTRY
{
	struct _CLIST_ENTRY* Flink;
	struct _CLIST_ENTRY* Blink;
} CLIST_ENTRY, * PCLIST_ENTRY;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct _CLIST_ENTRY_EX
{
	CLIST_ENTRY List;
	ULONG64 IsInitialize;													//是否初始化过了
} CLIST_ENTRY_EX, * PCLIST_ENTRY_EX;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct _HandleInfo
{
	CLIST_ENTRY_EX	List;													//存储进程链表
	ULONG64			Object;
} CHandleInfo, * PCHandleInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct _CTIME_FIELDS {
	USHORT Year;															// range [1601...]
	USHORT Month;															// range [1..12]
	USHORT Day;																// range [1..31]
	USHORT Hour;															// range [0..23]
	USHORT Minute;															// range [0..59]
	USHORT Second;															// range [0..59]
	USHORT Milliseconds;													// range [0..999]
	USHORT Weekday;															// range [0..6] == [Sunday..Saturday]
}CTIME_FIELDS, * PCTIME_FIELDS;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//命令枚举体																													
enum _CommunicatOpCode
{
	um_Cmd_Enum_Process_info,
	um_Cmd_Enum_ProcessVad_info,
	um_Cmd_Enum_ProcessThread_info,
	um_Cmd_Enum_ProcessHandle_info,
	um_Cmd_Enum_ProcessModule_info,
	um_Cmd_Enum_Driver_info,
	um_Cmd_Enum_File_info,
	um_Cmd_Enum_Registry_info,
	um_Cmd_Enum_Gdt_info,
	um_Cmd_Enum_Idt_info,
	um_Cmd_Enum_SSDT_info,
	um_Cmd_Enum_SSDTShadow_info,
	um_Cmd_Enum_KernelCallBack_info,
	um_Cmd_Enum_MiniFilterCallBack_info,
	um_Cmd_Enum_ObjectCallBack_info,
	um_Cmd_Enum_ObjectCallBackEx_info,
	um_Cmd_Enum_ObjectMajorFunction_info,
	um_Cmd_Enum_Dpc_info,
	um_Cmd_DeleteFile_info,
	um_Cmd_ReturnSsdtAndSsdtShadow_info,
	um_Cmd_FileDeoccupy_info,
	um_Cmd_KillProcess_info,
	um_Cmd_RWProcessMemOry_info,
	um_Cmd_HookSystemServiceTable_info,
	um_Cmd_Enum_HalTable_info,
	um_Cmd_Enum_SystemDevice_info,
	um_Cmd_Hook_Ssdt,
	um_Cmd_Init_Data,
	um_Cmd_Set_ProcessPortection,
	um_Cmd_Get_ProcessPortection,
	um_Cmd_Enum_FilterDriver_info,
	um_Cmd_DebugFlags_info,

	um_Cmd_Test,
	um_Cmd_Enum_Wdf01000_info,
	um_Cmd_Enum_WdfFunction_info,
	um_Cmd_Enum_WorkerThread_info,											//枚举内核工作线程队列

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//通信结构体	
typedef struct _CCommunicationInfo
{
	IN ULONG64 m_Cmd;														//命令
	IN ULONG64 m_pIndata;													//存储输入数据
	OUT PVOID64 m_pOutData;													//存储返回的数据
	OUT PVOID64 m_nRet;														//存储返回值
	IN PVOID64 m_pParam;													//备用
}CCommunicationInfo, * PCCommunicationInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储获取到的进程信息
typedef struct _ProcessInfo
{
	CHandleInfo	HandleInfo;													//存储进程链表
	CTIME_FIELDS CreateTime;												//运行时间
	ULONG32 FullFileNameLength;												//存储进程文件位置字符长度
	ULONG32 FullFileNameDrviceLength;										//存储进程文件位置字符长度
	ULONG64 ProcessId;														//存储进程ID
	ULONG64 ParentPId;														//存储进程父ID
	ULONG64 Eprocess;														//进程EPROCESS
	union
	{
		ULONG64 Flag1;														//存储进程标志
		struct
		{
			ULONG64 DebugPort : 1;											//是否调试状态
			ULONG64 IsUserVisit : 1;										//存储进程是否可以用户层访问
			ULONG64 Session : 4;											//会话ID
			ULONG64 Is64Process : 1;										//是否是64位进程 1:x64  0:x32
		};
	};
	ULONG64 Peb;															//进程三环PEB
	ULONG64 ImageBaseAddr;													//地址
	ULONG64 ImageBaseAddress;												//进程加载地址

	WCHAR CommandLine[0x500];												//命令行参数
	WCHAR UserName[MAX_BASE_FILE_NAME];										//存储进程用户名
	CHAR ImageBaseName[MAX_BASE_FILE_NAME];									//存储进程名字
	WCHAR FullFileName[MY_MAX_PATH];										//存储进程文件位置
	WCHAR FullFileNameDrvice[MY_MAX_PATH];									//存储进程文件位置
}CProcessInfo, * PCProcessInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储获取到的文件信息
typedef struct _FileInfo
{
	CLIST_ENTRY List;														//链表结构,存储整个信息
	ULONG64				IsInitialize;										//是否初始化过了
	CTIME_FIELDS		CreationTime;										//创建时间
	CTIME_FIELDS		ChangeTime;											//访问时间
	ULONG64				AllocationSize;										//文件大小
	ULONG				FileAttributes;										//文件属性
	WCHAR				FileFullName[MY_MAX_PATH];							//文件名
}CFileInfo, * PCFileInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储获取到的注册表信息
typedef struct _RegistryInfo
{
	CLIST_ENTRY List;														//链表结构,存储整个信息

	ULONG64 nType;															//结构体类型
	//0为 _KeyInfo结构
	//1为 _ValueInfo结构

	ULONG64 IsInitialize;													//是否初始化过了

	union
	{
		struct
		{
			WCHAR KeyName[MY_MAX_PATH];										//路径
		};

		struct
		{
			ULONG64 ValueType;												//值类型
			WCHAR ValueName[MY_MAX_PATH];									//值名称
			WCHAR ValueData[MY_MAX_PATH];									//值数据
		};
	};
}CRegistryInfo, * PCRegistryInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//GDT描述符
typedef struct _CGdt
{
	union
	{
		ULONG32 dwBaseAddr;													//64位使用
		struct
		{
			ULONG32 SegLimit0 : 16;
			ULONG32 BaseAddr0 : 16;											//
		};
	};

	union
	{
		ULONG32 dwBaseLimit;												//64位使用
		struct
		{
			ULONG32 BaseAddr1 : 8;
			ULONG32 Type : 4;												//当 s==1时 查(3.4.5.1 Code- and Data-Segment Descriptor Types) 当s==0时 查(3.5 SYSTEM DESCRIPTOR TYPES)
			ULONG32 S : 1;													//0 = system; 1 = code or data
			ULONG32 Dpl : 2;												//段权限等级 0-3
			ULONG32 P : 1;													//是否是有有效地址
			ULONG32 SegLimit1 : 4;
			ULONG32 AVL : 1;
			ULONG32 L : 1;													//0 == 32位模式; 1 == 64位长模式
			ULONG32 DB : 1;													//0 = 16-bit segment; 1 = 32-bit segment
			ULONG32 G : 1;													//粒度 0 == Byte ; 1 == PAGE
			ULONG32 BaseAddr2 : 8;
		};
	};

}CGdt, * PCGdt;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储遍历到的GDT信息
typedef struct _GdtInfo
{
	CLIST_ENTRY List;														//
	ULONG64 IsInitialize;													//是否初始化过了
	ULONG64 nCpuId;															//存储在第几号CPU上
	ULONG64 GdtBase;														//存储GDT地址
	ULONG64 nIndex;															//存储第几个描述符
	ULONG64 Is64Segment;													//是否是64位段
	CGdt GdtData;															//存储数据
	CGdt GdtData1;															//64位段使用
}CGdtInfo, * PCGdtInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct _CIdt
{

	union
	{
		ULONG32 LowByte;
		struct
		{
			ULONG32 Offset0 : 16;
			ULONG32 SegmentSelector : 16;
		};
	};

	union
	{
		ULONG32 HightByte;
		struct
		{
			ULONG32 Reserve0 : 8;
			ULONG32 Type : 4;												//类型 CallGate(1100) TSSAvailable(1001) TSSBusy(1011) InterruptGate(1110) TrapGate(1111)

			ULONG32 S : 1;													//0
			ULONG32 Dpl : 2;												//段权限等级 0-3
			ULONG32 P : 1;													//0
			ULONG32 Offset1 : 16;
		};
	};

	ULONG32 Offset2;
	ULONG32 Reserve1;
}CIdt, * PCIdt;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储遍历到的GDT信息
typedef struct _IdtInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG64 nCpuId;															//存储在第几号CPU上
	ULONG64 IdtBase;														//存储IDT地址
	ULONG64 nIndex;															//存储中断号
	CIdt IdtData;															//存储数据
	WCHAR szPath[MY_MAX_PATH];												//存储函数所在模块路径
	//WCHAR [MY_MAX_PATH];													//公司名
}CIdtInfo, * PCIdtInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储遍历到的VAD信息
typedef struct _ProcessVadInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG64 VadNode;														//存储数节点地址
	ULONG64 StartingVpn;													//内存起始地址
	ULONG64 EndingVpn;														//内存结束地址
	ULONG64 CommitCharge;													//引用计数
	union
	{
		ULONG64 Flags;														//标志
		struct
		{
			ULONG64 PrivateMemory : 1;										//Private : 1 Mapped : 0
			ULONG64 Protection : 5;											//页保护标志 : 读 写 执行
		};
	};
	WCHAR ExeFilePath[MY_MAX_PATH];											//文件名
}CProcessVadInfo, * PCProcessVadInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储遍历到的Thread信息
typedef struct _ProcessThreadInfo
{
	CLIST_ENTRY_EX List;													//
	CHAR Priority;															//存储优先级
	UCHAR State;															//存储状态
	ULONG32 ContextSwitches;												//存储切换次数
	ULONG64 UniqueThread;													//存储线程ID
	ULONG64 Ethread;														//存储线程对象
	ULONG64 Teb;															//存储TEB
	ULONG64 StartAddress;													//存储入口地址
	CTIME_FIELDS CreateTime;												//存储创建时间
	//WCHAR FileVendor[0x20];												//文件厂商
	WCHAR MoudleName[MY_MAX_PATH];											//存储模块名
	UCHAR ThreadTypeFlag;													//存储线程类型
}CProcessThreadInfo, * PCProcessThreadInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储遍历到的Module信息
typedef struct _ProcessModuleInfo
{
	CLIST_ENTRY_EX List;													//
	WCHAR ModuleName[MY_MAX_PATH];											//模块名
	ULONG64 ModuleBaseAddr;													//模块基址
	ULONG64 ModuleSize;														//模块大小
	WCHAR ModuleFullPath[MY_MAX_PATH];										//模块路径
	//WCHAR Signature[MAX_BASE_FILE_NAME];									//数字签名
	//WCHAR CoparyName[MAX_BASE_FILE_NAME];									//公司名
}CProcessModuleInfo, * PCProcessModuleInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储遍历到的Handle信息
typedef struct _ProcessHandleInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG64 Quote;															//引用
	ULONG64 Index;															//索引
	ULONG64 Power;															//权限
	ULONG64 HandleObject;													//句柄对象
	ULONG64 Handle;															//句柄
	WCHAR HandleName[MY_MAX_PATH];											//存储句柄名
	WCHAR HandleType[MAX_BASE_FILE_NAME];									//存储句柄类型名
}CProcessHandleInfo, * PCProcessHandleInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储遍历到驱动对象信息
typedef struct _DriverInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG32 ImageFullBaseNameLength;										//存储对象路径长度
	ULONG32 LoadOrder;														//加载顺序
	ULONG64 ImageBaseAddr;													//存储基地址
	ULONG64 Size;															//大小
	ULONG64 DriverObject;													//驱动对象
	ULONG64 DriverStart;													//驱动开始地址
	//WCHAR FileVendor[100];												//文件厂商
	WCHAR DriverName[MY_MAX_PATH];											//驱动名
	WCHAR ServerName[MY_MAX_PATH];											//服务名
	WCHAR ImageFullBaseName[MY_MAX_PATH];									//对象路径
	WCHAR ImageBaseName[MAX_BASE_FILE_NAME];								//存储进程名字
}CDriverInfo, * PCDriverInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储 SSSDT信息
typedef struct _SsdtInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG64 NumberOrder;													//函数序号
	ULONG64 ServiceNumber;													//函数服务号
	ULONG64 NtFunAddr;														//内核函数地址
	ULONG64 UsFunAddr;														//用户函数地址
	ULONG64 SrcNtFunAddr;													//新的内核地址
	UCHAR IsHook;															//是否被HOOK
	WCHAR FunName[MAX_BASE_FILE_NAME];										//函数名
	WCHAR Path[MY_MAX_PATH];												//函数所在模块
}CSsdtInfo, * PCSsdtInfo;

typedef struct _HookSsdtInfo
{
	ULONG32 State;															//状态
	ULONG32 Level;															//层级
	ULONG32 CallNumber;														//调用号
	WCHAR FunName[MAX_BASE_FILE_NAME];										//函数名
}CHookSsdtInfo, * PCHookSsdtInfo;

typedef struct _AlterHookSsdtInfo
{
	ULONG32 Index;															//要修改的索引
	CHookSsdtInfo BaseSsdtInfo;												//要修改的数据
}CAlterHookSsdtInfo, * PCAlterHookSsdtInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//回调类型
enum _KernalCallBackType
{
	um_KernalCallBackType_ShutDown = 0,
	um_KernalCallBackType_DbgCheck,
	um_KernalCallBackType_Pnp,
	um_KernalCallBackType_LoadImage,
	um_KernalCallBackType_CreateProcess,
	um_KernalCallBackType_CreateThread,
	um_KernalCallBackType_CreateRegistry,
	um_KernalCallBackType_IoTime,
};

//存储 内核回调信息
typedef struct _KernelCallBackInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG64 CallBackType;													//回调类型
	ULONG64 CallBackAddr;													//回调地址
	ULONG64 ModuleOffset;													//所在模块偏移
	ULONG64 Descr;															//备注
	WCHAR ModulePath[MY_MAX_PATH];											//所在模块路径
}CKernelCallBackInfo, * PCKernelCallBackInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储对象类型回调信息
typedef struct _ObjectTypeCallBackInfo
{
	CLIST_ENTRY_EX List;													//
	UCHAR ObjectCallBackType;												//0代表注册的回调 _ObjectTypeCallBackInfo 1代表对象存储的回调_ObjectCallBackExInfo
	WCHAR szObjectTypeName[MAX_BASE_FILE_NAME];								//类型名
	ULONG64 PreOperation;													//PreOperation
	ULONG64 PostOperation;													//PostOperation
	ULONG64 pHandle;														//句柄
	WCHAR Altitude[MAX_BASE_FILE_NAME];										//海拔
	WCHAR ModulePath[MY_MAX_PATH];											//所在模块路径
}CObjectTypeCallBackInfo, * PCObjectTypeCallBackInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储对象类型回调信息
#define OBJECT_TYPE_CALLBACK_MAX_NUMBER		0x8		//对象类型回调最大数量
enum ObjectCallBackExFunType
{
	um_ObjectTypeName_DumpProcedure = 0,
	um_ObjectTypeName_OpenProcedure,
	um_ObjectTypeName_CloseProcedure,
	um_ObjectTypeName_DeleteProcedure,
	um_ObjectTypeName_ParseProcedure,
	um_ObjectTypeName_SecurityProcedure,
	um_ObjectTypeName_QueryNameProcedure,
	um_ObjectTypeName_OkayToCloseProcedure,
};

typedef struct _ObjectTypeCallBackExFunInfo
{
	ULONG64 FunAddr;														//存储函数地址
	ULONG64 FunType;														//回调类型		ObjectCallBackExFunType
	WCHAR ModulePath[MY_MAX_PATH];											//回调所在模块
}CObjectCallBackExFunInfo, * PCObjectCallBackExFunInfo;

typedef struct _ObjectCallBackExInfo
{
	CLIST_ENTRY_EX List;													//
	UCHAR ObjectCallBackType;												//0代表注册的回调 _ObjectTypeCallBackInfo 1代表对象存储的回调_ObjectCallBackExInfo
	ULONG32 ValidAccessMask;
	CObjectCallBackExFunInfo FunAddr[OBJECT_TYPE_CALLBACK_MAX_NUMBER];		//数组

	//CLIST_ENTRY_EX FunAddrList;											//
	ULONG64 Object;															//存储对象
	WCHAR TypeName[MAX_BASE_FILE_NAME];										//存储类型名
}CObjectTypeCallBackExInfo, * PCObjectTypeCallBackExInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储回调类型信息
typedef struct _CallBackInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG64 dqFunctionAddr;													//回调地址
	ULONG64 dqObjectAddr;													//对象地址
	WCHAR szTypeName[MAX_BASE_FILE_NAME];									//类型名
	WCHAR szPath[MY_MAX_PATH];												//所在模块路径
}CCallBackInfo, * PCCallBackInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储MiniFilter回调信息
typedef struct _MiniFilterCallBackInfo
{
	CLIST_ENTRY_EX List;													//
	WCHAR szFilterTypeName[MAX_BASE_FILE_NAME];								//类型名
	ULONG64 PreOperation;													//PreOperation
	ULONG64 PostOperation;													//PostOperation
	ULONG64 pFilterAddr;													//过滤器地址
	WCHAR Altitude[MAX_BASE_FILE_NAME];										//海拔
	WCHAR ModulePath[MY_MAX_PATH];											//所在模块路径
}CMiniFilterCallBackInfo, * PCMiniFilterCallBackInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储MiniFilter回调信息
#define MAJORFUNCTION_MAX_NUMBER	(0x1C)									//IRP函数最大值
typedef struct _SysMajorFunctionInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG32 Ord;															//序号
	ULONG32 Type;															//IRP类型
	ULONG64 FunAddr;														//IRP函数地址
	WCHAR ModulePath[MY_MAX_PATH];											//所在模块路径
}CSysMajorFunctionInfo, * PCSysMajorFunctionInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//发包方需要填的
enum  UmFilterMessageDataType
{
	um_FilterMessageDataType_HookSSDT,
	um_FilterMessageDataType_GetStructOffset,
	um_FilterMessageDataType_GetStructSize,
	um_FilterMessageDataType_GetGlobalVariables,
};

typedef struct _FilterGetMessageHeadInfo
{
	FILTER_REPLY_HEADER Header;												//必须结构
	ULONG64 PackType;														//包类型 决定结构体
	ULONG64 PackSize;														//包大小
}CFilterGetMessageHeadInfo, * PCFilterGetMessageHeadInfo;

typedef struct _FilterUserGetMessageHeadInfo
{
	FILTER_MESSAGE_HEADER  Header;											//必须结构
	ULONG64 PackType;														//包类型 决定结构体
	ULONG64 PackSize;														//包大小
}CFilterUserGetMessageHeadInfo, * PCFilterUserGetMessageHeadInfo;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct _StructInfo
{
#define MAXBYTE     0x20 
	WCHAR szModuleName[MAXBYTE];											//要获取的模块名
	WCHAR szmemberName[MAXBYTE];											//要获取的结构体成员名
	WCHAR szClassType[MAXBYTE];												//要获取的结构体类型名
	ULONG64 nOffset;														//要获取的偏移量
}CStructInfo, * PCStructInfo;

typedef struct _FilterUserGetMessageStructInfo
{
	CFilterUserGetMessageHeadInfo Header;
	CStructInfo	StructInfo;													//存储要获取的结构体信息
}CFilterUserGetMessageStructInfo, * PCFilterUserGetMessageStructInfo;

typedef struct _GlobalVariables
{
#define MAXBYTE     0x20 
	WCHAR szModuleName[MAXBYTE];											//要获取的模块名
	WCHAR szVarName[MAXBYTE];												//要获取的结构体类型名
	ULONG64 nOffset;														//要获取的偏移量
}CGlobalVariables, * PCGlobalVariables;

typedef struct _FilterUserGetMessageGlobalVariables
{
	CFilterUserGetMessageHeadInfo Header;
	CGlobalVariables	VarInfo;											//存储要获取的结构体信息
}CFilterUserGetMessageGlobalVariables, * PCFilterUserGetMessageGlobalVariables;

typedef struct _FilterUserSendMessageGlobalVariables
{
	CFilterGetMessageHeadInfo Header;
	CGlobalVariables VarInfo;											//存储要获取的结构体信息
}CFilterUserSendMessageGlobalVariables, * PCFilterUserSendMessageGlobalVariables;

typedef struct _FilterUserSendtMessageStructInfo
{
	CFilterGetMessageHeadInfo Header;
	CStructInfo	StructInfo;													//存储要获取的结构体信息
}CFilterUserSendtMessageStructInfo, * PCFilterUserSendtMessageStructInfo;

typedef struct _StructSize
{
#define MAXBYTE     0x20 
	WCHAR szModuleName[MAXBYTE];											//要获取的模块名
	WCHAR szClassName[MAXBYTE];												//要获取的结构体类型名
	ULONG64 nSize;															//要获取的偏移量
}CStructSize, * PCStructSize;

typedef struct _FilterUserGetMessageStructSize
{
	CFilterUserGetMessageHeadInfo Header;
	CStructSize	StructInfo;													//存储要获取的结构体信息
}CFilterUserGetMessageStructSize, * PCFilterUserGetMessageStructSize;

typedef struct _FilterUserSendtMessageStructSize
{
	CFilterGetMessageHeadInfo Header;
	CStructSize	StructInfo;													//存储要获取的结构体信息
}CFilterUserSendtMessageStructSize, * PCFilterUserSendtMessageStructSize;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum UmPragmaType
{
	Pragma_Type_Char,
	Pragma_Type_Short,
	Pragma_Type_Int,
	Pragma_Type_Dword,
	Pragma_Type_Addr,
	Pragma_Type_String,
	Pragma_Type_WString,
	Pragma_Type_Float,
	Pragma_Type_Double,
};

typedef struct _PragmaType
{
	ULONG64 Type;															//参数类型 1:代表1字节类型 2:代表2字节类型 3:代表4字节类型 
	// 4:代表8字节类型 5:代表地址 6:代表char字符串地址 
	// 7:代表wchar字符串地址 8:代表单浮点 9:代表双浮点
	ULONG64 pPragma;														//地址或值
}CPragmaType, * PCPragmaType;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储服务调用最大参数数量
#define SSDT_PARAGMA_MAX_NUMBER		(14)
//记录要Hoook的函数信息
typedef struct _SSDTHookInfo
{
	CFilterUserGetMessageHeadInfo Header;
	DWORD PID;																//当前进程ID
	DWORD TID;																//线程ID
	ULONG64 FunCallNumber;													//函数调用号
	ULONG64 FunAddr;														//0环函数地址
	ULONG64 RetValue;														//存储返回值
	ULONG64 ParagmaNumber;													//有几个参数
	WCHAR FunName[MY_MAX_PATH];												//函数名
	//WCHAR StrFormat[MY_MAX_PATH];											//参数格式
	//WCHAR ProcessName[MY_MAX_PATH];										//进程名
	CProcessInfo ProcessInfo;												//存储进程信息
	CPragmaType Paragma[SSDT_PARAGMA_MAX_NUMBER];							//存储参数 //如果是字符串
}CSSDTHookInfo, * PCSSDTHookInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储DPC信息
typedef struct _DpcInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG64 DpcObject;														//存储当前DPC对象
	ULONG64 TimeObject;														//存储当前Time对象
	ULONG64 TriggerCycle;													//触发周期
	ULONG64 FunCtionStartAddr;												//函数入口
	WCHAR ModulePath[MY_MAX_PATH];											//函数所在模块路径
}CDPcInfo, * PCDPcInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储读写内存的信息
#define MAX_READ_SIZE				0x1000									/*存储读取的最大大小*/
#define RWMEMORY_READ				0x00									/*读*/
#define RWMEMORY_WRITE				0x01									/*写*/
typedef struct _RWMemoryInfo
{
	ULONG64 Eprocess;														//目标进程对象
	ULONG64 DstAddr;														//目标地址
	ULONG64 offset;															//偏移
	ULONG64 dqSize;															//大小		
	ULONG64 Mode;															//模式,1:写  0:读
	PUCHAR  Buf;															//缓冲区
}CRWMemoryInfo, * PCRWMemoryInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum HalTableType
{
	HalDispatchTable_Type = 1,
	HalPrivateDispatchTable_Type,
};

//存储遍历到的Hal表函数地址信息
typedef struct _HalFunTableInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG64 pFunOrder;														//存储函数序号
	ULONG64 pFunAddr;														//存储当前函数地址
	ULONG64 pSrcFunAddr;													//存储原函数地址
	ULONG64 HookType;														//存储Hook类型
	WCHAR ModulePath[MY_MAX_PATH];											//所在模块路径
}CHalFunTableInfo, * PCHalFunTableInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum FileSystemDeviceType
{
	um_FileSystemDeviceType_Disk,
	um_FileSystemDeviceType_CdRom,
	um_FileSystemDeviceType_Network,
	um_FileSystemDeviceType_Tape,
};

//文件系统信息
typedef struct _FileSystemDeviceInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG64 nType;															//FileSystemDeviceType 类型
	ULONG64 DeviceObject;													//设备对象
	ULONG64 DriverObject;													//驱动对象
	WCHAR DeviceName[MY_MAX_PATH];											//设备名称
	WCHAR DriverName[MY_MAX_PATH];											//驱动名称
}CFileSystemDeviceInfo, * PCFileSystemDeviceInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储 WDF 驱动的信息
typedef struct _WdfInfo
{
	CLIST_ENTRY_EX List;													//
	ULONG64 pFunOrder;														//存储函数序号
	ULONG64 pFunAddr;														//存储当前函数地址
	ULONG64 pSrcFunAddr;													//存储原函数地址
	ULONG64 HookType;														//存储Hook类型
	WCHAR ModulePath[MY_MAX_PATH];											//所在模块路径
}CWdfInfo, * PCWdfInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//SSDT表中API索引
enum UmSSDTMonitor
{
	um_SSDT_Monitor_NtAccessCheck = 0,
	um_SSDT_Monitor_NtWorkerFactoryWorkerReady,
	um_SSDT_Monitor_NtAcceptConnectPort,
	um_SSDT_Monitor_NtMapUserPhysicalPagesScatter,
	um_SSDT_Monitor_NtWaitForSingleObject,
	um_SSDT_Monitor_NtCallbackReturn,
	um_SSDT_Monitor_NtReadFile,
	um_SSDT_Monitor_NtDeviceIoControlFile,
	um_SSDT_Monitor_NtWriteFile,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//设置HookSsdt的结构
typedef struct _HookSsdtTableInfo
{
	ULONG32 FunNumber;														//要设置的函数调用号
	ULONG32 State;															//要设置的函数状态0关闭Hook,1开启Hook
}CHookSsdtTableInfo, * PCHookSsdtTableInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum umUserWmMonitorMessageType
{
	wm_User_DlgProcessMonitor_Insert = 0x0401,
	wm_User_DlgProcessMonitor_InsertApi,
	wm_User_DlgProcessMonitor_AlterApi,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define PS_PROTECTION_TYPE_MASK     0x07    // 低 3 位 (0b00000111)
#define PS_PROTECTION_AUDIT_MASK    0x08    // 第 4 位 (0b00001000)
#define PS_PROTECTION_SIGNER_MASK   0xF0    // 高 4 位 (0b11110000)

// 提取 Type（bit 0-2）
#define PS_PROTECTION_GET_TYPE(Level)        ((Level) & PS_PROTECTION_TYPE_MASK)

// 提取 Audit（bit 3）
#define PS_PROTECTION_GET_AUDIT(Level)       (((Level) & PS_PROTECTION_AUDIT_MASK) >> 3)

// 提取 Signer（bit 4-7）
#define PS_PROTECTION_GET_SIGNER(Level)      (((Level) & PS_PROTECTION_SIGNER_MASK) >> 4)

// 设置 Type（bit 0-2）
#define PS_PROTECTION_SET_TYPE(Level, Type)  ((Level) = ((Level) & ~PS_PROTECTION_TYPE_MASK) | ((Type) & PS_PROTECTION_TYPE_MASK))

// 设置 Audit（bit 3）
#define PS_PROTECTION_SET_AUDIT(Level, Audit) ((Level) = ((Level) & ~PS_PROTECTION_AUDIT_MASK) | (((Audit) & 1) << 3))

// 设置 Signer（bit 4-7）
#define PS_PROTECTION_SET_SIGNER(Level, Signer) ((Level) = ((Level) & ~PS_PROTECTION_SIGNER_MASK) | (((Signer) & 0xF) << 4))

// 保护类型（Type）
typedef enum _PS_PROTECTED_TYPE {
	PsProtectedTypeNone = 0,
	PsProtectedTypeProtectedLight = 1,
	PsProtectedTypeProtected = 2,
	PsProtectedTypeMax = 3
} PS_PROTECTED_TYPE;

// 签名级别（Signer）
typedef enum _PS_PROTECTED_SIGNER {
	PsProtectedSignerNone = 0,
	PsProtectedSignerAuthenticode = 1,
	PsProtectedSignerCodeGen = 2,
	PsProtectedSignerAntimalware = 3,
	PsProtectedSignerLsa = 4,
	PsProtectedSignerWindows = 5,
	PsProtectedSignerWinTcb = 6,
	PsProtectedSignerWinSystem = 7,
	PsProtectedSignerApp = 8
} PS_PROTECTED_SIGNER;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define FILTER_DEVICE_TYPENAME_LEN 0x20
#define FILTER_DEVICE_NAME_LEN FILTER_DEVICE_TYPENAME_LEN
//存储文件过滤设备信息
typedef struct _FilterDeviceInfo
{
	CLIST_ENTRY_EX List;													//
	WCHAR TypeName[FILTER_DEVICE_TYPENAME_LEN];								//过滤驱动类型
	WCHAR FilterDriverName[MY_MAX_PATH];									//过滤驱动名
	WCHAR FilterDriverPath[MY_MAX_PATH];									//过滤驱动路径
	WCHAR SrcDriverName[MY_MAX_PATH];										//原驱动名
	ULONG64 FilterDeviceObject;												//过滤设备对象
	WCHAR FilterDeviceName[FILTER_DEVICE_NAME_LEN];							//过滤设备名																														  //文件厂商
}CFilterDeviceInfo, * PCFilterDeviceInfo;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//存储调试标志结构体
#define USER_GET_DEBUG_FLAG		0x1		//获取调试标志
#define USER_SET_DEBUG_FLAG		0x0		//设置调试标志

typedef struct _DebugFlagInfo
{
	union
	{
		ULONG64 DebugFlag;														//存储标志
		struct
		{
			ULONG64 UserOperate : 1;											//用户操作 1：代表获取 0：代表设置
			ULONG64 kdDebuggerEnable : 1;
			ULONG64 KdDebuggerNotPresent : 1;
			ULONG64 SharedDataKdDebuggerEnabled : 1;
			ULONG64 KdPitchDebugger : 1;
		};
	};
}CDebugFlagInfo, * PCDebugFlagInfo;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
