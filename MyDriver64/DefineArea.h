#pragma once
#include <fltKernel.h>
#include <dontuse.h>
#include <ntstrsafe.h>
#include<intrin.h>
#include <minwindef.h>
#include "Head.h"
#define MyDbgPrintfEx(...)	(DbgPrintEx(DPFLTR_IHVDRIVER_ID,0,__VA_ARGS__))
#define MmIsAddressValid(x)	(MmIsAddressValidEx0(x))
#define MmIsAddressValidEx(x,y)	(MmIsAddressValidEx1(x,y))