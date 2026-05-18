#include "arcanus/ArcanusApp.h"

#include <Windows.h>

namespace {
HMODULE g_module = nullptr;
HANDLE g_thread = nullptr;

DWORD WINAPI ArcanusThread(LPVOID) {
    arcanus::ArcanusApp app;
    app.Run();
    FreeLibraryAndExitThread(g_module, 0);
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        g_thread = CreateThread(nullptr, 0, ArcanusThread, nullptr, 0, nullptr);
        if (g_thread != nullptr) {
            CloseHandle(g_thread);
        }
    }
    return TRUE;
}

