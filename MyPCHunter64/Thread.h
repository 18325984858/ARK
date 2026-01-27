#pragma once

#include <Winsock2.h>
#include "CLoadDriver.h"
#include "CThreadPool.h"
//
//
// 声明:为什么选择多搞一个线程来操作呢
// 目的一:实现对操作的同步,一次只能有一个操作进入内核
// 目的二:一次操作只能对一个控件生效,避免多个操作来操作一个控件
// 
//

//线程结构体参数
typedef struct _ThreadInfo
{
	_LoadDriver::UserCallBackType CallNumber;	//调用号
	PVOID Paragma;								//参数
}CThreadInfo, PCThreadInfo;

DWORD WINAPI UniversalThreadFunction(PVOID lpThreadParameter/*PCThreadInfo*/);




class _CThreadPack :public CTask
{
public:
	virtual LPVOID DoTask();
public:
	_CThreadPack();
	_CThreadPack(_LoadDriver::UserCallBackType dwCallNumber, PVOID pParagma);
	virtual ~_CThreadPack();
private:
	_LoadDriver::UserCallBackType CallNumber;	//调用号
	PVOID Paragma;								//参数
};