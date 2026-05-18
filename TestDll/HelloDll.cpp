// TestDll/HelloDll.cpp
// 最小注入测试 DLL：DllMain 在 DLL_PROCESS_ATTACH 时弹一个 MessageBox。
//
// 编译：
//   cl /LD /MT /W3 /O2 HelloDll.cpp /link /OUT:HelloDll.dll user32.lib
//
// 注意：APC 注入触发 LoadLibraryW 时，目标线程会在 alertable 等待时拿到 APC，
// 此时 LoadLibraryW 会调用 DllMain。MessageBoxW 是阻塞的 —— 调用 it 会让该线程
// 暂停在弹窗，直到用户关掉。这对测试注入是否成功很直观（看到弹窗即注入成功）。

#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(reserved);
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        wchar_t buf[128];
        wsprintfW(buf, L"HelloWorld\nPID = %lu\nTID = %lu",
            GetCurrentProcessId(), GetCurrentThreadId());
        MessageBoxW(NULL, buf, L"HelloDll", MB_OK | MB_ICONINFORMATION);
    }
    return TRUE;
}
