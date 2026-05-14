
// MyPCHunter64.h: PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif

#include "resource.h"		// 主符号


// CMyPCHunter64App:
// 有关此类的实现，请参阅 MyPCHunter64.cpp
//

class CMyPCHunter64App : public CWinApp
{
public:
	CMyPCHunter64App();

// 重写
public:
	virtual BOOL InitInstance();

// 实现

	DECLARE_MESSAGE_MAP()
};

extern CMyPCHunter64App theApp;

// === 全局日志接口（实现在 MyPCHunter64.cpp）===
void AppLog_Init();
void AppLog_Write(const char* level, const char* fmt, ...);
#ifndef LOGI
#define LOGI(...) AppLog_Write("INFO ", __VA_ARGS__)
#define LOGW(...) AppLog_Write("WARN ", __VA_ARGS__)
#define LOGE(...) AppLog_Write("ERROR", __VA_ARGS__)
#endif
