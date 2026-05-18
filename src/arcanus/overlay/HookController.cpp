#include "arcanus/overlay/HookController.h"

#include "arcanus/ArcanusApp.h"

#ifdef ARCANUS_WITH_DX11_HOOK
#include <MinHook.h>
#endif

#ifdef ARCANUS_WITH_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#include <filesystem>

namespace arcanus {
namespace {

HookController* g_controller = nullptr;

#ifdef ARCANUS_WITH_DX11_HOOK
using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

PresentFn g_original_present = nullptr;
ResizeBuffersFn g_original_resize_buffers = nullptr;

HRESULT __stdcall PresentHook(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    if (g_controller != nullptr) {
        g_controller->RenderPresent(swap_chain);
    }
    return g_original_present(swap_chain, sync_interval, flags);
}

HRESULT __stdcall ResizeBuffersHook(IDXGISwapChain* swap_chain, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT new_format, UINT flags) {
    if (g_controller != nullptr) {
        g_controller->CleanupRenderTarget();
    }

    const auto result = g_original_resize_buffers(swap_chain, buffer_count, width, height, new_format, flags);

    if (SUCCEEDED(result) && g_controller != nullptr) {
        g_controller->CreateRenderTarget(swap_chain);
    }

    return result;
}
#endif

LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (g_controller != nullptr) {
        return g_controller->HandleWndProc(hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

bool IsMouseMessage(UINT msg) {
    return msg == WM_MOUSEMOVE ||
           msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK ||
           msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP || msg == WM_RBUTTONDBLCLK ||
           msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP || msg == WM_MBUTTONDBLCLK ||
           msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL ||
           msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP || msg == WM_XBUTTONDBLCLK;
}

bool IsKeyboardMessage(UINT msg) {
    return msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_CHAR;
}

std::filesystem::path FindReadableUiFont() {
    const wchar_t* windir = _wgetenv(L"WINDIR");
    const std::filesystem::path fonts_dir = windir != nullptr
        ? std::filesystem::path(windir) / L"Fonts"
        : std::filesystem::path(L"C:\\Windows\\Fonts");

    const std::filesystem::path candidates[] = {
        fonts_dir / L"segoeui.ttf",
        fonts_dir / L"arial.ttf",
        fonts_dir / L"tahoma.ttf"
    };

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) {
            return candidate;
        }
    }

    return {};
}

}

HookController::HookController(Diagnostics& diagnostics)
    : diagnostics_(diagnostics) {}

bool HookController::Initialize(ArcanusApp& app) {
    app_ = &app;
    g_controller = this;
    EnableDpiAwareness();

#if defined(ARCANUS_WITH_DX11_HOOK) && defined(ARCANUS_WITH_IMGUI)
    return InstallDx11Hook();
#else
    initialized_ = false;
    diagnostics_.SetHookStatus("inactive: build without ImGui/DX11 hook");
    diagnostics_.Info("Running without graphics hook");
    return true;
#endif
}

void HookController::Shutdown() {
#ifdef ARCANUS_WITH_DX11_HOOK
    if (initialized_) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
#endif
    CleanupImGui();
    if (initialized_) {
        diagnostics_.Info("Graphics hook shutdown");
    }
    initialized_ = false;
    app_ = nullptr;
    g_controller = nullptr;
}

LRESULT HookController::HandleWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        if (wparam == VK_F1 && app_ != nullptr) {
            app_->ToggleOverlay();
            return 0;
        }
        if (wparam == VK_OEM_3 && app_ != nullptr) {
            app_->ToggleDevPanel();
            return 0;
        }
        if (wparam == VK_F3 && app_ != nullptr) {
            app_->RequestBattleSim();
            return 0;
        }
        if (wparam == VK_END && app_ != nullptr) {
            app_->SoftDisable();
            return 0;
        }
    }

#ifdef ARCANUS_WITH_IMGUI
    if (imgui_initialized_) {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
        const ImGuiIO& io = ImGui::GetIO();
        if (IsMouseMessage(msg) && io.WantCaptureMouse) {
            return 1;
        }
        if (IsKeyboardMessage(msg) && io.WantCaptureKeyboard) {
            return 1;
        }
    }
#endif

    if (original_wndproc_ != nullptr) {
        return CallWindowProcW(original_wndproc_, hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

bool HookController::InstallDx11Hook() {
#ifndef ARCANUS_WITH_DX11_HOOK
    return false;
#else
    HWND dummy_window = CreateDummyWindow();
    if (dummy_window == nullptr) {
        diagnostics_.SetHookStatus("dx11 error: dummy window failed");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 1;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = dummy_window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device* temp_device = nullptr;
    ID3D11DeviceContext* temp_context = nullptr;
    IDXGISwapChain* temp_swap_chain = nullptr;

    const D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL created_level{};
    const auto create_result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        feature_levels,
        2,
        D3D11_SDK_VERSION,
        &desc,
        &temp_swap_chain,
        &temp_device,
        &created_level,
        &temp_context);

    if (FAILED(create_result)) {
        DestroyWindow(dummy_window);
        diagnostics_.SetHookStatus("dx11 error: temporary device failed");
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(temp_swap_chain);
    void* present = vtable[8];
    void* resize_buffers = vtable[13];

    temp_swap_chain->Release();
    temp_context->Release();
    temp_device->Release();
    DestroyWindow(dummy_window);

    if (MH_Initialize() != MH_OK) {
        diagnostics_.SetHookStatus("dx11 error: MinHook init failed");
        return false;
    }

    if (MH_CreateHook(present, &PresentHook, reinterpret_cast<void**>(&g_original_present)) != MH_OK ||
        MH_CreateHook(resize_buffers, &ResizeBuffersHook, reinterpret_cast<void**>(&g_original_resize_buffers)) != MH_OK ||
        MH_EnableHook(present) != MH_OK ||
        MH_EnableHook(resize_buffers) != MH_OK) {
        MH_Uninitialize();
        diagnostics_.SetHookStatus("dx11 error: hook install failed");
        return false;
    }

    initialized_ = true;
    diagnostics_.SetHookStatus("DX11 hooks active");
    diagnostics_.Info("DX11 Present/ResizeBuffers hooks installed");
    return true;
#endif
}

void HookController::InitializeImGui(IDXGISwapChain* swap_chain) {
#ifdef ARCANUS_WITH_IMGUI
    if (imgui_initialized_ || swap_chain == nullptr) {
        return;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap_chain->GetDesc(&desc))) {
        diagnostics_.Warn("SwapChain GetDesc failed");
        return;
    }

    if (FAILED(swap_chain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device_)))) {
        diagnostics_.Warn("SwapChain GetDevice failed");
        return;
    }

    device_->GetImmediateContext(&context_);
    game_window_ = desc.OutputWindow;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    const auto font_path = FindReadableUiFont();
    if (!font_path.empty()) {
        ImFontConfig font_config{};
        font_config.OversampleH = 2;
        font_config.OversampleV = 2;
        const auto font_path_utf8 = font_path.u8string();
        io.Fonts->AddFontFromFileTTF(
            reinterpret_cast<const char*>(font_path_utf8.c_str()),
            17.0f,
            &font_config,
            io.Fonts->GetGlyphRangesCyrillic());
    } else {
        io.Fonts->AddFontDefault();
        diagnostics_.Warn("No Windows UI font found; Cyrillic glyphs may be missing");
    }

    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(game_window_);
    ImGui_ImplDX11_Init(device_, context_);

    original_wndproc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(game_window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&StaticWndProc)));

    CreateRenderTarget(swap_chain);
    imgui_initialized_ = true;
    diagnostics_.Info("ImGui initialized on game swap chain");
#endif
}

void HookController::CleanupRenderTarget() {
    if (render_target_ != nullptr) {
        render_target_->Release();
        render_target_ = nullptr;
    }
#ifdef ARCANUS_WITH_IMGUI
    if (imgui_initialized_) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }
#endif
}

void HookController::CreateRenderTarget(IDXGISwapChain* swap_chain) {
    if (swap_chain == nullptr || device_ == nullptr || render_target_ != nullptr) {
        return;
    }

    ID3D11Texture2D* back_buffer = nullptr;
    if (SUCCEEDED(swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer)))) {
        device_->CreateRenderTargetView(back_buffer, nullptr, &render_target_);
        back_buffer->Release();
    }
}

void HookController::RenderPresent(IDXGISwapChain* swap_chain) {
#ifdef ARCANUS_WITH_IMGUI
    InitializeImGui(swap_chain);
    if (!imgui_initialized_ || context_ == nullptr || render_target_ == nullptr || app_ == nullptr) {
        return;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    app_->RenderFrame();

    ImGui::Render();
    context_->OMSetRenderTargets(1, &render_target_, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif
}

void HookController::CleanupImGui() {
#ifdef ARCANUS_WITH_IMGUI
    if (game_window_ != nullptr && original_wndproc_ != nullptr) {
        SetWindowLongPtrW(game_window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_wndproc_));
        original_wndproc_ = nullptr;
    }

    CleanupRenderTarget();

    if (imgui_initialized_) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        imgui_initialized_ = false;
    }
#endif
    if (context_ != nullptr) {
        context_->Release();
        context_ = nullptr;
    }
    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
    game_window_ = nullptr;
}

HWND HookController::CreateDummyWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ArcanusDx11DummyWindow";
    RegisterClassExW(&wc);
    return CreateWindowExW(0, wc.lpszClassName, L"Arcanus", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
}

void HookController::EnableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 != nullptr) {
        using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto set_context = reinterpret_cast<SetDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (set_context != nullptr) {
            set_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    SetProcessDPIAware();
}

}
