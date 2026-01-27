#pragma once


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//size
int g_Size_PEB;															//_PEBµÄ´óÐ¡
#define _SIZE_PEB												g_Size_PEB
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_POOL_ATTRIBUTE
#define POOL_RW													0x2
#define POOL_P													0x1
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////KPRCB
int g_Offset_KPRCB_CurrentThread;
int g_Offset_KPRCB_RspBase;														//_KPCR_RspBaseµÄÆ«ÒÆ
int g_Offset_KPCR_GdtBase;														//_KPCR_GdtBaseµÄÆ«ÒÆ
int g_Offset_KPCR_IdtBase;														//_KPCR_GdtBaseµÄÆ«ÒÆ
int g_Offset_KPRCB_TimerTable;														//_KPCR_GdtBaseµÄÆ«ÒÆ

#define _KPCR_CurrentThread										g_Offset_KPRCB_CurrentThread
#define _KPCR_RspBase											g_Offset_KPRCB_RspBase
#define _KPCR_GdtBase											g_Offset_KPCR_GdtBase
#define _KPCR_IdtBase											g_Offset_KPCR_IdtBase
#define _KPRCB_TimerTable										g_Offset_KPRCB_TimerTable			//_KTIMER_TABLE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_ETHREAD
int g_Offset_KTHREAD_ApcState;                      // _ETHREAD_ApcState µÄÆ«ÒÆ
int g_Offset_KTHREAD_ThreadFlags;					// _ETHREAD_ThreadFlagsSpare µÄÆ«ÒÆ
int g_Offset_KTHREAD_SystemCallNumber;              // _ETHREAD_SystemCallNumber µÄÆ«ÒÆ
int g_Offset_KTHREAD_Priority;                      // _ETHREAD_Priority µÄÆ«ÒÆ
int g_Offset_KTHREAD_Teb;                           // _ETHREAD_Teb µÄÆ«ÒÆ
int g_Offset_KTHREAD_ContextSwitches;               // _ETHREAD_ContextSwitches µÄÆ«ÒÆ
int g_Offset_KTHREAD_State;                         // _ETHREAD_State µÄÆ«ÒÆ
int g_Offset_KTHREAD_Process;                       // _ETHREAD_Process µÄÆ«ÒÆ
int g_Offset_KTHREAD_PreviousMode;                  // _ETHREAD_PreviousMode µÄÆ«ÒÆ
int g_Offset_ETHREAD_CreateTime;                    // _ETHREAD_CreateTime µÄÆ«ÒÆ
int g_Offset_ETHREAD_StartAddress;                  // _ETHREAD_StartAddress µÄÆ«ÒÆ
int g_Offset_KTHREAD_UniqueProcess;                 // _ETHREAD_UniqueProcess µÄÆ«ÒÆ
int g_Offset_KTHREAD_UniqueThread;                  // _ETHREAD_UniqueThread µÄÆ«ÒÆ
int g_Offset_ETHREAD_Win32StartAddress;             // _ETHREAD_Win32StartAddress µÄÆ«ÒÆ
int g_Offset_ETHREAD_ThreadListEntry;				// _ETHREAD_ThreadListEntry_Flink µÄÆ«ÒÆ

#define _ThreadFlagsSpare_GuiThread								0x80
#define _ThreadFlagsSpare_RestrictedGuiThread					0x100000
#define _KTHREAD_ApcState										g_Offset_KTHREAD_ApcState
#define _KTHREAD_ThreadFlagsSpare_ThreadFlags					g_Offset_KTHREAD_ThreadFlags
#define _KTHREAD_SystemCallNumber								g_Offset_KTHREAD_SystemCallNumber
#define _KTHREAD_Priority										g_Offset_KTHREAD_Priority
#define _KTHREAD_Teb											g_Offset_KTHREAD_Teb
#define _KTHREAD_ContextSwitches								g_Offset_KTHREAD_ContextSwitches
#define _KTHREAD_State											g_Offset_KTHREAD_State
#define _KTHREAD_Process										g_Offset_KTHREAD_Process
#define _KTHREAD_PreviousMode									g_Offset_KTHREAD_PreviousMode
#define _ETHREAD_CreateTime										g_Offset_ETHREAD_CreateTime
#define _ETHREAD_StartAddress									g_Offset_ETHREAD_StartAddress
#define _KTHREAD_UniqueProcess									g_Offset_KTHREAD_UniqueProcess
#define _KTHREAD_UniqueThread									g_Offset_KTHREAD_UniqueThread
#define _ETHREAD_Win32StartAddress								g_Offset_ETHREAD_Win32StartAddress
#define _ETHREAD_ThreadListEntry								g_Offset_ETHREAD_ThreadListEntry
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_KAPC_STATE
int g_Offset_KAPC_STATE_Process;						//_KAPC_STATE_ProcessµÄÆ«ÒÆ
#define _KAPC_STATE_Process										g_Offset_KAPC_STATE_Process
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_MMSUPPORT_FULL
int g_Offset_MMSUPPORT_FULL_Shared;
#define _MMSUPPORT_FULL_Shared									g_Offset_MMSUPPORT_FULL_Shared
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_MMSUPPORT_SHARED
int g_Offset_MMSUPPORT_SHARED_ShadowMapping;
#define _MMSUPPORT_SHARED_ShadowMapping							g_Offset_MMSUPPORT_SHARED_ShadowMapping
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// _EPROCESS

int g_Offset_EPROCESS_CreateTime;						 //_EPROCESS_ActiveProcessLinks_FlinkµÄÆ«ÒÆ;
int g_Offset_EPROCESS_UniqueProcessId;					 //_EPROCESS_UniqueProcessIdµÄÆ«ÒÆ;		
int g_Offset_EPROCESS_ActiveProcessLinks_Flink;			 //_EPROCESS_ActiveProcessLinks_FlinkµÄÆ«ÒÆ;
int g_Offset_KPROCESS_AddressPolicy;					 //_EPROCESS_AddressPolicyµÄÆ«ÒÆ;	
int g_Offset_EPROCESS_Token;     						  // _EPROCESS_Token µÄÆ«ÒÆ
int g_Offset_EPROCESS_InheritedFromUniqueProcessId;      // _EPROCESS_InheritedFromUniqueProcessId µÄÆ«ÒÆ
int g_Offset_EPROCESS_Peb;                               // _EPROCESS_Peb µÄÆ«ÒÆ
int g_Offset_EPROCESS_Session;                           // _EPROCESS_Session µÄÆ«ÒÆ
int g_Offset_EPROCESS_ObjectTable;                       // _EPROCESS_ObjectTable µÄÆ«ÒÆ
int g_Offset_EPROCESS_DebugPort;                         // _EPROCESS_DebugPort µÄÆ«ÒÆ
int g_Offset_EPROCESS_WoW64Process;                      // _EPROCESS_WoW64Process µÄÆ«ÒÆ
int g_Offset_EPROCESS_ImageFilePointer;                  // _EPROCESS_ImageFilePointer µÄÆ«ÒÆ
int g_Offset_EPROCESS_ImageFileName;                     // _EPROCESS_ImageFileName µÄÆ«ÒÆ
int g_Offset_EPROCESS_SeAuditProcessCreationInfo;        // _EPROCESS_SeAuditProcessCreationInfo µÄÆ«ÒÆ
int g_Offset_EPROCESS_ThreadListHead;                    // _EPROCESS_ThreadListHead µÄÆ«ÒÆ
int g_Offset_EPROCESS_ActiveThreads;                     // _EPROCESS_ActiveThreads µÄÆ«ÒÆ
int g_Offset_EPROCESS_Vm;                                // _EPROCESS_Vm µÄÆ«ÒÆ
int g_Offset_EPROCESS_VadRoot;                           // _EPROCESS_VadRoot µÄÆ«ÒÆ
int g_Offset_EPROCESS_VadCount;                          // _EPROCESS_VadCount µÄÆ«ÒÆ
int g_Offset_EPROCESS_Protection;                        // _EPROCESS_Protection µÄÆ«ÒÆ
int g_Offset_EPROCESS_AddressPolicyFrozen;               // _EPROCESS_AddressPolicyFrozen µÄÆ«ÒÆ
int g_Offset_EPROCESS_SystemProcess;

#define _EPROCESS_AddressPolicy									g_Offset_KPROCESS_AddressPolicy
#define _EPROCESS_UniqueProcessId								g_Offset_EPROCESS_UniqueProcessId
#define _EPROCESS_ActiveProcessLinks_Flink						g_Offset_EPROCESS_ActiveProcessLinks_Flink
#define _EPROCESS_CreateTime									g_Offset_EPROCESS_CreateTime
#define _EPROCESS_Token											g_Offset_EPROCESS_Token
#define _EPROCESS_InheritedFromUniqueProcessId					g_Offset_EPROCESS_InheritedFromUniqueProcessId
#define _EPROCESS_Peb											g_Offset_EPROCESS_Peb
#define _EPROCESS_Session										g_Offset_EPROCESS_Session
#define _EPROCESS_ObjectTable									g_Offset_EPROCESS_ObjectTable
#define _EPROCESS_DebugPort										g_Offset_EPROCESS_DebugPort
#define _EPROCESS_WoW64Process									g_Offset_EPROCESS_WoW64Process
#define _EPROCESS_ImageFilePointer								g_Offset_EPROCESS_ImageFilePointer
#define _EPROCESS_ImageFileName									g_Offset_EPROCESS_ImageFileName
#define _EPROCESS_SeAuditProcessCreationInfo					g_Offset_EPROCESS_SeAuditProcessCreationInfo
#define _EPROCESS_ThreadListHead								g_Offset_EPROCESS_ThreadListHead
#define _EPROCESS_ActiveThreads									g_Offset_EPROCESS_ActiveThreads
#define _EPROCESS_Vm											g_Offset_EPROCESS_Vm
#define _EPROCESS_VadRoot										g_Offset_EPROCESS_VadRoot
#define _EPROCESS_VadCount										g_Offset_EPROCESS_VadCount
#define _EPROCESS_Protection									g_Offset_EPROCESS_Protection
#define _EPROCESS_AddressPolicyFrozen							g_Offset_EPROCESS_AddressPolicyFrozen
#define _EPROCESS_SystemProcess									g_Offset_EPROCESS_SystemProcess
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// _PEB
int g_Offset_PEB_BeingDebugged;                      // _PEB_BeingDebugged µÄÆ«ÒÆ
int g_Offset_PEB_ImageBaseAddress;                   // _PEB_ImageBaseAddress µÄÆ«ÒÆ
int g_Offset_PEB_Ldr;                                // _PEB_Ldr µÄÆ«ÒÆ
int g_Offset_PEB_ProcessParameters;                  // _PEB_ProcessParameters µÄÆ«ÒÆ

#define _PEB_BeingDebugged										g_Offset_PEB_BeingDebugged
#define _PEB_ImageBaseAddress									g_Offset_PEB_ImageBaseAddress
#define _PEB_Ldr												g_Offset_PEB_Ldr					//_PEB_LDR_DATA
#define _PEB_ProcessParameters									g_Offset_PEB_ProcessParameters
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// _PEB_LDR_DATA
int g_Offset_PEB_LDR_DATA_InLoadOrderModuleList;     // _PEB_LDR_DATA_InLoadOrderModuleList µÄÆ«ÒÆ

#define _PEB_LDR_DATA_InLoadOrderModuleList						0x10					//_LDR_DATA_TABLE_ENTRY
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// _LDR_DATA_TABLE_ENTRY
int g_Offset_LDR_DATA_TABLE_ENTRY_DllBase;           // _LDR_DATA_TABLE_ENTRY_DllBase µÄÆ«ÒÆ
int g_Offset_LDR_DATA_TABLE_ENTRY_SizeOfImage;       // _LDR_DATA_TABLE_ENTRY_SizeOfImage µÄÆ«ÒÆ
int g_Offset_LDR_DATA_TABLE_ENTRY_FullDllName;       // _LDR_DATA_TABLE_ENTRY_FullDllName µÄÆ«ÒÆ
int g_Offset_LDR_DATA_TABLE_ENTRY_BaseDllName;       // _LDR_DATA_TABLE_ENTRY_BaseDllName µÄÆ«ÒÆ

#define _LDR_DATA_TABLE_ENTRY_DllBase							g_Offset_LDR_DATA_TABLE_ENTRY_DllBase					
#define _LDR_DATA_TABLE_ENTRY_SizeOfImage						g_Offset_LDR_DATA_TABLE_ENTRY_SizeOfImage					
#define _LDR_DATA_TABLE_ENTRY_FullDllName 						g_Offset_LDR_DATA_TABLE_ENTRY_FullDllName					
#define _LDR_DATA_TABLE_ENTRY_BaseDllName 						g_Offset_LDR_DATA_TABLE_ENTRY_BaseDllName					
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_RTL_USER_PROCESS_PARAMETERS
int g_Offset_RTL_USER_PROCESS_PARAMETERS_CommandLine; // _RTL_USER_PROCESS_PARAMETERS_CommandLine µÄÆ«ÒÆ

#define _RTL_USER_PROCESS_PARAMETERS_CommandLine				g_Offset_RTL_USER_PROCESS_PARAMETERS_CommandLine

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// _MM_SESSION_SPACE
int g_Offset_MM_SESSION_SPACE_SessionId;             // _MM_SESSION_SPACE_SessionId µÄÆ«ÒÆ

#define _MM_SESSION_SPACE_SessionId								g_Offset_MM_SESSION_SPACE_SessionId
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_FILE_OBJECT
int g_Offset_FILE_OBJECT_DeviceObject;               // _FILE_OBJECT_DeviceObject µÄÆ«ÒÆ
int g_Offset_FILE_OBJECT_FileName;                   // _FILE_OBJECT_FileName µÄÆ«ÒÆ

#define _FILE_OBJECT_DeviceObject								g_Offset_FILE_OBJECT_DeviceObject
#define _FILE_OBJECT_FileName									g_Offset_FILE_OBJECT_FileName
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// _DRIVER_OBJECT
int g_Offset_DRIVER_OBJECT_DriverStart;              // _DRIVER_OBJECT_DriverStart µÄÆ«ÒÆ
int g_Offset_DRIVER_OBJECT_DriverSize;               // _DRIVER_OBJECT_DriverSize µÄÆ«ÒÆ
int g_Offset_DRIVER_OBJECT_DriverSection;            // _DRIVER_OBJECT_DriverSection µÄÆ«ÒÆ
int g_Offset_DRIVER_OBJECT_DriverExtension;          // _DRIVER_OBJECT_DriverExtension µÄÆ«ÒÆ
int g_Offset_DRIVER_OBJECT_DriverName;               // _DRIVER_OBJECT_DriverName µÄÆ«ÒÆ
int g_Offset_DRIVER_OBJECT_FastIoDispatch;           // _DRIVER_OBJECT_FastIoDispatch µÄÆ«ÒÆ
int g_Offset_DRIVER_OBJECT_MajorFunction;            // _DRIVER_OBJECT_MajorFunction µÄÆ«ÒÆ

#define _DRIVER_OBJECT_DriverStart								g_Offset_DRIVER_OBJECT_DriverStart
#define _DRIVER_OBJECT_DriverSize								g_Offset_DRIVER_OBJECT_DriverSize
#define _DRIVER_OBJECT_DriverSection							g_Offset_DRIVER_OBJECT_DriverSection
#define _DRIVER_OBJECT_DriverExtension							g_Offset_DRIVER_OBJECT_DriverExtension
#define _DRIVER_OBJECT_DriverName								g_Offset_DRIVER_OBJECT_DriverName
#define _DRIVER_OBJECT_FastIoDispatch 							g_Offset_DRIVER_OBJECT_FastIoDispatch
#define _DRIVER_OBJECT_MajorFunction							g_Offset_DRIVER_OBJECT_MajorFunction
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_DRIVER_EXTENSION
int g_Offset_DRIVER_EXTENSION_DriverObject;          // _DRIVER_EXTENSION_DriverObject µÄÆ«ÒÆ
int g_Offset_DRIVER_EXTENSION_ServiceKeyName;       // _DRIVER_EXTENSION_ServiceKeyName µÄÆ«ÒÆ

#define _DRIVER_EXTENSION_DriverObject							g_Offset_DRIVER_EXTENSION_DriverObject
#define _DRIVER_EXTENSION_ServiceKeyName						g_Offset_DRIVER_EXTENSION_ServiceKeyName
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// _OBJECT_HEADER
int g_Offset_OBJECT_HEADER_PointerCount;             // _OBJECT_HEADER_PointerCount µÄÆ«ÒÆ
int g_Offset_OBJECT_HEADER_TypeIndex;               // _OBJECT_HEADER_TypeIndex µÄÆ«ÒÆ
int g_Offset_OBJECT_HEADER_InfoMask;                // _OBJECT_HEADER_InfoMask µÄÆ«ÒÆ
int g_OBJECT_HEADER_SIZE;

#define _OBJECT_HEADER_SIZE										g_OBJECT_HEADER_SIZE
#define _OBJECT_HEADER_PointerCount								g_Offset_OBJECT_HEADER_PointerCount
#define _OBJECT_HEADER_TypeIndex								g_Offset_OBJECT_HEADER_TypeIndex
#define _OBJECT_HEADER_InfoMask									g_Offset_OBJECT_HEADER_InfoMask
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_DEVICE_OBJECT
int g_Offset_DEVICE_OBJECT_Queue;                   // _DEVICE_OBJECT_Queue µÄÆ«ÒÆ

#define _DEVICE_OBJECT_Queue									g_Offset_DEVICE_OBJECT_Queue				/*_LIST_ENTRY*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// _HANDLE_TABLE
int g_Offset_HANDLE_TABLE_NextHandleNeedingPool;    // _HANDLE_TABLE_NextHandleNeedingPool µÄÆ«ÒÆ
int g_Offset_HANDLE_TABLE_TableCode;               // _HANDLE_TABLE_TableCode µÄÆ«ÒÆ

#define _HANDLE_TABLE_NextHandleNeedingPool						g_Offset_HANDLE_TABLE_NextHandleNeedingPool
#define _HANDLE_TABLE_TableCode									g_Offset_HANDLE_TABLE_TableCode
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_OBJECT_TYPE
int g_Offset_OBJECT_TYPE_Name;                     // _OBJECT_TYPE_Name µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_Index;                    // _OBJECT_TYPE_Index µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_TypeInfo;                 // _OBJECT_TYPE_TypeInfo µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_CallbackList;             // _OBJECT_TYPE_CallbackList µÄÆ«ÒÆ

#define _OBJECT_TYPE_Name										g_Offset_OBJECT_TYPE_Name				/*_UNICODE_STRING*/
#define _OBJECT_TYPE_Index										g_Offset_OBJECT_TYPE_Index				/*UChar*/
#define _OBJECT_TYPE_TypeInfo									g_Offset_OBJECT_TYPE_TypeInfo				/*_OBJECT_TYPE_INITIALIZER*/
#define _OBJECT_TYPE_CallbackList								g_Offset_OBJECT_TYPE_CallbackList				/*LIST_ENTRY*/
#define _ObjectTypeFlags_SupportsObjectCallbacks_byte			0x40				
#define _OBJECT_TYPE_OBJECT_TYPE_INITIALIZER_ObjectTypeFlags	0x42				/*USHORT*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_OBJECT_TYPE_INITIALIZER
int g_Offset_OBJECT_TYPE_INITIALIZER_ValidAccessMask; // _OBJECT_TYPE_INITIALIZER_ValidAccessMask µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_INITIALIZER_DumpProcedure;  // _OBJECT_TYPE_INITIALIZER_DumpProcedure µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_INITIALIZER_OpenProcedure;  // _OBJECT_TYPE_INITIALIZER_OpenProcedure µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_INITIALIZER_CloseProcedure; // _OBJECT_TYPE_INITIALIZER_CloseProcedure µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_INITIALIZER_DeleteProcedure; // _OBJECT_TYPE_INITIALIZER_DeleteProcedure µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_INITIALIZER_ParseProcedure; // _OBJECT_TYPE_INITIALIZER_ParseProcedure µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_INITIALIZER_SecurityProcedure; // _OBJECT_TYPE_INITIALIZER_SecurityProcedure µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_INITIALIZER_QueryNameProcedure; // _OBJECT_TYPE_INITIALIZER_QueryNameProcedure µÄÆ«ÒÆ
int g_Offset_OBJECT_TYPE_INITIALIZER_OkayToCloseProcedure; // _OBJECT_TYPE_INITIALIZER_OkayToCloseProcedure µÄÆ«ÒÆ

#define _OBJECT_TYPE_INITIALIZER_ValidAccessMask				g_Offset_OBJECT_TYPE_INITIALIZER_ValidAccessMask
#define _OBJECT_TYPE_INITIALIZER_DumpProcedure					g_Offset_OBJECT_TYPE_INITIALIZER_DumpProcedure
#define _OBJECT_TYPE_INITIALIZER_OpenProcedure					g_Offset_OBJECT_TYPE_INITIALIZER_OpenProcedure
#define _OBJECT_TYPE_INITIALIZER_CloseProcedure					g_Offset_OBJECT_TYPE_INITIALIZER_CloseProcedure
#define _OBJECT_TYPE_INITIALIZER_DeleteProcedure				g_Offset_OBJECT_TYPE_INITIALIZER_DeleteProcedure
#define _OBJECT_TYPE_INITIALIZER_ParseProcedure					g_Offset_OBJECT_TYPE_INITIALIZER_ParseProcedure
#define _OBJECT_TYPE_INITIALIZER_SecurityProcedure				g_Offset_OBJECT_TYPE_INITIALIZER_SecurityProcedure
#define _OBJECT_TYPE_INITIALIZER_QueryNameProcedure				g_Offset_OBJECT_TYPE_INITIALIZER_QueryNameProcedure
#define _OBJECT_TYPE_INITIALIZER_OkayToCloseProcedure			g_Offset_OBJECT_TYPE_INITIALIZER_OkayToCloseProcedure
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_TOKEN
int g_Offset_TOKEN_LogonSession;                   // _TOKEN_LogonSession µÄÆ«ÒÆ

#define _TOKEN_LogonSession										g_Offset_TOKEN_LogonSession
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_SEP_LOGON_SESSION_REFERENCES
int g_Offset_SEP_LOGON_SESSION_REFERENCES_AccountName; // _SEP_LOGON_SESSION_REFERENCES_AccountName µÄÆ«ÒÆ

#define _SEP_LOGON_SESSION_REFERENCES_AccountName				g_Offset_SEP_LOGON_SESSION_REFERENCES_AccountName
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_MMVAD
int g_Offset_MMVAD_Core;                          // _MMVAD_Core µÄÆ«ÒÆ
int g_Offset_MMVAD_Subsection;                    // _MMVAD_Subsection µÄÆ«ÒÆ

#define _MMVAD_Core												g_Offset_MMVAD_Core				/*_MMVAD_SHORT*/
#define _MMVAD_Subsection										g_Offset_MMVAD_Subsection				/*_SUBSECTION*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_MMVAD_SHORT
int g_Offset_MMVAD_SHORT_StartingVpn;             // _MMVAD_SHORT_StartingVpn µÄÆ«ÒÆ
int g_Offset_MMVAD_SHORT_EndingVpn;               // _MMVAD_SHORT_EndingVpn µÄÆ«ÒÆ
int g_Offset_MMVAD_SHORT_StartingVpnHigh;         // _MMVAD_SHORT_StartingVpnHigh µÄÆ«ÒÆ
int g_Offset_MMVAD_SHORT_EndingVpnHigh;           // _MMVAD_SHORT_EndingVpnHigh µÄÆ«ÒÆ
int g_Offset_MMVAD_SHORT_u;                       // _MMVAD_SHORT_u µÄÆ«ÒÆ
int g_Offset_MMVAD_SHORT_u1;                      // _MMVAD_SHORT_u1 µÄÆ«ÒÆ

#define _MMVAD_SHORT_StartingVpn								g_Offset_MMVAD_SHORT_StartingVpn				/*unsigned long*/
#define _MMVAD_SHORT_EndingVpn									g_Offset_MMVAD_SHORT_EndingVpn				/*unsigned long*/
#define _MMVAD_SHORT_StartingVpnHigh							g_Offset_MMVAD_SHORT_StartingVpnHigh				/*unsigned char*/
#define _MMVAD_SHORT_EndingVpnHigh								g_Offset_MMVAD_SHORT_EndingVpnHigh				/*unsigned char*/
#define _MMVAD_SHORT_u											g_Offset_MMVAD_SHORT_u				/*struct*/
#define _MMVAD_SHORT_u1											g_Offset_MMVAD_SHORT_u1				/*_MMVAD_SHORT*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_SUBSECTION
int g_Offset_SUBSECTION_ControlArea;              // _SUBSECTION_ControlArea µÄÆ«ÒÆ

#define _SUBSECTION_ControlArea									g_Offset_SUBSECTION_ControlArea				/*_CONTROL_AREA*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_CONTROL_AREA
int g_Offset_CONTROL_AREA_FilePointer;            // _CONTROL_AREA_FilePointer µÄÆ«ÒÆ

#define _CONTROL_AREA_FilePointer								g_Offset_CONTROL_AREA_FilePointer				/*_EX_FAST_REF*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//_OBJECT_DIRECTORY
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_OBJECT_SYMBOLIC_LINK
int g_Offset_OBJECT_SYMBOLIC_LINK_LinkTarget;     // _OBJECT_SYMBOLIC_LINK_LinkTarget µÄÆ«ÒÆ

#define _OBJECT_SYMBOLIC_LINK_LinkTarget						g_Offset_OBJECT_SYMBOLIC_LINK_LinkTarget					/*_UNICODE_STRING*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_ETW_SILODRIVERSTATE
int g_Offset_ETW_SILODRIVERSTATE_EtwpLoggerContext; // _ETW_SILODRIVERSTATE_EtwpLoggerContext µÄÆ«ÒÆ

#define _ETW_SILODRIVERSTATE_EtwpLoggerContext					g_Offset_ETW_SILODRIVERSTATE_EtwpLoggerContext				/*_WMI_LOGGER_CONTEXT*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_WMI_LOGGER_CONTEXT
int g_Offset_WMI_LOGGER_CONTEXT_GetCpuClock;      // _WMI_LOGGER_CONTEXT_GetCpuClock µÄÆ«ÒÆ

#define _WMI_LOGGER_CONTEXT_GetCpuClock							g_Offset_WMI_LOGGER_CONTEXT_GetCpuClock				/*ULONG64*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_FLT_FILTER
int g_Offset_FLT_FILTER_Name;                     // _FLT_FILTER_Name µÄÆ«ÒÆ
int g_Offset_FLT_FILTER_DefaultAltitude;          // _FLT_FILTER_DefaultAltitude µÄÆ«ÒÆ
int g_Offset_FLT_FILTER_DriverObject;             // _FLT_FILTER_DriverObject µÄÆ«ÒÆ
int g_Offset_FLT_FILTER_Operations;               // _FLT_FILTER_Operations µÄÆ«ÒÆ

#define _FLT_FILTER_Name										g_Offset_FLT_FILTER_Name				//_UNICODE_STRING
#define _FLT_FILTER_DefaultAltitude								g_Offset_FLT_FILTER_DefaultAltitude				//_UNICODE_STRING
#define _FLT_FILTER_DriverObject								g_Offset_FLT_FILTER_DriverObject				//_DRIVER_OBJECT
#define _FLT_FILTER_Operations									g_Offset_FLT_FILTER_Operations				//_FLT_OPERATION_REGISTRATION
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_FLT_OBJECT
int g_Offset_FLT_OBJECT_PointerCount;             // _FLT_OBJECT_PointerCount µÄÆ«ÒÆ
int g_Offset_FLT_OBJECT_PrimaryLink;              // _FLT_OBJECT_PrimaryLink µÄÆ«ÒÆ
int g_Offset_FLT_OBJECT_UniqueIdentifier;         // _FLT_OBJECT_UniqueIdentifier µÄÆ«ÒÆ

#define _FLT_OBJECT_PointerCount								g_Offset_FLT_OBJECT_PointerCount				//PULONG32
#define _FLT_OBJECT_PrimaryLink									g_Offset_FLT_OBJECT_PrimaryLink				//_LIST_ENTRY
#define _FLT_OBJECT_UniqueIdentifier							g_Offset_FLT_OBJECT_UniqueIdentifier				//_GUID
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_EPARTITION
int g_Offset_EPARTITION_ExPartition;              // _EPARTITION_ExPartition µÄÆ«ÒÆ

#define _EPARTITION_ExPartition									g_Offset_EPARTITION_ExPartition				//_EX_PARTITION
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_EX_PARTITION
int g_Offset_EX_PARTITION_WorkQueues;							// _EX_PARTITION_WorkQueues µÄÆ«ÒÆ

#define _EX_PARTITION_WorkQueues								g_Offset_EX_PARTITION_WorkQueues				//_EX_WORK_QUEUE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_EX_WORK_QUEUE
int g_Offset_EX_WORK_QUEUE_WorkPriQueue;						// _EX_WORK_QUEUE_WorkPriQueue µÄÆ«ÒÆ

#define _EX_WORK_QUEUE_WorkPriQueue								g_Offset_EX_WORK_QUEUE_WorkPriQueue				//_KPRIQUEUE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_KPRIQUEUE
int g_Offset_KPRIQUEUE_Header;									// _KPRIQUEUE_Header µÄÆ«ÒÆ

#define _KPRIQUEUE_Header										g_Offset_KPRIQUEUE_Header				//_DISPATCHER_HEADER
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_ENODE
int g_Offset_ENODE_Ncb;                          			 	// _ENODE_Ncb µÄÆ«ÒÆ
int g_Offset_ENODE_HotAddProcessorWorkItem;						// _ENODE_HotAddProcessorWorkItem µÄÆ«ÒÆ

#define _ENODE_Ncb												g_Offset_ENODE_Ncb				//_KNODE
#define _ENODE_HotAddProcessorWorkItem							g_Offset_ENODE_HotAddProcessorWorkItem				//_WORK_QUEUE_ITEM
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
