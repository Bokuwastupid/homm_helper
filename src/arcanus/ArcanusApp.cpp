#include "arcanus/ArcanusApp.h"

#include "arcanus/memory/DataPointerProbe.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <thread>

namespace arcanus {

ArcanusApp::ArcanusApp()
    : paths_(ResolveRuntimePaths()),
      database_(diagnostics_),
      ollama_(diagnostics_),
      scanner_(diagnostics_),
      renderer_(diagnostics_, ollama_),
      hooks_(diagnostics_) {}

ArcanusApp::~ArcanusApp() {
    Stop();
}

void ArcanusApp::Run() {
    diagnostics_.Info("ARCANUS worker thread started");
    diagnostics_.Info("Module dir: " + paths_.module_dir.string());
    diagnostics_.Info("Core.zip: " + paths_.core_zip.string());
    std::error_code ec;
    std::filesystem::create_directories(paths_.cache_dir, ec);
    std::filesystem::create_directories(paths_.logs_dir, ec);
    database_.Start(paths_);
    ollama_.Start();
    hooks_.Initialize(*this);

    while (running_) {
        PollHotkeys();
        RefreshState();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    hooks_.Shutdown();
    DataPointerProbe::Shutdown(diagnostics_);
    ollama_.Stop();
    database_.Stop();
    diagnostics_.Info("ARCANUS worker thread stopped");
}

void ArcanusApp::Stop() {
    running_ = false;
}

void ArcanusApp::RenderFrame() {
    renderer_.Render(
        *state_.Load(),
        *advice_.Load(),
        *database_.Snapshot(),
        *ollama_.Snapshot(),
        overlay_visible_,
        dev_panel_visible_);
}

void ArcanusApp::ToggleOverlay() {
    overlay_visible_ = !overlay_visible_.load();
    diagnostics_.Info(overlay_visible_ ? "Overlay visible" : "Overlay hidden");
}

void ArcanusApp::ToggleDevPanel() {
    dev_panel_visible_ = !dev_panel_visible_.load();
    diagnostics_.Info(dev_panel_visible_ ? "Developer panel visible" : "Developer panel hidden");
}

void ArcanusApp::SoftDisable() {
    overlay_visible_ = false;
    dev_panel_visible_ = false;
    diagnostics_.Warn("End pressed: overlay hidden. Runtime unload is disabled for hook stability.");
}

void ArcanusApp::RequestBattleSim() {
    battle_sim_requested_ = true;
    diagnostics_.Info("Battle sim hotkey requested");
}

void ArcanusApp::PollHotkeys() {
#if !defined(ARCANUS_WITH_DX11_HOOK)
    if ((GetAsyncKeyState(VK_F1) & 1) != 0) {
        ToggleOverlay();
    }

    if ((GetAsyncKeyState(VK_OEM_3) & 1) != 0) {
        ToggleDevPanel();
    }

    if ((GetAsyncKeyState(VK_F3) & 1) != 0) {
        RequestBattleSim();
    }

    if ((GetAsyncKeyState(VK_END) & 1) != 0) {
        SoftDisable();
    }
#endif
}

void ArcanusApp::RefreshState() {
    auto snapshot = scanner_.Capture();
    if (battle_sim_requested_.exchange(false)) {
        snapshot.scanner_status += " | battle-sim-requested";
    }
    auto next_advice = advisor_.Evaluate(snapshot);

    state_.Publish(std::move(snapshot));
    advice_.Publish(std::move(next_advice));
}

}
