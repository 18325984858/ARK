#include "DefineArea.h"


NTSTATUS ZwQuerySystemInformation(SYSTEM_INFORMATION_CLASS SystemInformationClass,
	void* SystemInformation,
	ULONG SystemInformationLength,
	ULONG* ReturnLength);

typedef NTSTATUS(__fastcall* PSPTERMINATEPROCESS)(PEPROCESS pEprocess,
	ULONG64 pEthread,
	NTSTATUS ExitCode,
	UCHAR Flags);//PspTerminateProcess



typedef
VOID
(*PKNORMAL_ROUTINE) (
	IN PVOID NormalContext,
	IN PVOID SystemArgument1,
	IN PVOID SystemArgument2
	);

typedef
VOID
(*PKKERNEL_ROUTINE) (
	IN struct _KAPC* Apc,
	IN OUT PKNORMAL_ROUTINE* NormalRoutine,
	IN OUT PVOID* NormalContext,
	IN OUT PVOID* SystemArgument1,
	IN OUT PVOID* SystemArgument2
	);

typedef CCHAR KPROCESSOR_MODE;

typedef
VOID
(*PKRUNDOWN_ROUTINE) (
	IN struct _KAPC* Apc
	);

EXTERN_C VOID KeInitializeApc(
	PRKAPC Apc,
	PRKTHREAD Thread,
	ULONG64 Environment,
	PKKERNEL_ROUTINE KernelRoutine,
	PKRUNDOWN_ROUTINE RundownRoutine,
	PKNORMAL_ROUTINE NormalRoutine,
	KPROCESSOR_MODE ApcMode,
	PVOID NormalContext
);

EXTERN_C BOOLEAN KeInsertQueueApc(
	__inout PRKAPC Apc,
	__in_opt PVOID SystemArgument1,
	__in_opt PVOID SystemArgument2,
	__in KPRIORITY Increment
);


