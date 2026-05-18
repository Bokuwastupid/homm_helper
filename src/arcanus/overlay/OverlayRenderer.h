#pragma once

#include "arcanus/ai/TacticalAdvisor.h"
#include "arcanus/core/GameState.h"
#include "arcanus/db/GameDatabase.h"
#include "arcanus/diagnostics/Diagnostics.h"
#include "arcanus/llm/OllamaClient.h"

namespace arcanus {

class OverlayRenderer {
public:
    OverlayRenderer(Diagnostics& diagnostics, OllamaClient& ollama);

    void Render(
        const GameState& state,
        const Advice& advice,
        const DatabaseStatus& database,
        const OllamaStatus& ai,
        bool overlay_visible,
        bool dev_panel_visible);

private:
    void RenderVisualDebugBoxes(const GameState& state) const;
    void RenderMapGridDebug(const GameState& state);
    void RenderTextFallback(const GameState& state, const Advice& advice, bool dev_panel_visible);
    bool ShouldShowObjectKind(const MapObjectState& object, const std::string& kind) const;

    Diagnostics& diagnostics_;
    OllamaClient& ollama_;
    float ui_scale_ = 1.0f;
    bool show_object_debug_boxes_ = false;
    bool show_map_grid_boxes_ = true;
    bool show_near_route_arrows_ = false;
    bool show_route_labels_ = true;
    bool show_hero_marker_ = true;
    bool hide_esp_on_reward_dialog_ = true;
    bool hide_esp_in_game_ui_ = true;
    bool show_kind_res_ = true;
    bool show_kind_chest_ = true;
    bool show_kind_mine_ = true;
    bool show_kind_item_ = true;
    bool show_kind_hire_ = true;
    bool show_kind_city_ = true;
    bool show_kind_event_ = true;
    bool show_kind_todo_ = true;
    bool show_kind_market_ = true;
    bool show_kind_tavern_ = true;
    bool show_kind_portal_ = true;
    bool show_kind_prison_ = true;
    bool show_kind_outpost_ = true;
    bool show_kind_block_ = false;
    bool show_kind_garrison_ = false;
    bool show_encounter_fight_ = true;
    bool show_encounter_stat_ = true;
    bool show_encounter_service_ = true;
    bool show_encounter_other_ = true;
    bool use_game_projection_ = true;
    bool clip_esp_to_play_area_ = true;
    int route_arrow_distance_ = 12;
    int route_arrow_max_ = 3;
    int route_label_max_ = 12;
    int esp_label_mode_ = 0;
    int esp_max_boxes_ = 80;
    int projection_budget_per_frame_ = 900;
    float map_box_size_ = 18.0f;
    int last_visible_objects_ = 0;
    int last_drawn_boxes_ = 0;
    int last_drawn_arrows_ = 0;
    int last_drawn_labels_ = 0;
    int last_filtered_objects_ = 0;
    int last_offscreen_objects_ = 0;
    int last_projected_objects_ = 0;
    int last_projection_failed_ = 0;
    int last_no_projection_context_ = 0;
    int last_projection_calls_ = 0;
    int last_projection_budget_skips_ = 0;
    int last_projection_camera_mode_ = 0;
    bool last_game_anchor_ok_ = false;
};

}
