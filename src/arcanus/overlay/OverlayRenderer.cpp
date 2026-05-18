#include "arcanus/overlay/OverlayRenderer.h"

#include "arcanus/core/DisplayNames.h"
#include "arcanus/core/GameState.h"
#include "arcanus/memory/DataPointerProbe.h"
#include "arcanus/memory/KnownOffsets.h"
#include "arcanus/memory/SafeMemory.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#ifdef ARCANUS_WITH_IMGUI
#include <imgui.h>
#endif

namespace arcanus {
namespace {

#ifdef ARCANUS_WITH_IMGUI
void ApplyArcanusStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = ImVec2(10.0f, 8.0f);
    style.FramePadding = ImVec2(7.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.88f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.56f, 0.58f, 1.0f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.035f, 0.04f, 0.90f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.05f, 0.05f, 0.055f, 0.70f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.04f, 0.04f, 0.045f, 0.96f);
    colors[ImGuiCol_Border] = ImVec4(1.0f, 0.55f, 0.0f, 0.55f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.11f, 0.92f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.42f, 0.24f, 0.04f, 0.95f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.62f, 0.34f, 0.04f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.03f, 0.03f, 0.035f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.07f, 0.02f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.55f, 0.0f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.55f, 0.0f, 0.9f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.72f, 0.2f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.12f, 0.05f, 0.95f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.42f, 0.24f, 0.04f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.65f, 0.36f, 0.03f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.12f, 0.04f, 0.95f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.42f, 0.24f, 0.04f, 0.95f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.60f, 0.34f, 0.05f, 1.0f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.08f, 0.03f, 0.95f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.45f, 0.26f, 0.05f, 1.0f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.30f, 0.17f, 0.04f, 1.0f);
}
#endif

bool IsValidDebugBox(const VisualDebugBox& box) {
    return box.enabled &&
           box.right > box.left &&
           box.bottom > box.top &&
           box.right - box.left >= 4.0f &&
           box.bottom - box.top >= 4.0f;
}

const char* SideStateName(int state) {
    switch (state) {
    case 0: return "Wait";
    case 1: return "Normal";
    case 2: return "HeroMove";
    case 3: return "HeroFlyMove";
    case 4: return "MoveByPortal";
    case 5: return "SelectMagicCastNode";
    case 6: return "SelectMagicUI";
    case 7: return "MagicCast";
    case 8: return "Cheat";
    case 9: return "WaitDefenderLeaveCity";
    case 10: return "WaitFightTournament";
    case 11: return "Strange";
    default: return "Unknown";
    }
}

bool IsPureMapSideState(int state) {
    return state == 1 || state == 2 || state == 3 || state == 4;
}

std::string ObjectKind(const MapObjectState& object) {
    if (!object.kind.empty()) {
        return object.kind;
    }
    if (object.value.find("kind=res") != std::string::npos) return "res";
    if (object.value.find("kind=chest") != std::string::npos) return "chest";
    if (object.value.find("kind=mine") != std::string::npos) return "mine";
    if (object.value.find("kind=item") != std::string::npos) return "item";
    if (object.value.find("kind=hire") != std::string::npos) return "hire";
    if (object.value.find("kind=city") != std::string::npos) return "city";
    if (object.value.find("kind=event") != std::string::npos) return "event";
    if (object.value.find("kind=todo") != std::string::npos) return "todo";
    if (object.value.find("kind=market") != std::string::npos) return "market";
    if (object.value.find("kind=tavern") != std::string::npos) return "tavern";
    if (object.value.find("kind=portal") != std::string::npos) return "portal";
    if (object.value.find("kind=prison") != std::string::npos) return "prison";
    if (object.value.find("kind=outpost") != std::string::npos) return "outpost";
    if (object.value.find("kind=block") != std::string::npos) return "block";
    if (object.value.find("kind=garrison") != std::string::npos) return "garrison";
    return {};
}

bool IsEncounterKind(const std::string& kind) {
    return kind == "event" ||
           kind == "todo" ||
           kind == "trade_lab" ||
           kind == "item_market" ||
           kind == "random_hire" ||
           kind == "unit_upgrade" ||
           kind == "eternal_dragon" ||
           kind == "insara_eye" ||
           kind == "chimerologist" ||
           kind == "sacrificial_shrine" ||
           kind == "gladiator_arena" ||
           kind == "mirage" ||
           kind == "fickle_shrine" ||
           kind == "magic_mine" ||
           kind == "town_gate" ||
           kind == "unit_res_trade_lab" ||
           kind == "pocket_dimension";
}

std::string ChestGroup(const std::string& type) {
    if (type.find("pandora") != std::string::npos) return "pandora";
    if (type.find("scroll") != std::string::npos) return "scroll";
    return "regular";
}

std::string EncounterGroup(const std::string& kind, const std::string& type) {
    if (kind == "hire" || kind == "random_hire" || kind == "market" || kind == "tavern" ||
        kind == "trade_lab" || kind == "unit_res_trade_lab" || kind == "item_market" ||
        kind == "unit_upgrade" || kind == "town_gate" || kind == "portal") {
        return "service";
    }
    if (kind == "insara_eye" || kind == "sacrificial_shrine" || kind == "fickle_shrine" ||
        type.find("stat") != std::string::npos || type.find("shrine") != std::string::npos ||
        type.find("xp") != std::string::npos || type.find("exp") != std::string::npos) {
        return "stat";
    }
    if (kind == "event" || kind == "gladiator_arena" || kind == "eternal_dragon" ||
        kind == "mirage" || type.find("battle") != std::string::npos ||
        type.find("fight") != std::string::npos || type.find("arena") != std::string::npos ||
        type.find("monster") != std::string::npos || type.find("guard") != std::string::npos) {
        return "fight";
    }
    return "other";
}

ImU32 ObjectColor(const std::string& type, float alpha = 0.95f) {
    ImVec4 color(1.0f, 0.1f, 0.1f, alpha);
    if (type.find("res_mine") != std::string::npos || type.find("mine") != std::string::npos) {
        color = ImVec4(0.25f, 0.75f, 1.0f, alpha);
    } else if (type.find("pandora") != std::string::npos) {
        color = ImVec4(0.95f, 0.35f, 1.0f, alpha);
    } else if (type.find("scroll") != std::string::npos) {
        color = ImVec4(1.0f, 0.82f, 0.28f, alpha);
    } else if (type.find("chest") != std::string::npos) {
        color = ImVec4(1.0f, 0.72f, 0.18f, alpha);
    } else if (type.find("res") != std::string::npos || type.find("gold") != std::string::npos) {
        color = ImVec4(0.35f, 1.0f, 0.35f, alpha);
    } else if (type.find("item") != std::string::npos || type.find("artifact") != std::string::npos) {
        color = ImVec4(1.0f, 0.25f, 0.85f, alpha);
    } else if (type.find("hire") != std::string::npos) {
        color = ImVec4(0.7f, 0.45f, 1.0f, alpha);
    } else if (type.find("event") != std::string::npos || type.find("todo") != std::string::npos ||
               type.find("arena") != std::string::npos || type.find("shrine") != std::string::npos ||
               type.find("dragon") != std::string::npos || type.find("mirage") != std::string::npos) {
        color = ImVec4(1.0f, 0.18f, 0.18f, alpha);
    } else if (type.find("market") != std::string::npos || type.find("tavern") != std::string::npos ||
               type.find("portal") != std::string::npos || type.find("prison") != std::string::npos ||
               type.find("outpost") != std::string::npos || type.find("trade_lab") != std::string::npos ||
               type.find("hire") != std::string::npos || type.find("upgrade") != std::string::npos ||
               type.find("dimension") != std::string::npos || type.find("gate") != std::string::npos) {
        color = ImVec4(1.0f, 0.85f, 0.2f, alpha);
    } else if (type.find("city") != std::string::npos) {
        color = ImVec4(1.0f, 1.0f, 1.0f, alpha);
    }
    return ImGui::ColorConvertFloat4ToU32(color);
}

int GridDistance(const HeroState& hero, const MapObjectState& object) {
    return std::abs(object.x - hero.x) + std::abs(object.y - hero.y);
}

int ObjectPriority(const std::string& kind, const std::string& type) {
    if (kind == "chest") {
        if (type.find("pandora") != std::string::npos) return 125;
        return type.find("scroll") != std::string::npos ? 115 : 100;
    }
    if (kind == "res") {
        if (type.find("gold") != std::string::npos) return 95;
        if (type.find("crystal") != std::string::npos || type.find("gem") != std::string::npos) return 90;
        return 82;
    }
    if (kind == "mine") return 78;
    if (kind == "item") return 76;
    if (kind == "event") return 92;
    if (kind == "todo") return 88;
    if (IsEncounterKind(kind)) return 86;
    if (kind == "prison") return 84;
    if (kind == "outpost") return 72;
    if (kind == "market") return 64;
    if (kind == "tavern") return 62;
    if (kind == "portal") return 60;
    if (kind == "hire") return 58;
    if (kind == "city") return 54;
    if (kind == "garrison") return 35;
    if (kind == "block") return 20;
    return 40;
}

std::string ShortenLabel(std::string value, std::size_t max_len) {
    if (value.size() <= max_len) {
        return value;
    }
    value.resize(max_len > 3 ? max_len - 3 : max_len);
    if (max_len > 3) {
        value += "...";
    }
    return value;
}

std::string CompactObjectLabel(const std::string& kind, const std::string& type) {
    if (kind == "res" || kind == "mine" || kind == "chest" || kind == "item") {
        return display::ShortObjectName(kind, type, 16);
    }
    if (kind == "event") return "encounter";
    if (kind == "todo") return "event";
    if (kind == "trade_lab") return "trade";
    if (kind == "item_market") return "item market";
    if (kind == "random_hire") return "hire";
    if (kind == "unit_upgrade") return "upgrade";
    if (kind == "eternal_dragon") return "dragon";
    if (kind == "insara_eye") return "eye";
    if (kind == "chimerologist") return "chimerologist";
    if (kind == "sacrificial_shrine") return "shrine";
    if (kind == "gladiator_arena") return "arena";
    if (kind == "mirage") return "mirage";
    if (kind == "fickle_shrine") return "shrine";
    if (kind == "magic_mine") return "magic mine";
    if (kind == "town_gate") return "gate";
    if (kind == "unit_res_trade_lab") return "unit trade";
    if (kind == "pocket_dimension") return "dimension";
    if (kind == "market") return "market";
    if (kind == "tavern") return "tavern";
    if (kind == "portal") return "portal";
    if (kind == "prison") return "prison";
    if (kind == "outpost") return "outpost";
    if (kind == "hire") return "hire";
    if (kind == "city") return "city";
    if (kind == "block") return "block";
    if (kind == "garrison") return "garrison";
    return type.empty() ? "object" : display::ShortObjectName(kind, type, 18);
}

std::string CleanObjectLabel(const MapObjectState& object, const std::string& kind) {
    std::string label = CompactObjectLabel(kind, object.type);
    if (!object.reward_preview.empty() && (kind == "chest" || kind == "item" || IsEncounterKind(kind))) {
        label = ShortenLabel(label, 18) + "\n" + ShortenLabel(object.reward_preview, 34);
    }
    return ShortenLabel(label, object.reward_preview.empty() ? 20 : 56);
}

void DrawCornerBox(ImDrawList* draw_list, const ImVec2& center, float half, ImU32 color, ImU32 fill) {
    const ImVec2 min(center.x - half, center.y - half);
    const ImVec2 max(center.x + half, center.y + half);
    const float corner = std::max(5.0f, half * 0.65f);
    if (fill != 0) {
        draw_list->AddRectFilled(min, max, fill);
    }
    draw_list->AddLine(min, ImVec2(min.x + corner, min.y), color, 2.0f);
    draw_list->AddLine(min, ImVec2(min.x, min.y + corner), color, 2.0f);
    draw_list->AddLine(ImVec2(max.x, min.y), ImVec2(max.x - corner, min.y), color, 2.0f);
    draw_list->AddLine(ImVec2(max.x, min.y), ImVec2(max.x, min.y + corner), color, 2.0f);
    draw_list->AddLine(ImVec2(min.x, max.y), ImVec2(min.x + corner, max.y), color, 2.0f);
    draw_list->AddLine(ImVec2(min.x, max.y), ImVec2(min.x, max.y - corner), color, 2.0f);
    draw_list->AddLine(max, ImVec2(max.x - corner, max.y), color, 2.0f);
    draw_list->AddLine(max, ImVec2(max.x, max.y - corner), color, 2.0f);
}

void DrawArrowLine(ImDrawList* draw_list, const ImVec2& from, const ImVec2& to, ImU32 color) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 1.0f) {
        return;
    }

    const float ux = dx / len;
    const float uy = dy / len;
    const ImVec2 start(from.x + ux * 20.0f, from.y + uy * 20.0f);
    const ImVec2 end(to.x - ux * 12.0f, to.y - uy * 12.0f);
    draw_list->AddLine(start, end, color, 1.7f);
    const ImVec2 tip(to.x, to.y);
    const ImVec2 left(to.x - ux * 12.0f - uy * 5.0f, to.y - uy * 12.0f + ux * 5.0f);
    const ImVec2 right(to.x - ux * 12.0f + uy * 5.0f, to.y - uy * 12.0f - ux * 5.0f);
    draw_list->AddTriangleFilled(tip, left, right, color);
}

struct ScreenRect {
    ImVec2 min{};
    ImVec2 max{};
};

bool RectsOverlap(const ScreenRect& a, const ScreenRect& b) {
    return a.min.x < b.max.x && a.max.x > b.min.x &&
           a.min.y < b.max.y && a.max.y > b.min.y;
}

bool RectFitsDisplay(const ScreenRect& rect, const ImVec2& display) {
    constexpr float margin = 3.0f;
    return rect.min.x >= margin &&
           rect.min.y >= margin &&
           rect.max.x <= display.x - margin &&
           rect.max.y <= display.y - margin;
}

void DrawLabelAt(ImDrawList* draw_list, const ScreenRect& rect, ImU32 color, const std::string& label) {
    draw_list->AddRectFilled(rect.min, rect.max, IM_COL32(0, 0, 0, 205), 3.0f);
    draw_list->AddRect(rect.min, rect.max, IM_COL32(0, 0, 0, 235), 3.0f, 0, 1.0f);
    draw_list->AddText(ImVec2(rect.min.x + 4.0f, rect.min.y + 2.0f), color, label.c_str());
}

bool DrawObjectLabel(
    ImDrawList* draw_list,
    const ImVec2& center,
    float half,
    ImU32 color,
    const std::string& label,
    const ImVec2& display,
    std::vector<ScreenRect>& occupied_labels) {
    const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
    const ImVec2 box_size(text_size.x + 8.0f, text_size.y + 5.0f);
    const ImVec2 candidates[] = {
        ImVec2(center.x + half + 6.0f, center.y - box_size.y * 0.5f),
        ImVec2(center.x - box_size.x * 0.5f, center.y - half - box_size.y - 6.0f),
        ImVec2(center.x - box_size.x * 0.5f, center.y + half + 6.0f),
        ImVec2(center.x - half - box_size.x - 6.0f, center.y - box_size.y * 0.5f),
        ImVec2(center.x + half + 6.0f, center.y + half + 4.0f),
        ImVec2(center.x - half - box_size.x - 6.0f, center.y + half + 4.0f),
    };

    for (const auto& pos : candidates) {
        const ScreenRect rect{pos, ImVec2(pos.x + box_size.x, pos.y + box_size.y)};
        if (!RectFitsDisplay(rect, display)) {
            continue;
        }

        ScreenRect padded = rect;
        padded.min.x -= 3.0f;
        padded.min.y -= 3.0f;
        padded.max.x += 3.0f;
        padded.max.y += 3.0f;
        bool overlaps = false;
        for (const auto& occupied : occupied_labels) {
            if (RectsOverlap(padded, occupied)) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) {
            continue;
        }

        DrawLabelAt(draw_list, rect, color, label);
        occupied_labels.push_back(padded);
        return true;
    }

    return false;
}

std::string DebugObjectLabel(const MapObjectState& object, const std::string& kind) {
    auto label = display::ShortObjectName(kind, object.type, 18) + " | " + kind + ": " + object.type +
        " [" + std::to_string(object.x) + "," + std::to_string(object.y) + "] node " +
        std::to_string(object.node);
    if (!object.reward_preview.empty()) {
        label += " | reward: " + object.reward_preview;
    }
    return ShortenLabel(label, 110);
}

struct UnityVector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

using MapNodeToWorldFn = void(__fastcall*)(UnityVector3* ret, void* map, int node, const void* method);
using MapCellToWorldFn = void(__fastcall*)(UnityVector3* ret, int x, int y, const void* method);
using CameraWorldToScreenInjectedFn = void(__fastcall*)(void* unity_self, const UnityVector3* position, int eye, UnityVector3* ret, const void* method);

bool IsFiniteScreenPoint(const UnityVector3& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) && point.z > 0.01f;
}

std::uintptr_t UnityObjectNativePointer(std::uintptr_t managed_object) {
    if (!SafeMemory::IsProbablyUserPointer(managed_object)) {
        return 0;
    }
    return SafeMemory::ReadPtr(managed_object + 0x10).value_or(0);
}

bool HasGameProjectionContext() {
    const auto map_ptr = DataPointerProbe::WorldMapPointer();
    const auto managed_camera = DataPointerProbe::UnityCameraPointer();
    const auto native_camera = UnityObjectNativePointer(managed_camera);
    return SafeMemory::IsProbablyUserPointer(map_ptr) &&
        (SafeMemory::IsProbablyUserPointer(native_camera) || SafeMemory::IsProbablyUserPointer(managed_camera));
}

bool ProjectWorldToScreen(
    CameraWorldToScreenInjectedFn camera_world_to_screen,
    std::uintptr_t camera_ptr,
    const UnityVector3& world,
    const ImVec2& display,
    ImVec2& out) {
    if (!SafeMemory::IsProbablyUserPointer(camera_ptr) ||
        !SafeMemory::IsProbablyUserPointer(reinterpret_cast<std::uintptr_t>(camera_world_to_screen))) {
        return false;
    }

    bool ok = false;
    UnityVector3 screen{};
#if defined(_MSC_VER)
    __try {
        camera_world_to_screen(reinterpret_cast<void*>(camera_ptr), &world, 2, &screen, nullptr);
        ok = IsFiniteScreenPoint(screen);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
#else
    camera_world_to_screen(reinterpret_cast<void*>(camera_ptr), &world, 2, &screen, nullptr);
    ok = IsFiniteScreenPoint(screen);
#endif
    if (!ok) {
        return false;
    }

    out = ImVec2(screen.x, display.y - screen.y);
    return std::isfinite(out.x) && std::isfinite(out.y);
}

bool TryProjectGameCell(std::uintptr_t game_assembly_base, int x, int y, const ImVec2& display, ImVec2& out, int* camera_mode = nullptr) {
    if (game_assembly_base == 0 || x < 0 || y < 0) {
        return false;
    }

    const auto managed_camera = DataPointerProbe::UnityCameraPointer();
    const auto native_camera = UnityObjectNativePointer(managed_camera);
    if (!SafeMemory::IsProbablyUserPointer(native_camera) && !SafeMemory::IsProbablyUserPointer(managed_camera)) {
        return false;
    }

    auto map_cell_to_world = reinterpret_cast<MapCellToWorldFn>(game_assembly_base + offsets::rva::MapCellToWorld);
    auto camera_world_to_screen = reinterpret_cast<CameraWorldToScreenInjectedFn>(game_assembly_base + offsets::rva::CameraWorldToScreenPointInjected);
    if (!SafeMemory::IsProbablyUserPointer(reinterpret_cast<std::uintptr_t>(map_cell_to_world)) ||
        !SafeMemory::IsProbablyUserPointer(reinterpret_cast<std::uintptr_t>(camera_world_to_screen))) {
        return false;
    }

    bool ok = false;
    UnityVector3 world{};
#if defined(_MSC_VER)
    __try {
        map_cell_to_world(&world, x, y, nullptr);
        ok = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
#else
    map_cell_to_world(&world, x, y, nullptr);
    ok = true;
#endif
    if (!ok) {
        return false;
    }

    if (ProjectWorldToScreen(camera_world_to_screen, native_camera, world, display, out)) {
        if (camera_mode != nullptr) *camera_mode = 1;
        return true;
    }
    if (ProjectWorldToScreen(camera_world_to_screen, managed_camera, world, display, out)) {
        if (camera_mode != nullptr) *camera_mode = 2;
        return true;
    }
    return false;
}

bool TryProjectGameNode(std::uintptr_t game_assembly_base, int node, const ImVec2& display, ImVec2& out, int* camera_mode = nullptr) {
    if (game_assembly_base == 0 || node < 0) {
        return false;
    }

    const auto map_ptr = DataPointerProbe::WorldMapPointer();
    const auto managed_camera = DataPointerProbe::UnityCameraPointer();
    const auto native_camera = UnityObjectNativePointer(managed_camera);
    if (!SafeMemory::IsProbablyUserPointer(map_ptr) ||
        (!SafeMemory::IsProbablyUserPointer(native_camera) && !SafeMemory::IsProbablyUserPointer(managed_camera))) {
        return false;
    }

    auto map_node_to_world = reinterpret_cast<MapNodeToWorldFn>(game_assembly_base + offsets::rva::MapNodeToWorld);
    auto camera_world_to_screen = reinterpret_cast<CameraWorldToScreenInjectedFn>(game_assembly_base + offsets::rva::CameraWorldToScreenPointInjected);
    if (!SafeMemory::IsProbablyUserPointer(reinterpret_cast<std::uintptr_t>(map_node_to_world)) ||
        !SafeMemory::IsProbablyUserPointer(reinterpret_cast<std::uintptr_t>(camera_world_to_screen))) {
        return false;
    }

    bool ok = false;
    UnityVector3 world{};
#if defined(_MSC_VER)
    __try {
        map_node_to_world(&world, reinterpret_cast<void*>(map_ptr), node, nullptr);
        ok = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
#else
    map_node_to_world(&world, reinterpret_cast<void*>(map_ptr), node, nullptr);
    ok = true;
#endif
    if (!ok) {
        return false;
    }

    if (ProjectWorldToScreen(camera_world_to_screen, native_camera, world, display, out)) {
        if (camera_mode != nullptr) *camera_mode = 1;
        return true;
    }
    if (ProjectWorldToScreen(camera_world_to_screen, managed_camera, world, display, out)) {
        if (camera_mode != nullptr) *camera_mode = 2;
        return true;
    }
    return false;
}

bool IsInsideEspPlayArea(const ImVec2& point, const ImVec2& display) {
    const float top = 36.0f;
    const float right = display.x - 230.0f;
    const float bottom = display.y - 150.0f;
    return point.x >= 0.0f && point.x <= right && point.y >= top && point.y <= bottom;
}

}

OverlayRenderer::OverlayRenderer(Diagnostics& diagnostics, OllamaClient& ollama)
    : diagnostics_(diagnostics), ollama_(ollama) {}

void OverlayRenderer::Render(
    const GameState& state,
    const Advice& advice,
    const DatabaseStatus& database,
    const OllamaStatus& ai,
    bool overlay_visible,
    bool dev_panel_visible) {
#ifdef ARCANUS_WITH_IMGUI
    ApplyArcanusStyle();
    ImGui::GetStyle().FontScaleMain = ui_scale_;
    RenderMapGridDebug(state);
    RenderVisualDebugBoxes(state);

    if (!overlay_visible && !dev_panel_visible) {
        return;
    }

    if (overlay_visible) {
        ImGui::SetNextWindowSize(ImVec2(390.0f, 520.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.88f);
        ImGui::Begin("ARCANUS Tactical AI", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::TextUnformatted("ARCANUS | Read-only Overlay");
        ImGui::Separator();
        ImGui::Text("State: %s | DB: %s | Data: %s",
            ToString(state.mode).c_str(),
            ToString(database.state).c_str(),
            state.live_data ? "LIVE" : "WAITING");
        ImGui::Text("Scanner: %s | %s | confidence %d%%",
            state.scanner_debug.source.c_str(),
            state.scanner_debug.status.c_str(),
            state.scanner_debug.confidence);
        if (state.live_data && last_no_projection_context_ > 0) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.45f, 0.25f, 1.0f),
                "Projection unavailable: waiting WorldCamera");
        }
        if (state.live_data) {
            ImGui::Text("Faction: %s", state.faction.c_str());
            ImGui::Text("Hero: %s | День %d | Неделя %d | Месяц %d",
                state.hero.name.c_str(), state.turn.day, state.turn.week, state.turn.month);
            ImGui::Text("Lvl %d | XP %d | Move %d | Mana %d | A/D/SP/K %d/%d/%d/%d",
                state.hero.level,
                state.hero.xp,
                state.hero.movement_points,
                state.hero.mana,
                state.hero.attack,
                state.hero.defense,
                state.hero.spell_power,
                state.hero.knowledge);
            if (state.scanner_debug.selected_hero_node >= 0) {
                ImGui::Text("Hero node %d | pos %d,%d",
                    state.scanner_debug.selected_hero_node,
                    state.hero.x,
                    state.hero.y);
            }
            ImGui::Text("Gold %d | Wood %d | Ore %d | Crystal %d",
                state.resources.gold, state.resources.wood, state.resources.ore, state.resources.crystal);
            if (state.scanner_debug.object_reward_preview_count > 0) {
                ImGui::TextColored(
                    ImVec4(0.45f, 1.0f, 0.55f, 1.0f),
                    "Reward preview: %d object(s)",
                    state.scanner_debug.object_reward_preview_count);
            } else if (state.scanner_debug.reward_set_count > 0) {
                ImGui::TextColored(
                    state.scanner_debug.reward_preview_linked_count > 0
                        ? ImVec4(0.45f, 1.0f, 0.55f, 1.0f)
                        : ImVec4(1.0f, 0.65f, 0.25f, 1.0f),
                    "Reward preview: %d linked / %d sets",
                    state.scanner_debug.reward_preview_linked_count,
                    state.scanner_debug.reward_set_count);
            }
            if (!state.army.empty()) {
                ImGui::TextUnformatted("Army:");
                for (const auto& stack : state.army) {
                    const auto unit_name = display::ShortRewardName(stack.unit, 18);
                    ImGui::BulletText("%s x%d", unit_name.c_str(), stack.count);
                }
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f), "Живые данные игры ещё не подключены");
            ImGui::TextWrapped("%s", state.scanner_status.c_str());
        }
        ImGui::Separator();
        ImGui::TextUnformatted(advice.headline.c_str());
        if (advice.win_probability > 0.0f) {
            ImGui::ProgressBar(advice.win_probability / 100.0f, ImVec2(220.0f, 0.0f));
        }
        for (const auto& line : advice.lines) {
            ImVec4 color = ImVec4(0.55f, 0.75f, 1.0f, 1.0f);
            if (line.priority == AdvicePriority::Critical) color = ImVec4(1.0f, 0.15f, 0.15f, 1.0f);
            if (line.priority == AdvicePriority::Important) color = ImVec4(1.0f, 0.55f, 0.15f, 1.0f);
            if (line.priority == AdvicePriority::Recommend) color = ImVec4(0.3f, 0.95f, 0.45f, 1.0f);
            ImGui::TextColored(color, "[%s] %s", ToString(line.priority).c_str(), line.title.c_str());
            ImGui::TextWrapped("%s", line.body.c_str());
        }
        ImGui::Separator();
        if (ai.state == AIState::Requesting || ai.state == AIState::Receiving) {
            ImGui::TextUnformatted(ai.message.c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted("|");
            ImGui::SameLine();
            ImGui::TextUnformatted("ожидание Ollama...");
        } else if (ImGui::Button(state.live_data ? "Объяснить совет" : "Объяснить статус")) {
#if !defined(ARCANUS_INPROCESS_OLLAMA)
            ollama_.Request("disabled");
#else
            std::ostringstream prompt;
            prompt << "Объясни кратко текущий совет ARCANUS.\n";
            prompt << "State JSON: " << ToJson(state) << "\n";
            prompt << "Headline: " << advice.headline << "\n";
            for (const auto& line : advice.lines) {
                prompt << "- " << line.title << ": " << line.body << "\n";
            }
            if (!ollama_.Request(prompt.str())) {
                diagnostics_.Warn("Ollama request skipped: worker busy");
            }
#endif
        }
        if (ai.state == AIState::Success && !ai.last_response.empty()) {
            ImGui::TextWrapped("%s", ai.last_response.c_str());
        } else if (ai.state == AIState::Error || ai.state == AIState::ErrorTimeout) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "%s", ai.message.c_str());
        }
        ImGui::End();
    }

    if (dev_panel_visible) {
        ImGui::SetNextWindowSize(ImVec2(820.0f, 620.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.90f);
        ImGui::Begin("ARCANUS Developer Panel", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::TextColored(ImVec4(0.55f, 0.78f, 1.0f, 1.0f), "ARCANUS read-only diagnostics");
        ImGui::SameLine();
        ImGui::TextDisabled("F1 HUD | ~ Dev Panel | End hide");
        ImGui::Separator();

        if (ImGui::BeginTabBar("##arcanus-dev-tabs")) {
            if (ImGui::BeginTabItem("Status")) {
                ImGui::Text("Hook: %s", diagnostics_.HookStatus().c_str());
                ImGui::Text("Scanner latency: %.3f ms", diagnostics_.ScannerLatencyMs());
                ImGui::Text("GameAssembly base: 0x%p", reinterpret_cast<void*>(diagnostics_.GameAssemblyBase()));
                ImGui::Text("State: %s | Live: %s | DB: %s",
                    ToString(state.mode).c_str(),
                    state.live_data ? "yes" : "no",
                    ToString(database.state).c_str());
                ImGui::SliderFloat("UI scale", &ui_scale_, 0.8f, 2.0f, "%.2f");
                ImGui::Separator();
                ImGui::TextWrapped("Overlay does not write game values. Route lines are debug direct lines until real pathfinding is implemented.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("ESP")) {
                const char* camera_mode = last_projection_camera_mode_ == 1
                    ? "native camera"
                    : (last_projection_camera_mode_ == 2 ? "managed camera" : "none");
                ImGui::Text("Projection: %s",
                    use_game_projection_ && last_game_anchor_ok_ ? "game-node -> world -> camera" : "unavailable");
                ImGui::SameLine();
                ImGui::TextDisabled("| camera: %s", camera_mode);
                ImGui::Text("Visible %d | projected %d | boxes %d | labels %d | arrows %d",
                    last_visible_objects_,
                    last_projected_objects_,
                    last_drawn_boxes_,
                    last_drawn_labels_,
                    last_drawn_arrows_);
                ImGui::Text("filtered %d | offscreen %d | failed %d | no ctx %d",
                    last_filtered_objects_,
                    last_offscreen_objects_,
                    last_projection_failed_,
                    last_no_projection_context_);
                ImGui::Text("projection calls/frame %d | budget skips %d",
                    last_projection_calls_,
                    last_projection_budget_skips_);
                ImGui::Separator();

                ImGui::Checkbox("World object boxes", &show_map_grid_boxes_);
                ImGui::SameLine();
                ImGui::Checkbox("Route labels", &show_route_labels_);
                ImGui::SameLine();
                ImGui::Checkbox("Hero marker", &show_hero_marker_);
                ImGui::Checkbox("Clip ESP to play area", &clip_esp_to_play_area_);
                ImGui::Checkbox("Direct route arrows (debug, not pathfinding)", &show_near_route_arrows_);
                ImGui::SameLine();
                ImGui::Checkbox("Raw visual boxes", &show_object_debug_boxes_);
                ImGui::Checkbox("Hide world ESP while reward dialog is open", &hide_esp_on_reward_dialog_);
                ImGui::Checkbox("Hide world ESP in game UI states", &hide_esp_in_game_ui_);

                const char* label_modes[] = {"Clean gameplay labels", "Debug full labels"};
                ImGui::Combo("ESP label mode", &esp_label_mode_, label_modes, IM_ARRAYSIZE(label_modes));
                ImGui::SliderInt("Max boxes", &esp_max_boxes_, 10, 500);
                ImGui::SliderInt("Max labels", &route_label_max_, 0, 120);
                ImGui::SliderInt("Arrow distance", &route_arrow_distance_, 1, 80);
                ImGui::SliderInt("Max arrows", &route_arrow_max_, 0, 20);
                ImGui::SliderInt("Projection max attempts/frame", &projection_budget_per_frame_, 300, 2000);
                ImGui::SliderFloat("Box size", &map_box_size_, 4.0f, 64.0f, "%.1f");

                ImGui::Separator();
                ImGui::TextUnformatted("Object layers");
                if (ImGui::BeginTable("##object-layers", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg)) {
                    ImGui::TableNextColumn(); ImGui::Checkbox("Resources", &show_kind_res_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Chests", &show_kind_chest_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Mines", &show_kind_mine_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Items", &show_kind_item_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Hire", &show_kind_hire_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Cities", &show_kind_city_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Encounters", &show_kind_event_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Fights", &show_encounter_fight_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Stats", &show_encounter_stat_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Services", &show_encounter_service_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Other events", &show_encounter_other_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Events", &show_kind_todo_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Markets", &show_kind_market_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Taverns", &show_kind_tavern_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Portals", &show_kind_portal_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Prisons", &show_kind_prison_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Outposts", &show_kind_outpost_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Blockers", &show_kind_block_);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Garrisons", &show_kind_garrison_);
                    ImGui::EndTable();
                }
                ImGui::TextColored(
                    ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
                    "Chests includes normal chests, Pandora boxes, and scroll boxes.");

                ImGui::Separator();
                ImGui::Checkbox("Use game node projection", &use_game_projection_);
                const auto managed_camera = DataPointerProbe::UnityCameraPointer();
                const auto native_camera = UnityObjectNativePointer(managed_camera);
                ImGui::Text("WorldCamera: 0x%p | UnityCamera: 0x%p | NativeCamera: 0x%p | Map: 0x%p",
                    reinterpret_cast<void*>(DataPointerProbe::WorldCameraPointer()),
                    reinterpret_cast<void*>(managed_camera),
                    reinterpret_cast<void*>(native_camera),
                    reinterpret_cast<void*>(DataPointerProbe::WorldMapPointer()));
                ImGui::Text("Hero grid: %d,%d | node %d | Mouse: %.0f,%.0f",
                    state.hero.x,
                    state.hero.y,
                    state.scanner_debug.selected_hero_node,
                    ImGui::GetIO().MousePos.x,
                    ImGui::GetIO().MousePos.y);
                ImGui::Text("Turn phase/mode: %d/%d | side state: %d (%s)",
                    state.scanner_debug.turn_phase,
                    state.scanner_debug.turn_mode,
                    state.scanner_debug.side_current_state,
                    SideStateName(state.scanner_debug.side_current_state));

                int reward_rows = 0;
                for (const auto& object : state.visible_objects) {
                    if (!object.reward_preview.empty()) {
                        ++reward_rows;
                    }
                }
                ImGui::Separator();
                ImGui::Text("Object reward previews %d | rows on screen %d",
                    state.scanner_debug.object_reward_preview_count,
                    reward_rows);
                ImGui::Text("Side RewardSets total %d | linked %d | unmatched %d",
                    state.scanner_debug.reward_set_count,
                    state.scanner_debug.reward_preview_linked_count,
                    state.scanner_debug.reward_preview_unmatched_count);
                ImGui::TextWrapped("Object rewards: %s",
                    state.scanner_debug.object_reward_sets_preview.empty()
                        ? "-"
                        : state.scanner_debug.object_reward_sets_preview.c_str());
                if (state.scanner_debug.reward_set_count > 0 && state.scanner_debug.reward_preview_linked_count == 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                        "RewardSets found, but not linked to visible map ids yet.");
                }
                ImGui::TextWrapped("Raw reward sets: %s",
                    state.scanner_debug.reward_sets_preview.empty() ? "-" : state.scanner_debug.reward_sets_preview.c_str());
                ImGui::TextWrapped("Unmatched reward sets: %s",
                    state.scanner_debug.reward_unmatched_preview.empty() ? "-" : state.scanner_debug.reward_unmatched_preview.c_str());
                if (reward_rows > 0 && ImGui::BeginTable("##reward-preview-table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Kind");
                    ImGui::TableSetupColumn("Type");
                    ImGui::TableSetupColumn("Node");
                    ImGui::TableSetupColumn("Preview");
                    ImGui::TableHeadersRow();
                    int shown = 0;
                    for (const auto& object : state.visible_objects) {
                        if (object.reward_preview.empty()) {
                            continue;
                        }
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(ObjectKind(object).c_str());
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(object.type.c_str());
                        ImGui::TableNextColumn(); ImGui::Text("%d [%d,%d]", object.node, object.x, object.y);
                        ImGui::TableNextColumn(); ImGui::TextWrapped("%s", object.reward_preview.c_str());
                        if (++shown >= 16) {
                            break;
                        }
                    }
                    ImGui::EndTable();
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Scanner")) {
                const auto& scan = state.scanner_debug;
                ImGui::Text("Source: %s | Status: %s | Confidence: %d%%",
                    scan.source.c_str(), scan.status.c_str(), scan.confidence);
                ImGui::TextWrapped("Reject/Last: %s", scan.reject_reason.empty() ? "-" : scan.reject_reason.c_str());
                ImGui::Separator();
                ImGui::Text("GameAssembly: 0x%p", reinterpret_cast<void*>(scan.game_assembly_base));
                ImGui::Text("Data*:        0x%p", reinterpret_cast<void*>(scan.data));
                ImGui::Text("DataObjects*: 0x%p", reinterpret_cast<void*>(scan.data_objects));
                ImGui::Text("DataHeroes*:  0x%p", reinterpret_cast<void*>(scan.data_heroes));
                ImGui::Text("DataSquads*:  0x%p", reinterpret_cast<void*>(scan.data_squads));
                ImGui::Text("MapData*:     0x%p | %dx%d | groups: %d",
                    reinterpret_cast<void*>(scan.map_data),
                    scan.map_size_x,
                    scan.map_size_z,
                    scan.map_data_object_group_count);
                ImGui::Text("Map object positions: %d", scan.map_object_position_count);
                ImGui::TextWrapped("Map name: %s", scan.map_name.empty() ? "-" : scan.map_name.c_str());
                ImGui::Text("MapObjects[]: 0x%p", reinterpret_cast<void*>(scan.map_data_objects_array));
                ImGui::Text("HeroesList*:  0x%p | dataHeroCount: %d",
                    reinterpret_cast<void*>(scan.data_heroes_list), scan.data_hero_count);
                ImGui::Text("DataSides*:   0x%p", reinterpret_cast<void*>(scan.data_sides));
                ImGui::Text("DataTurnMode*:0x%p | phase %d | mode %d | side %d",
                    reinterpret_cast<void*>(scan.data_turn_mode),
                    scan.turn_phase,
                    scan.turn_mode,
                    scan.turn_current_side_index);
                ImGui::Text("SideArray*:   0x%p", reinterpret_cast<void*>(scan.side_array));
                ImGui::Text("MySide*:      0x%p", reinterpret_cast<void*>(scan.my_side));
                ImGui::Text("Side state:   %d (%s)",
                    scan.side_current_state,
                    SideStateName(scan.side_current_state));
                ImGui::Text("ResHeap*:     0x%p", reinterpret_cast<void*>(scan.res_heap));
                ImGui::Text("RewardSets*:  0x%p | sets: %d",
                    reinterpret_cast<void*>(scan.side_reward_sets), scan.reward_set_count);
                ImGui::Text("Object reward previews: %d", scan.object_reward_preview_count);
                ImGui::TextWrapped("Object reward raw: %s",
                    scan.object_reward_sets_preview.empty() ? "-" : scan.object_reward_sets_preview.c_str());
                ImGui::Text("Side reward previews: linked %d | unmatched %d",
                    scan.reward_preview_linked_count, scan.reward_preview_unmatched_count);
                ImGui::TextWrapped("Reward raw: %s",
                    scan.reward_sets_preview.empty() ? "-" : scan.reward_sets_preview.c_str());
                ImGui::TextWrapped("Reward unmatched: %s",
                    scan.reward_unmatched_preview.empty() ? "-" : scan.reward_unmatched_preview.c_str());
                ImGui::Text("myIndex: %d | sideCount: %d", scan.my_index, scan.side_count);
                ImGui::Text("SideHeroes*:  0x%p", reinterpret_cast<void*>(scan.side_heroes));
                ImGui::Text("HeroList*:    0x%p", reinterpret_cast<void*>(scan.side_heroes_list));
                ImGui::Text("selectedHero: %d | heroCount: %d | ids: %s",
                    scan.last_selected_hero, scan.side_hero_count,
                    scan.side_hero_ids_preview.empty() ? "-" : scan.side_hero_ids_preview.c_str());
                ImGui::Text("SelectedHero*: 0x%p | node: %d",
                    reinterpret_cast<void*>(scan.selected_hero), scan.selected_hero_node);
                ImGui::Text("HeroParty*:    0x%p", reinterpret_cast<void*>(scan.hero_party));
                ImGui::Text("PartyUnits*:   0x%p | units: %d",
                    reinterpret_cast<void*>(scan.hero_party_units_list), scan.hero_party_unit_count);
                ImGui::TextWrapped("Object counts: %s",
                    scan.map_object_counts_preview.empty() ? "-" : scan.map_object_counts_preview.c_str());
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Database")) {
                ImGui::Text("State: %s", ToString(database.state).c_str());
                ImGui::TextWrapped("%s", database.message.c_str());
                ImGui::Text("Entries: %d | JSON: %d | Indexed: %d",
                    database.total_entries, database.json_entries, database.indexed_entries);
                for (std::size_t i = 0; i < std::min<std::size_t>(database.tables.size(), 20); ++i) {
                    ImGui::BulletText("%s: %d", database.tables[i].name.c_str(), database.tables[i].files);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("AI")) {
                ImGui::Text("State: %s | Model: %s", ToString(ai.state).c_str(), ai.model.c_str());
                ImGui::TextWrapped("%s", ai.message.c_str());
                ImGui::TextWrapped("In-DLL Ollama is disabled in this build to avoid game freezes/crashes. Use external arcanus_ai_bridge.exe later.");
                if (ImGui::Button("Reset AI panel")) {
                    ollama_.Reset();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Context")) {
                const auto json = ToJson(state);
                ImGui::TextWrapped("%s", json.c_str());
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Logs")) {
                for (const auto& entry : diagnostics_.Snapshot()) {
                    ImGui::Text("[%s] %s", ToString(entry.level).c_str(), entry.message.c_str());
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::End();
    }
#else
    RenderTextFallback(state, advice, dev_panel_visible);
#endif
}

bool OverlayRenderer::ShouldShowObjectKind(const MapObjectState& object, const std::string& kind) const {
    if (IsEncounterKind(kind)) {
        const auto group = EncounterGroup(kind, object.type);
        if (group == "fight" && !show_encounter_fight_) return false;
        if (group == "stat" && !show_encounter_stat_) return false;
        if (group == "service" && !show_encounter_service_) return false;
        if (group == "other" && !show_encounter_other_) return false;
    }
    if (kind == "res") return show_kind_res_;
    if (kind == "chest") return show_kind_chest_;
    if (kind == "mine") return show_kind_mine_;
    if (kind == "item") return show_kind_item_;
    if (kind == "hire") return show_kind_hire_;
    if (kind == "city") return show_kind_city_;
    if (kind == "event") return show_kind_event_;
    if (kind == "todo") return show_kind_todo_;
    if (kind == "market") return show_kind_market_;
    if (kind == "tavern") return show_kind_tavern_;
    if (kind == "portal") return show_kind_portal_;
    if (kind == "prison") return show_kind_prison_;
    if (kind == "outpost") return show_kind_outpost_;
    if (kind == "block") return show_kind_block_;
    if (kind == "garrison") return show_kind_garrison_;
    if (IsEncounterKind(kind)) return show_kind_event_;
    return true;
}

void OverlayRenderer::RenderVisualDebugBoxes(const GameState& state) const {
#ifdef ARCANUS_WITH_IMGUI
    if (!show_object_debug_boxes_ || state.debug_boxes.empty()) {
        return;
    }

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    for (const auto& box : state.debug_boxes) {
        if (!IsValidDebugBox(box)) {
            continue;
        }

        const ImVec2 min(box.left, box.top);
        const ImVec2 max(box.right, box.bottom);
        const ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(box.r, box.g, box.b, box.a));
        const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(box.r, box.g, box.b, 0.08f));

        draw_list->AddRectFilled(min, max, fill);
        draw_list->AddRect(min, max, color, 0.0f, 0, 2.0f);

        const std::string label = box.label.empty() ? box.kind : box.label;
        if (!label.empty()) {
            const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
            const ImVec2 text_min(box.left, std::max(0.0f, box.top - text_size.y - 5.0f));
            const ImVec2 text_max(text_min.x + text_size.x + 8.0f, text_min.y + text_size.y + 4.0f);
            draw_list->AddRectFilled(text_min, text_max, IM_COL32(0, 0, 0, 180));
            draw_list->AddText(ImVec2(text_min.x + 4.0f, text_min.y + 2.0f), color, label.c_str());
        }
    }
#else
    (void)state;
#endif
}

void OverlayRenderer::RenderMapGridDebug(const GameState& state) {
#ifdef ARCANUS_WITH_IMGUI
    const bool any_map_layer = show_map_grid_boxes_ || show_near_route_arrows_ || show_hero_marker_;
    last_visible_objects_ = static_cast<int>(state.visible_objects.size());
    last_drawn_boxes_ = 0;
    last_drawn_arrows_ = 0;
    last_drawn_labels_ = 0;
    last_filtered_objects_ = 0;
    last_offscreen_objects_ = 0;
    last_projected_objects_ = 0;
    last_projection_failed_ = 0;
    last_no_projection_context_ = 0;
    last_projection_calls_ = 0;
    last_projection_budget_skips_ = 0;
    last_projection_camera_mode_ = 0;
    last_game_anchor_ok_ = false;
    if (!state.live_data || !any_map_layer) {
        return;
    }
    if (hide_esp_on_reward_dialog_ && state.scanner_debug.reward_set_count > 0) {
        return;
    }
    if (hide_esp_in_game_ui_ &&
        state.scanner_debug.side_current_state >= 0 &&
        !IsPureMapSideState(state.scanner_debug.side_current_state)) {
        return;
    }

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;

    const bool game_projection_available = use_game_projection_ && HasGameProjectionContext();
    last_game_anchor_ok_ = game_projection_available;

    int projection_calls = 0;
    int projection_budget_skips = 0;
    const int effective_projection_budget = std::clamp(
        std::max({projection_budget_per_frame_, esp_max_boxes_ * 4, route_label_max_ * 4, 600}),
        300,
        2500);

    auto project_cell = [&](int node, ImVec2& out, bool* hit_budget = nullptr) {
        if (hit_budget != nullptr) {
            *hit_budget = false;
        }
        if (game_projection_available && node >= 0) {
            if (projection_calls >= effective_projection_budget) {
                ++projection_budget_skips;
                if (hit_budget != nullptr) {
                    *hit_budget = true;
                }
                return false;
            }
            ++projection_calls;
            int camera_mode = 0;
            const bool ok = TryProjectGameNode(diagnostics_.GameAssemblyBase(), node, display, out, &camera_mode);
            if (ok && last_projection_camera_mode_ == 0) {
                last_projection_camera_mode_ = camera_mode;
            }
            return ok;
        }
        (void)node;
        return false;
    };

    if (!game_projection_available) {
        last_no_projection_context_ = last_visible_objects_;
        return;
    }

    ImVec2 hero_screen{};
    const bool hero_projected = project_cell(state.scanner_debug.selected_hero_node, hero_screen);
    const bool hero_on_screen = hero_projected &&
        hero_screen.x >= -200.0f && hero_screen.x <= display.x + 200.0f &&
        hero_screen.y >= -200.0f && hero_screen.y <= display.y + 200.0f;

    if (show_hero_marker_ && hero_on_screen) {
        const ImU32 hero_color = IM_COL32(80, 255, 120, 230);
        draw_list->AddCircle(hero_screen, map_box_size_ * 0.65f, hero_color, 16, 2.5f);
        draw_list->AddText(ImVec2(hero_screen.x + 8.0f, hero_screen.y - 18.0f), hero_color, "HERO/GAME");
    }

    if (state.visible_objects.empty() || (!show_map_grid_boxes_ && !show_near_route_arrows_)) {
        return;
    }

    int drawn_boxes = 0;
    int drawn_arrows = 0;
    int drawn_labels = 0;
    int filtered_objects = 0;
    int offscreen_objects = 0;
    int projected_objects = 0;
    int projection_failed = 0;
    struct ProjectedObject {
        const MapObjectState* object = nullptr;
        std::string kind;
        ImVec2 center{};
        ImU32 color = 0;
        int distance = 0;
        int priority = 0;
    };
    struct CandidateObject {
        const MapObjectState* object = nullptr;
        std::string kind;
        int distance = 0;
        int priority = 0;
    };
    std::vector<CandidateObject> candidates;
    candidates.reserve(state.visible_objects.size());

    for (const auto& object : state.visible_objects) {
        const std::string kind = ObjectKind(object);
        if (!ShouldShowObjectKind(object, kind)) {
            ++filtered_objects;
            continue;
        }

        const int distance = GridDistance(state.hero, object);
        const int reward_bonus = object.reward_preview.empty() ? 0 : 80;
        candidates.push_back(CandidateObject{
            &object,
            kind,
            distance,
            ObjectPriority(kind, object.type) + reward_bonus,
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const CandidateObject& a, const CandidateObject& b) {
        const int bucket_a = a.distance / 6;
        const int bucket_b = b.distance / 6;
        if (bucket_a != bucket_b) {
            return bucket_a < bucket_b;
        }
        if (a.priority != b.priority) {
            return a.priority > b.priority;
        }
        return a.distance < b.distance;
    });

    std::vector<ProjectedObject> projected;
    projected.reserve(std::min<std::size_t>(
        candidates.size(),
        static_cast<std::size_t>(std::clamp(esp_max_boxes_ + route_label_max_ + route_arrow_max_, 20, 2000))));

    const int candidate_projection_limit = std::clamp(
        std::max({effective_projection_budget, esp_max_boxes_ * 4, route_label_max_ * 4, 300}),
        300,
        2500);
    int projected_attempts = 0;
    for (const auto& candidate : candidates) {
        if (projected_attempts >= candidate_projection_limit) {
            break;
        }

        ImVec2 center{};
        bool hit_budget = false;
        if (!project_cell(candidate.object->node, center, &hit_budget)) {
            if (hit_budget) {
                break;
            }
            ++projection_failed;
            continue;
        }
        ++projected_attempts;
        ++projected_objects;

        if (center.x < -100.0f || center.x > display.x + 100.0f ||
            center.y < -100.0f || center.y > display.y + 100.0f ||
            (clip_esp_to_play_area_ && !IsInsideEspPlayArea(center, display))) {
            ++offscreen_objects;
            continue;
        }

        const std::string color_key = candidate.kind.empty() ? candidate.object->type : (candidate.kind + ":" + candidate.object->type);
        const ImU32 color = ObjectColor(color_key);
        projected.push_back(ProjectedObject{
            candidate.object,
            candidate.kind,
            center,
            color,
            candidate.distance,
            candidate.priority,
        });
    }

    std::sort(projected.begin(), projected.end(), [](const ProjectedObject& a, const ProjectedObject& b) {
        const int score_a = a.priority * 100 - a.distance * 4;
        const int score_b = b.priority * 100 - b.distance * 4;
        if (score_a != score_b) {
            return score_a > score_b;
        }
        return a.distance < b.distance;
    });

    const float half = map_box_size_ * 0.5f;
    if (show_map_grid_boxes_) {
        for (const auto& item : projected) {
            if (drawn_boxes >= std::clamp(esp_max_boxes_, 10, 2000)) {
                break;
            }
            const std::string color_key = item.kind.empty() ? item.object->type : (item.kind + ":" + item.object->type);
            (void)color_key;
            DrawCornerBox(draw_list, item.center, half, item.color, 0);
            ++drawn_boxes;
        }
    }

    if (show_route_labels_) {
        std::vector<ScreenRect> occupied_labels;
        occupied_labels.reserve(static_cast<std::size_t>(std::max(0, route_label_max_)));
        for (const auto& item : projected) {
            if (drawn_labels >= route_label_max_) {
                break;
            }
            if (item.object->reward_preview.empty() && item.distance > route_arrow_distance_) {
                continue;
            }
            const std::string label = esp_label_mode_ == 1
                ? DebugObjectLabel(*item.object, item.kind)
                : CleanObjectLabel(*item.object, item.kind);
            if (!label.empty()) {
                if (DrawObjectLabel(draw_list, item.center, half, item.color, label, display, occupied_labels)) {
                    ++drawn_labels;
                }
            }
        }
    }

    if (show_near_route_arrows_ && hero_on_screen) {
        for (const auto& item : projected) {
            if (drawn_arrows >= route_arrow_max_) {
                break;
            }
            if (item.distance <= 0 || item.distance > route_arrow_distance_) {
                continue;
            }
            DrawArrowLine(draw_list, hero_screen, item.center, item.color);
            ++drawn_arrows;
        }
    }
    if (drawn_arrows > 0 && show_route_labels_ && hero_on_screen) {
        draw_list->AddText(
            ImVec2(hero_screen.x + 12.0f, hero_screen.y + 10.0f),
            IM_COL32(255, 220, 80, 230),
            "debug direct line, not path");
    }
    last_drawn_boxes_ = drawn_boxes;
    last_drawn_arrows_ = drawn_arrows;
    last_drawn_labels_ = drawn_labels;
    last_filtered_objects_ = filtered_objects;
    last_offscreen_objects_ = offscreen_objects;
    last_projected_objects_ = projected_objects;
    last_projection_failed_ = projection_failed;
    last_projection_calls_ = projection_calls;
    last_projection_budget_skips_ = projection_budget_skips;
#else
    (void)state;
#endif
}

void OverlayRenderer::RenderTextFallback(const GameState&, const Advice&, bool) {
    // No-op until ImGui is enabled. The worker, scanner, diagnostics, and
    // advisor still run so this DLL can be smoke-tested without vendored UI.
}

}
