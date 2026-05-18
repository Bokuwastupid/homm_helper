#pragma once

#include "arcanus/diagnostics/Diagnostics.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

namespace arcanus {

class ArcanusApp;

class HookController {
public:
    explicit HookController(Diagnostics& diagnostics);

    bool Initialize(ArcanusApp& app);
    void Shutdown();
    LRESULT HandleWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    void CleanupRenderTarget();
    void CreateRenderTarget(IDXGISwapChain* swap_chain);
    void RenderPresent(IDXGISwapChain* swap_chain);

private:
    bool InstallDx11Hook();
    void InitializeImGui(IDXGISwapChain* swap_chain);
    void CleanupImGui();
    static HWND CreateDummyWindow();
    static void EnableDpiAwareness();

    Diagnostics& diagnostics_;
    ArcanusApp* app_ = nullptr;
    bool initialized_ = false;
    bool imgui_initialized_ = false;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11RenderTargetView* render_target_ = nullptr;
    HWND game_window_ = nullptr;
    WNDPROC original_wndproc_ = nullptr;
};

}
