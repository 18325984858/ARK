// MyPCHunter64/SvcUtil.cpp
#include "pch.h"
#include <winsvc.h>
#include "SvcUtil.h"
#include <vector>
#include <sstream>

namespace
{
    void SetErr(std::wstring* err, const wchar_t* op, DWORD gle)
    {
        if (!err) return;
        wchar_t buf[256];
        swprintf_s(buf, L"%s 失败 (GLE=%lu)", op, gle);
        *err = buf;
    }

    struct ScmHandle
    {
        SC_HANDLE h = nullptr;
        ~ScmHandle() { if (h) CloseServiceHandle(h); }
        operator SC_HANDLE() const { return h; }
    };
}

namespace SvcUtil
{
    DWORD LoadAndStart(const std::wstring& svcName, const std::wstring& sysPath, std::wstring* err)
    {
        ScmHandle scm; scm.h = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (!scm.h) { DWORD gle = GetLastError(); SetErr(err, L"OpenSCManager", gle); return gle; }

        ScmHandle svc;
        svc.h = CreateServiceW(scm, svcName.c_str(), svcName.c_str(),
            SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
            sysPath.c_str(), NULL, NULL, NULL, NULL, NULL);
        if (!svc.h)
        {
            DWORD gle = GetLastError();
            if (gle != ERROR_SERVICE_EXISTS) { SetErr(err, L"CreateService", gle); return gle; }
            svc.h = OpenServiceW(scm, svcName.c_str(), SERVICE_ALL_ACCESS);
            if (!svc.h) { gle = GetLastError(); SetErr(err, L"OpenService", gle); return gle; }
        }

        if (!StartServiceW(svc, 0, NULL))
        {
            DWORD gle = GetLastError();
            if (gle != ERROR_SERVICE_ALREADY_RUNNING) { SetErr(err, L"StartService", gle); return gle; }
        }
        return 0;
    }

    DWORD StopAndDelete(const std::wstring& svcName, std::wstring* err)
    {
        ScmHandle scm; scm.h = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (!scm.h) { DWORD gle = GetLastError(); SetErr(err, L"OpenSCManager", gle); return gle; }
        ScmHandle svc; svc.h = OpenServiceW(scm, svcName.c_str(), SERVICE_ALL_ACCESS);
        if (!svc.h) { DWORD gle = GetLastError(); SetErr(err, L"OpenService", gle); return gle; }

        SERVICE_STATUS ss = { 0 };
        if (!ControlService(svc, SERVICE_CONTROL_STOP, &ss))
        {
            DWORD gle = GetLastError();
            // 已经停了或不可停都继续走 DeleteService
            if (gle != ERROR_SERVICE_NOT_ACTIVE && gle != ERROR_INVALID_SERVICE_CONTROL)
            {
                SetErr(err, L"ControlService(STOP)", gle);
                // 继续尝试 DeleteService
            }
        }

        if (!DeleteService(svc))
        {
            DWORD gle = GetLastError();
            SetErr(err, L"DeleteService", gle);
            return gle;
        }
        return 0;
    }

    DWORD ChangeStartType(const std::wstring& svcName, DWORD newType, std::wstring* err)
    {
        ScmHandle scm; scm.h = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (!scm.h) { DWORD gle = GetLastError(); SetErr(err, L"OpenSCManager", gle); return gle; }
        ScmHandle svc; svc.h = OpenServiceW(scm, svcName.c_str(), SERVICE_CHANGE_CONFIG);
        if (!svc.h) { DWORD gle = GetLastError(); SetErr(err, L"OpenService", gle); return gle; }

        if (!ChangeServiceConfigW(svc, SERVICE_NO_CHANGE, newType, SERVICE_NO_CHANGE,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL))
        {
            DWORD gle = GetLastError();
            SetErr(err, L"ChangeServiceConfig", gle);
            return gle;
        }
        return 0;
    }

    DWORD QueryStartType(const std::wstring& svcName)
    {
        ScmHandle scm; scm.h = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        if (!scm.h) return 0xFFFFFFFF;
        ScmHandle svc; svc.h = OpenServiceW(scm, svcName.c_str(), SERVICE_QUERY_CONFIG);
        if (!svc.h) return 0xFFFFFFFF;
        DWORD need = 0;
        QueryServiceConfigW(svc, NULL, 0, &need);
        if (need == 0) return 0xFFFFFFFF;
        std::vector<BYTE> buf(need);
        auto cfg = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(buf.data());
        if (!QueryServiceConfigW(svc, cfg, need, &need)) return 0xFFFFFFFF;
        return cfg->dwStartType;
    }
}
