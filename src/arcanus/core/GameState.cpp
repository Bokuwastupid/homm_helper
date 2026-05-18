#include "arcanus/core/GameState.h"
#include "arcanus/core/JsonWriter.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace arcanus {

namespace {

std::string HexAddress(std::uintptr_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << value;
    return stream.str();
}

}

std::string ToString(GameMode mode) {
    switch (mode) {
    case GameMode::Menu: return "menu";
    case GameMode::Map: return "map";
    case GameMode::Combat: return "combat";
    case GameMode::Town: return "town";
    default: return "unknown";
    }
}

bool IsGroveLikeFaction(const std::string& faction) {
    auto lower = faction;
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower == "grove" || lower == "sylvan";
}

std::string ToJson(const GameState& state) {
    JsonWriter json;
    json.BeginObject();
    json.KeyValue("game_state", ToString(state.mode));
    json.Key("turn");
    json.BeginObject();
    json.KeyValue("day", state.turn.day);
    json.KeyValue("week", state.turn.week);
    json.KeyValue("month", state.turn.month);
    json.EndObject();

    json.Key("hero");
    json.BeginObject();
    json.KeyValue("name", state.hero.name);
    json.KeyValue("level", state.hero.level);
    json.KeyValue("xp", state.hero.xp);
    json.KeyValue("movement_points", state.hero.movement_points);
    json.KeyValue("mana", state.hero.mana);
    json.KeyValue("mana_max", state.hero.mana_max);
    json.KeyValue("attack", state.hero.attack);
    json.KeyValue("defense", state.hero.defense);
    json.KeyValue("sp", state.hero.spell_power);
    json.KeyValue("knowledge", state.hero.knowledge);
    json.Key("position");
    json.BeginObject();
    json.KeyValue("x", state.hero.x);
    json.KeyValue("y", state.hero.y);
    json.EndObject();
    json.EndObject();

    json.Key("army");
    json.BeginArray();
    for (const auto& stack : state.army) {
        json.BeginObject();
        json.KeyValue("slot", stack.slot);
        json.KeyValue("unit", stack.unit);
        json.KeyValue("count", stack.count);
        json.KeyValue("hp_current", stack.hp_current);
        json.KeyValue("hp_max", stack.hp_max);
        json.KeyValue("energy_meter", stack.energy_meter);
        json.EndObject();
    }
    json.EndArray();

    json.Key("resources");
    json.BeginObject();
    json.KeyValue("gold", state.resources.gold);
    json.KeyValue("wood", state.resources.wood);
    json.KeyValue("ore", state.resources.ore);
    json.KeyValue("crystal", state.resources.crystal);
    json.KeyValue("gems", state.resources.gems);
    json.KeyValue("mercury", state.resources.mercury);
    json.KeyValue("sulfur", state.resources.sulfur);
    json.EndObject();

    json.KeyValue("faction", state.faction);
    json.Key("visible_objects");
    json.BeginArray();
    for (const auto& object : state.visible_objects) {
        json.BeginObject();
        json.KeyValue("type", object.type);
        json.KeyValue("kind", object.kind);
        json.KeyValue("id_map_object", object.id_map_object);
        json.KeyValue("node", object.node);
        json.KeyValue("x", object.x);
        json.KeyValue("y", object.y);
        json.KeyValue("value", object.value);
        json.KeyValue("reward_preview", object.reward_preview);
        json.EndObject();
    }
    json.EndArray();
    json.KeyValue("scanner_status", state.scanner_status);
    json.KeyValue("pointer_error", state.pointer_error);
    json.KeyValue("live_data", state.live_data);
    json.Key("debug_boxes");
    json.BeginArray();
    for (const auto& box : state.debug_boxes) {
        json.BeginObject();
        json.KeyValue("label", box.label);
        json.KeyValue("kind", box.kind);
        json.KeyValue("left", box.left);
        json.KeyValue("top", box.top);
        json.KeyValue("right", box.right);
        json.KeyValue("bottom", box.bottom);
        json.KeyValue("enabled", box.enabled);
        json.EndObject();
    }
    json.EndArray();
    json.Key("scanner_debug");
    json.BeginObject();
    json.KeyValue("source", state.scanner_debug.source);
    json.KeyValue("status", state.scanner_debug.status);
    json.KeyValue("reject_reason", state.scanner_debug.reject_reason);
    json.KeyValue("game_assembly_base", HexAddress(state.scanner_debug.game_assembly_base));
    json.KeyValue("data", HexAddress(state.scanner_debug.data));
    json.KeyValue("data_objects", HexAddress(state.scanner_debug.data_objects));
    json.KeyValue("data_heroes", HexAddress(state.scanner_debug.data_heroes));
    json.KeyValue("data_squads", HexAddress(state.scanner_debug.data_squads));
    json.KeyValue("data_heroes_list", HexAddress(state.scanner_debug.data_heroes_list));
    json.KeyValue("map_data", HexAddress(state.scanner_debug.map_data));
    json.KeyValue("map_data_objects_array", HexAddress(state.scanner_debug.map_data_objects_array));
    json.KeyValue("data_sides", HexAddress(state.scanner_debug.data_sides));
    json.KeyValue("data_turn_mode", HexAddress(state.scanner_debug.data_turn_mode));
    json.KeyValue("side_array", HexAddress(state.scanner_debug.side_array));
    json.KeyValue("my_side", HexAddress(state.scanner_debug.my_side));
    json.KeyValue("res_heap", HexAddress(state.scanner_debug.res_heap));
    json.KeyValue("side_reward_sets", HexAddress(state.scanner_debug.side_reward_sets));
    json.KeyValue("side_heroes", HexAddress(state.scanner_debug.side_heroes));
    json.KeyValue("side_heroes_list", HexAddress(state.scanner_debug.side_heroes_list));
    json.KeyValue("my_index", state.scanner_debug.my_index);
    json.KeyValue("side_count", state.scanner_debug.side_count);
    json.KeyValue("turn_phase", state.scanner_debug.turn_phase);
    json.KeyValue("turn_mode", state.scanner_debug.turn_mode);
    json.KeyValue("turn_current_side_index", state.scanner_debug.turn_current_side_index);
    json.KeyValue("side_current_state", state.scanner_debug.side_current_state);
    json.KeyValue("last_selected_hero", state.scanner_debug.last_selected_hero);
    json.KeyValue("side_hero_count", state.scanner_debug.side_hero_count);
    json.KeyValue("side_hero_ids_preview", state.scanner_debug.side_hero_ids_preview);
    json.KeyValue("selected_hero", HexAddress(state.scanner_debug.selected_hero));
    json.KeyValue("hero_party", HexAddress(state.scanner_debug.hero_party));
    json.KeyValue("hero_party_units_list", HexAddress(state.scanner_debug.hero_party_units_list));
    json.KeyValue("data_hero_count", state.scanner_debug.data_hero_count);
    json.KeyValue("selected_hero_node", state.scanner_debug.selected_hero_node);
    json.KeyValue("hero_party_unit_count", state.scanner_debug.hero_party_unit_count);
    json.KeyValue("object_reward_preview_count", state.scanner_debug.object_reward_preview_count);
    json.KeyValue("reward_set_count", state.scanner_debug.reward_set_count);
    json.KeyValue("reward_preview_linked_count", state.scanner_debug.reward_preview_linked_count);
    json.KeyValue("reward_preview_unmatched_count", state.scanner_debug.reward_preview_unmatched_count);
    json.KeyValue("map_size_x", state.scanner_debug.map_size_x);
    json.KeyValue("map_size_z", state.scanner_debug.map_size_z);
    json.KeyValue("map_data_object_group_count", state.scanner_debug.map_data_object_group_count);
    json.KeyValue("map_object_position_count", state.scanner_debug.map_object_position_count);
    json.KeyValue("map_name", state.scanner_debug.map_name);
    json.KeyValue("map_object_counts_preview", state.scanner_debug.map_object_counts_preview);
    json.KeyValue("object_reward_sets_preview", state.scanner_debug.object_reward_sets_preview);
    json.KeyValue("reward_sets_preview", state.scanner_debug.reward_sets_preview);
    json.KeyValue("reward_unmatched_preview", state.scanner_debug.reward_unmatched_preview);
    json.KeyValue("confidence", state.scanner_debug.confidence);
    json.EndObject();

    if (state.combat.has_value()) {
        json.Key("combat");
        json.BeginObject();
        json.KeyValue("round", state.combat->round);
        json.KeyValue("morale_player", state.combat->morale_player);
        json.KeyValue("luck_player", state.combat->luck_player);
        json.Key("initiative_queue");
        json.BeginArray();
        for (const auto& unit : state.combat->initiative_queue) {
            json.Value(unit);
        }
        json.EndArray();
        json.EndObject();
    } else {
        json.KeyNull("combat");
    }

    json.EndObject();
    return json.Str();
}

}
