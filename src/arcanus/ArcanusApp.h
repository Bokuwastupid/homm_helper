#pragma once

#include "arcanus/ai/TacticalAdvisor.h"
#include "arcanus/core/GameState.h"
#include "arcanus/core/RuntimePaths.h"
#include "arcanus/core/SnapshotStore.h"
#include "arcanus/db/GameDatabase.h"
#include "arcanus/diagnostics/Diagnostics.h"
#include "arcanus/llm/OllamaClient.h"
#include "arcanus/memory/ReadOnlyScanner.h"
#include "arcanus/overlay/HookController.h"
#include "arcanus/overlay/OverlayRenderer.h"

#include <atomic>

namespace arcanus {

class ArcanusApp {
public:
    ArcanusApp();
    ~ArcanusApp();

    void Run();
    void Stop();
    void RenderFrame();
    void ToggleOverlay();
    void ToggleDevPanel();
    void SoftDisable();
    void RequestBattleSim();

private:
    void PollHotkeys();
    void RefreshState();

    std::atomic_bool running_{true};
    std::atomic_bool overlay_visible_{true};
    std::atomic_bool dev_panel_visible_{false};
    std::atomic_bool battle_sim_requested_{false};

    RuntimePaths paths_;
    SnapshotStore<GameState> state_;
    SnapshotStore<Advice> advice_;

    Diagnostics diagnostics_;
    GameDatabase database_;
    OllamaClient ollama_;
    ReadOnlyScanner scanner_;
    TacticalAdvisor advisor_;
    OverlayRenderer renderer_;
    HookController hooks_;
};

}
