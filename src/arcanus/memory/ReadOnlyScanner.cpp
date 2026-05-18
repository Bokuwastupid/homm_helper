#include "arcanus/memory/ReadOnlyScanner.h"

#include "arcanus/core/DisplayNames.h"
#include "arcanus/memory/DataPointerProbe.h"
#include "arcanus/memory/KnownOffsets.h"
#include "arcanus/memory/SafeMemory.h"

#include <Psapi.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <array>
#include <cstdio>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <sstream>
#include <utility>

namespace arcanus {

namespace {

std::string RewardTypeName(int type) {
    switch (type) {
    case 0: return "units";
    case 1: return "resources";
    case 4: return "hero_xp";
    case 5: return "level_xp";
    case 6: return "unit_box";
    case 7: return "units_box";
    case 8: return "optional_units";
    case 9: return "move";
    case 12: return "mana";
    case 15: return "stats";
    case 16: return "spell";
    case 19: return "random_spell";
    case 20: return "side_xp";
    case 23: return "random_item";
    case 24: return "item";
    case 25: return "level_item";
    case 26: return "skill";
    case 29: return "hero_buff";
    case 31: return "level_up";
    case 33: return "growth_restore";
    case 39: return "prop_resource";
    default: return "reward_" + std::to_string(type);
    }
}

std::string Shorten(std::string value, std::size_t max_len) {
    if (value.size() <= max_len) {
        return value;
    }
    if (max_len <= 3) {
        return value.substr(0, max_len);
    }
    value.resize(max_len - 3);
    value += "...";
    return value;
}

std::string LowerCopy(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool IsDefaultRewardToken(const std::string& value) {
    const auto lower = LowerCopy(value);
    return lower.empty() || lower == "default" || lower == "none" || lower == "null";
}

bool IsIntegerToken(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    std::size_t index = value[0] == '-' || value[0] == '+' ? 1 : 0;
    if (index >= value.size()) {
        return false;
    }
    for (; index < value.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

std::string StripKnownPrefix(std::string value) {
    return display::ShortRewardName(value, 24);
}

std::string ResourceDisplayName(std::string value) {
    if (const auto resource = display::ResourceName(value); !resource.empty()) {
        return resource;
    }
    return LowerCopy(display::ShortRewardName(value, 24));
}

bool IsResourceToken(const std::string& value) {
    return display::IsResourceToken(value);
}

std::string JoinRewardParams(const std::vector<std::string>& params, std::size_t max_items = 3) {
    std::ostringstream out;
    for (std::size_t i = 0; i < params.size() && i < max_items; ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << Shorten(display::ShortRewardName(params[i], 20), 20);
    }
    if (params.size() > max_items) {
        out << ", ...";
    }
    return out.str();
}

std::string FormatResourcePairs(const std::vector<std::string>& params) {
    std::ostringstream out;
    int written = 0;
    for (std::size_t i = 0; i + 1 < params.size(); i += 2) {
        if (!IsResourceToken(params[i]) || !IsIntegerToken(params[i + 1])) {
            continue;
        }
        if (written > 0) {
            out << ", ";
        }
        out << ResourceDisplayName(params[i]) << " +" << params[i + 1];
        if (++written >= 4) {
            if (i + 2 < params.size()) {
                out << ", ...";
            }
            break;
        }
    }
    return out.str();
}

std::string FormatRewardItem(
    int reward_type,
    const std::string& type_name,
    const std::string& reward_name,
    const std::vector<std::string>& params) {
    std::string base = IsDefaultRewardToken(type_name) ? RewardTypeName(reward_type) : type_name;
    if (!IsDefaultRewardToken(reward_name) && reward_name.size() <= 40) {
        base = reward_name;
    }

    const auto lower_base = LowerCopy(base);
    if (const auto resource_pairs = FormatResourcePairs(params); !resource_pairs.empty()) {
        return resource_pairs;
    }
    if (params.size() >= 2 && IsResourceToken(params[0]) && IsIntegerToken(params[1])) {
        return ResourceDisplayName(params[0]) + " +" + params[1];
    }
    if (IsResourceToken(base) && !params.empty() && IsIntegerToken(params[0])) {
        return ResourceDisplayName(base) + " +" + params[0];
    }

    if ((reward_type == 4 || reward_type == 5 || lower_base.find("xp") != std::string::npos ||
         lower_base.find("experience") != std::string::npos) &&
        !params.empty() && IsIntegerToken(params[0])) {
        return "xp +" + params[0];
    }
    if ((reward_type == 12 || lower_base.find("mana") != std::string::npos) &&
        !params.empty() && IsIntegerToken(params[0])) {
        return "mana +" + params[0];
    }
    if ((reward_type == 15 || lower_base.find("stat") != std::string::npos) && !params.empty()) {
        return "stat: " + JoinRewardParams(params, 2);
    }
    if ((reward_type == 16 || reward_type == 19 ||
         lower_base.find("spell") != std::string::npos ||
         lower_base.find("magic") != std::string::npos) ||
        std::ranges::any_of(params, [](const std::string& param) {
            const auto lower = LowerCopy(param);
            return lower.find("magic_scroll") != std::string::npos || lower.find("scroll") != std::string::npos;
        })) {
        for (const auto& param : params) {
            const auto lower = LowerCopy(param);
            if (lower.find("magic_scroll") != std::string::npos || lower.find("scroll") != std::string::npos) {
                return "magic scroll";
            }
        }
        return params.empty() ? "spell" : ("spell: " + Shorten(display::ShortRewardName(params[0], 22), 22));
    }
    if ((reward_type == 23 || reward_type == 24 || reward_type == 25 ||
         lower_base.find("item") != std::string::npos || lower_base.find("artifact") != std::string::npos) &&
        !params.empty()) {
        return "item: " + Shorten(display::ShortRewardName(params[0], 22), 22);
    }
    if ((reward_type == 0 || reward_type == 6 || reward_type == 7 || reward_type == 8 ||
         lower_base.find("unit") != std::string::npos) &&
        !params.empty()) {
        if (params.size() >= 2 && IsIntegerToken(params[1])) {
            return Shorten(display::ShortRewardName(params[0], 20), 20) + " x" + params[1];
        }
        return "units: " + Shorten(display::ShortRewardName(params[0], 22), 22);
    }

    if (!params.empty() && IsIntegerToken(params[0])) {
        return "reward +" + params[0];
    }

    if (IsDefaultRewardToken(base)) {
        return {};
    }
    if (!params.empty()) {
        return Shorten(display::ShortRewardName(base, 22), 22) + "(" + JoinRewardParams(params) + ")";
    }
    return Shorten(display::ShortRewardName(base, 28), 28);
}

std::vector<std::string> ReadStringList(std::uintptr_t list_ptr, int max_items) {
    std::vector<std::string> values;
    const auto item_ptrs = SafeMemory::ReadIl2CppPtrList(list_ptr, max_items);
    if (!item_ptrs) {
        return values;
    }

    for (const auto string_ptr : *item_ptrs) {
        if (!SafeMemory::IsProbablyUserPointer(string_ptr)) {
            continue;
        }
        if (auto value = SafeMemory::ReadIl2CppString(string_ptr, 256); value && !value->empty()) {
            values.push_back(*value);
        }
    }
    return values;
}

std::string BuildRewardPreview(std::uintptr_t reward_set_ptr) {
    const auto rewards_list_ptr = SafeMemory::ReadPtr(reward_set_ptr + offsets::data_reward_set::Rewards);
    if (!rewards_list_ptr || !SafeMemory::IsProbablyUserPointer(*rewards_list_ptr)) {
        return {};
    }

    const auto rewards = SafeMemory::ReadIl2CppPtrList(*rewards_list_ptr, 16);
    if (!rewards || rewards->empty()) {
        return {};
    }

    std::ostringstream out;
    int written = 0;
    for (const auto reward_ptr : *rewards) {
        if (!SafeMemory::IsProbablyUserPointer(reward_ptr)) {
            continue;
        }

        const int reward_type = SafeMemory::Read<std::int32_t>(reward_ptr + offsets::data_reward::RewardType).value_or(-1);
        std::string type_name;
        if (const auto type_ptr = SafeMemory::ReadPtr(reward_ptr + offsets::data_reward::StringRewardType);
            type_ptr && SafeMemory::IsProbablyUserPointer(*type_ptr)) {
            if (auto value = SafeMemory::ReadIl2CppString(*type_ptr, 128); value && !value->empty()) {
                type_name = *value;
            }
        }
        std::string reward_name;
        if (const auto name_ptr = SafeMemory::ReadPtr(reward_ptr + offsets::data_reward::RewardName);
            name_ptr && SafeMemory::IsProbablyUserPointer(*name_ptr)) {
            if (auto value = SafeMemory::ReadIl2CppString(*name_ptr, 128); value && !value->empty()) {
                reward_name = *value;
            }
        }

        std::vector<std::string> params;
        if (const auto params_ptr = SafeMemory::ReadPtr(reward_ptr + offsets::data_reward::Parameters);
            params_ptr && SafeMemory::IsProbablyUserPointer(*params_ptr)) {
            params = ReadStringList(*params_ptr, 8);
        }

        auto formatted = FormatRewardItem(reward_type, type_name, reward_name, params);
        if (formatted.empty()) {
            continue;
        }

        if (written > 0) {
            out << " + ";
        }
        out << formatted;

        ++written;
        if (written >= 3) {
            break;
        }
    }

    return Shorten(out.str(), 128);
}

} // namespace

ReadOnlyScanner::ReadOnlyScanner(Diagnostics& diagnostics)
    : diagnostics_(diagnostics) {}

GameState ReadOnlyScanner::Capture() {
    const auto start = std::chrono::steady_clock::now();

    if (game_assembly_base_ == 0) {
        if (auto base = ResolveGameAssemblyBase()) {
            game_assembly_base_ = *base;
            diagnostics_.SetGameAssemblyBase(game_assembly_base_);
            diagnostics_.Info("GameAssembly.dll base resolved");
            DataPointerProbe::Install(game_assembly_base_, diagnostics_);
        }
    }

    const auto hooked_data = DataPointerProbe::DataPointer();
    if (hooked_data != 0 && hooked_data != data_ptr_ && ValidateDataCandidate(hooked_data)) {
        data_ptr_ = hooked_data;
        data_source_ = "hook";
        map_data_ptr_ = 0;
        last_map_heap_scan_ = {};
        diagnostics_.Info("Data* refreshed from hook");
        SavePointerCache();
    }

    const auto hooked_map_data = DataPointerProbe::MapDataPointer();
    if (hooked_map_data != 0 && hooked_map_data != map_data_ptr_ && ValidateMapDataCandidate(hooked_map_data)) {
        map_data_ptr_ = hooked_map_data;
        diagnostics_.Info("MapData* refreshed from hook");
        SavePointerCache();
    }

    if (data_ptr_ == 0 || map_data_ptr_ == 0) {
        if (const auto cache = LoadPointerCache()) {
            if (data_ptr_ == 0 &&
                cache->data != 0 &&
                cache->process_id == GetCurrentProcessId() &&
                cache->game_assembly_base == game_assembly_base_ &&
                ValidateDataCandidate(cache->data)) {
                data_ptr_ = cache->data;
                data_source_ = "cache";
                diagnostics_.Info("Data* restored from pointer cache");
            }

            if (map_data_ptr_ == 0 &&
                cache->map_data != 0 &&
                cache->process_id == GetCurrentProcessId() &&
                cache->game_assembly_base == game_assembly_base_ &&
                ValidateMapDataCandidate(cache->map_data)) {
                map_data_ptr_ = cache->map_data;
                diagnostics_.Info("MapData* restored from pointer cache");
            }
        }
    }

    if (data_ptr_ == 0) {
        data_ptr_ = DataPointerProbe::DataPointer();
        if (data_ptr_ != 0) {
            data_source_ = "hook";
            SavePointerCache();
        }
    }

    if (map_data_ptr_ == 0) {
        map_data_ptr_ = DataPointerProbe::MapDataPointer();
        if (map_data_ptr_ != 0) {
            SavePointerCache();
        }
    }

    if (data_ptr_ == 0) {
        const auto now = std::chrono::steady_clock::now();
        if (last_heap_scan_.time_since_epoch().count() == 0 ||
            now - last_heap_scan_ > std::chrono::seconds(3)) {
            last_heap_scan_ = now;
            if (auto found = FindDataByHeapScan()) {
                data_ptr_ = *found;
                data_source_ = "heap-scan";
                diagnostics_.Info("Data* found by heap scan");
                SavePointerCache();
            }
        }
    }

    if (map_data_ptr_ == 0 && data_ptr_ != 0) {
        const auto now = std::chrono::steady_clock::now();
        if (last_map_heap_scan_.time_since_epoch().count() == 0 ||
            now - last_map_heap_scan_ > std::chrono::seconds(15)) {
            last_map_heap_scan_ = now;
            if (auto found = FindMapDataByHeapScan()) {
                map_data_ptr_ = *found;
                diagnostics_.Info("MapData* found by heap scan");
                SavePointerCache();
            }
        }
    }

    auto state = CaptureMockState();
    if (data_ptr_ != 0) {
        if (auto captured = CaptureFromData(data_ptr_)) {
            if (map_data_ptr_ != 0 &&
                captured->scanner_debug.map_data_object_group_count > 0 &&
                (captured->scanner_debug.map_object_position_count == 0 ||
                 (captured->visible_objects.empty() && !captured->scanner_debug.map_object_counts_preview.empty())) &&
                !captured->scanner_debug.map_object_counts_preview.empty()) {
                diagnostics_.Warn("MapData* looks stale or has no matching object positions; dropping and rescanning");
                map_data_ptr_ = 0;
                last_map_heap_scan_ = {};

                const auto hooked_map_data_after_stale = DataPointerProbe::MapDataPointer();
                if (hooked_map_data_after_stale != 0 && ValidateMapDataCandidate(hooked_map_data_after_stale)) {
                    map_data_ptr_ = hooked_map_data_after_stale;
                    diagnostics_.Info("MapData* refreshed from hook after stale cache");
                    SavePointerCache();
                } else if (auto found = FindMapDataByHeapScan()) {
                    map_data_ptr_ = *found;
                    diagnostics_.Info("MapData* refreshed by heap scan after stale cache");
                    SavePointerCache();
                }

                if (map_data_ptr_ != 0) {
                    if (auto refreshed = CaptureFromData(data_ptr_)) {
                        captured = std::move(refreshed);
                    }
                }
            }
            state = *captured;
        } else {
            diagnostics_.Warn("Current Data* became invalid; waiting for next hook/heap scan candidate");
            data_ptr_ = 0;
            data_source_ = "none";
            state = CaptureMockState();
        }
    }

    const auto end = std::chrono::steady_clock::now();
    const auto latency = std::chrono::duration<double, std::milli>(end - start).count();
    diagnostics_.SetScannerLatencyMs(latency);
    return state;
}

std::optional<std::uintptr_t> ReadOnlyScanner::FindPattern(HMODULE module, const std::string& pattern) const {
    auto bytes = ParsePattern(pattern);
    auto range = ModuleRange(module);
    if (!range.has_value() || bytes.empty()) {
        return std::nullopt;
    }

    const auto [base, size] = *range;
    if (size < bytes.size()) {
        return std::nullopt;
    }

    for (std::size_t offset = 0; offset <= size - bytes.size(); ++offset) {
        bool matched = true;
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            if (!bytes[index].value.has_value()) {
                continue;
            }
            const auto current = SafeMemory::Read<std::uint8_t>(base + offset + index);
            if (!current.has_value() || *current != *bytes[index].value) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return base + offset;
        }
    }

    debug_.status = "waiting";
    if (debug_.reject_reason.empty()) {
        debug_.reject_reason = "heap scan found no valid Data* candidate";
    }
    return std::nullopt;
}

std::optional<GameState> ReadOnlyScanner::CaptureFromData(std::uintptr_t data_ptr) const {
    ScannerDebugState debug;
    debug.source = data_source_;
    debug.status = "reading";
    debug.game_assembly_base = game_assembly_base_;
    debug.data = data_ptr;
    debug.map_data = map_data_ptr_;
    debug.reject_reason = last_reject_reason_;

    GameState state;
    state.mode = GameMode::Map;
    state.scanner_status = "memory-read-only";
    state.live_data = true;
    state.scanner_debug = debug;
    std::unordered_map<int, int> object_nodes;

    const auto day = SafeMemory::Read<std::int32_t>(data_ptr + offsets::data::Day);
    const auto week = SafeMemory::Read<std::int32_t>(data_ptr + offsets::data::Week);
    const auto month = SafeMemory::Read<std::int32_t>(data_ptr + offsets::data::Month);
    if (!day || !week || !month) {
        debug.status = "pointer-error";
        debug.reject_reason = "cannot read day/week/month";
        debug_ = debug;
        return std::nullopt;
    }

    state.turn.day = *day;
    state.turn.week = *week;
    state.turn.month = *month;

    if (const auto turn_mode_ptr = SafeMemory::ReadPtr(data_ptr + offsets::data::TurnMode);
        turn_mode_ptr && SafeMemory::IsProbablyUserPointer(*turn_mode_ptr)) {
        debug.data_turn_mode = *turn_mode_ptr;
        debug.turn_phase = SafeMemory::Read<std::int32_t>(*turn_mode_ptr + offsets::data_turn_mode::TurnPhase).value_or(-1);
        debug.turn_mode = SafeMemory::Read<std::int32_t>(*turn_mode_ptr + offsets::data_turn_mode::TurnMode).value_or(-1);
        debug.turn_current_side_index = SafeMemory::Read<std::int32_t>(*turn_mode_ptr + offsets::data_turn_mode::CurrentSideIndex).value_or(-1);
    }

    if (map_data_ptr_ != 0 && SafeMemory::IsProbablyUserPointer(map_data_ptr_)) {
        debug.map_size_x = SafeMemory::Read<std::int32_t>(map_data_ptr_ + offsets::map_data::SizeX).value_or(0);
        debug.map_size_z = SafeMemory::Read<std::int32_t>(map_data_ptr_ + offsets::map_data::SizeZ).value_or(0);
        if (const auto name_ptr = SafeMemory::ReadPtr(map_data_ptr_ + offsets::map_data::MapName)) {
            debug.map_name = SafeMemory::ReadIl2CppString(*name_ptr, 512).value_or("");
        }
        if (const auto objects_array = SafeMemory::ReadPtr(map_data_ptr_ + offsets::map_data::Objects);
            objects_array && SafeMemory::IsProbablyUserPointer(*objects_array)) {
            debug.map_data_objects_array = *objects_array;
            debug.map_data_object_group_count = SafeMemory::ReadIl2CppArrayLength(*objects_array, 100000).value_or(0);
            if (debug.map_data_object_group_count > 0 && debug.map_data_object_group_count < 100000) {
                for (int group_index = 0; group_index < debug.map_data_object_group_count && group_index < 2048; ++group_index) {
                    const auto group_ptr = SafeMemory::ReadPtr(*objects_array + offsets::il2cpp::ArrayItems + static_cast<std::uintptr_t>(group_index) * sizeof(std::uintptr_t));
                    if (!group_ptr || !SafeMemory::IsProbablyUserPointer(*group_ptr)) {
                        continue;
                    }

                    const auto sid_ptr = SafeMemory::ReadPtr(*group_ptr + offsets::map_data_objects::Sid);
                    const auto ids_ptr = SafeMemory::ReadPtr(*group_ptr + offsets::map_data_objects::Ids);
                    const auto nodes_ptr = SafeMemory::ReadPtr(*group_ptr + offsets::map_data_objects::Nodes);
                    if (!sid_ptr || !ids_ptr || !nodes_ptr) {
                        continue;
                    }

                    const auto sid = SafeMemory::ReadIl2CppString(*sid_ptr, 128).value_or("object");
                    const auto ids = SafeMemory::ReadIl2CppIntArray(*ids_ptr, 65536);
                    const auto nodes = SafeMemory::ReadIl2CppIntArray(*nodes_ptr, 65536);
                    if (!ids || !nodes || ids->empty() || ids->size() != nodes->size()) {
                        continue;
                    }

                    for (std::size_t index = 0; index < ids->size(); ++index) {
                        const int node = (*nodes)[index];
                        if (node < 0 || debug.map_size_x <= 0) {
                            continue;
                        }
                        object_nodes.emplace((*ids)[index], node);
                        ++debug.map_object_position_count;
                    }
                }
            }
        }
    }

    if (const auto objects_ptr = SafeMemory::ReadPtr(data_ptr + offsets::data::Objects);
        objects_ptr && SafeMemory::IsProbablyUserPointer(*objects_ptr)) {
        debug.data_objects = *objects_ptr;
        std::ostringstream object_counts;
        std::ostringstream object_reward_preview;
        bool wrote_count = false;
        int object_reward_preview_rows = 0;
        std::unordered_set<int> exposed_map_object_ids;
        auto append_object_count = [&](const char* label, std::size_t field, bool expose_on_map) {
            const auto list_ptr = SafeMemory::ReadPtr(*objects_ptr + field);
            if (!list_ptr || !SafeMemory::IsProbablyUserPointer(*list_ptr)) {
                return;
            }
            const auto object_ptrs = SafeMemory::ReadIl2CppPtrList(*list_ptr, 100000);
            if (!object_ptrs || object_ptrs->empty()) {
                return;
            }
            int active = 0;
            for (const auto object_ptr : *object_ptrs) {
                if (!SafeMemory::IsProbablyUserPointer(object_ptr)) {
                    continue;
                }
                const auto released = SafeMemory::Read<std::uint8_t>(object_ptr + offsets::data_object::Released).value_or(1);
                if (released == 0) {
                    ++active;
                    if (expose_on_map && debug.map_size_x > 0) {
                        const auto id = SafeMemory::Read<std::int32_t>(object_ptr + offsets::data_object::IdMapObject);
                        if (!id) {
                            continue;
                        }
                        if (!exposed_map_object_ids.insert(*id).second) {
                            continue;
                        }
                        const auto found_node = object_nodes.find(*id);
                        if (found_node == object_nodes.end()) {
                            continue;
                        }

                        std::string type = label;
                        if (const auto sid_ptr = SafeMemory::ReadPtr(object_ptr + offsets::data_object::SidConfig)) {
                            if (auto sid = SafeMemory::ReadIl2CppString(*sid_ptr, 256); sid && !sid->empty()) {
                                type = *sid;
                            }
                        }

                        MapObjectState object;
                        object.type = type;
                        object.kind = label;
                        object.id_map_object = *id;
                        object.node = found_node->second;
                        object.x = found_node->second % debug.map_size_x;
                        object.y = found_node->second / debug.map_size_x;
                        object.value = "id=" + std::to_string(*id) +
                            ";node=" + std::to_string(found_node->second) +
                            ";kind=" + label +
                            ";debug=active-logical-object";

                        if (const auto reward_set_ptr = SafeMemory::ReadPtr(object_ptr + offsets::data_object::RewardSet);
                            reward_set_ptr && SafeMemory::IsProbablyUserPointer(*reward_set_ptr)) {
                            if (auto reward_preview = BuildRewardPreview(*reward_set_ptr); !reward_preview.empty()) {
                                object.reward_preview = reward_preview;
                                object.value += ";object_reward=" + reward_preview;
                                ++debug.object_reward_preview_count;
                                if (object_reward_preview_rows < 12) {
                                    if (object_reward_preview_rows > 0) {
                                        object_reward_preview << " | ";
                                    }
                                    object_reward_preview
                                        << label << "#" << *id
                                        << ":" << Shorten(type, 24)
                                        << "=" << Shorten(reward_preview, 52);
                                    ++object_reward_preview_rows;
                                }
                            }
                        }
                        state.visible_objects.push_back(std::move(object));
                    }
                }
            }
            if (wrote_count) {
                object_counts << " | ";
            }
            wrote_count = true;
            object_counts << label << ":" << active << "/" << object_ptrs->size();
        };

        append_object_count("res", offsets::data_objects::ResObjs, true);
        append_object_count("chest", offsets::data_objects::ChestObjs, true);
        append_object_count("mine", offsets::data_objects::ResMines, true);
        append_object_count("city", offsets::data_objects::CityObjs, true);
        append_object_count("hire", offsets::data_objects::HireObjs, true);
        append_object_count("item", offsets::data_objects::ItemObjs, true);
        append_object_count("event", offsets::data_objects::EventBankObjs, true);
        append_object_count("todo", offsets::data_objects::TodoObjs, true);
        append_object_count("market", offsets::data_objects::MarketObjs, true);
        append_object_count("tavern", offsets::data_objects::TavernObjs, true);
        append_object_count("portal", offsets::data_objects::PortalObjs, true);
        append_object_count("prison", offsets::data_objects::PrisonObjs, true);
        append_object_count("trade_lab", offsets::data_objects::ResTradeLabs, true);
        append_object_count("outpost", offsets::data_objects::Outposts, true);
        append_object_count("block", offsets::data_objects::BlockObjs, true);
        append_object_count("garrison", offsets::data_objects::Garrisons, true);
        append_object_count("item_market", offsets::data_objects::ItemMarkets, true);
        append_object_count("random_hire", offsets::data_objects::RandomHires, true);
        append_object_count("unit_upgrade", offsets::data_objects::UnitUpgrades, true);
        append_object_count("eternal_dragon", offsets::data_objects::EternalDragons, true);
        append_object_count("insara_eye", offsets::data_objects::InsarasEyes, true);
        append_object_count("chimerologist", offsets::data_objects::Chimerologists, true);
        append_object_count("sacrificial_shrine", offsets::data_objects::SacrificialShrines, true);
        append_object_count("gladiator_arena", offsets::data_objects::GladiatorArenas, true);
        append_object_count("mirage", offsets::data_objects::Mirages, true);
        append_object_count("fickle_shrine", offsets::data_objects::FickleShrines, true);
        append_object_count("magic_mine", offsets::data_objects::MagicMines, true);
        append_object_count("town_gate", offsets::data_objects::TownGates, true);
        append_object_count("unit_res_trade_lab", offsets::data_objects::UnitResTradeLabs, true);
        append_object_count("pocket_dimension", offsets::data_objects::PocketDimensions, true);
        append_object_count("all", offsets::data_objects::AllObjects, false);
        debug.map_object_counts_preview = object_counts.str();
        debug.object_reward_sets_preview = object_reward_preview.str();
    }
    if (const auto heroes_ptr = SafeMemory::ReadPtr(data_ptr + offsets::data::Heroes);
        heroes_ptr && SafeMemory::IsProbablyUserPointer(*heroes_ptr)) {
        debug.data_heroes = *heroes_ptr;
    }
    if (const auto squads_ptr = SafeMemory::ReadPtr(data_ptr + offsets::data::Squads);
        squads_ptr && SafeMemory::IsProbablyUserPointer(*squads_ptr)) {
        debug.data_squads = *squads_ptr;
    }

    const auto sides_ptr = SafeMemory::ReadPtr(data_ptr + offsets::data::Sides);
    if (!sides_ptr) {
        state.pointer_error = true;
        debug.status = "pointer-error";
        debug.reject_reason = "Data.sides is unreadable";
        state.scanner_debug = debug;
        debug_ = debug;
        return state;
    }
    debug.data_sides = *sides_ptr;

    const auto my_index = SafeMemory::Read<std::int32_t>(*sides_ptr + offsets::data_sides::MyIndex);
    const auto side_array = SafeMemory::ReadPtr(*sides_ptr + offsets::data_sides::SideArray);
    if (!my_index || !side_array || *my_index < 0) {
        state.pointer_error = true;
        debug.status = "pointer-error";
        debug.reject_reason = "DataSides myIndex/sideArray invalid";
        state.scanner_debug = debug;
        debug_ = debug;
        return state;
    }
    debug.my_index = *my_index;
    debug.side_array = *side_array;

    const auto side_count = SafeMemory::Read<std::int32_t>(*side_array + offsets::il2cpp::ArrayLength);
    if (!side_count || *my_index >= *side_count) {
        state.pointer_error = true;
        debug.status = "pointer-error";
        debug.reject_reason = "side array length invalid";
        state.scanner_debug = debug;
        debug_ = debug;
        return state;
    }
    debug.side_count = *side_count;

    const auto my_side = SafeMemory::ReadPtr(*side_array + offsets::il2cpp::ArrayItems + static_cast<std::uintptr_t>(*my_index) * sizeof(std::uintptr_t));
    if (!my_side) {
        state.pointer_error = true;
        debug.status = "pointer-error";
        debug.reject_reason = "my side pointer unreadable";
        state.scanner_debug = debug;
        debug_ = debug;
        return state;
    }
    debug.my_side = *my_side;
    debug.side_current_state = SafeMemory::Read<std::int32_t>(*my_side + offsets::side::CurrentState).value_or(-1);

    std::unordered_map<int, std::string> reward_previews;
    if (const auto reward_sets_ptr = SafeMemory::ReadPtr(*my_side + offsets::side::RewardSets);
        reward_sets_ptr && SafeMemory::IsProbablyUserPointer(*reward_sets_ptr)) {
        debug.side_reward_sets = *reward_sets_ptr;
        if (const auto reward_list_ptr = SafeMemory::ReadPtr(*reward_sets_ptr + offsets::data_reward_sets::List);
            reward_list_ptr && SafeMemory::IsProbablyUserPointer(*reward_list_ptr)) {
            if (const auto reward_sets = SafeMemory::ReadIl2CppPtrList(*reward_list_ptr, 4096)) {
                debug.reward_set_count = static_cast<int>(reward_sets->size());
                std::ostringstream preview;
                int preview_count = 0;
                for (const auto reward_set_ptr : *reward_sets) {
                    if (!SafeMemory::IsProbablyUserPointer(reward_set_ptr)) {
                        continue;
                    }
                    const auto object_id = SafeMemory::Read<std::int32_t>(reward_set_ptr + offsets::data_reward_set::ObjectId);
                    if (!object_id || *object_id < 0) {
                        continue;
                    }
                    auto reward_preview = BuildRewardPreview(reward_set_ptr);
                    if (reward_preview.empty()) {
                        continue;
                    }
                    reward_previews[*object_id] = reward_preview;
                    if (preview_count < 8) {
                        if (preview_count > 0) {
                            preview << " | ";
                        }
                        preview << "obj" << *object_id << ":" << Shorten(reward_preview, 48);
                        ++preview_count;
                    }
                }
                debug.reward_sets_preview = preview.str();
            }
        }
    }

    std::unordered_set<int> linked_reward_ids;
    if (!reward_previews.empty()) {
        for (auto& object : state.visible_objects) {
            auto found = reward_previews.find(object.id_map_object);
            if (found == reward_previews.end() && object.node >= 0) {
                found = reward_previews.find(object.node);
            }
            if (found != reward_previews.end()) {
                if (object.reward_preview.empty()) {
                    object.reward_preview = found->second;
                    object.value += ";side_reward=" + found->second;
                } else {
                    object.value += ";side_reward_after_open=" + found->second;
                }
                linked_reward_ids.insert(found->first);
            }
        }
    }
    debug.reward_preview_linked_count = static_cast<int>(linked_reward_ids.size());
    debug.reward_preview_unmatched_count = static_cast<int>(
        reward_previews.size() >= linked_reward_ids.size() ? reward_previews.size() - linked_reward_ids.size() : 0);
    if (!reward_previews.empty()) {
        std::ostringstream unmatched;
        int unmatched_count = 0;
        for (const auto& [object_id, preview] : reward_previews) {
            if (linked_reward_ids.contains(object_id)) {
                continue;
            }
            if (unmatched_count > 0) {
                unmatched << " | ";
            }
            unmatched << "obj" << object_id << ":" << Shorten(preview, 48);
            if (++unmatched_count >= 8) {
                break;
            }
        }
        debug.reward_unmatched_preview = unmatched.str();
    }

    if (const auto faction_ptr = SafeMemory::ReadPtr(*my_side + offsets::side::Fraction)) {
        state.faction = SafeMemory::ReadIl2CppString(*faction_ptr).value_or("Unknown");
    }

    if (const auto selected_hero = SafeMemory::Read<std::int32_t>(*my_side + offsets::side::LastSelectedHero)) {
        debug.last_selected_hero = *selected_hero;
        if (*selected_hero >= 0) {
            state.hero.name = "Hero ID " + std::to_string(*selected_hero);
        }
    }

    std::vector<int> side_hero_ids;
    if (const auto side_heroes_ptr = SafeMemory::ReadPtr(*my_side + offsets::side::SideHeroes);
        side_heroes_ptr && SafeMemory::IsProbablyUserPointer(*side_heroes_ptr)) {
        debug.side_heroes = *side_heroes_ptr;
        if (const auto list_ptr = SafeMemory::ReadPtr(*side_heroes_ptr + offsets::side_heroes::Heroes);
            list_ptr && SafeMemory::IsProbablyUserPointer(*list_ptr)) {
            debug.side_heroes_list = *list_ptr;
            if (const auto ids = SafeMemory::ReadIl2CppIntList(*list_ptr, 32)) {
                side_hero_ids = *ids;
                debug.side_hero_count = static_cast<int>(ids->size());
                std::ostringstream preview;
                for (std::size_t index = 0; index < ids->size() && index < 10; ++index) {
                    if (index > 0) {
                        preview << ",";
                    }
                    preview << (*ids)[index];
                }
                if (ids->size() > 10) {
                    preview << ",...";
                }
                debug.side_hero_ids_preview = preview.str();
            }
        }
    }

    int target_hero_id = debug.last_selected_hero;
    if (target_hero_id < 0 && !side_hero_ids.empty()) {
        target_hero_id = side_hero_ids.front();
    }

    if (debug.data_heroes != 0 && target_hero_id >= 0) {
        if (const auto list_ptr = SafeMemory::ReadPtr(debug.data_heroes + offsets::data_heroes::List);
            list_ptr && SafeMemory::IsProbablyUserPointer(*list_ptr)) {
            debug.data_heroes_list = *list_ptr;
            if (const auto hero_ptrs = SafeMemory::ReadIl2CppPtrList(*list_ptr, 256)) {
                debug.data_hero_count = static_cast<int>(hero_ptrs->size());
                for (const auto hero_ptr : *hero_ptrs) {
                    if (!SafeMemory::IsProbablyUserPointer(hero_ptr)) {
                        continue;
                    }

                    const auto hero_id = SafeMemory::Read<std::int32_t>(hero_ptr + offsets::session_hero::Id);
                    if (!hero_id || *hero_id != target_hero_id) {
                        continue;
                    }

                    debug.selected_hero = hero_ptr;
                    state.hero.name = "Hero ID " + std::to_string(*hero_id);

                    if (const auto config_sid_ptr = SafeMemory::ReadPtr(hero_ptr + offsets::session_hero::ConfigSid)) {
                        if (auto config_sid = SafeMemory::ReadIl2CppString(*config_sid_ptr, 256);
                            config_sid && !config_sid->empty()) {
                            state.hero.name = *config_sid;
                        }
                    }

                    state.hero.level = SafeMemory::Read<std::int32_t>(hero_ptr + offsets::session_hero::CurrentLevel).value_or(1);
                    state.hero.xp = SafeMemory::Read<std::int32_t>(hero_ptr + offsets::session_hero::CurrentExp).value_or(0);
                    state.hero.movement_points = SafeMemory::Read<std::int32_t>(hero_ptr + offsets::session_hero::WorldMovePoints).value_or(0);
                    state.hero.mana = SafeMemory::Read<std::int32_t>(hero_ptr + offsets::session_hero::Mana).value_or(0);
                    debug.selected_hero_node = SafeMemory::Read<std::int32_t>(hero_ptr + offsets::session_hero::Node).value_or(-1);
                    if (debug.selected_hero_node >= 0 && debug.map_size_x > 0) {
                        state.hero.x = debug.selected_hero_node % debug.map_size_x;
                        state.hero.y = debug.selected_hero_node / debug.map_size_x;
                    }

                    const auto stats_by_level = SafeMemory::ReadPtr(hero_ptr + offsets::session_hero::StatsByLevel);
                    const auto additional_stats = SafeMemory::ReadPtr(hero_ptr + offsets::session_hero::AdditionalStats);
                    auto read_stat = [&](std::optional<std::uintptr_t> stat_ptr, std::size_t field) -> int {
                        if (!stat_ptr || !SafeMemory::IsProbablyUserPointer(*stat_ptr)) {
                            return 0;
                        }
                        return SafeMemory::Read<std::int32_t>(*stat_ptr + field).value_or(0);
                    };

                    state.hero.attack =
                        read_stat(stats_by_level, offsets::hero_stat::Offence) +
                        read_stat(additional_stats, offsets::hero_stat::Offence);
                    state.hero.defense =
                        read_stat(stats_by_level, offsets::hero_stat::Defence) +
                        read_stat(additional_stats, offsets::hero_stat::Defence);
                    state.hero.spell_power =
                        read_stat(stats_by_level, offsets::hero_stat::SpellPower) +
                        read_stat(additional_stats, offsets::hero_stat::SpellPower);
                    state.hero.knowledge =
                        read_stat(stats_by_level, offsets::hero_stat::Intelligence) +
                        read_stat(additional_stats, offsets::hero_stat::Intelligence);

                    if (const auto party_ptr = SafeMemory::ReadPtr(hero_ptr + offsets::session_hero::Party);
                        party_ptr && SafeMemory::IsProbablyUserPointer(*party_ptr)) {
                        debug.hero_party = *party_ptr;
                        if (const auto units_list_ptr = SafeMemory::ReadPtr(*party_ptr + offsets::data_party::Units);
                            units_list_ptr && SafeMemory::IsProbablyUserPointer(*units_list_ptr)) {
                            debug.hero_party_units_list = *units_list_ptr;
                            if (const auto unit_ptrs = SafeMemory::ReadIl2CppPtrList(*units_list_ptr, 16)) {
                                debug.hero_party_unit_count = static_cast<int>(unit_ptrs->size());
                                int slot = 0;
                                for (const auto unit_ptr : *unit_ptrs) {
                                    if (!SafeMemory::IsProbablyUserPointer(unit_ptr)) {
                                        continue;
                                    }

                                    const auto amount = SafeMemory::Read<std::int32_t>(unit_ptr + offsets::data_party_unit::Amount);
                                    const auto sid_ptr = SafeMemory::ReadPtr(unit_ptr + offsets::data_party_unit::Sid);
                                    if (!amount || *amount <= 0 || *amount > 100000 || !sid_ptr) {
                                        continue;
                                    }

                                    const auto sid = SafeMemory::ReadIl2CppString(*sid_ptr, 256);
                                    if (!sid || sid->empty()) {
                                        continue;
                                    }

                                    ArmyStackState stack;
                                    stack.slot = slot++;
                                    stack.unit = *sid;
                                    stack.count = *amount;
                                    state.army.push_back(std::move(stack));
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    const auto res_ptr = SafeMemory::ReadPtr(*my_side + offsets::side::Res);
    if (!res_ptr) {
        state.pointer_error = true;
        debug.status = "pointer-error";
        debug.reject_reason = "Side.res heap pointer unreadable";
        state.scanner_debug = debug;
        debug_ = debug;
        return state;
    }
    debug.res_heap = *res_ptr;

    auto read_resource = [&](std::size_t field) -> int {
        const auto resource_ptr = SafeMemory::ReadPtr(*res_ptr + field);
        if (!resource_ptr) {
            state.pointer_error = true;
            return 0;
        }
        return SafeMemory::Read<std::int32_t>(*resource_ptr + offsets::resource::Value).value_or(0);
    };

    state.resources.gold = read_resource(offsets::res_heap::Gold);
    state.resources.wood = read_resource(offsets::res_heap::Wood);
    state.resources.ore = read_resource(offsets::res_heap::Ore);
    state.resources.gems = read_resource(offsets::res_heap::Gemstones);
    state.resources.crystal = read_resource(offsets::res_heap::Crystals);
    state.resources.mercury = read_resource(offsets::res_heap::Mercury);
    state.resources.sulfur = read_resource(offsets::res_heap::Dust);
    debug.status = state.pointer_error ? "partial" : "live";
    debug.reject_reason = state.pointer_error ? "one or more resource pointers unreadable" : "";
    debug.confidence = state.pointer_error ? 45 : (debug.source == "hook" ? 95 : 80);
    state.scanner_debug = debug;
    debug_ = debug;
    return state;
}

std::optional<std::uintptr_t> ReadOnlyScanner::ResolveGameAssemblyBase() const {
    HMODULE module = GetModuleHandleW(L"GameAssembly.dll");
    if (module == nullptr) {
        return std::nullopt;
    }
    return reinterpret_cast<std::uintptr_t>(module);
}

std::optional<std::uintptr_t> ReadOnlyScanner::FindDataByHeapScan() const {
    diagnostics_.Info("Scanning heap for Data* candidate");
    debug_.source = "heap-scan";
    debug_.status = "scanning";
    debug_.game_assembly_base = game_assembly_base_;

    SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);

    auto address = reinterpret_cast<std::uintptr_t>(system_info.lpMinimumApplicationAddress);
    const auto max_address = reinterpret_cast<std::uintptr_t>(system_info.lpMaximumApplicationAddress);

    constexpr std::size_t kMaxRegionBytes = 64 * 1024 * 1024;
    constexpr std::size_t kChunkBytes = 1024 * 1024;

    while (address < max_address) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0) {
            address += 0x10000;
            continue;
        }

        const auto region_base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto region_size = static_cast<std::size_t>(mbi.RegionSize);
        address = region_base + region_size;

        if (!ValidateReadableRegion(mbi) || region_size < 0x120 || region_size > kMaxRegionBytes) {
            continue;
        }

        for (std::size_t offset = 0; offset < region_size; offset += kChunkBytes) {
            const auto chunk_size = std::min(kChunkBytes, region_size - offset);
            std::vector<std::byte> buffer(chunk_size);
            SIZE_T bytes_read = 0;
            const auto chunk_address = region_base + offset;
            if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(chunk_address), buffer.data(), buffer.size(), &bytes_read) == FALSE ||
                bytes_read < 0x120) {
                continue;
            }

            for (std::size_t i = 0; i + offsets::data::Month + sizeof(std::int32_t) <= bytes_read; i += 8) {
                std::int32_t day = 0;
                std::int32_t week = 0;
                std::int32_t month = 0;
                std::memcpy(&day, buffer.data() + i + offsets::data::Day, sizeof(day));
                std::memcpy(&week, buffer.data() + i + offsets::data::Week, sizeof(week));
                std::memcpy(&month, buffer.data() + i + offsets::data::Month, sizeof(month));

                if (day < 1 || day > 7 || week < 1 || week > 12 || month < 1 || month > 4) {
                    continue;
                }

                const auto candidate = chunk_address + i;
                if (ValidateStableDataCandidate(candidate)) {
                    return candidate;
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<std::uintptr_t> ReadOnlyScanner::FindMapDataByHeapScan() const {
    diagnostics_.Info("Scanning heap for MapData* candidate");

    SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);

    auto address = reinterpret_cast<std::uintptr_t>(system_info.lpMinimumApplicationAddress);
    const auto max_address = reinterpret_cast<std::uintptr_t>(system_info.lpMaximumApplicationAddress);

    constexpr std::size_t kMaxRegionBytes = 64 * 1024 * 1024;
    constexpr std::size_t kChunkBytes = 1024 * 1024;

    while (address < max_address) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0) {
            address += 0x10000;
            continue;
        }

        const auto region_base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto region_size = static_cast<std::size_t>(mbi.RegionSize);
        address = region_base + region_size;

        if (!ValidateReadableRegion(mbi) || region_size < 0x110 || region_size > kMaxRegionBytes) {
            continue;
        }

        for (std::size_t offset = 0; offset < region_size; offset += kChunkBytes) {
            const auto chunk_size = std::min(kChunkBytes, region_size - offset);
            std::vector<std::byte> buffer(chunk_size);
            SIZE_T bytes_read = 0;
            const auto chunk_address = region_base + offset;
            if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(chunk_address), buffer.data(), buffer.size(), &bytes_read) == FALSE ||
                bytes_read < 0x110) {
                continue;
            }

            for (std::size_t i = 0; i + offsets::map_data::Objects + sizeof(std::uintptr_t) <= bytes_read; i += 8) {
                std::int32_t size_x = 0;
                std::int32_t size_z = 0;
                std::uintptr_t objects_array = 0;
                std::memcpy(&size_x, buffer.data() + i + offsets::map_data::SizeX, sizeof(size_x));
                std::memcpy(&size_z, buffer.data() + i + offsets::map_data::SizeZ, sizeof(size_z));
                std::memcpy(&objects_array, buffer.data() + i + offsets::map_data::Objects, sizeof(objects_array));

                if (size_x < 16 || size_x > 512 || size_z < 16 || size_z > 512 || !SafeMemory::IsProbablyUserPointer(objects_array)) {
                    continue;
                }

                const auto candidate = chunk_address + i;
                if (ValidateMapDataCandidate(candidate)) {
                    return candidate;
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<PointerCache> ReadOnlyScanner::LoadPointerCache() const {
    char temp_path[MAX_PATH]{};
    if (GetTempPathA(static_cast<DWORD>(std::size(temp_path)), temp_path) == 0) {
        return std::nullopt;
    }

    std::string path = std::string(temp_path) + "arcanus_hommoe_pointer_cache.txt";
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    PointerCache cache;
    std::string line;
    while (std::getline(file, line)) {
        unsigned long long value = 0;
        if (std::sscanf(line.c_str(), "pid=%llu", &value) == 1) {
            cache.process_id = static_cast<DWORD>(value);
        } else if (std::sscanf(line.c_str(), "game_assembly_base=%llx", &value) == 1) {
            cache.game_assembly_base = static_cast<std::uintptr_t>(value);
        } else if (std::sscanf(line.c_str(), "data=%llx", &value) == 1) {
            cache.data = static_cast<std::uintptr_t>(value);
        } else if (std::sscanf(line.c_str(), "map_data=%llx", &value) == 1) {
            cache.map_data = static_cast<std::uintptr_t>(value);
        }
    }

    if (cache.process_id == 0 || cache.game_assembly_base == 0) {
        return std::nullopt;
    }
    return cache;
}

void ReadOnlyScanner::SavePointerCache() const {
    char temp_path[MAX_PATH]{};
    if (GetTempPathA(static_cast<DWORD>(std::size(temp_path)), temp_path) == 0) {
        return;
    }

    std::string path = std::string(temp_path) + "arcanus_hommoe_pointer_cache.txt";
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return;
    }

    file << "pid=" << GetCurrentProcessId() << "\n";
    file << std::hex;
    file << "game_assembly_base=" << game_assembly_base_ << "\n";
    file << "data=" << data_ptr_ << "\n";
    file << "map_data=" << map_data_ptr_ << "\n";
}

bool ReadOnlyScanner::ValidateStableDataCandidate(std::uintptr_t data_ptr) const {
    if (!ValidateDataCandidate(data_ptr)) {
        return false;
    }

    const auto first = CaptureFromData(data_ptr);
    if (!first.has_value()) {
        return Reject("stable check: first capture failed");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    if (!ValidateDataCandidate(data_ptr)) {
        return false;
    }
    const auto second = CaptureFromData(data_ptr);
    if (!second.has_value()) {
        return Reject("stable check: second capture failed");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    if (!ValidateDataCandidate(data_ptr)) {
        return false;
    }
    const auto third = CaptureFromData(data_ptr);
    if (!third.has_value()) {
        return Reject("stable check: third capture failed");
    }

    const auto same_turn =
        first->turn.day == second->turn.day &&
        second->turn.day == third->turn.day &&
        first->turn.week == second->turn.week &&
        second->turn.week == third->turn.week &&
        first->turn.month == second->turn.month &&
        second->turn.month == third->turn.month;

    const auto same_resources =
        first->resources.gold == second->resources.gold &&
        second->resources.gold == third->resources.gold &&
        first->resources.wood == second->resources.wood &&
        second->resources.wood == third->resources.wood &&
        first->resources.ore == second->resources.ore &&
        second->resources.ore == third->resources.ore;

    if (!same_turn) {
        return Reject("stable check: turn changed while validating candidate");
    }
    if (!same_resources) {
        return Reject("stable check: resources changed while validating candidate");
    }

    return true;
}

bool ReadOnlyScanner::ValidateMapDataCandidate(std::uintptr_t candidate) const {
    const auto size_x = SafeMemory::Read<std::int32_t>(candidate + offsets::map_data::SizeX);
    const auto size_z = SafeMemory::Read<std::int32_t>(candidate + offsets::map_data::SizeZ);
    if (!size_x || !size_z || *size_x < 16 || *size_x > 512 || *size_z < 16 || *size_z > 512) {
        return false;
    }

    const auto objects_array = SafeMemory::ReadPtr(candidate + offsets::map_data::Objects);
    if (!objects_array || !SafeMemory::IsProbablyUserPointer(*objects_array)) {
        return false;
    }

    const auto group_count = SafeMemory::ReadIl2CppArrayLength(*objects_array, 100000);
    if (!group_count || *group_count <= 0 || *group_count > 10000) {
        return false;
    }

    int valid_groups = 0;
    for (int group_index = 0; group_index < *group_count && group_index < 2048; ++group_index) {
        const auto group_ptr = SafeMemory::ReadPtr(*objects_array + offsets::il2cpp::ArrayItems + static_cast<std::uintptr_t>(group_index) * sizeof(std::uintptr_t));
        if (!group_ptr || !SafeMemory::IsProbablyUserPointer(*group_ptr)) {
            continue;
        }

        const auto sid_ptr = SafeMemory::ReadPtr(*group_ptr + offsets::map_data_objects::Sid);
        const auto ids_ptr = SafeMemory::ReadPtr(*group_ptr + offsets::map_data_objects::Ids);
        const auto nodes_ptr = SafeMemory::ReadPtr(*group_ptr + offsets::map_data_objects::Nodes);
        if (!sid_ptr || !ids_ptr || !nodes_ptr) {
            continue;
        }

        const auto sid = SafeMemory::ReadIl2CppString(*sid_ptr, 128);
        const auto ids_len = SafeMemory::ReadIl2CppArrayLength(*ids_ptr, 65536);
        const auto nodes_len = SafeMemory::ReadIl2CppArrayLength(*nodes_ptr, 65536);
        if (sid && !sid->empty() && ids_len && nodes_len && *ids_len > 0 && *ids_len == *nodes_len) {
            ++valid_groups;
        }
        if (valid_groups >= 3) {
            return true;
        }
    }

    return false;
}

bool ReadOnlyScanner::ValidateDataCandidate(std::uintptr_t data_ptr) const {
    const auto day = SafeMemory::Read<std::int32_t>(data_ptr + offsets::data::Day);
    const auto week = SafeMemory::Read<std::int32_t>(data_ptr + offsets::data::Week);
    const auto month = SafeMemory::Read<std::int32_t>(data_ptr + offsets::data::Month);
    if (!day || !week || !month) {
        return Reject("candidate: cannot read day/week/month");
    }
    if (*day < 1 || *day > 7 || *week < 1 || *week > 12 || *month < 1 || *month > 4) {
        return Reject("candidate: turn values out of bounds");
    }

    const auto sides_ptr = SafeMemory::ReadPtr(data_ptr + offsets::data::Sides);
    if (!sides_ptr || !SafeMemory::IsProbablyUserPointer(*sides_ptr)) {
        return Reject("candidate: Data.sides pointer invalid");
    }

    const auto my_index = SafeMemory::Read<std::int32_t>(*sides_ptr + offsets::data_sides::MyIndex);
    const auto side_array = SafeMemory::ReadPtr(*sides_ptr + offsets::data_sides::SideArray);
    if (!my_index || !side_array || *my_index < 0 || *my_index > 7 || !SafeMemory::IsProbablyUserPointer(*side_array)) {
        return Reject("candidate: DataSides myIndex/sideArray invalid");
    }

    const auto side_count = SafeMemory::Read<std::int32_t>(*side_array + offsets::il2cpp::ArrayLength);
    if (!side_count || *side_count <= 0 || *side_count > 8 || *my_index >= *side_count) {
        return Reject("candidate: side array length invalid");
    }

    const auto state = CaptureFromData(data_ptr);
    if (!state.has_value() || state->pointer_error || !state->live_data) {
        return Reject("candidate: capture failed or partial");
    }

    if (state->faction != "human" &&
        state->faction != "undead" &&
        state->faction != "dungeon" &&
        state->faction != "nature" &&
        state->faction != "demon" &&
        state->faction != "unfrozen") {
        return Reject("candidate: faction SID not recognized");
    }

    const auto& res = state->resources;
    const auto resources_plausible =
        res.gold >= 0 && res.gold < 1000000 &&
        res.wood >= 0 && res.wood < 100000 &&
        res.ore >= 0 && res.ore < 100000 &&
        res.gems >= 0 && res.gems < 100000 &&
        res.crystal >= 0 && res.crystal < 100000 &&
        res.mercury >= 0 && res.mercury < 100000 &&
        res.sulfur >= 0 && res.sulfur < 100000;

    if (!resources_plausible) {
        return Reject("candidate: resource values out of bounds");
    }

    if ((res.gold + res.wood + res.ore + res.gems + res.crystal + res.mercury + res.sulfur) == 0) {
        return Reject("candidate: all resources are zero");
    }

    return true;
}

bool ReadOnlyScanner::Reject(std::string reason) const {
    last_reject_reason_ = std::move(reason);
    debug_.reject_reason = last_reject_reason_;
    if (debug_.status.empty() || debug_.status == "waiting" || debug_.status == "reading") {
        debug_.status = "rejected";
    }
    return false;
}

bool ReadOnlyScanner::ValidateReadableRegion(const MEMORY_BASIC_INFORMATION& mbi) const {
    if (mbi.State != MEM_COMMIT) {
        return false;
    }

    if ((mbi.Protect & PAGE_GUARD) != 0 || (mbi.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }

    const auto protect = mbi.Protect & 0xFF;
    return protect == PAGE_READONLY ||
           protect == PAGE_READWRITE ||
           protect == PAGE_WRITECOPY ||
           protect == PAGE_EXECUTE_READ ||
           protect == PAGE_EXECUTE_READWRITE ||
           protect == PAGE_EXECUTE_WRITECOPY;
}

GameState ReadOnlyScanner::CaptureMockState() const {
    GameState state;
    state.mode = GameMode::Unknown;
    state.scanner_status = data_ptr_ == 0 ? "read-only | " + DataPointerProbe::Status() + " | heap scan fallback active" : "read-only | pointer invalid";
    state.live_data = false;
    state.faction = "Unknown";
    state.hero.name = "Live data not connected";
    state.pointer_error = data_ptr_ != 0;
    state.scanner_debug = debug_;
    state.scanner_debug.source = data_source_;
    state.scanner_debug.game_assembly_base = game_assembly_base_;
    state.scanner_debug.data = data_ptr_;
    state.scanner_debug.map_data = map_data_ptr_;
    if (state.scanner_debug.status == "live") {
        state.scanner_debug.status = "waiting";
    }
    if (state.scanner_debug.reject_reason.empty()) {
        state.scanner_debug.reject_reason = last_reject_reason_;
    }
    return state;
}

std::vector<AobByte> ReadOnlyScanner::ParsePattern(const std::string& pattern) {
    std::istringstream stream(pattern);
    std::string token;
    std::vector<AobByte> bytes;

    while (stream >> token) {
        if (token == "?" || token == "??") {
            bytes.push_back({std::nullopt});
            continue;
        }

        unsigned int value = 0;
        std::istringstream hex(token);
        hex >> std::hex >> value;
        if (!hex.fail() && value <= 0xFF) {
            bytes.push_back({static_cast<std::uint8_t>(value)});
        }
    }

    return bytes;
}

std::optional<std::pair<std::uintptr_t, std::size_t>> ReadOnlyScanner::ModuleRange(HMODULE module) {
    if (module == nullptr) {
        module = GetModuleHandleW(nullptr);
    }

    MODULEINFO info{};
    if (GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)) == FALSE) {
        return std::nullopt;
    }

    return std::make_pair(reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll), static_cast<std::size_t>(info.SizeOfImage));
}

}
