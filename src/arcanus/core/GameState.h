#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arcanus {

enum class GameMode {
    Menu,
    Map,
    Combat,
    Town,
    Unknown
};

struct TurnState {
    int day = 1;
    int week = 1;
    int month = 1;
};

struct SkillState {
    std::string name;
    std::string level;
};

struct ArtifactState {
    std::string slot;
    std::string name;
    std::string set;
};

struct HeroState {
    std::string name = "Unknown";
    int level = 1;
    int xp = 0;
    int movement_points = 0;
    int mana = 0;
    int mana_max = 0;
    int attack = 0;
    int defense = 0;
    int spell_power = 0;
    int knowledge = 0;
    int x = 0;
    int y = 0;
    std::vector<SkillState> skills;
    std::vector<std::string> spells;
    std::vector<ArtifactState> artifacts;
};

struct ArmyStackState {
    int slot = 0;
    std::string unit = "Unknown";
    int count = 0;
    int hp_current = 0;
    int hp_max = 0;
    std::vector<std::string> abilities;
    float energy_meter = 0.0f;
};

struct ResourceState {
    int gold = 0;
    int wood = 0;
    int ore = 0;
    int crystal = 0;
    int gems = 0;
    int mercury = 0;
    int sulfur = 0;
};

struct MapObjectState {
    std::string type;
    std::string kind;
    int id_map_object = -1;
    int node = -1;
    int x = 0;
    int y = 0;
    std::string value;
    std::string reward_preview;
};

struct VisualDebugBox {
    std::string label;
    std::string kind;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float r = 1.0f;
    float g = 0.1f;
    float b = 0.1f;
    float a = 0.9f;
    bool enabled = false;
};

struct EnemyHeroState {
    std::string name;
    std::string army_estimate;
    int x = 0;
    int y = 0;
    bool unreliable = false;
};

struct CombatUnitState {
    std::string side;
    std::string unit;
    int q = 0;
    int r = 0;
    int count = 0;
    int hp = 0;
    bool waited = false;
    float energy_meter = 0.0f;
    std::vector<std::string> effects;
};

struct CombatState {
    std::vector<CombatUnitState> units;
    std::vector<std::string> initiative_queue;
    int round = 1;
    int morale_player = 0;
    int morale_enemy = 0;
    int luck_player = 0;
    int luck_enemy = 0;
    std::vector<std::string> hero_spells_used_this_combat;
};

struct ScannerDebugState {
    std::string source = "none";
    std::string status = "waiting";
    std::string reject_reason;
    std::uintptr_t game_assembly_base = 0;
    std::uintptr_t data = 0;
    std::uintptr_t data_objects = 0;
    std::uintptr_t data_heroes = 0;
    std::uintptr_t data_squads = 0;
    std::uintptr_t data_heroes_list = 0;
    std::uintptr_t map_data = 0;
    std::uintptr_t map_data_objects_array = 0;
    std::uintptr_t data_sides = 0;
    std::uintptr_t data_turn_mode = 0;
    std::uintptr_t side_array = 0;
    std::uintptr_t my_side = 0;
    std::uintptr_t res_heap = 0;
    std::uintptr_t side_reward_sets = 0;
    std::uintptr_t side_heroes = 0;
    std::uintptr_t side_heroes_list = 0;
    int my_index = -1;
    int side_count = 0;
    int turn_phase = -1;
    int turn_mode = -1;
    int turn_current_side_index = -1;
    int side_current_state = -1;
    int last_selected_hero = -1;
    int side_hero_count = 0;
    std::string side_hero_ids_preview;
    std::uintptr_t selected_hero = 0;
    std::uintptr_t hero_party = 0;
    std::uintptr_t hero_party_units_list = 0;
    int data_hero_count = 0;
    int selected_hero_node = -1;
    int hero_party_unit_count = 0;
    int object_reward_preview_count = 0;
    int reward_set_count = 0;
    int reward_preview_linked_count = 0;
    int reward_preview_unmatched_count = 0;
    int map_size_x = 0;
    int map_size_z = 0;
    int map_data_object_group_count = 0;
    int map_object_position_count = 0;
    std::string map_name;
    std::string map_object_counts_preview;
    std::string object_reward_sets_preview;
    std::string reward_sets_preview;
    std::string reward_unmatched_preview;
    int confidence = 0;
};

struct GameState {
    GameMode mode = GameMode::Unknown;
    TurnState turn;
    HeroState hero;
    std::vector<ArmyStackState> army;
    ResourceState resources;
    std::string faction = "Unknown";
    std::vector<std::string> active_laws;
    std::vector<MapObjectState> visible_objects;
    std::vector<VisualDebugBox> debug_boxes;
    std::vector<EnemyHeroState> visible_enemies;
    std::optional<CombatState> combat;
    bool pointer_error = false;
    bool live_data = false;
    std::string scanner_status = "mock";
    ScannerDebugState scanner_debug;
};

std::string ToString(GameMode mode);
std::string ToJson(const GameState& state);
bool IsGroveLikeFaction(const std::string& faction);

}
