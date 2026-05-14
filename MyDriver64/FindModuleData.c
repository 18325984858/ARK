#include "Head.h"
#include "KernelStruct.h"
#include "Hook/EtwHook.h"
#include "CommunCation.h"

ULONG64						g_NtoskrnlAddr = NULL;
SIZE_T						g_NtoskrnlSize = NULL;
ULONG64						g_Win32kAddr = NULL;
SIZE_T						g_Win32kSize = NULL;
PUCHAR						g_NewNtoskrnlAddr = NULL;
ULONG64						g_NewNtoskrnlSize = NULL;
PULONG32					g_NewKiServiceTable = NULL;
PUCHAR						g_NewWin32kAddr = NULL;
PULONG32					g_NewW32pServiceTable = NULL;
PVOID						g_NewWin32kHandle = NULL;
PVOID						g_NewNtoskrnlHandle = NULL;
UNICODE_STRING				g_RootDriverName = { 0 };
UNICODE_STRING				g_RootDeviceName = { 0 };
UNICODE_STRING				g_RootFileName = { 0 };
UNICODE_STRING				g_RootDirectoryName = { 0 };
UNICODE_STRING				g_RootProcessName = { 0 };
UNICODE_STRING				g_RootThreadName = { 0 };
UNICODE_STRING				g_RootCallbackName = { 0 };
UNICODE_STRING				g_RootSymbolicLinkName = { 0 };
UNICODE_STRING				g_RootFileSystemName = { 0 };

CPageAddrInfo				g_SystemAddrBase = { 0 };
CallBackMaskInfo			g_PspNotifyEnableMaskInfo = { 0 };

PUCHAR						ObHeaderCookie = NULL;
PULONG64					ObTypeIndexTable = NULL;
PULONG64					PspCidTable = NULL;
PULONG64					ObpInfoMaskToOffset = NULL;
PULONG64					ObpRootDirectoryObject = NULL;
PULONG64					KeServiceDescriptorTable = NULL;
PULONG64					KeServiceDescriptorTableShadow = NULL;
PULONG64					PspCreateProcessNotifyRoutine = NULL;
PULONG32					PspCreateProcessNotifyRoutineExCount = NULL;
PULONG32					PspCreateProcessNotifyRoutineCount = NULL;
PULONG64					PspCreateThreadNotifyRoutine = NULL;
PULONG32					PspCreateThreadNotifyRoutineNonSystemCount = NULL;
PULONG32					PspCreateThreadNotifyRoutineCount = NULL;
PULONG64					IopNotifyShutdownQueueHead = NULL;
PULONG64					IopFsNotifyChangeQueueHead = NULL;
PULONG64					IopFsNotifyChangeQueueHeadMountAware = NULL;
PULONG64					KeBugCheckCallbackListHead = NULL;
PULONG64					PnpDeviceClassNotifyList = NULL;
PULONG64					PnpDeferredRegistrationList = NULL;
PULONG64					PnpProfileNotifyList = NULL;
PULONG64					PspLoadImageNotifyRoutine = NULL;
PULONG32					PspLoadImageNotifyRoutineCount = NULL;
PULONG64					CallbackListHead = NULL;
PULONG32					MiFlags = NULL;
ULONG64						PerfGlobalGroupMask = NULL;
ULONG64						EtwpDebuggerData = NULL;
ULONG64						EtwpHostSiloState = NULL;
PLIST_ENTRY					IopTimerQueueHead = NULL;
PULONG64					KiProcessorBlock = NULL;
PULONG64					KiWaitNever = NULL;
PULONG64					KiWaitAlways = NULL;
PULONG64					PspSystemPartition = NULL;
PBOOLEAN					KdPitchDebugger;
//PULONG32					KdEnteredDebugger = NULL;
//PULONG32					KdDebuggerEnabled = NULL;
//PULONG32					KdDebuggerNotPresent = NULL;
// PHAL_DISPATCH				HalDispatchTable = NULL;
// PHAL_PRIVATE_DISPATCH		HalPrivateDispatchTable = NULL;
PLIST_ENTRY					IopNetworkFileSystemQueueHead = NULL;
PLIST_ENTRY					IopCdRomFileSystemQueueHead = NULL;
PLIST_ENTRY					IopDiskFileSystemQueueHead = NULL;
PLIST_ENTRY					IopTapeFileSystemQueueHead = NULL;
PLIST_ENTRY					PsLoadedModuleList = NULL;

PSPTERMINATEPROCESS			MyPspTerminateProcess = NULL;
int g_Offset_KAPC_STATE_Process = -1;
int g_Size_PEB = -1;
int g_Offset_KPROCESS_AddressPolicy = -1;
int g_Offset_EPROCESS_UniqueProcessId = -1;
int g_Offset_EPROCESS_ActiveProcessLinks_Flink = -1;
int g_Offset_EPROCESS_Token = -1;   						  // _EPROCESS_Token 的偏移
int g_Offset_EPROCESS_InheritedFromUniqueProcessId = -1;    // _EPROCESS_InheritedFromUniqueProcessId 的偏移
int g_Offset_EPROCESS_Peb = -1;                              // _EPROCESS_Peb 的偏移
int g_Offset_EPROCESS_Session = -1;                          // _EPROCESS_Session 的偏移
int g_Offset_EPROCESS_ObjectTable = -1;                       // _EPROCESS_ObjectTable 的偏移
int g_Offset_EPROCESS_DebugPort = -1;                         // _EPROCESS_DebugPort 的偏移
int g_Offset_EPROCESS_WoW64Process;                      // _EPROCESS_WoW64Process 的偏移
int g_Offset_EPROCESS_ImageFilePointer = -1;                  // _EPROCESS_ImageFilePointer 的偏移
int g_Offset_EPROCESS_ImageFileName = -1;                    // _EPROCESS_ImageFileName 的偏移
int g_Offset_EPROCESS_SeAuditProcessCreationInfo = -1;        // _EPROCESS_SeAuditProcessCreationInfo 的偏移
int g_Offset_EPROCESS_ThreadListHead = -1;                    // _EPROCESS_ThreadListHead 的偏移
int g_Offset_EPROCESS_ActiveThreads = -1;                     // _EPROCESS_ActiveThreads 的偏移
int g_Offset_EPROCESS_Vm = -1;                                // _EPROCESS_Vm 的偏移
int g_Offset_EPROCESS_VadRoot = -1;                           // _EPROCESS_VadRoot 的偏移
int g_Offset_EPROCESS_VadCount = -1;                          // _EPROCESS_VadCount 的偏移
int g_Offset_EPROCESS_Protection = -1;                        // _EPROCESS_Protection 的偏移
int g_Offset_EPROCESS_AddressPolicyFrozen = -1;               // _EPROCESS_AddressPolicyFrozen 的偏移
int g_Offset_EPROCESS_SystemProcess = -1;

// Wdf01000.sys 结构体偏移（-1 表示 R3 PDB 未能解析，使用硬编码兼底）
int g_Offset_FxLibraryGlobalsType_IoConnectInterruptEx = -1;
int g_Offset_FxLibraryGlobalsType_FxDriverGlobalsList = -1;
int g_Offset_FX_DRIVER_GLOBALS_WdfBindInfo = -1;
int g_Offset_WDF_BIND_INFO_FuncCount = -1;
int g_Offset_WDF_BIND_INFO_FuncTable = -1;
int g_Offset_KPRCB_CurrentThread = -1;      // _KPRCB_CurrentThread 的偏移
int g_Offset_KPRCB_RspBase = -1;            // _KPRCB_RspBase 的偏移
int g_Offset_KPCR_GdtBase = -1;            // _KPRCB_GdtBase 的偏移
int g_Offset_KPCR_IdtBase = -1;            // _KPRCB_IdtBase 的偏移
int g_Offset_KPRCB_TimerTable = -1;         // _KPRCB_TimerTable 的偏移
int g_Offset_MMSUPPORT_FULL_Shared = -1;
int g_Offset_MMSUPPORT_SHARED_ShadowMapping = -1;
int g_Offset_PEB_BeingDebugged = -1;                      // _PEB_BeingDebugged 的偏移
int g_Offset_PEB_ImageBaseAddress = -1;                   // _PEB_ImageBaseAddress 的偏移
int g_Offset_PEB_Ldr = -1;                                // _PEB_Ldr 的偏移
int g_Offset_PEB_ProcessParameters = -1;                  // _PEB_ProcessParameters 的偏移
int g_Offset_PEB_LDR_DATA_InLoadOrderModuleList = -1;     // _PEB_LDR_DATA_InLoadOrderModuleList 的偏移
int g_Offset_LDR_DATA_TABLE_ENTRY_DllBase = -1;           // _LDR_DATA_TABLE_ENTRY_DllBase 的偏移
int g_Offset_LDR_DATA_TABLE_ENTRY_SizeOfImage = -1;       // _LDR_DATA_TABLE_ENTRY_SizeOfImage 的偏移
int g_Offset_LDR_DATA_TABLE_ENTRY_FullDllName = -1;       // _LDR_DATA_TABLE_ENTRY_FullDllName 的偏移
int g_Offset_LDR_DATA_TABLE_ENTRY_BaseDllName = -1;       // _LDR_DATA_TABLE_ENTRY_BaseDllName 的偏移
int g_Offset_RTL_USER_PROCESS_PARAMETERS_CommandLine = -1; // _RTL_USER_PROCESS_PARAMETERS_CommandLine 的偏移
int g_Offset_MM_SESSION_SPACE_SessionId = -1;             // _MM_SESSION_SPACE_SessionId 的偏移
int g_Offset_FILE_OBJECT_DeviceObject = -1;               // _FILE_OBJECT_DeviceObject 的偏移
int g_Offset_FILE_OBJECT_FileName = -1;                   // _FILE_OBJECT_FileName 的偏移
int g_Offset_DRIVER_OBJECT_DriverStart = -1;              // _DRIVER_OBJECT_DriverStart 的偏移
int g_Offset_DRIVER_OBJECT_DriverSize = -1;               // _DRIVER_OBJECT_DriverSize 的偏移
int g_Offset_DRIVER_OBJECT_DriverSection = -1;            // _DRIVER_OBJECT_DriverSection 的偏移
int g_Offset_DRIVER_OBJECT_DriverExtension = -1;          // _DRIVER_OBJECT_DriverExtension 的偏移
int g_Offset_DRIVER_OBJECT_DriverName = -1;               // _DRIVER_OBJECT_DriverName 的偏移
int g_Offset_DRIVER_OBJECT_FastIoDispatch = -1;           // _DRIVER_OBJECT_FastIoDispatch 的偏移
int g_Offset_DRIVER_OBJECT_MajorFunction = -1;            // _DRIVER_OBJECT_MajorFunction 的偏移
int g_Offset_DRIVER_EXTENSION_DriverObject = -1;          // _DRIVER_EXTENSION_DriverObject 的偏移
int g_Offset_DRIVER_EXTENSION_ServiceKeyName = -1;        // _DRIVER_EXTENSION_ServiceKeyName 的偏移
int g_Offset_OBJECT_HEADER_PointerCount = -1;             // _OBJECT_HEADER_PointerCount 的偏移
int g_Offset_OBJECT_HEADER_TypeIndex = -1;                // _OBJECT_HEADER_TypeIndex 的偏移
int g_Offset_OBJECT_HEADER_InfoMask = -1;                 // _OBJECT_HEADER_InfoMask 的偏移
int g_OBJECT_HEADER_SIZE = -1;                            // _OBJECT_HEADER 的大小
int g_Offset_DEVICE_OBJECT_Queue = -1;                    // _DEVICE_OBJECT_Queue 的偏移
int g_Offset_HANDLE_TABLE_NextHandleNeedingPool = -1;     // _HANDLE_TABLE_NextHandleNeedingPool 的偏移
int g_Offset_HANDLE_TABLE_TableCode = -1;                 // _HANDLE_TABLE_TableCode 的偏移
int g_Offset_OBJECT_TYPE_Name = -1;                       // _OBJECT_TYPE_Name 的偏移
int g_Offset_OBJECT_TYPE_Index = -1;                      // _OBJECT_TYPE_Index 的偏移
int g_Offset_OBJECT_TYPE_TypeInfo = -1;                   // _OBJECT_TYPE_TypeInfo 的偏移
int g_Offset_OBJECT_TYPE_CallbackList = -1;               // _OBJECT_TYPE_CallbackList 的偏移
int g_Offset_OBJECT_TYPE_INITIALIZER_ValidAccessMask = -1; // _OBJECT_TYPE_INITIALIZER_ValidAccessMask 的偏移
int g_Offset_OBJECT_TYPE_INITIALIZER_DumpProcedure = -1;  // _OBJECT_TYPE_INITIALIZER_DumpProcedure 的偏移
int g_Offset_OBJECT_TYPE_INITIALIZER_OpenProcedure = -1;  // _OBJECT_TYPE_INITIALIZER_OpenProcedure 的偏移
int g_Offset_OBJECT_TYPE_INITIALIZER_CloseProcedure = -1; // _OBJECT_TYPE_INITIALIZER_CloseProcedure 的偏移
int g_Offset_OBJECT_TYPE_INITIALIZER_DeleteProcedure = -1; // _OBJECT_TYPE_INITIALIZER_DeleteProcedure 的偏移
int g_Offset_OBJECT_TYPE_INITIALIZER_ParseProcedure = -1; // _OBJECT_TYPE_INITIALIZER_ParseProcedure 的偏移
int g_Offset_OBJECT_TYPE_INITIALIZER_SecurityProcedure = -1; // _OBJECT_TYPE_INITIALIZER_SecurityProcedure 的偏移
int g_Offset_OBJECT_TYPE_INITIALIZER_QueryNameProcedure = -1; // _OBJECT_TYPE_INITIALIZER_QueryNameProcedure 的偏移
int g_Offset_OBJECT_TYPE_INITIALIZER_OkayToCloseProcedure = -1; // _OBJECT_TYPE_INITIALIZER_OkayToCloseProcedure 的偏移
int g_Offset_TOKEN_LogonSession = -1;                    // _TOKEN_LogonSession 的偏移
int g_Offset_MMVAD_Core = -1;                            // _MMVAD_Core 的偏移
int g_Offset_MMVAD_Subsection = -1;                      // _MMVAD_Subsection 的偏移
int g_Offset_SEP_LOGON_SESSION_REFERENCES_AccountName = -1; // _SEP_LOGON_SESSION_REFERENCES_AccountName 的偏移
int g_Offset_MMVAD_SHORT_StartingVpn = -1;               // _MMVAD_SHORT_StartingVpn 的偏移
int g_Offset_MMVAD_SHORT_EndingVpn = -1;                 // _MMVAD_SHORT_EndingVpn 的偏移
int g_Offset_MMVAD_SHORT_StartingVpnHigh = -1;           // _MMVAD_SHORT_StartingVpnHigh 的偏移
int g_Offset_MMVAD_SHORT_EndingVpnHigh = -1;             // _MMVAD_SHORT_EndingVpnHigh 的偏移
int g_Offset_MMVAD_SHORT_u = -1;                         // _MMVAD_SHORT_u 的偏移
int g_Offset_MMVAD_SHORT_u1 = -1;                        // _MMVAD_SHORT_u1 的偏移
int g_Offset_SUBSECTION_ControlArea = -1;                // _SUBSECTION_ControlArea 的偏移
int g_Offset_CONTROL_AREA_FilePointer = -1;              // _CONTROL_AREA_FilePointer 的偏移
int g_Offset_OBJECT_SYMBOLIC_LINK_LinkTarget = -1;       // _OBJECT_SYMBOLIC_LINK_LinkTarget 的偏移
int g_Offset_ETW_SILODRIVERSTATE_EtwpLoggerContext = -1; // _ETW_SILODRIVERSTATE_EtwpLoggerContext 的偏移
int g_Offset_WMI_LOGGER_CONTEXT_GetCpuClock = -1;        // _WMI_LOGGER_CONTEXT_GetCpuClock 的偏移
int g_Offset_FLT_FILTER_Name = -1;                       // _FLT_FILTER_Name 的偏移
int g_Offset_FLT_FILTER_DefaultAltitude = -1;            // _FLT_FILTER_DefaultAltitude 的偏移
int g_Offset_FLT_FILTER_DriverObject = -1;               // _FLT_FILTER_DriverObject 的偏移
int g_Offset_FLT_FILTER_Operations = -1;                 // _FLT_FILTER_Operations 的偏移
int g_Offset_FLT_OBJECT_PointerCount = -1;               // _FLT_OBJECT_PointerCount 的偏移
int g_Offset_FLT_OBJECT_PrimaryLink = -1;                // _FLT_OBJECT_PrimaryLink 的偏移
int g_Offset_FLT_OBJECT_UniqueIdentifier = -1;           // _FLT_OBJECT_UniqueIdentifier 的偏移
int g_Offset_EPARTITION_ExPartition = -1;                // _EPARTITION_ExPartition 的偏移
int g_Offset_EX_PARTITION_WorkQueues = -1;               // _EX_PARTITION_WorkQueues 的偏移
int g_Offset_EX_WORK_QUEUE_WorkPriQueue = -1;            // _EX_WORK_QUEUE_WorkPriQueue 的偏移
int g_Offset_KPRIQUEUE_Header = -1;                      // _KPRIQUEUE_Header 的偏移
int g_Offset_ENODE_Ncb = -1;                             // _ENODE_Ncb 的偏移
int g_Offset_ENODE_HotAddProcessorWorkItem = -1;          // _ENODE_HotAddProcessorWorkItem 的偏移
int g_Offset_KTHREAD_ApcState = -1;                      // _ETHREAD_ApcState 的偏移
int g_Offset_KTHREAD_ThreadFlags = -1;                   // _ETHREAD_ThreadFlagsSpare 的偏移
int g_Offset_KTHREAD_SystemCallNumber = -1;               // _ETHREAD_SystemCallNumber 的偏移
int g_Offset_KTHREAD_Priority = -1;                       // _ETHREAD_Priority 的偏移
int g_Offset_KTHREAD_Teb = -1;                            // _ETHREAD_Teb 的偏移
int g_Offset_KTHREAD_ContextSwitches = -1;                // _ETHREAD_ContextSwitches 的偏移
int g_Offset_KTHREAD_State = -1;                          // _ETHREAD_State 的偏移
int g_Offset_KTHREAD_Process = -1;                        // _ETHREAD_Process 的偏移
int g_Offset_KTHREAD_PreviousMode = -1;                   // _ETHREAD_PreviousMode 的偏移
int g_Offset_ETHREAD_CreateTime = -1;                     // _ETHREAD_CreateTime 的偏移
int g_Offset_ETHREAD_StartAddress = -1;                   // _ETHREAD_StartAddress 的偏移
int g_Offset_KTHREAD_UniqueProcess = -1;                  // _ETHREAD_UniqueProcess 的偏移
int g_Offset_KTHREAD_UniqueThread = -1;                   // _ETHREAD_UniqueThread 的偏移
int g_Offset_ETHREAD_Win32StartAddress = -1;              // _ETHREAD_Win32StartAddress 的偏移
int g_Offset_ETHREAD_ThreadListEntry = -1;                // _ETHREAD_ThreadListEntry_Flink 的偏移

int g_Offset_EPROCESS_CreateTime = -1;

struct _VALUE_OFFSET
{
	ULONG64 Value;
	int Offset;
};


UCHAR GetOffset()
{
	UCHAR nRet = TRUE;
	do
	{
		g_Size_PEB = ToUserSendGetStructSizeMessgae(L"ntoskrnel.exe", L"_PEB");
		if (g_Size_PEB == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Size_PEB 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KPROCESS_AddressPolicy = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KPROCESS", L"AddressPolicy");
		if (g_Offset_KPROCESS_AddressPolicy == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_AddressPolicy 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_UniqueProcessId = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"UniqueProcessId");
		if (g_Offset_EPROCESS_UniqueProcessId == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_UniqueProcessId 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;

		}

		g_Offset_EPROCESS_ActiveProcessLinks_Flink = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"ActiveProcessLinks");
		if (g_Offset_EPROCESS_ActiveProcessLinks_Flink == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_ActiveProcessLinks_Flink 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;

		}

		g_Offset_EPROCESS_Token = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"Token");
		if (g_Offset_EPROCESS_Token == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_Token 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;

		}

		g_Offset_EPROCESS_InheritedFromUniqueProcessId = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"InheritedFromUniqueProcessId");
		if (g_Offset_EPROCESS_InheritedFromUniqueProcessId == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_InheritedFromUniqueProcessId 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_Peb = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"Peb");
		if (g_Offset_EPROCESS_Peb == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_Peb 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_Session = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"Session");
		if (g_Offset_EPROCESS_Session == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_Session 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_ObjectTable = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"ObjectTable");
		if (g_Offset_EPROCESS_ObjectTable == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_ObjectTable 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_DebugPort = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"DebugPort");
		if (g_Offset_EPROCESS_DebugPort == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_DebugPort 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_WoW64Process = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"WoW64Process");
		if (g_Offset_EPROCESS_WoW64Process == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_WoW64Process 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_ImageFilePointer = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"ImageFilePointer");
		if (g_Offset_EPROCESS_ImageFilePointer == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_ImageFilePointer 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_ImageFileName = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"ImageFileName");
		if (g_Offset_EPROCESS_ImageFileName == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_ImageFileName 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_SeAuditProcessCreationInfo = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"SeAuditProcessCreationInfo");
		if (g_Offset_EPROCESS_SeAuditProcessCreationInfo == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_SeAuditProcessCreationInfo 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_ThreadListHead = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"ThreadListHead");
		if (g_Offset_EPROCESS_ThreadListHead == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_ThreadListHead 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_ActiveThreads = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"ActiveThreads");
		if (g_Offset_EPROCESS_ActiveThreads == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_ActiveThreads 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_Vm = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"Vm");
		if (g_Offset_EPROCESS_Vm == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_Vm 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_VadRoot = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"VadRoot");
		if (g_Offset_EPROCESS_VadRoot == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_VadRoot 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_VadCount = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"VadCount");
		if (g_Offset_EPROCESS_VadCount == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_VadCount 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_Protection = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"Protection");
		if (g_Offset_EPROCESS_Protection == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_Protection 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_CreateTime = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"CreateTime");
		if (g_Offset_EPROCESS_CreateTime == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_CreateTime 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_AddressPolicyFrozen = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"Flags3");
		if (g_Offset_EPROCESS_AddressPolicyFrozen == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_AddressPolicyFrozen 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_EPROCESS_SystemProcess = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPROCESS", L"Flags3");
		if (g_Offset_EPROCESS_SystemProcess == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPROCESS_SystemProcess 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_KPRCB_CurrentThread = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KPRCB", L"CurrentThread");
		if (g_Offset_KPRCB_CurrentThread == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_KPRCB_CurrentThread 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_KPRCB_CurrentThread += (ToUserSendGetStructSizeMessgae(L"ntoskrnel.exe", L"_KPCR") - ToUserSendGetStructSizeMessgae(L"ntoskrnel.exe", L"_KPRCB"));
		g_Offset_KPRCB_RspBase = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KPRCB", L"RspBase");
		if (g_Offset_KPRCB_RspBase == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_KPRCB_RspBase 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_KPCR_GdtBase = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KPCR", L"GdtBase");
		if (g_Offset_KPCR_GdtBase == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_KPCR_GdtBase 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_KPCR_IdtBase = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KPCR", L"IdtBase");
		if (g_Offset_KPCR_IdtBase == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_KPCR_IdtBase 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_KPRCB_TimerTable = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KPRCB", L"TimerTable");
		if (g_Offset_KPRCB_TimerTable == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_KPRCB_TimerTable 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_KAPC_STATE_Process = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KAPC_STATE", L"Process");
		if (g_Offset_KAPC_STATE_Process == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_KAPC_STATE_Process 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_MMSUPPORT_FULL_Shared = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MMSUPPORT_FULL", L"Shared");
		if (g_Offset_MMSUPPORT_FULL_Shared == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_MMSUPPORT_FULL_Shared 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		g_Offset_MMSUPPORT_SHARED_ShadowMapping = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MMSUPPORT_SHARED", L"ShadowMapping");
		if (g_Offset_MMSUPPORT_SHARED_ShadowMapping == -1)
		{
			MyDbgPrintfEx("[%s] 获取 g_Offset_MMSUPPORT_SHARED_ShadowMapping 失败！\n", __FUNCTION__);
			nRet = FALSE;
			//break;
		}

		// _PEB 相关偏移
		g_Offset_PEB_BeingDebugged = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_PEB", L"BeingDebugged");
		if (g_Offset_PEB_BeingDebugged == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_PEB_BeingDebugged 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_PEB_ImageBaseAddress = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_PEB", L"ImageBaseAddress");
		if (g_Offset_PEB_ImageBaseAddress == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_PEB_ImageBaseAddress 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_PEB_Ldr = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_PEB", L"Ldr");
		if (g_Offset_PEB_Ldr == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_PEB_Ldr 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_PEB_ProcessParameters = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_PEB", L"ProcessParameters");
		if (g_Offset_PEB_ProcessParameters == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_PEB_ProcessParameters 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _PEB_LDR_DATA 相关偏移
		g_Offset_PEB_LDR_DATA_InLoadOrderModuleList = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_PEB_LDR_DATA", L"InLoadOrderModuleList");
		if (g_Offset_PEB_LDR_DATA_InLoadOrderModuleList == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_PEB_LDR_DATA_InLoadOrderModuleList 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _LDR_DATA_TABLE_ENTRY 相关偏移
		g_Offset_LDR_DATA_TABLE_ENTRY_DllBase = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_LDR_DATA_TABLE_ENTRY", L"DllBase");
		if (g_Offset_LDR_DATA_TABLE_ENTRY_DllBase == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_LDR_DATA_TABLE_ENTRY_DllBase 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_LDR_DATA_TABLE_ENTRY_SizeOfImage = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_LDR_DATA_TABLE_ENTRY", L"SizeOfImage");
		if (g_Offset_LDR_DATA_TABLE_ENTRY_SizeOfImage == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_LDR_DATA_TABLE_ENTRY_SizeOfImage 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_LDR_DATA_TABLE_ENTRY_FullDllName = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_LDR_DATA_TABLE_ENTRY", L"FullDllName");
		if (g_Offset_LDR_DATA_TABLE_ENTRY_FullDllName == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_LDR_DATA_TABLE_ENTRY_FullDllName 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_LDR_DATA_TABLE_ENTRY_BaseDllName = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_LDR_DATA_TABLE_ENTRY", L"BaseDllName");
		if (g_Offset_LDR_DATA_TABLE_ENTRY_BaseDllName == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_LDR_DATA_TABLE_ENTRY_BaseDllName 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _RTL_USER_PROCESS_PARAMETERS 相关偏移
		g_Offset_RTL_USER_PROCESS_PARAMETERS_CommandLine = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_RTL_USER_PROCESS_PARAMETERS", L"CommandLine");
		if (g_Offset_RTL_USER_PROCESS_PARAMETERS_CommandLine == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_RTL_USER_PROCESS_PARAMETERS_CommandLine 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _MM_SESSION_SPACE 相关偏移
		g_Offset_MM_SESSION_SPACE_SessionId = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MM_SESSION_SPACE", L"SessionId");
		if (g_Offset_MM_SESSION_SPACE_SessionId == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_MM_SESSION_SPACE_SessionId 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _FILE_OBJECT 相关偏移
		g_Offset_FILE_OBJECT_DeviceObject = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_FILE_OBJECT", L"DeviceObject");
		if (g_Offset_FILE_OBJECT_DeviceObject == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_FILE_OBJECT_DeviceObject 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_FILE_OBJECT_FileName = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_FILE_OBJECT", L"FileName");
		if (g_Offset_FILE_OBJECT_FileName == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_FILE_OBJECT_FileName 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _DRIVER_OBJECT 相关偏移
		g_Offset_DRIVER_OBJECT_DriverStart = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_DRIVER_OBJECT", L"DriverStart");
		if (g_Offset_DRIVER_OBJECT_DriverStart == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_DRIVER_OBJECT_DriverStart 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_DRIVER_OBJECT_DriverSize = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_DRIVER_OBJECT", L"DriverSize");
		if (g_Offset_DRIVER_OBJECT_DriverSize == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_DRIVER_OBJECT_DriverSize 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_DRIVER_OBJECT_DriverSection = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_DRIVER_OBJECT", L"DriverSection");
		if (g_Offset_DRIVER_OBJECT_DriverSection == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_DRIVER_OBJECT_DriverSection 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_DRIVER_OBJECT_DriverExtension = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_DRIVER_OBJECT", L"DriverExtension");
		if (g_Offset_DRIVER_OBJECT_DriverExtension == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_DRIVER_OBJECT_DriverExtension 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_DRIVER_OBJECT_DriverName = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_DRIVER_OBJECT", L"DriverName");
		if (g_Offset_DRIVER_OBJECT_DriverName == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_DRIVER_OBJECT_DriverName 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_DRIVER_OBJECT_FastIoDispatch = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_DRIVER_OBJECT", L"FastIoDispatch");
		if (g_Offset_DRIVER_OBJECT_FastIoDispatch == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_DRIVER_OBJECT_FastIoDispatch 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_DRIVER_OBJECT_MajorFunction = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_DRIVER_OBJECT", L"MajorFunction");
		if (g_Offset_DRIVER_OBJECT_MajorFunction == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_DRIVER_OBJECT_MajorFunction 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _DRIVER_EXTENSION 相关偏移
		g_Offset_DRIVER_EXTENSION_DriverObject = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_DRIVER_EXTENSION", L"DriverObject");
		if (g_Offset_DRIVER_EXTENSION_DriverObject == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_DRIVER_EXTENSION_DriverObject 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_DRIVER_EXTENSION_ServiceKeyName = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_DRIVER_EXTENSION", L"ServiceKeyName");
		if (g_Offset_DRIVER_EXTENSION_ServiceKeyName == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_DRIVER_EXTENSION_ServiceKeyName 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _OBJECT_HEADER 相关偏移
		g_Offset_OBJECT_HEADER_PointerCount = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_HEADER", L"PointerCount");
		if (g_Offset_OBJECT_HEADER_PointerCount == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_HEADER_PointerCount 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_HEADER_TypeIndex = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_HEADER", L"TypeIndex");
		if (g_Offset_OBJECT_HEADER_TypeIndex == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_HEADER_TypeIndex 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_HEADER_InfoMask = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_HEADER", L"InfoMask");
		if (g_Offset_OBJECT_HEADER_InfoMask == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_HEADER_InfoMask 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_OBJECT_HEADER_SIZE = ToUserSendGetStructSizeMessgae(L"ntoskrnel.exe", L"_OBJECT_HEADER") - ToUserSendGetStructSizeMessgae(L"ntoskrnel.exe", L"_QUAD");
		if (g_OBJECT_HEADER_SIZE == -1) {
			MyDbgPrintfEx("[%s] 获取 g_OBJECT_HEADER_SIZE 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _DEVICE_OBJECT 相关偏移
		g_Offset_DEVICE_OBJECT_Queue = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_DEVICE_OBJECT", L"Queue");
		if (g_Offset_DEVICE_OBJECT_Queue == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_DEVICE_OBJECT_Queue 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _HANDLE_TABLE 相关偏移
		g_Offset_HANDLE_TABLE_NextHandleNeedingPool = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_HANDLE_TABLE", L"NextHandleNeedingPool");
		if (g_Offset_HANDLE_TABLE_NextHandleNeedingPool == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_HANDLE_TABLE_NextHandleNeedingPool 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_HANDLE_TABLE_TableCode = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_HANDLE_TABLE", L"TableCode");
		if (g_Offset_HANDLE_TABLE_TableCode == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_HANDLE_TABLE_TableCode 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _OBJECT_TYPE 相关偏移
		g_Offset_OBJECT_TYPE_Name = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE", L"Name");
		if (g_Offset_OBJECT_TYPE_Name == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_Name 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_Index = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE", L"Index");
		if (g_Offset_OBJECT_TYPE_Index == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_Index 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_TypeInfo = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE", L"TypeInfo");
		if (g_Offset_OBJECT_TYPE_TypeInfo == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_TypeInfo 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_CallbackList = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE", L"CallbackList");
		if (g_Offset_OBJECT_TYPE_CallbackList == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_CallbackList 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _OBJECT_TYPE_INITIALIZER 相关偏移
		g_Offset_OBJECT_TYPE_INITIALIZER_ValidAccessMask = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE_INITIALIZER", L"ValidAccessMask");
		if (g_Offset_OBJECT_TYPE_INITIALIZER_ValidAccessMask == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_INITIALIZER_ValidAccessMask 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_INITIALIZER_DumpProcedure = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE_INITIALIZER", L"DumpProcedure");
		if (g_Offset_OBJECT_TYPE_INITIALIZER_DumpProcedure == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_INITIALIZER_DumpProcedure 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_INITIALIZER_OpenProcedure = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE_INITIALIZER", L"OpenProcedure");
		if (g_Offset_OBJECT_TYPE_INITIALIZER_OpenProcedure == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_INITIALIZER_OpenProcedure 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_INITIALIZER_CloseProcedure = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE_INITIALIZER", L"CloseProcedure");
		if (g_Offset_OBJECT_TYPE_INITIALIZER_CloseProcedure == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_INITIALIZER_CloseProcedure 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_INITIALIZER_DeleteProcedure = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE_INITIALIZER", L"DeleteProcedure");
		if (g_Offset_OBJECT_TYPE_INITIALIZER_DeleteProcedure == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_INITIALIZER_DeleteProcedure 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_INITIALIZER_ParseProcedure = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE_INITIALIZER", L"ParseProcedure");
		if (g_Offset_OBJECT_TYPE_INITIALIZER_ParseProcedure == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_INITIALIZER_ParseProcedure 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_INITIALIZER_SecurityProcedure = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE_INITIALIZER", L"SecurityProcedure");
		if (g_Offset_OBJECT_TYPE_INITIALIZER_SecurityProcedure == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_INITIALIZER_SecurityProcedure 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_INITIALIZER_QueryNameProcedure = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE_INITIALIZER", L"QueryNameProcedure");
		if (g_Offset_OBJECT_TYPE_INITIALIZER_QueryNameProcedure == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_INITIALIZER_QueryNameProcedure 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_OBJECT_TYPE_INITIALIZER_OkayToCloseProcedure = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_TYPE_INITIALIZER", L"OkayToCloseProcedure");
		if (g_Offset_OBJECT_TYPE_INITIALIZER_OkayToCloseProcedure == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_TYPE_INITIALIZER_OkayToCloseProcedure 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _TOKEN 相关偏移
		g_Offset_TOKEN_LogonSession = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_TOKEN", L"LogonSession");
		if (g_Offset_TOKEN_LogonSession == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_TOKEN_LogonSession 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _MMVAD 相关偏移
		g_Offset_MMVAD_Core = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MMVAD", L"Core");
		if (g_Offset_MMVAD_Core == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_MMVAD_Core 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_MMVAD_Subsection = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MMVAD", L"Subsection");
		if (g_Offset_MMVAD_Subsection == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_MMVAD_Subsection 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _SEP_LOGON_SESSION_REFERENCES 相关偏移
		g_Offset_SEP_LOGON_SESSION_REFERENCES_AccountName = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_SEP_LOGON_SESSION_REFERENCES", L"AccountName");
		if (g_Offset_SEP_LOGON_SESSION_REFERENCES_AccountName == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_SEP_LOGON_SESSION_REFERENCES_AccountName 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _MMVAD_SHORT 相关偏移
		g_Offset_MMVAD_SHORT_StartingVpn = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MMVAD_SHORT", L"StartingVpn");
		if (g_Offset_MMVAD_SHORT_StartingVpn == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_MMVAD_SHORT_StartingVpn 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_MMVAD_SHORT_EndingVpn = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MMVAD_SHORT", L"EndingVpn");
		if (g_Offset_MMVAD_SHORT_EndingVpn == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_MMVAD_SHORT_EndingVpn 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_MMVAD_SHORT_StartingVpnHigh = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MMVAD_SHORT", L"StartingVpnHigh");
		if (g_Offset_MMVAD_SHORT_StartingVpnHigh == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_MMVAD_SHORT_StartingVpnHigh 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_MMVAD_SHORT_EndingVpnHigh = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MMVAD_SHORT", L"EndingVpnHigh");
		if (g_Offset_MMVAD_SHORT_EndingVpnHigh == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_MMVAD_SHORT_EndingVpnHigh 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_MMVAD_SHORT_u = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MMVAD_SHORT", L"u");
		if (g_Offset_MMVAD_SHORT_u == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_MMVAD_SHORT_u 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_MMVAD_SHORT_u1 = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_MMVAD_SHORT", L"u1");
		if (g_Offset_MMVAD_SHORT_u1 == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_MMVAD_SHORT_u1 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _SUBSECTION 相关偏移
		g_Offset_SUBSECTION_ControlArea = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_SUBSECTION", L"ControlArea");
		if (g_Offset_SUBSECTION_ControlArea == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_SUBSECTION_ControlArea 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _CONTROL_AREA 相关偏移
		g_Offset_CONTROL_AREA_FilePointer = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_CONTROL_AREA", L"FilePointer");
		if (g_Offset_CONTROL_AREA_FilePointer == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_CONTROL_AREA_FilePointer 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _OBJECT_SYMBOLIC_LINK 相关偏移
		g_Offset_OBJECT_SYMBOLIC_LINK_LinkTarget = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_OBJECT_SYMBOLIC_LINK", L"LinkTarget");
		if (g_Offset_OBJECT_SYMBOLIC_LINK_LinkTarget == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_OBJECT_SYMBOLIC_LINK_LinkTarget 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _ETW_SILODRIVERSTATE 相关偏移
		g_Offset_ETW_SILODRIVERSTATE_EtwpLoggerContext = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_ETW_SILODRIVERSTATE", L"EtwpLoggerContext");
		if (g_Offset_ETW_SILODRIVERSTATE_EtwpLoggerContext == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_ETW_SILODRIVERSTATE_EtwpLoggerContext 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _WMI_LOGGER_CONTEXT 相关偏移
		g_Offset_WMI_LOGGER_CONTEXT_GetCpuClock = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_WMI_LOGGER_CONTEXT", L"GetCpuClock");
		if (g_Offset_WMI_LOGGER_CONTEXT_GetCpuClock == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_WMI_LOGGER_CONTEXT_GetCpuClock 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _FLT_FILTER 相关偏移
		g_Offset_FLT_FILTER_Name = ToUserSendGetStructInfoMessgae(L"fltmgr.sys", L"_FLT_FILTER", L"Name");
		if (g_Offset_FLT_FILTER_Name == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_FLT_FILTER_Name 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_FLT_FILTER_DefaultAltitude = ToUserSendGetStructInfoMessgae(L"fltmgr.sys", L"_FLT_FILTER", L"DefaultAltitude");
		if (g_Offset_FLT_FILTER_DefaultAltitude == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_FLT_FILTER_DefaultAltitude 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_FLT_FILTER_DriverObject = ToUserSendGetStructInfoMessgae(L"fltmgr.sys", L"_FLT_FILTER", L"DriverObject");
		if (g_Offset_FLT_FILTER_DriverObject == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_FLT_FILTER_DriverObject 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_FLT_FILTER_Operations = ToUserSendGetStructInfoMessgae(L"fltmgr.sys", L"_FLT_FILTER", L"Operations");
		if (g_Offset_FLT_FILTER_Operations == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_FLT_FILTER_Operations 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _FLT_OBJECT 相关偏移
		g_Offset_FLT_OBJECT_PointerCount = ToUserSendGetStructInfoMessgae(L"fltmgr.sys", L"_FLT_OBJECT", L"PointerCount");
		if (g_Offset_FLT_OBJECT_PointerCount == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_FLT_OBJECT_PointerCount 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_FLT_OBJECT_PrimaryLink = ToUserSendGetStructInfoMessgae(L"fltmgr.sys", L"_FLT_OBJECT", L"PrimaryLink");
		if (g_Offset_FLT_OBJECT_PrimaryLink == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_FLT_OBJECT_PrimaryLink 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_FLT_OBJECT_UniqueIdentifier = ToUserSendGetStructInfoMessgae(L"fltmgr.sys", L"_FLT_OBJECT", L"UniqueIdentifier");
		if (g_Offset_FLT_OBJECT_UniqueIdentifier == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_FLT_OBJECT_UniqueIdentifier 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _EPARTITION 相关偏移
		g_Offset_EPARTITION_ExPartition = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EPARTITION", L"ExPartition");
		if (g_Offset_EPARTITION_ExPartition == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_EPARTITION_ExPartition 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _EX_PARTITION 相关偏移
		g_Offset_EX_PARTITION_WorkQueues = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EX_PARTITION", L"WorkQueues");
		if (g_Offset_EX_PARTITION_WorkQueues == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_EX_PARTITION_WorkQueues 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _EX_WORK_QUEUE 相关偏移
		g_Offset_EX_WORK_QUEUE_WorkPriQueue = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_EX_WORK_QUEUE", L"WorkPriQueue");
		if (g_Offset_EX_WORK_QUEUE_WorkPriQueue == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_EX_WORK_QUEUE_WorkPriQueue 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _KPRIQUEUE 相关偏移
		g_Offset_KPRIQUEUE_Header = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KPRIQUEUE", L"Header");
		if (g_Offset_KPRIQUEUE_Header == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KPRIQUEUE_Header 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		// _ENODE 相关偏移
		g_Offset_ENODE_Ncb = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_ENODE", L"Ncb");
		if (g_Offset_ENODE_Ncb == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_ENODE_Ncb 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_ENODE_HotAddProcessorWorkItem = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_ENODE", L"HotAddProcessorWorkItem");
		if (g_Offset_ENODE_HotAddProcessorWorkItem == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_ENODE_HotAddProcessorWorkItem 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KTHREAD_ApcState = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KTHREAD", L"ApcState");
		if (g_Offset_KTHREAD_ApcState == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_ApcState 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KTHREAD_ThreadFlags = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KTHREAD", L"ThreadFlags");
		if (g_Offset_KTHREAD_ThreadFlags == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_ThreadFlags 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KTHREAD_SystemCallNumber = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KTHREAD", L"SystemCallNumber");
		if (g_Offset_KTHREAD_SystemCallNumber == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_SystemCallNumber 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KTHREAD_Priority = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KTHREAD", L"Priority");
		if (g_Offset_KTHREAD_Priority == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_Priority 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KTHREAD_Teb = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KTHREAD", L"Teb");
		if (g_Offset_KTHREAD_Teb == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_Teb 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KTHREAD_ContextSwitches = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KTHREAD", L"ContextSwitches");
		if (g_Offset_KTHREAD_ContextSwitches == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_ContextSwitches 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KTHREAD_State = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KTHREAD", L"State");
		if (g_Offset_KTHREAD_State == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_State 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KTHREAD_Process = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KTHREAD", L"Process");
		if (g_Offset_KTHREAD_Process == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_Process 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KTHREAD_PreviousMode = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_KTHREAD", L"PreviousMode");
		if (g_Offset_KTHREAD_PreviousMode == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_PreviousMode 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_ETHREAD_CreateTime = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_ETHREAD", L"CreateTime");
		if (g_Offset_ETHREAD_CreateTime == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_ETHREAD_CreateTime 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_ETHREAD_StartAddress = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_ETHREAD", L"StartAddress");
		if (g_Offset_ETHREAD_StartAddress == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_ETHREAD_StartAddress 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		ULONG64 OffsetCID = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_ETHREAD", L"Cid");
		g_Offset_KTHREAD_UniqueProcess = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_CLIENT_ID", L"UniqueProcess") + OffsetCID;
		if (g_Offset_KTHREAD_UniqueProcess == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_UniqueProcess 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_KTHREAD_UniqueThread = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_CLIENT_ID", L"UniqueThread") + OffsetCID;
		if (g_Offset_KTHREAD_UniqueThread == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_KTHREAD_UniqueThread 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_ETHREAD_Win32StartAddress = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_ETHREAD", L"Win32StartAddress");
		if (g_Offset_ETHREAD_Win32StartAddress == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_ETHREAD_Win32StartAddress 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		g_Offset_ETHREAD_ThreadListEntry = ToUserSendGetStructInfoMessgae(L"ntoskrnel.exe", L"_ETHREAD", L"ThreadListEntry");
		if (g_Offset_ETHREAD_ThreadListEntry == -1) {
			MyDbgPrintfEx("[%s] 获取 g_Offset_ETHREAD_ThreadListEntry 失败！\n", __FUNCTION__);
			nRet = FALSE;
		}

		PsLoadedModuleList = ToUserSendGetGlobalVariablesMessgae(L"ntoskrnel.exe", L"PsLoadedModuleList") + g_NtoskrnlAddr;
		if (!PsLoadedModuleList)
		{
			MyDbgPrintfEx("[%s] 获取PsLoadedModuleList变量失败 Data[%I64X]!\n", __FUNCTION__, PsLoadedModuleList);
			nRet = FALSE;
		}

	
		KdPitchDebugger = ToUserSendGetGlobalVariablesMessgae(L"ntoskrnel.exe", L"KdPitchDebugger") + g_NtoskrnlAddr;
		if (!KdPitchDebugger)
		{
			MyDbgPrintfEx("[%s] 获取KdPitchDebugger变量失败 Data[%I64X]!\n", __FUNCTION__, KdPitchDebugger);
			nRet = FALSE;
		}
	/*
		KdDebuggerEnabled = ToUserSendGetGlobalVariablesMessgae(L"ntoskrnel.exe", L"KdDebuggerEnabled") + g_NtoskrnlAddr;
		if (!KdDebuggerEnabled)
		{
			MyDbgPrintfEx("[%s] 获取KdDebuggerEnabled变量失败 Data[%I64X]!\n", __FUNCTION__, KdDebuggerEnabled);
			nRet = FALSE;
		}
		KdDebuggerNotPresent = ToUserSendGetGlobalVariablesMessgae(L"ntoskrnel.exe", L"KdDebuggerNotPresent") + g_NtoskrnlAddr;
		if (!KdDebuggerNotPresent)
		{
			MyDbgPrintfEx("[%s] 获取KdDebuggerNotPresent变量失败 Data[%I64X]!\n", __FUNCTION__, KdDebuggerNotPresent);
			nRet = FALSE;
		}
		*/

		// Wdf01000.sys 结构体偏移：PDB 取不到不视为致命错误，调用方有硬编码兼底
		g_Offset_FxLibraryGlobalsType_IoConnectInterruptEx =
			ToUserSendGetStructInfoMessgae(L"Wdf01000.sys", L"FxLibraryGlobalsType", L"IoConnectInterruptEx");
		g_Offset_FxLibraryGlobalsType_FxDriverGlobalsList =
			ToUserSendGetStructInfoMessgae(L"Wdf01000.sys", L"FxLibraryGlobalsType", L"FxDriverGlobalsList");
		g_Offset_FX_DRIVER_GLOBALS_WdfBindInfo =
			ToUserSendGetStructInfoMessgae(L"Wdf01000.sys", L"FX_DRIVER_GLOBALS", L"WdfBindInfo");
		g_Offset_WDF_BIND_INFO_FuncCount =
			ToUserSendGetStructInfoMessgae(L"Wdf01000.sys", L"_WDF_BIND_INFO", L"FuncCount");
		g_Offset_WDF_BIND_INFO_FuncTable =
			ToUserSendGetStructInfoMessgae(L"Wdf01000.sys", L"_WDF_BIND_INFO", L"FuncTable");
		MyDbgPrintfEx("[GetOffset] WDF: IoConn=%d List=%d BindInfo=%d FuncCount=%d FuncTable=%d\n",
			g_Offset_FxLibraryGlobalsType_IoConnectInterruptEx,
			g_Offset_FxLibraryGlobalsType_FxDriverGlobalsList,
			g_Offset_FX_DRIVER_GLOBALS_WdfBindInfo,
			g_Offset_WDF_BIND_INFO_FuncCount,
			g_Offset_WDF_BIND_INFO_FuncTable);
	} while (FALSE);

	return nRet;
}

UCHAR InitGlobalVariable(PDRIVER_OBJECT pDriverObject)
{
	UCHAR Ret = TRUE;

	do
	{
		RtlInitUnicodeString(&g_RootDeviceName, L"Device");
		RtlInitUnicodeString(&g_RootDriverName, L"Driver");
		RtlInitUnicodeString(&g_RootThreadName, L"Thread");
		RtlInitUnicodeString(&g_RootProcessName, L"Process");
		RtlInitUnicodeString(&g_RootCallbackName, L"Callback");
		RtlInitUnicodeString(&g_RootFileName, L"File");
		RtlInitUnicodeString(&g_RootDirectoryName, L"Directory");
		RtlInitUnicodeString(&g_RootFileSystemName, L"FileSystem");
		RtlInitUnicodeString(&g_RootSymbolicLinkName, L"SymbolicLink");

		//初始化系统地址Base PTE PDE PPE PDPTE 
		if (!InitSystemPteBase())
		{
			MyDbgPrintfEx("[%s] InitSystemAddrBase函数调用失败!\n", __FUNCTION__);
			Ret = FALSE;
		}

		//MyDbgPrintfEx("Pte:%I64X Pde:%I64X Pxe:%I64X Ppe:%I64X\n",g_SystemAddrBase.PteBase, g_SystemAddrBase.PdeBase, g_SystemAddrBase.PxeBase, g_SystemAddrBase.PpeBase);

		//获取 内核模块地址 大小
		if (!GetNtoskrnlMoudleBaseAddr(pDriverObject, L"ntoskrnl.exe", &g_NtoskrnlAddr, &g_NtoskrnlSize))
		{
			MyDbgPrintfEx("[%s] 获取NtoskrnlAddr和大小失败!\n", __FUNCTION__);
			Ret = FALSE;
		}
		//MyDbgPrintfEx("[%s] 获取g_NtoskrnlAddr:%I64X Size:%X\n", __FUNCTION__, g_NtoskrnlAddr, g_NtoskrnlSize);


		//获取 内核模块地址 大小
		if (!GetNtoskrnlMoudleBaseAddr(pDriverObject, L"win32k.sys", &g_Win32kAddr, &g_Win32kSize))
		{
			MyDbgPrintfEx("[%s] 获取Win32k和大小失败!\n", __FUNCTION__);
			Ret = FALSE;
		}
		//MyDbgPrintfEx("[%s] 获取g_Win32kAddr:%I64X Size:%X\n", __FUNCTION__, g_Win32kAddr, g_Win32kSize);


		//初始化全局句柄表变量
		PspCidTable = GetPspCidTable();
		if (!MmIsAddressValid(PspCidTable))
		{
			MyDbgPrintfEx("[%s] 获取PspCidTable变量失败 Data[%I64X]!\n", __FUNCTION__, PspCidTable);
			Ret = FALSE;
		}

		ObHeaderCookie = GetObHeaderCookie();
		if (!MmIsAddressValid(ObHeaderCookie))
		{
			MyDbgPrintfEx("[%s] 获取ObHeaderCookie变量失败 Data[%I64X]!\n", __FUNCTION__, ObHeaderCookie);
			Ret = FALSE;
		}

		ObTypeIndexTable = GetObTypeIndexTable();
		if (!MmIsAddressValid(ObTypeIndexTable))
		{
			MyDbgPrintfEx("[%s] 获取ObTypeIndexTable变量失败 Data[%I64X]!\n", __FUNCTION__, ObTypeIndexTable);
			Ret = FALSE;
		}

		ObpInfoMaskToOffset = GetObpInfoMaskToOffset();
		if (!MmIsAddressValid(ObpInfoMaskToOffset))
		{
			MyDbgPrintfEx("[%s] 获取ObpInfoMaskToOffset变量失败 Data[%I64X]!\n", __FUNCTION__, ObpInfoMaskToOffset);
			Ret = FALSE;
		}

		ObpRootDirectoryObject = GetObpRootDirectoryObject();
		if (!MmIsAddressValid(ObpRootDirectoryObject))
		{
			MyDbgPrintfEx("[%s] 获取ObpRootDirectoryObject变量失败 Data[%I64X]!\n", __FUNCTION__, ObpRootDirectoryObject);
			Ret = FALSE;
		}

		KeServiceDescriptorTable = GetKeServiceDescriptorTable();
		if (!MmIsAddressValid(KeServiceDescriptorTable))
		{
			MyDbgPrintfEx("[%s] 获取KeServiceDescriptorTable变量失败 Data[%I64X]!\n", __FUNCTION__, KeServiceDescriptorTable);
			Ret = FALSE;
		}

		KeServiceDescriptorTableShadow = GetKeServiceDescriptorTableShadow();
		if (!MmIsAddressValid(KeServiceDescriptorTableShadow))
		{
			MyDbgPrintfEx("[%s] 获取KeServiceDescriptorTableShadow变量失败 Data[%I64X]!\n", __FUNCTION__, KeServiceDescriptorTableShadow);
			Ret = FALSE;
		}

		PspCreateProcessNotifyRoutine = GetPspCreateProcessNotifyRoutine();
		if (!MmIsAddressValid(PspCreateProcessNotifyRoutine))
		{
			MyDbgPrintfEx("[%s] 获取PspCreateProcessNotifyRoutine变量失败 Data[%I64X]!\n", __FUNCTION__, PspCreateProcessNotifyRoutine);
			Ret = FALSE;
		}

		PspCreateProcessNotifyRoutineExCount = GetPspCreateProcessNotifyRoutineExCount();
		if (!MmIsAddressValid(PspCreateProcessNotifyRoutineExCount))
		{
			MyDbgPrintfEx("[%s] 获取PspCreateProcessNotifyRoutineExCount变量失败 Data[%I64X]!\n", __FUNCTION__, PspCreateProcessNotifyRoutineExCount);
			Ret = FALSE;
		}

		PspCreateProcessNotifyRoutineCount = GetPspCreateProcessNotifyRoutineCount();
		if (!MmIsAddressValid(PspCreateProcessNotifyRoutineCount))
		{
			MyDbgPrintfEx("[%s] 获取PspCreateProcessNotifyRoutineCount变量失败 Data[%I64X]!\n", __FUNCTION__, PspCreateProcessNotifyRoutineCount);
			Ret = FALSE;
		}

		PspLoadImageNotifyRoutine = GetPspLoadImageNotifyRoutine();
		if (!MmIsAddressValid(PspLoadImageNotifyRoutine))
		{
			MyDbgPrintfEx("[%s] 获取PspLoadImageNotifyRoutine变量失败 Data[%I64X]!\n", __FUNCTION__, PspLoadImageNotifyRoutine);
			Ret = FALSE;
		}

		PspLoadImageNotifyRoutineCount = GetPspLoadImageNotifyRoutineCount();
		if (!MmIsAddressValid(PspLoadImageNotifyRoutineCount))
		{
			MyDbgPrintfEx("[%s] 获取PspLoadImageNotifyRoutine变量失败 Data[%I64X]!\n", __FUNCTION__, PspLoadImageNotifyRoutine);
			Ret = FALSE;
		}

		PspCreateThreadNotifyRoutine = GetPspCreateThreadNotifyRoutine();
		if (!MmIsAddressValid(PspCreateThreadNotifyRoutine))
		{
			MyDbgPrintfEx("[%s] 获取PspCreateThreadNotifyRoutine变量失败 Data[%I64X]!\n", __FUNCTION__, PspCreateThreadNotifyRoutine);
			Ret = FALSE;
		}

		PspCreateThreadNotifyRoutineNonSystemCount = GetPspCreateThreadNotifyRoutineNonSystemCount();
		if (!MmIsAddressValid(PspCreateThreadNotifyRoutineNonSystemCount))
		{
			MyDbgPrintfEx("[%s] 获取PspCreateThreadNotifyRoutineNonSystemCount变量失败 Data[%I64X]!\n", __FUNCTION__, PspCreateThreadNotifyRoutineNonSystemCount);
			Ret = FALSE;
		}

		PspCreateThreadNotifyRoutineCount = GetPspCreateThreadNotifyRoutineCount();
		if (!MmIsAddressValid(PspCreateThreadNotifyRoutineCount))
		{
			MyDbgPrintfEx("[%s] 获取PspCreateThreadNotifyRoutineCount变量失败 Data[%I64X]!\n", __FUNCTION__, PspCreateThreadNotifyRoutineCount);
			Ret = FALSE;
		}

		IopNotifyShutdownQueueHead = GetIopNotifyShutdownQueueHead();
		if (!MmIsAddressValid(IopNotifyShutdownQueueHead))
		{
			MyDbgPrintfEx("[%s] 获取IopNotifyShutdownQueueHead变量失败 Data[%I64X]!\n", __FUNCTION__, IopNotifyShutdownQueueHead);
			Ret = FALSE;
		}

		// IopFsNotifyChangeQueueHead：用 HDE 签名定位（不依赖 R3 PDB，DriverEntry 早期就能用）
		// MountAware 在 Win10/11 上合并到了同一全局，所以另一个保持 NULL，遍历时跳过即可。
		IopFsNotifyChangeQueueHead = (PULONG64)GetIopFsNotifyChangeQueueHead();
		MyDbgPrintfEx("[InitGlobalVariable] IopFsNotifyChangeQueueHead=%p\n", IopFsNotifyChangeQueueHead);

		KeBugCheckCallbackListHead = GetKeBugCheckCallbackListHead();
		if (!MmIsAddressValid(KeBugCheckCallbackListHead))
		{
			MyDbgPrintfEx("[%s] 获取KeBugCheckCallbackListHead变量失败 Data[%I64X]!\n", __FUNCTION__, KeBugCheckCallbackListHead);
			Ret = FALSE;
		}

		PnpDeviceClassNotifyList = GetPnpDeviceClassNotifyList();
		if (!MmIsAddressValid(PnpDeviceClassNotifyList))
		{
			MyDbgPrintfEx("[%s] 获取PnpDeviceClassNotifyList变量失败 Data[%I64X]!\n", __FUNCTION__, PnpDeviceClassNotifyList);
			Ret = FALSE;
		}

		PnpDeferredRegistrationList = GetPnpDeferredRegistrationList();
		if (!MmIsAddressValid(PnpDeferredRegistrationList))
		{
			MyDbgPrintfEx("[%s] 获取PnpDeferredRegistrationList变量失败 Data[%I64X]!\n", __FUNCTION__, PnpDeferredRegistrationList);
			Ret = FALSE;
		}

		PnpProfileNotifyList = GetPnpProfileNotifyList();
		if (!MmIsAddressValid(PnpProfileNotifyList))
		{
			MyDbgPrintfEx("[%s] 获取PnpProfileNotifyList变量失败 Data[%I64X]!\n", __FUNCTION__, PnpProfileNotifyList);
			Ret = FALSE;
		}


		CallbackListHead = GetCallbackListHead();
		if (!MmIsAddressValid(CallbackListHead))
		{
			MyDbgPrintfEx("[%s] 获取CallbackListHead变量失败 Data[%I64X]!\n", __FUNCTION__, CallbackListHead);
			Ret = FALSE;
		}

		EtwpHostSiloState = GetEtwpHostSiloState();
		if (!MmIsAddressValid(EtwpHostSiloState))
		{
			MyDbgPrintfEx("[%s] 获取EtwpHostSiloState变量失败 Data[%I64X]!\n", __FUNCTION__, EtwpHostSiloState);
			Ret = FALSE;
		}

		EtwpDebuggerData = GetEtwpDebuggerData();
		if (!MmIsAddressValid(EtwpDebuggerData))
		{
			MyDbgPrintfEx("[%s] 获取EtwpDebuggerData变量失败 Data[%I64X]!\n", __FUNCTION__, EtwpDebuggerData);
			Ret = FALSE;
		}

		PerfGlobalGroupMask = GetPerfGlobalGroupMask();
		if (!MmIsAddressValid(PerfGlobalGroupMask))
		{
			MyDbgPrintfEx("[%s] 获取PerfGlobalGroupMask变量失败 Data[%I64X]!\n", __FUNCTION__, PerfGlobalGroupMask);
			Ret = FALSE;
		}

		HalpPerformanceCounter = GetHalpPerformanceCounter();
		if (!MmIsAddressValid(HalpPerformanceCounter))
		{
			MyDbgPrintfEx("[%s] 获取HalpPerformanceCounter变量失败 Data[%I64X]!\n", __FUNCTION__, HalpPerformanceCounter);
			Ret = FALSE;
		}

		MiFlags = GetMiFlags();
		if (!MmIsAddressValid(MiFlags))
		{
			MyDbgPrintfEx("[%s] 获取MiFlags变量失败 Data[%I64X]!\n", __FUNCTION__, MiFlags);
			Ret = FALSE;
		}

		IopTimerQueueHead = (PLIST_ENTRY)GetIopTimerQueueHead();
		if (!MmIsAddressValid(IopTimerQueueHead))
		{
			MyDbgPrintfEx("[%s] 获取IopTimerQueueHead变量失败 Data[%I64X]!\n", __FUNCTION__, IopTimerQueueHead);
			Ret = FALSE;
		}

		KiProcessorBlock = GetKiProcessorBlock();
		if (!MmIsAddressValid(KiProcessorBlock))
		{
			MyDbgPrintfEx("[%s] 获取KiProcessorBlock变量失败 Data[%I64X]!\n", __FUNCTION__, KiProcessorBlock);
			Ret = FALSE;
		}

		KiWaitNever = GetKiWaitNever();
		if (!MmIsAddressValid(KiWaitNever))
		{
			MyDbgPrintfEx("[%s] 获取KiWaitNever变量失败 Data[%I64X]!\n", __FUNCTION__, KiWaitNever);
			Ret = FALSE;
		}

		KiWaitAlways = GetKiWaitAlways();
		if (!MmIsAddressValid(KiWaitAlways))
		{
			MyDbgPrintfEx("[%s] 获取KiWaitAlways变量失败 Data[%I64X]!\n", __FUNCTION__, KiWaitAlways);
			Ret = FALSE;
		}
		/*

				g_PspNotifyEnableMaskInfo.pSrcMask = GetPspNotifyEnableMask();
				if (!MmIsAddressValid(g_PspNotifyEnableMaskInfo.pSrcMask))
				{
					MyDbgPrintfEx("[%s] 获取PspNotifyEnableMask变量失败 Data[%I64X]!\n", __FUNCTION__, g_PspNotifyEnableMaskInfo.pSrcMask);
					Ret = FALSE;
					break;
				}
		*/
		PspSystemPartition = GetPspSystemPartition();
		if (!MmIsAddressValid(PspSystemPartition))
		{
			MyDbgPrintfEx("[%s] 获取PspSystemPartition变量失败 Data[%I64X]!\n", __FUNCTION__, PspSystemPartition);
			Ret = FALSE;
		}

		MyPspTerminateProcess = GetPspTerminateProcess();
		if (!MmIsAddressValid(MyPspTerminateProcess))
		{
			MyDbgPrintfEx("[%s] 获取MyPspTerminateProcess变量失败 Data[%I64X]!\n", __FUNCTION__, MyPspTerminateProcess);
			Ret = FALSE;
		}

		IopNetworkFileSystemQueueHead = GetIopNetworkFileSystemQueueHead();
		if (!MmIsAddressValid(IopNetworkFileSystemQueueHead))
		{
			MyDbgPrintfEx("[%s] 获取IopNetworkFileSystemQueueHead变量失败 Data[%I64X]!\n", __FUNCTION__, IopNetworkFileSystemQueueHead);
			Ret = FALSE;
		}

		IopCdRomFileSystemQueueHead = GetIopCdRomFileSystemQueueHead();
		if (!MmIsAddressValid(IopCdRomFileSystemQueueHead))
		{
			MyDbgPrintfEx("[%s] 获取IopCdRomFileSystemQueueHead变量失败 Data[%I64X]!\n", __FUNCTION__, IopCdRomFileSystemQueueHead);
			Ret = FALSE;
		}

		IopDiskFileSystemQueueHead = GetIopDiskFileSystemQueueHead();
		if (!MmIsAddressValid(IopDiskFileSystemQueueHead))
		{
			MyDbgPrintfEx("[%s] 获取IopDiskFileSystemQueueHead变量失败 Data[%I64X]!\n", __FUNCTION__, IopDiskFileSystemQueueHead);
			Ret = FALSE;
		}

		IopTapeFileSystemQueueHead = GetIopTapeFileSystemQueueHead();
		if (!MmIsAddressValid(IopTapeFileSystemQueueHead))
		{
			MyDbgPrintfEx("[%s] 获取IopTapeFileSystemQueueHead变量失败 Data[%I64X]!\n", __FUNCTION__, IopTapeFileSystemQueueHead);
			Ret = FALSE;
		}

		//重载内核
		LoadNtosKrnel();

		//重载Win32k
		LoadWin32k();

	} while (0);

	return Ret;
}

UCHAR GetNtoskrnlMoudleBaseAddr(PDRIVER_OBJECT pDriver, PWCHAR pModuleName, PULONG64 MoudleAddr, PSIZE_T MoudleSize)
{
	////////////////////////////////////////////////////////////////////////
	//变量声明区域
	PLDR_DATA_TABLE_ENTRY pldr = 0;
	PLIST_ENTRY CurrentList = 0;
	PLIST_ENTRY NextList = 0;
	UNICODE_STRING NtoskrnlName = { 0 };
	////////////////////////////////////////////////////////////////////////

	//验证名称参数
	if (!MmIsAddressValid(pModuleName))
	{
		return FALSE;
	}

	////////////////////////////////////////////////////////////////////////
	//变量初始化区域
	pldr = (PLDR_DATA_TABLE_ENTRY)pDriver->DriverSection;
	CurrentList = pldr->InLoadOrderLinks.Blink;
	NextList = CurrentList->Blink;
	RtlInitUnicodeString(&NtoskrnlName, pModuleName);
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	//项目需求区域
	while (CurrentList != NextList)
	{
		pldr = CONTAINING_RECORD(NextList, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

		if (pldr->DllBase != 0 && MmIsAddressValid(pldr) != FALSE)
		{
			if (RtlCompareUnicodeString(&NtoskrnlName, &pldr->BaseDllName, TRUE) == 0)
			{
				if (MmIsAddressValid(MoudleAddr))
				{
					*MoudleAddr = (ULONG64)pldr->DllBase;//得到模块基地址
				}

				if (MmIsAddressValid(MoudleSize))
				{
					*MoudleSize = (SIZE_T)pldr->SizeOfImage;//得到模块大小
				}

				return TRUE;
			}
		}
		NextList = NextList->Blink;
	}
	return FALSE;
	////////////////////////////////////////////////////////////////////////
}

PULONG64 GetKiProcessorBlock()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrKeUserModeCallback = { 0 };
	RtlInitUnicodeString(&StrKeUserModeCallback, L"KeUserModeCallback");
	pFunAddr = MmGetSystemRoutineAddress(&StrKeUserModeCallback);

	//RTL_OSVERSIONINFOEXW version = { 0 };
	//RtlGetVersion(&version);

	//版本Win10 19045 版本
	//if (version.dwMajorVersion == 10 && version.dwBuildNumber == 19045)
	//{
	if (MmIsAddressValid(pFunAddr))
	{
		UCHAR Opcode[] = { 0x8B,0x83,0x4C,0x02,0x00,0x00,0x48,0x8D,0x0D };
		for (int i = 0; i < 1000; i++)
		{
			if (RtlCompareMemory(&pFunAddr[i], Opcode, sizeof(Opcode)) == sizeof(Opcode))
			{
				PUCHAR pAddr = &pFunAddr[i] + sizeof(Opcode);
				ULONG32 Offset = *(PULONG32)pAddr;					//获取偏移
				return pAddr + 4 + Offset;							//获取函数地址
			}
		}
	}
	//}
	return NULL;
}

ULONG64 GetPspCidTable()
{
	//定位PspReferenceCidTableEntry获取全局变量PspCidTable

	/*
	PAGE : 00000001405F113F 48 89 7C 24 20					mov[rsp + arg_18], rdi
	PAGE : 00000001405F1144 41 56							push    r14
	PAGE : 00000001405F1146 48 83 EC 40						sub     rsp, 40h
	PAGE : 00000001405F114A 48 8B 05 7F B4 70 00			mov     rax, cs : PspCidTable
	*/
	UCHAR szBuf[] = { 0x48,0x89,0x7C,0x24,0x20,0x41,0x56,0x48,0x83,0xEC,0x40,0x48,0x8B,0x05 };
	CONST UCHAR* pMask = "xxxxxxxxxxxxxx";


	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "PAGE");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	//
	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetObHeaderCookie()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrKeUserModeCallback = { 0 };
	RtlInitUnicodeString(&StrKeUserModeCallback, L"ObGetObjectType");
	pFunAddr = MmGetSystemRoutineAddress(&StrKeUserModeCallback);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//PAGE:00000001406DE312 0F B6 0D 13 E4 61 00          movzx   ecx, byte ptr cs:ObHeaderCookie

	UCHAR szBuf[] = { 0x0F,0xB6,0x0D };

	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;
}

ULONG64 GetObTypeIndexTable()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrKeUserModeCallback = { 0 };
	RtlInitUnicodeString(&StrKeUserModeCallback, L"ObGetObjectType");
	pFunAddr = MmGetSystemRoutineAddress(&StrKeUserModeCallback);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//PAGE:00000001406DE31C 48 8D 0D 5D EB 61 00          lea     rcx, ObTypeIndexTable
	UCHAR szBuf[] = { 0x48,0x8D,0x0D };

	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;
}

ULONG64 GetObpInfoMaskToOffset()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrKeUserModeCallback = { 0 };
	RtlInitUnicodeString(&StrKeUserModeCallback, L"PsInsertSiloContext");
	pFunAddr = MmGetSystemRoutineAddress(&StrKeUserModeCallback);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//PAGE : 00000001405CE3B9 83 E0 7F and eax, 7Fh
	//PAGE : 00000001405CE3BC 48 8D 15 7D 7A 65 00          lea     rdx, ObpInfoMaskToOffset
	UCHAR szBuf[] = { 0x83,0xE0,0x7F,0x48,0x8D,0x15 };

	for (int i = 0; i < 0x200; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;
}

ULONG64 GetObpRootDirectoryObject()
{
	//定位ObQueryNameStringMode获取全局变量ObpRootDirectoryObject
	//PAGE : 00000001406B368E 48 3B 8C 24 80 00 00 00       cmp     rcx, [rsp + 118h + var_98]
	//PAGE : 00000001406B3696 0F 84 48 01 00 00             jz      loc_1406B37E4
	//PAGE : 00000001406B3696
	//PAGE : 00000001406B369C 48 3B 0D 6D 23 57 00          cmp     rcx, cs : ObpRootDirectoryObject


	UCHAR szBuf[] = { 0x48,0x3B,0x8C,0x24,0x80,0x00,0x00,0x00,0x0F,0x84,0x00,0x00,0x00,0x00,0x48,0x3B,0x0D };
	CONST UCHAR* pMask = "xxxxxxxxxx????xxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "PAGE");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetKeServiceDescriptorTable()
{
	//.text : 000000014040952F 25 FF 0F 00 00				and		eax, 0FFFh
	//.text : 0000000140409534 4C 8D 15 85 83 9F 00			lea     r10, KeServiceDescriptorTable

	UCHAR szBuf[] = { 0x25,0xFF,0x0F,0x00,0x00,0x4C,0x8D,0x15 };
	CONST UCHAR* pMask = "xxxxxxxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "text");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;

}

ULONG64 GetKeServiceDescriptorTableShadow()
{
	//.text : 000000014040952F 25 FF 0F 00 00				and		eax, 0FFFh
	//.text : 0000000140409534 4C 8D 15 85 83 9F 00			lea     r10, KeServiceDescriptorTable
	//.tex t: 000000014040953B 4C 8D 1D FE 34 8F 00			lea     r11, KeServiceDescriptorTableShadow

	UCHAR szBuf[] = { 0x25,0xFF,0x0F,0x00,0x00,0x4C,0x8D,0x15,0x00,0x00,0x00,0x00,0x4C,0x8D,0x1D };
	CONST UCHAR* pMask = "xxxxxxxx????xxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "text");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;

}

ULONG64 GetPspCreateProcessNotifyRoutine()
{

	//PAGE : 000000014076ACB7 83 E9 01                      sub     ecx, 1
	//PAGE : 000000014076ACBA 75 22                         jnz     short loc_14076ACDE
	//PAGE : 000000014076ACBA
	//PAGE : 000000014076ACBC 48 8D 0D 9D 15 58 00          lea     rcx, PspCreateProcessNotifyRoutine
	UCHAR szBuf[] = { 0x83,0xE9,0x01,0x75,0x22,0x48,0x8D,0x0D };
	CONST UCHAR* pMask = "xxxxxxxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "PAGE");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetPspCreateProcessNotifyRoutineExCount()
{
	//PAGE : 000000014078A2A7 85 F6                         test    esi, esi
	//PAGE : 000000014078A2A9 74 38                         jz      short loc_14078A2E3
	//PAGE : 000000014078A2AB F0 FF 05 26 47 5A 00          lock inc cs : PspCreateProcessNotifyRoutineExCount
	UCHAR szBuf[] = { 0x85,0xF6,0x74,0x38,0xF0,0xFF,0x05 };
	CONST UCHAR* pMask = "xxxxxxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "PAGE");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetPspCreateProcessNotifyRoutineCount()
{
	//PAGE : 000000014078A2DC 33 D2							xor edx, edx
	//PAGE : 000000014078A2DE E9 71 FF FF FF                jmp     loc_14078A254
	//PAGE : 000000014078A2E3                               loc_14078A2E3 : ; CODE XREF : PspSetCreateProcessNotifyRoutine + 91↑j
	//PAGE : 000000014078A2E3 F0 FF 05 EA 46 5A 00          lock inc cs : PspCreateProcessNotifyRoutineCount	

	UCHAR szBuf[] = { 0x33,0xD2,0xE9,0x71,0xFF,0xFF,0xFF,0xF0,0xFF,0x05 };
	CONST UCHAR* pMask = "xxxxxxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "PAGE");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetPspLoadImageNotifyRoutine()
{
	//PAGE : 000000014076ACDE 83 F9 01                      cmp     ecx, 1
	//PAGE : 000000014076ACE1 75 12                         jnz     short loc_14076ACF5
	//PAGE : 000000014076ACE1
	//PAGE : 000000014076ACE3 48 8D 0D 76 17 58 00          lea     rcx, PspLoadImageNotifyRoutine

	UCHAR szBuf[] = { 0x83,0xF9,0x01,0x75,0x12,0x48,0x8D,0x0D };
	CONST UCHAR* pMask = "xxxxxxxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "PAGE");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetPspLoadImageNotifyRoutineCount()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrFunName = { 0 };
	RtlInitUnicodeString(&StrFunName, L"PsSetLoadImageNotifyRoutineEx");
	pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//PAGE : 0000000140789F8F F0 FF 05 3A 4A 5A 00          lock inc cs:PspLoadImageNotifyRoutineCount
	UCHAR szBuf[] = { 0xF0,0xFF,0x05 };

	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;
}

ULONG64 GetPspCreateThreadNotifyRoutine()
{
	//PAGE : 000000014076ACE3 48 8D 0D 76 17 58 00          lea     rcx, PspLoadImageNotifyRoutine
	//PAGE : 000000014076ACEA EB D7                         jmp     short loc_14076ACC3
	//PAGE : 000000014076ACEA
	//PAGE : 000000014076ACEC; -------------------------------------------------------------------------- -
	//PAGE : 000000014076ACEC
	//PAGE : 000000014076ACEC                               loc_14076ACEC : ; CODE XREF : PspEnumerateCallback + 5↑j
	//PAGE : 000000014076ACEC 48 8D 0D 6D 13 58 00          lea     rcx, PspCreateThreadNotifyRoutine

	UCHAR szBuf[] = { 0x48,0x8D,0x0D,0x00,0x00,0x00,0x00,0xEB,0xD7,0x48,0x8D,0x0D };
	CONST UCHAR* pMask = "xxx????xxxxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "PAGE");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetIopNotifyShutdownQueueHead()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrFunName = { 0 };
	RtlInitUnicodeString(&StrFunName, L"IoRegisterShutdownNotification");
	pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}


	//PAGE : 00000001407ADC56 48 8B D7                      mov     rdx, rdi
	//PAGE : 00000001407ADC59 48 8D 0D 00 7E 49 00          lea     rcx, IopNotifyShutdownQueueHead
	UCHAR szBuf[] = { 0x48,0x8B,0xD7,0x48,0x8D,0x0D };

	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;
}

// 用反汇编签名定位 nt!IopFsNotifyChangeQueueHead。
// 在 nt!IoRegisterFsRegistrationChangeMountAware 函数体内 ~+0x7a 处:
//   lea r12, [nt!IopFsNotifyChangeQueueHead]   字节序列: 4C 8D 25 ?? ?? ?? ??
// 旧系统回退到 IoRegisterFsRegistrationChange 内部同样模式。
ULONG64 GetIopFsNotifyChangeQueueHead()
{
	PUCHAR pFunAddr = NULL;
	UNICODE_STRING StrFunName = { 0 };
	RtlInitUnicodeString(&StrFunName, L"IoRegisterFsRegistrationChangeMountAware");
	pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
	if (!MmIsAddressValid(pFunAddr))
	{
		RtlInitUnicodeString(&StrFunName, L"IoRegisterFsRegistrationChange");
		pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
		if (!MmIsAddressValid(pFunAddr)) return FALSE;
	}

	UCHAR szBuf[] = { 0x4C, 0x8D, 0x25 };
	for (int i = 0; i < 0x200; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			PUCHAR p = &pFunAddr[i] + sizeof(szBuf);
			LONG32 Offset = *(PLONG32)p;
			return (ULONG64)(p + 4 + Offset);
		}
	}
	return FALSE;
}

ULONG64 GetKeBugCheckCallbackListHead()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrFunName = { 0 };
	RtlInitUnicodeString(&StrFunName, L"KeRegisterBugCheckCallback");
	pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//.text : 00000001403CA83D 4C 8D 05 8C 77 86 00          lea     r8, KeBugCheckCallbackListHead
	UCHAR szBuf[] = { 0x4C,0x8D,0x05 };

	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;

}

ULONG64 GetPnpDeviceClassNotifyList()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrFunName = { 0 };
	RtlInitUnicodeString(&StrFunName, L"IoRegisterPlugPlayNotification");
	pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//PAGE : 00000001405D9B7E 48 8D 15 3B 57 75 00          lea     rdx, PnpDeviceClassNotifyList
	UCHAR szBuf[] = { 0x48,0x8D,0x15 };

	for (int i = 0; i < 0x1000; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;
}

ULONG64 GetPnpDeferredRegistrationList()
{
	//PAGE : 00000001405D9DB2 44 88 76 3A                   mov[rsi + 3Ah], r14b
	//PAGE : 00000001405D9DB6 E8 E5 5C C7 FF                call    ExAcquireFastMutex
	//PAGE : 00000001405D9DB6
	//PAGE : 00000001405D9DBB 48 8B 05 66 4D 75 00          mov     rax, cs : qword_140D2EB28
	//PAGE : 00000001405D9DC2 48 8D 0D 57 4D 75 00          lea     rcx, PnpDeferredRegistrationList


	UCHAR szBuf[] = { 0x44,0x88,0x76,0x3A,0xE8,0x00,0x00,0x00,0x00,0x48,0x8B,0x05,0x00,0x00,0x00,0x00,0x48,0x8D,0x0D };
	CONST UCHAR* pMask = "xxxxx????xxx????xxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "PAGE");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetPnpProfileNotifyList()
{

	//PAGE : 00000001405D9CF3 E8 38 8C D1 FF                call    KeAcquireGuardedMutex
	//PAGE : 00000001405D9CF3
	//PAGE : 00000001405D9CF8 48 8B 05 59 4E 75 00          mov     rax, cs : qword_140D2EB58
	//PAGE : 00000001405D9CFF 48 8D 15 4A 4E 75 00          lea     rdx, PnpProfileNotifyList

	UCHAR szBuf[] = { 0xE8,0x00,0x00,0x00,0x00,0x48,0x8B,0x05,0x00,0x00,0x00,0x00,0x48,0x8D,0x15 };
	CONST UCHAR* pMask = "x????xxx????xxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "PAGE");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetPspCreateThreadNotifyRoutineCount()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrFunName = { 0 };
	RtlInitUnicodeString(&StrFunName, L"PsRemoveCreateThreadNotifyRoutine");
	pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//PAGE : 0000000140909A2D 48 83 7B 10 00                cmp     qword ptr[rbx + 10h], 0
	//PAGE : 0000000140909A32 48 8D 0D A3 4F 42 00          lea     rcx, PspCreateThreadNotifyRoutineCount
	UCHAR szBuf[] = { 0x48,0x83,0x7B,0x10,0x00,0x48,0x8D,0x0D };
	for (int i = 0; i < 0x1000; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;
}

ULONG64 GetPspCreateThreadNotifyRoutineNonSystemCount()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrFunName = { 0 };
	RtlInitUnicodeString(&StrFunName, L"PsRemoveCreateThreadNotifyRoutine");
	pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//PAGE : 0000000140909A39 48 8D 15 8C 4F 42 00          lea     rdx, PspCreateThreadNotifyRoutineNonSystemCount
	UCHAR szBuf[] = { 0x48,0x8D,0x15 };
	for (int i = 0; i < 0x1000; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;
}

ULONG64 GetCallbackListHead()
{

	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrFunName = { 0 };
	RtlInitUnicodeString(&StrFunName, L"CmUnRegisterCallback");
	pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//PAGE:0000000140866EEF 48 8D 0D 5A 14 3E 00          lea     rcx, CallbackListHead	
	UCHAR szBuf[] = { 0x48,0x8D,0x0D };
	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;
}

ULONG64 GetEtwpHostSiloState()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrFunName = { 0 };
	RtlInitUnicodeString(&StrFunName, L"EtwSendTraceBuffer");
	pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//.text : 00000001405A1007 48 8B 15 FA 9F 75 00          mov     rdx, cs : EtwpHostSiloState
	UCHAR szBuf[] = { 0x48,0x8B,0x15 };
	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;
}

ULONG64 GetEtwpDebuggerData()
{

	//.data : 0000000140C10DA8 20                            EtwpDebuggerData db  20h; 
	//.data : 0000000140C10DA9 02                            db    2
	//.data : 0000000140C10DAA 2C                            db  2Ch;,
	//.data : 0000000140C10DAB 08                            db    8
	//.data : 0000000140C10DAC 04                            db    4
	//.data : 0000000140C10DAD 38                            db  38h; 8
	//.data : 0000000140C10DAE 0C                            db  0Ch
	UCHAR szBuf[] = { 0x00,0x00,0x2C,0x08,0x04,0x38,0x0C };
	CONST UCHAR* pMask = "??xxxxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, ".data");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}
	return pAddr;
}

ULONG64 GetPerfGlobalGroupMask()
{

	//.text:000000014040969A 0F 85 93 04 00 00						jnz     loc_140409B33
	//.text:00000001404096A0 F7 05 DE 2D 8F 00 40 00 00 00			test    cs : PerfGlobalGroupMask.Masks + 8, 40h
	UCHAR szBuf[] = { 0x0F,0x85,0x00,0x00,0x00,0x00,0xF7,0x05 };
	CONST UCHAR* pMask = "xx????xx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, ".text");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetHalpPerformanceCounter()
{
	PUCHAR pFunAddr = NULL;

	UNICODE_STRING StrFunName = { 0 };
	RtlInitUnicodeString(&StrFunName, L"KeQueryPerformanceCounter");
	pFunAddr = MmGetSystemRoutineAddress(&StrFunName);
	if (!MmIsAddressValid(pFunAddr))
	{
		return FALSE;
	}

	//.text : 00000001402FAEA2 48 8B 3D 9F 0F 95 00          mov     rdi, cs : HalpPerformanceCounter
	UCHAR szBuf[] = { 0x48,0x8B,0x3D };
	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return FALSE;

}

ULONG64 GetIopTimerQueueHead()
{
	UNICODE_STRING IopTimerQueueHeadStr = { 0 };
	RtlInitUnicodeString(&IopTimerQueueHeadStr, L"IoInitializeTimer");

	PUCHAR pFunAddr = MmGetSystemRoutineAddress(&IopTimerQueueHeadStr);
	if (pFunAddr == NULL)
	{
		return FALSE;
	}


	//IopTimerQueueHead特征码
	//PAGE : 00000001407C2ECC 48 89 78 20                   mov[rax + 20h], rdi
	//PAGE : 00000001407C2ED0 48 8D 0D 79 2D 48 00          lea     rcx, IopTimerQueueHead; 

	ULONG64 dqRet = NULL;
	for (int i = 0; i < 500; i++)
	{
		if (pFunAddr[i] == 0x48 &&
			pFunAddr[i + 1] == 0x89 &&
			pFunAddr[i + 2] == 0x78 &&
			pFunAddr[i + 3] == 0x20 &&
			pFunAddr[i + 4] == 0x48 &&
			pFunAddr[i + 5] == 0x8D &&
			pFunAddr[i + 6] == 0x0D)
		{
			//找到了
			PUCHAR pAddr = (ULONG64)&pFunAddr[i] + 0x7;
			ULONG32 Offset = *(PULONG32)pAddr;		//获取偏移
			dqRet = (pAddr + 4) + Offset;			//
			break;
		}
	}
	return dqRet;
}

ULONG64 GetMiFlags()
{
	//.text : 000000014020E9F8 48 83 C4 20						add     rsp, 20h
	//.text : 000000014020E9FC 5F								pop     rdi
	//.text : 000000014020E9FD C3								retn
	//.text : 000000014020E9FE CC								db 0CCh
	//.text : 000000014020E9FF F7 05 F7 D9 AE 00 00 00 C0 00	test    cs : MiFlags, 0C00000h
	UCHAR szBuf[] = { 0x48,0x83,0xC4,0x20,0x5F,0xC3,0xCC,0xF7,0x05 };
	CONST UCHAR* pMask = "xxxxxxxxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, ".text");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetKiWaitNever()
{

	UNICODE_STRING KeSetTimerExStr = { 0 };
	RtlInitUnicodeString(&KeSetTimerExStr, L"KeSetTimerEx");

	PUCHAR pFunAddr = MmGetSystemRoutineAddress(&KeSetTimerExStr);
	if (pFunAddr == NULL)
	{
		return FALSE;
	}
	//.text:000000014025981C 48 8B 05 E5 2F AA 00          mov     rax, cs:KiWaitNever
	UCHAR szBuf[] = { 0x48,0x8B,0x05 };
	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return NULL;
}

ULONG64 GetKiWaitAlways()
{

	UNICODE_STRING KeSetTimerExStr = { 0 };
	RtlInitUnicodeString(&KeSetTimerExStr, L"KeSetTimerEx");

	PUCHAR pFunAddr = MmGetSystemRoutineAddress(&KeSetTimerExStr);
	if (pFunAddr == NULL)
	{
		return FALSE;
	}
	//.text:0000000140259826 48 8B 35 E3 31 AA 00          mov     rsi, cs:KiWaitAlways
	UCHAR szBuf[] = { 0x48 ,0x8B, 0x35 };
	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return NULL;
}

ULONG64 GetPspNotifyEnableMask()
{


	return NULL;
}

ULONG64 GetPspSystemPartition()
{
	UNICODE_STRING StrExQueueWorkItem = { 0 };
	RtlInitUnicodeString(&StrExQueueWorkItem, L"ExQueueWorkItem");

	PUCHAR pFunAddr = MmGetSystemRoutineAddress(&StrExQueueWorkItem);
	if (pFunAddr == NULL)
	{
		return FALSE;
	}

	//.text:00000001402C10CE 4C 8B 05 8B B6 A3 00          mov     r8, cs : PspSystemPartition; _EPARTITION
	UCHAR szBuf[] = { 0x4C ,0x8B, 0x05 };
	for (int i = 0; i < 0x100; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return NULL;
}

ULONG64 GetPspTerminateProcess()
{
	//PAGE : 00000001406F610B 44 8B CD                      mov     r9d, ebp
	//PAGE : 00000001406F610E 45 8B C7                      mov     r8d, r15d
	//PAGE : 00000001406F6111 48 8B D7                      mov     rdx, rdi
	//PAGE : 00000001406F6114 49 8B CE                      mov     rcx, r14
	//PAGE : 00000001406F6117 E8 24 01 00 00                call    PspTerminateProcess
	UCHAR szBuf[] = { 0x44,0x8B,0xCD,0x45,0x8B,0xC7,0x48,0x8B,0xD7,0x49,0x8B,0xCE,0xE8 };
	CONST UCHAR* pMask = "xxxxxxxxxxxxx";
	PUCHAR pAddr = FindMoudleInMemoryAddrEx(g_NtoskrnlAddr, g_NtoskrnlSize, szBuf, pMask, "PAGE");
	if (!MmIsAddressValid(pAddr))
	{
		return NULL;
	}

	pAddr = pAddr + sizeof(szBuf);

	ULONG32 offset = *(PULONG32)pAddr;

	return pAddr + 4 + offset;
}

ULONG64 GetIopNetworkFileSystemQueueHead()
{

	UNICODE_STRING StrIoRegisterFsRegistrationChangeMountAware = { 0 };
	RtlInitUnicodeString(&StrIoRegisterFsRegistrationChangeMountAware, L"IoRegisterFsRegistrationChangeMountAware");

	PUCHAR pFunAddr = MmGetSystemRoutineAddress(&StrIoRegisterFsRegistrationChangeMountAware);
	if (pFunAddr == NULL)
	{
		return FALSE;
	}

	//PAGE : 00000001407B8DA5 45 33 C0 xor r8d, r8d
	//PAGE : 00000001407B8DA8 48 8D 0D C1 CC 48 00          lea     rcx, IopNetworkFileSystemQueueHead
	UCHAR szBuf[] = { 0xC0,0x48 ,0x8D, 0x0D };
	for (int i = 0; i < 0x1000; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return NULL;
}

ULONG64 GetIopCdRomFileSystemQueueHead()
{

	UNICODE_STRING StrIoRegisterFsRegistrationChangeMountAware = { 0 };
	RtlInitUnicodeString(&StrIoRegisterFsRegistrationChangeMountAware, L"IoRegisterFsRegistrationChangeMountAware");

	PUCHAR pFunAddr = MmGetSystemRoutineAddress(&StrIoRegisterFsRegistrationChangeMountAware);
	if (pFunAddr == NULL)
	{
		return FALSE;
	}

	//PAGE : 00000001407B8DB7 41 B0 01                      mov     r8b, 1
	//PAGE : 00000001407B8DBA 48 8D 0D 4F CC 48 00          lea     rcx, IopCdRomFileSystemQueueHead
	UCHAR szBuf[] = { 0x01,0x48 ,0x8D, 0x0D };
	for (int i = 0; i < 0x1000; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return NULL;
}

ULONG64 GetIopDiskFileSystemQueueHead()
{

	UNICODE_STRING StrIoRegisterFsRegistrationChangeMountAware = { 0 };
	RtlInitUnicodeString(&StrIoRegisterFsRegistrationChangeMountAware, L"IoRegisterFsRegistrationChangeMountAware");

	PUCHAR pFunAddr = MmGetSystemRoutineAddress(&StrIoRegisterFsRegistrationChangeMountAware);
	if (pFunAddr == NULL)
	{
		return FALSE;
	}

	//PAGE : 00000001407B8DC9 41 B0 01                      mov     r8b, 1
	//PAGE : 00000001407B8DCC 48 8D 0D 6D CC 48 00          lea     rcx, IopDiskFileSystemQueueHead
	UCHAR szBuf[] = { 0x01,0x48 ,0x8D, 0x0D };
	for (int i = 0; i < 0x1000; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf) + (18 * 1);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return NULL;
}

ULONG64 GetIopTapeFileSystemQueueHead()
{

	UNICODE_STRING StrIoRegisterFsRegistrationChangeMountAware = { 0 };
	RtlInitUnicodeString(&StrIoRegisterFsRegistrationChangeMountAware, L"IoRegisterFsRegistrationChangeMountAware");

	PUCHAR pFunAddr = MmGetSystemRoutineAddress(&StrIoRegisterFsRegistrationChangeMountAware);
	if (pFunAddr == NULL)
	{
		return FALSE;
	}

	//PAGE : 00000001407B8DC9 41 B0 01                      mov     r8b, 1
	//PAGE : 00000001407B8DCC 48 8D 0D 3B CC 48 00          lea     rcx, IopTapeFileSystemQueueHead
	UCHAR szBuf[] = { 0x01,0x48 ,0x8D, 0x0D };
	for (int i = 0; i < 0x1000; i++)
	{
		if (RtlCompareMemory(&pFunAddr[i], szBuf, sizeof(szBuf)) == sizeof(szBuf))
		{
			pFunAddr = &pFunAddr[i] + sizeof(szBuf) + (18 * 2);
			ULONG32 Offset = *(PULONG32)pFunAddr;
			return pFunAddr + 4 + Offset;
		}
	}
	return NULL;
}