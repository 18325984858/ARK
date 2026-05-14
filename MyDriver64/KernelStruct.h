#pragma once
#include <ntddk.h>
#include "Struct.h"

#define NUMBER_HASH_BUCKETS 37
#pragma pack(push)
#pragma pack(8)
typedef struct CObjectCallBackInfo;
typedef struct CObjectCallBackInfoExtend;
typedef struct _OBJECT_DIRECTORY* POBJECT_DIRECTORY;
typedef struct _OBJECT_DIRECTORY_ENTRY* POBJECT_DIRECTORY_ENTRY;
typedef struct _DEVICE_MAP* PDEVICE_MAP;
typedef struct _MY_CALLBACK_OBJECT* PMY_CALLBACK_OBJECT;
typedef struct _MY_CALLBACK_REGISTRATION* PMY_CALLBACK_REGISTRATION;

typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemBasicInformation,
	SystemProcessorInformation,             // obsolete...delete
	SystemPerformanceInformation,
	SystemTimeOfDayInformation,
	SystemPathInformation,
	SystemProcessInformation,
	SystemCallCountInformation,
	SystemDeviceInformation,
	SystemProcessorPerformanceInformation,
	SystemFlagsInformation,
	SystemCallTimeInformation,
	SystemModuleInformation,
	SystemLocksInformation,
	SystemStackTraceInformation,
	SystemPagedPoolInformation,
	SystemNonPagedPoolInformation,
	SystemHandleInformation,
	SystemObjectInformation,
	SystemPageFileInformation,
	SystemVdmInstemulInformation,
	SystemVdmBopInformation,
	SystemFileCacheInformation,
	SystemPoolTagInformation,
	SystemInterruptInformation,
	SystemDpcBehaviorInformation,
	SystemFullMemoryInformation,
	SystemLoadGdiDriverInformation,
	SystemUnloadGdiDriverInformation,
	SystemTimeAdjustmentInformation,
	SystemSummaryMemoryInformation,
	SystemMirrorMemoryInformation,
	SystemPerformanceTraceInformation,
	SystemObsolete0,
	SystemExceptionInformation,
	SystemCrashDumpStateInformation,
	SystemKernelDebuggerInformation,
	SystemContextSwitchInformation,
	SystemRegistryQuotaInformation,
	SystemExtendServiceTableInformation,
	SystemPrioritySeperation,
	SystemVerifierAddDriverInformation,
	SystemVerifierRemoveDriverInformation,
	SystemProcessorIdleInformation,
	SystemLegacyDriverInformation,
	SystemCurrentTimeZoneInformation,
	SystemLookasideInformation,
	SystemTimeSlipNotification,
	SystemSessionCreate,
	SystemSessionDetach,
	SystemSessionInformation,
	SystemRangeStartInformation,
	SystemVerifierInformation,
	SystemVerifierThunkExtend,
	SystemSessionProcessInformation,
	SystemLoadGdiDriverInSystemSpace,
	SystemNumaProcessorMap,
	SystemPrefetcherInformation,
	SystemExtendedProcessInformation,
	SystemRecommendedSharedDataAlignment,
	SystemComPlusPackage,
	SystemNumaAvailableMemory,
	SystemProcessorPowerInformation,
	SystemEmulationBasicInformation,
	SystemEmulationProcessorInformation,
	SystemExtendedHandleInformation,
	SystemLostDelayedWriteInformation,
	SystemBigPoolInformation,
	SystemSessionPoolTagInformation,
	SystemSessionMappedViewInformation,
	SystemHotpatchInformation,
	SystemObjectSecurityMode,
	SystemWatchdogTimerHandler,
	SystemWatchdogTimerInformation,
	SystemLogicalProcessorInformation,
	SystemWow64SharedInformation,
	SystemRegisterFirmwareTableInformationHandler,
	SystemFirmwareTableInformation,
	SystemModuleInformationEx,
	SystemVerifierTriageInformation,
	SystemSuperfetchInformation,
	SystemMemoryListInformation,
	SystemFileCacheInformationEx,
	MaxSystemInfoClass  // MaxSystemInfoClass should always be the last enum
} SYSTEM_INFORMATION_CLASS;
typedef struct _RTL_PROCESS_MODULE_INFORMATION {
	HANDLE Section;                 // Not filled in
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR  FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, * PRTL_PROCESS_MODULE_INFORMATION;
typedef struct _RTL_PROCESS_MODULES {
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, * PRTL_PROCESS_MODULES;


typedef struct _LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	ULONG64 DllBase;
	ULONG64 EntryPoint;
	ULONG32 SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	union
	{
		UCHAR FlagGroup[4];
		ULONG32 Flags;
		struct
		{
			ULONG32 PackagedBinary : 1;										//pos : 1
			ULONG32 MarkedForRemoval : 1;									//pos : 2
			ULONG32 ImageDll : 1;											//pos : 3
			ULONG32 LoadNotificationsSent : 1;								//pos : 4
			ULONG32 TelemetryEntryProcessed : 1;							//pos : 5
			ULONG32 ProcessStaticImport : 1;								//pos : 6
			ULONG32 InLegacyLists : 1;										//pos : 7
			ULONG32 InIndexes : 1;											//pos : 8
			ULONG32 ShimDll : 1;											//pos : 9
			ULONG32 InExceptionTable : 1;									//pos : 10
			ULONG32 ReservedFlags1 : 2;										//pos : 12
			ULONG32 LoadInProgress : 1;										//pos : 13
			ULONG32 LoadConfigProcessed : 1;								//pos : 14
			ULONG32 EntryProcessed : 1;										//pos : 15
			ULONG32 ProtectDelayLoad : 1;									//pos : 16
			ULONG32 ReservedFlags3 : 2;										//pos : 18
			ULONG32 DontCallForThreads : 1;									//pos : 19
			ULONG32 ProcessAttachCalled : 1;								//pos : 20
			ULONG32 ProcessAttachFailed : 1;								//pos : 21
			ULONG32 CorDeferredValidate : 1;								//pos : 22
			ULONG32 CorImage : 1;											//pos : 23
			ULONG32 DontRelocate : 1;										//pos : 24
			ULONG32 CorILOnly : 1;											//pos : 25
			ULONG32 ChpeImage : 1;											//pos : 26
			ULONG32 ReservedFlags5 : 2;										//pos : 28
			ULONG32 Redirected : 1;											//pos : 29
			ULONG32 ReservedFlags6 : 2;										//pos : 31
			ULONG32 CompatDatabaseProcessed : 1;							//pos : 32
		};
	};
	USHORT ObsoleteLoadCount;
	USHORT TlsIndex;
	LIST_ENTRY64 HashLinks;
	ULONG32 TimeDateStamp;
	ULONG64 EntryPointActivationContext;									//_ACTIVATION_CONTEXT
	ULONG64 Lock;
	ULONG64 DdagNode;														//_LDR_DDAG_NODE
	LIST_ENTRY64 NodeModuleLink;
	ULONG64 LoadContext;													//_LDRP_LOAD_CONTEXT
	ULONG64 ParentDllBase;
	ULONG64 SwitchBackContext;
	PRTL_BALANCED_NODE BaseAddressIndexNode;
	PRTL_BALANCED_NODE MappingInfoIndexNode;
	ULONG64 OriginalBase;
	PLARGE_INTEGER LoadTime;
	ULONG32 BaseNameHashValue;
	ULONG32 LoadReason;														//_LDR_DLL_LOAD_REASON
	ULONG32 ImplicitPathOptions;
	ULONG32 ReferenceCount;
	ULONG32 DependentLoadFlags;
	UCHAR SigningLevel;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;
typedef struct _EX_FAST_REF
{
	union
	{
		ULONG64 Object;
		struct
		{
			ULONG64 RefCnt : 4;
		};
		ULONG64 Value;
	};

}EX_FAST_REF, * PEX_FAST_REF;
typedef struct _MMVAD_SHORT
{
	union
	{
		ULONG32 LongFlags1;
		struct _MMVAD_FLAGS1
		{
			ULONG32 CommitCharge : 31;
			ULONG32 MemCommit : 1;
		};
		struct  VadFlags
		{
			ULONG32 Lock : 1;
			ULONG32 LockContended : 1;
			ULONG32 DeleteInProgress : 1;
			ULONG32 NoChange : 1;
			ULONG32 VadType : 3;
			ULONG32 Protection : 5;
			ULONG32		PreferredNode : 6;
			ULONG32 PageSize : 2;
			ULONG32 PrivateMemory : 1;
		};
		/*struct PrivateVadFlags
		{
			ULONG32 Lock : 1;
			ULONG32 LockContended : 1;
			ULONG32 DeleteInProgress : 1;
			ULONG32 NoChange : 1;
			ULONG32 VadType : 3;
			ULONG32 Protection : 5;
			ULONG32 PreferredNode : 6;
			ULONG32 PageSize : 2;
			ULONG32 PrivateMemoryAlwaysSet : 1;
			ULONG32 WriteWatch : 1;
			ULONG32 FixedLargePageSize : 1;
			ULONG32 ZeroFillPagesOptional : 1;
			ULONG32 Graphics : 1;
			ULONG32 Enclave : 1;
			ULONG32 ShadowStack : 1;
			ULONG32 PhysicalMemoryPfnsReferenced : 1;
		};*/
		/*struct GraphicsVadFlags
		{
			ULONG32 Lock : 1;
			ULONG32 LockContended : 1;
			ULONG32 DeleteInProgress : 1;
			ULONG32 NoChange : 1;
			ULONG32 VadType : 3;
			ULONG32 Protection : 5;
			ULONG32 PreferredNode : 6;
			ULONG32 PageSize : 2;
			ULONG32 PrivateMemoryAlwaysSet : 1;
			ULONG32 WriteWatch : 1;
			ULONG32 FixedLargePageSize : 1;
			ULONG32 ZeroFillPagesOptional : 1;
			ULONG32 GraphicsAlwaysSet : 1;
			ULONG32 GraphicsUseCoherentBus : 1;
			ULONG32 GraphicsNoCache : 1;
			ULONG32 GraphicsPageProtection : 3;
		};*/
		/*struct SharedVadFlags
		{
			ULONG32 Lock : 1;
			ULONG32 LockContended : 1;
			ULONG32 DeleteInProgress : 1;
			ULONG32 NoChange : 1;
			ULONG32 VadType : 3;
			ULONG32 Protection : 5;
			ULONG32 PreferredNode : 6;
			ULONG32 PageSize : 2;
			ULONG32 PrivateMemoryAlwaysClear : 1;
			ULONG32 PrivateFixup : 1;
			ULONG32 HotPatchAllowed : 1;
		};*/
		ULONG32 VolatileVadLong;
	};
}MMVAD_SHORT, * PMMVAD_SHORT;
typedef struct PROCESS_HANDLE_STRUCT
{
	ULONG64 TableAddr;														//存储进程私有句柄表前8字节,句柄对象
	ULONG64 Power;															//存储进程私有句柄表后8字节,权限
}PROCESS_HANDLE_STRUCT, * PPROCESS_HANDLE_STRUCT;
typedef struct _OBJECT_TYPE_INITIALIZER
{
	USHORT GetStackLength;
	union
	{
		USHORT ObjectTypeFlags;
		struct
		{
			USHORT CaseInsensitive : 1;										//pos : 1
			USHORT UnnamedObjectsOnly : 1;									//pos : 2
			USHORT UseDefaultObject : 1;									//pos : 3
			USHORT SecurityRequired : 1;									//pos : 4
			USHORT MaintainHandleCount : 1;									//pos : 5
			USHORT MaintainTypeList : 1;									//pos : 6
			USHORT SupportsObjectCallbacks : 1;								//pos : 7
			USHORT CacheAligned : 1;										//pos : 8
		};
	};
	union
	{
		UCHAR VUseExtendedParameters;
		struct
		{
			UCHAR UseExtendedParameters : 1;								//pos : 1
			UCHAR Reserved : 7;												//pos : 8
		};
	};
	ULONG32 ObjectTypeCode;
	ULONG32 InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	ULONG32 ValidAccessMask;
	ULONG32 RetainAccess;
	POOL_TYPE PoolType;
	ULONG32 DefaultPagedPoolCharge;
	ULONG32 DefaultNonPagedPoolCharge;
	ULONG64 DumpProcedure;
	ULONG64 OpenProcedure;
	ULONG64 CloseProcedure;
	ULONG64 DeleteProcedure;
	ULONG64 ParseProcedure;
	ULONG64 ParseProcedureEx;
	ULONG64 SecurityProcedure;
	ULONG64 QueryNameProcedure;
	ULONG64 OkayToCloseProcedure;
	ULONG32 WaitObjectFlagMask;
	USHORT WaitObjectFlagOffset;
	USHORT WaitObjectPointerOffset;
}OBJECT_TYPE_INITIALIZER, * POBJECT_TYPE_INITIALIZER;
typedef struct _OBJECT_TYPE
{
	LIST_ENTRY64 TypeList;
	UNICODE_STRING Name;
	ULONG64 DefaultObject;
	UCHAR Index;
	ULONG32 TotalNumberOfObjects;
	ULONG32 TotalNumberOfHandles;
	ULONG32 HighWaterNumberOfObjects;
	ULONG32 HighWaterNumberOfHandles;
	OBJECT_TYPE_INITIALIZER TypeInfo;
	ULONG64 TypeLock;
	ULONG32 Key;
	LIST_ENTRY64 CallbackList;
}OBJECT_TYPE, * POBJECT_TYPE;
typedef struct _OBJECT_DIRECTORY_ENTRY
{
	POBJECT_DIRECTORY_ENTRY ChainLink;
	ULONG64 Object;
	ULONG32 HashValue;
}OBJECT_DIRECTORY_ENTRY, * POBJECT_DIRECTORY_ENTRY;
typedef struct _OBJECT_DIRECTORY
{
	POBJECT_DIRECTORY_ENTRY HashBuckets[NUMBER_HASH_BUCKETS];
	EX_PUSH_LOCK Lock;
	PDEVICE_MAP DeviceMap;
	POBJECT_DIRECTORY ShadowDirectory;
	ULONG64 NamespaceEntry;
	ULONG64 SessionObject;
	ULONG32 Flags;
	ULONG32 SessionId;
}OBJECT_DIRECTORY, * POBJECT_DIRECTORY;
typedef struct _DEVICE_MAP
{
	POBJECT_DIRECTORY DosDevicesDirectory;
	POBJECT_DIRECTORY GlobalDosDevicesDirectory;
	PULONG64 DosDevicesDirectoryHandle;
	INT ReferenceCount;
	ULONG32 DriveMap;
	UCHAR DriveType[32];
	PEJOB ServerSilo;														//_EJOB
}DEVICE_MAP, * PDEVICE_MAP;
typedef struct _MY_CALLBACK_OBJECT
{
	ULONG Signature;				//0x6C6C6143
	KSPIN_LOCK Lock;
	LIST_ENTRY RegisteredCallbacks;	//PMY_CALLBACK_REGISTRATION
	BOOLEAN AllowMultipleCallbacks;
	UCHAR reserved[3];
} MY_CALLBACK_OBJECT, * PMY_CALLBACK_OBJECT;
typedef struct _MY_CALLBACK_REGISTRATION
{
	LIST_ENTRY Link;
	PMY_CALLBACK_OBJECT CallbackObject;
	PCALLBACK_FUNCTION CallbackFunction;
	PVOID CallbackContext;
	ULONG Busy;
	BOOLEAN UnregisterWaiting;
} MY_CALLBACK_REGISTRATION, * PMY_CALLBACK_REGISTRATION;
typedef struct _OBJECT_HEADER_NAME_INFO
{
	POBJECT_DIRECTORY Directory;
	UNICODE_STRING Name;
	INT ReferenceCount;
	ULONG32 Reserved;
}OBJECT_HEADER_NAME_INFO, * POBJECT_HEADER_NAME_INFO;
typedef struct _KSYSTEM_SERVICE_TABLE
{
	ULONG64 ServiceTableBase;												// 函数地址表
	ULONG64 ServiceCounterTableBase;										// SSDT 函数被调用的次数
	ULONG64 NumberOfService;												// 函数个数
	ULONG64 ParamTableBase;													// 函数参数表
} KSYSTEM_SERVICE_TABLE, * PKSYSTEM_SERVICE_TABLE;
typedef struct _PageAddrInfo
{
	ULONG64 PteBase;					//指向线性地址的Pte
	ULONG64 PdeBase;					//指向线性地址的Pde
	ULONG64 PpeBase;					//指向线性地址的Ppe
	ULONG64 PxeBase;					//指向线性地址的Pxe
}CPageAddrInfo, * PCPageAddrInfo;
typedef struct _KTIMER_TABLE_ENTRY
{
	ULONG64 Lock;
	LIST_ENTRY Entry;
	ULARGE_INTEGER Time;
}KTIMER_TABLE_ENTRY, * PKTIMER_TABLE_ENTRY;
typedef struct _KTIMER_TABLE_STATE
{
	ULONG64 LastTimerExpiration[2];
	ULONG32 LastTimerHand[2];
}KTIMER_TABLE_STATE, * PKTIMER_TABLE_STATE;
typedef struct _KTIMER_TABLE
{
#define TIMEREXPIRY_MAX_NUMBER 64
#define TIMERENTRIES_MAX_NUMBER_UP 2
#define TIMERENTRIES_MAX_NUMBER_DOWN 256

	PKTIMER TimerExpiry[TIMEREXPIRY_MAX_NUMBER];
	PKTIMER_TABLE_ENTRY TimerEntries[TIMERENTRIES_MAX_NUMBER_UP][TIMERENTRIES_MAX_NUMBER_DOWN];
	PKTIMER_TABLE_STATE TableState;
}KTIMER_TABLE, * PKTIMER_TABLE;
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO {
	USHORT UniqueProcessId;
	USHORT CreatorBackTraceIndex;
	UCHAR ObjectTypeIndex;
	UCHAR HandleAttributes;
	USHORT HandleValue;
	PVOID Object;
	ULONG GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, * PSYSTEM_HANDLE_TABLE_ENTRY_INFO;
typedef struct _SYSTEM_HANDLE_INFORMATION {
	ULONG NumberOfHandles;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;
typedef struct _HAL_PRIVATE_DISPATCH
{
	ULONG Version;
	ULONG64 HalHandlerForBus;
	ULONG64 HalHandlerForConfigSpace;
	ULONG64 HalLocateHiberRanges;
	ULONG64 HalRegisterBusHandler;
	ULONG64 HalSetWakeEnable;
	ULONG64 HalSetWakeAlarm;
	ULONG64 HalPciTranslateBusAddress;
	ULONG64 HalPciAssignSlotResources;
	ULONG64 HalHaltSystem;
	ULONG64 HalFindBusAddressTranslation;
	ULONG64 HalResetDisplay;
	ULONG64 HalAllocateMapRegisters;
	ULONG64 KdSetupPciDeviceForDebugging;
	ULONG64 KdReleasePciDeviceForDebugging;
	ULONG64 KdGetAcpiTablePhase0;
	ULONG64 KdCheckPowerButton;
	ULONG64 HalVectorToIDTEntry;
	ULONG64 KdMapPhysicalMemory64;
	ULONG64 KdUnmapVirtualAddress;
	ULONG64 KdGetPciDataByOffset;
	ULONG64 KdSetPciDataByOffset;
	ULONG64 HalGetInterruptVectorOverride;
	ULONG64 HalGetVectorInputOverride;
	ULONG64 HalLoadMicrocode;
	ULONG64 HalUnloadMicrocode;
	ULONG64 HalPostMicrocodeUpdate;
	ULONG64 HalAllocateMessageTargetOverride;
	ULONG64 HalFreeMessageTargetOverride;
	ULONG64 HalDpReplaceBegin;
	ULONG64 HalDpReplaceTarget;
	ULONG64 HalDpReplaceControl;
	ULONG64 HalDpReplaceEnd;
	ULONG64 HalPrepareForBugcheck;
	ULONG64 HalQueryWakeTime;
	ULONG64 HalReportIdleStateUsage;
	ULONG64 HalTscSynchronization;
	ULONG64 HalWheaInitProcessorGenericSection;
	ULONG64 HalStopLegacyUsbInterrupts;
	ULONG64 HalReadWheaPhysicalMemory;
	ULONG64 HalWriteWheaPhysicalMemory;
	ULONG64 HalDpMaskLevelTriggeredInterrupts;
	ULONG64 HalDpUnmaskLevelTriggeredInterrupts;
	ULONG64 HalDpGetInterruptReplayState;
	ULONG64 HalDpReplayInterrupts;
	ULONG64 HalQueryIoPortAccessSupported;
	ULONG64 KdSetupIntegratedDeviceForDebugging;
	ULONG64 KdReleaseIntegratedDeviceForDebugging;
	ULONG64 HalGetEnlightenmentInformation;
	ULONG64 HalAllocateEarlyPages;
	ULONG64 HalMapEarlyPages;
	ULONG64 Dummy1;
	ULONG64 Dummy2;
	ULONG64 HalNotifyProcessorFreeze;
	ULONG64 HalPrepareProcessorForIdle;
	ULONG64 HalRegisterLogRoutine;
	ULONG64 HalResumeProcessorFromIdle;
	ULONG64 Dummy;
	ULONG64 HalVectorToIDTEntryEx;
	ULONG64 HalSecondaryInterruptQueryPrimaryInformation;
	ULONG64 HalMaskInterrupt;
	ULONG64 HalUnmaskInterrupt;
	ULONG64 HalIsInterruptTypeSecondary;
	ULONG64 HalAllocateGsivForSecondaryInterrupt;
	ULONG64 HalAddInterruptRemapping;
	ULONG64 HalRemoveInterruptRemapping;
	ULONG64 HalSaveAndDisableHvEnlightenment;
	ULONG64 HalRestoreHvEnlightenment;
	ULONG64 HalFlushIoBuffersExternalCache;
	ULONG64 HalFlushExternalCache;
	ULONG64 HalPciEarlyRestore;
	ULONG64 HalGetProcessorId;
	ULONG64 HalAllocatePmcCounterSet;
	ULONG64 HalCollectPmcCounters;
	ULONG64 HalFreePmcCounterSet;
	ULONG64 HalProcessorHalt;
	ULONG64 HalTimerQueryCycleCounter;
	ULONG64 Dummy3;
	ULONG64 HalPciMarkHiberPhase;
	ULONG64 HalQueryProcessorRestartEntryPoint;
	ULONG64 HalRequestInterrupt;
	ULONG64 HalEnumerateUnmaskedInterrupts;
	ULONG64 HalFlushAndInvalidatePageExternalCache;
	ULONG64 KdEnumerateDebuggingDevices;
	ULONG64 HalFlushIoRectangleExternalCache;
	ULONG64 HalPowerEarlyRestore;
	ULONG64 HalQueryCapsuleCapabilities;
	ULONG64 HalUpdateCapsule;
	ULONG64 HalPciMultiStageResumeCapable;
	ULONG64 HalDmaFreeCrashDumpRegisters;
	ULONG64 HalAcpiAoacCapable;
	ULONG64 HalInterruptSetDestination;
	ULONG64 HalGetClockConfiguration;
	ULONG64 HalClockTimerActivate;
	ULONG64 HalClockTimerInitialize;
	ULONG64 HalClockTimerStop;
	ULONG64 HalClockTimerArm;
	ULONG64 HalTimerOnlyClockInterruptPending;
	ULONG64 HalAcpiGetMultiNode;
	ULONG64 HalPowerSetRebootHandler;
	ULONG64 HalIommuRegisterDispatchTable;
	ULONG64 HalTimerWatchdogStart;
	ULONG64 HalTimerWatchdogResetCountdown;
	ULONG64 HalTimerWatchdogStop;
	ULONG64 HalTimerWatchdogGeneratedLastReset;
	ULONG64 HalTimerWatchdogTriggerSystemReset;
	ULONG64 HalInterruptVectorDataToGsiv;
	ULONG64 HalInterruptGetHighestPriorityInterrupt;
	ULONG64 HalProcessorOn;
	ULONG64 HalProcessorOff;
	ULONG64 HalProcessorFreeze;
	ULONG64 HalDmaLinkDeviceObjectByToken;
	ULONG64 HalDmaCheckAdapterToken;
	ULONG64 Dummy4;
	ULONG64 HalTimerConvertPerformanceCounterToAuxiliaryCounter;
	ULONG64 HalTimerConvertAuxiliaryCounterToPerformanceCounter;
	ULONG64 HalTimerQueryAuxiliaryCounterFrequency;
	ULONG64 HalConnectThermalInterrupt;
	ULONG64 HalIsEFIRuntimeActive;
	ULONG64 HalTimerQueryAndResetRtcErrors;
	ULONG64 HalAcpiLateRestore;
	ULONG64 KdWatchdogDelayExpiration;
	ULONG64 HalGetProcessorStats;
	ULONG64 HalTimerWatchdogQueryDueTime;
	ULONG64 HalConnectSyntheticInterrupt;
	ULONG64 HalPreprocessNmi;
	ULONG64 HalEnumerateEnvironmentVariablesWithFilter;
	ULONG64 HalCaptureLastBranchRecordStack;
	ULONG64 HalClearLastBranchRecordStack;
	ULONG64 HalConfigureLastBranchRecord;
	ULONG64 HalGetLastBranchInformation;
	ULONG64 HalResumeLastBranchRecord;
	ULONG64 HalStartLastBranchRecord;
	ULONG64 HalStopLastBranchRecord;
	ULONG64 HalIommuBlockDevice;
	ULONG64 HalIommuUnblockDevice;
	ULONG64 HalGetIommuInterface;
	ULONG64 HalRequestGenericErrorRecovery;
	ULONG64 HalTimerQueryHostPerformanceCounter;
	ULONG64 HalTopologyQueryProcessorRelationships;
	ULONG64 HalInitPlatformDebugTriggers;
	ULONG64 HalRunPlatformDebugTriggers;
	ULONG64 HalTimerGetReferencePage;
	ULONG64 HalGetHiddenProcessorPowerInterface;
	ULONG64 HalGetHiddenProcessorPackageId;
	ULONG64 HalGetHiddenPackageProcessorCount;
	ULONG64 HalGetHiddenProcessorApicIdByIndex;
	ULONG64 HalRegisterHiddenProcessorIdleState;
	ULONG64 HalIommuReportIommuFault;
	ULONG64 HalIommuDmaRemappingCapable;
}HAL_PRIVATE_DISPATCH, * PHAL_PRIVATE_DISPATCH;

typedef struct _KerneMessagePack
{
	CLIST_ENTRY List;
	PCFilterGetMessageHeadInfo Pack;					//存储包	
}CKerneMessagePack, * PCKerneMessagePack;

typedef struct _KerneMessageList
{
	PCKerneMessagePack PackList;
	KSPIN_LOCK Lock;														//资源访问同步对象
	KEVENT Event;															//通知对象
	ULONG64 IsInitialize;													//是否初始化过了
}CKerneMessageList, * PCKerneMessageList;
#pragma pack(pop)