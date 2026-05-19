// MyPCHunter64/SvcUtil.h
// SCM 工具封装：加载/启动/停止/卸载/改启动类型。
#pragma once
#include <Windows.h>
#include <winsvc.h>
#include <string>

namespace SvcUtil
{
    // 加载并启动驱动。svcName 为服务名（一般取文件名 stem），sysPath 是 .sys 全路径。
    // 已经存在的服务会复用。返回 0=成功，否则返回 GetLastError。
    DWORD LoadAndStart(const std::wstring& svcName, const std::wstring& sysPath, std::wstring* err = nullptr);

    // 停止 + 删除服务（优雅）
    DWORD StopAndDelete(const std::wstring& svcName, std::wstring* err = nullptr);

    // 改启动类型：SERVICE_BOOT_START(0) / SYSTEM_START(1) / AUTO_START(2) / DEMAND_START(3) / DISABLED(4)
    DWORD ChangeStartType(const std::wstring& svcName, DWORD newType, std::wstring* err = nullptr);

    // 查询启动类型；失败返回 0xFFFFFFFF
    DWORD QueryStartType(const std::wstring& svcName);
}
