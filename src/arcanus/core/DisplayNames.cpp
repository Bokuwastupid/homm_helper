#include "arcanus/core/DisplayNames.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arcanus::display {
namespace {

std::string Copy(std::string_view text) {
    return std::string(text.begin(), text.end());
}

std::string LowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool IsDigitText(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

std::vector<std::string> SplitUnderscore(const std::string& value) {
    std::vector<std::string> parts;
    std::string current;
    for (const char c : value) {
        if (c == '_' || c == '-' || c == '.') {
            if (!current.empty()) {
                parts.push_back(std::move(current));
                current.clear();
            }
            continue;
        }
        current.push_back(c);
    }
    if (!current.empty()) {
        parts.push_back(std::move(current));
    }
    return parts;
}

std::string JoinTitle(const std::vector<std::string>& parts) {
    std::ostringstream out;
    int written = 0;
    for (const auto& part : parts) {
        if (part.empty() || IsDigitText(part)) {
            continue;
        }
        if (written > 0) {
            out << ' ';
        }
        std::string word = part;
        word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
        out << word;
        ++written;
    }
    return out.str();
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

std::string StripCommonPrefixes(std::string value) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (const std::string_view prefix : {
                 "resource_", "res_", "magic_scroll_", "magic_", "spell_",
                 "artifact_", "item_", "unit_", "object_", "neutral_",
                 "chest_", "mine_", "map_", "data_"}) {
            if (StartsWith(value, prefix)) {
                value.erase(0, prefix.size());
                changed = true;
                break;
            }
        }
    }
    return value;
}

std::string StripSchoolTierPrefix(std::string value) {
    auto parts = SplitUnderscore(value);
    if (parts.size() >= 3) {
        const auto school = LowerCopy(parts[0]);
        const bool school_prefix =
            school == "night" || school == "nightshade" ||
            school == "day" || school == "daylight" ||
            school == "arcane" || school == "primal" ||
            school == "neutral";
        if (school_prefix && IsDigitText(parts[1])) {
            parts.erase(parts.begin(), parts.begin() + 2);
            std::ostringstream out;
            for (std::size_t i = 0; i < parts.size(); ++i) {
                if (i > 0) {
                    out << '_';
                }
                out << parts[i];
            }
            value = out.str();
        }
    }
    return value;
}

const std::unordered_map<std::string, std::string>& Aliases() {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"resource_gold", "gold"},
        {"resource_wood", "wood"},
        {"resource_ore", "ore"},
        {"resource_crystals", "crystal"},
        {"resource_crystal", "crystal"},
        {"resource_gemstones", "gems"},
        {"resource_gems", "gems"},
        {"resource_mercury", "mercury"},
        {"resource_dust", "sulfur"},
        {"gemstones", "gems"},
        {"crystals", "crystal"},
        {"dust", "sulfur"},

        {"chest", "chest"},
        {"camp_fire", "camp"},
        {"pandora_box", "Pandora"},
        {"scroll_box", "Scroll"},
        {"enchanted_scroll_box", "Scroll+"},
        {"magic_scroll", "Scroll"},
        {"enchanted_magic_scroll", "Scroll+"},

        {"night_2_magic_web", "Web"},
        {"magic_web", "Web"},
        {"web", "Web"},
        {"haste", "Haste"},
        {"healing_water", "Healing"},
        {"inner_light", "Inner Light"},
        {"twilight", "Twilight"},
        {"magic_arrow", "Arrow"},
        {"lightning_bolt", "Bolt"},
        {"ice_bolt", "Ice Bolt"},
        {"fireball", "Fireball"},
        {"chain_lightning", "Chain"},
        {"armageddon", "Armageddon"},
        {"anti_magic", "Anti-Magic"},
        {"black_hole", "Black Hole"},

        {"elf_tracker", "Tracker"},
        {"twinkle", "Twinkle"},
        {"twinkle_upg_alt", "Twinkle+"},
        {"ent", "Ent"},
        {"esquire", "Esquire"},
        {"crossbowman", "Crossbow"},
        {"griffin", "Griffin"},
        {"angel", "Angel"},
        {"archangel", "Archangel"},
        {"apotheosis", "Apotheosis"},

        {"learning_stone", "Stats"},
        {"gladiator_arena", "Arena"},
        {"sacrificial_shrine", "Shrine"},
        {"fickle_shrine", "Shrine"},
        {"insara_eye", "Eye"},
        {"eternal_dragon", "Dragon"},
        {"chimerologist", "Chimerologist"},
        {"random_hire", "Hire"},
        {"unit_upgrade", "Upgrade"},
        {"trade_lab", "Trade"},
        {"unit_res_trade_lab", "Unit Trade"},
        {"item_market", "Market"},
        {"pocket_dimension", "Dimension"},
        {"town_gate", "Gate"},
    };
    return aliases;
}

std::string NormalizeToken(std::string_view token) {
    std::string value = LowerCopy(Copy(token));
    value = StripSchoolTierPrefix(std::move(value));
    value = StripCommonPrefixes(std::move(value));
    value = StripSchoolTierPrefix(std::move(value));
    value = StripCommonPrefixes(std::move(value));
    return value;
}

} // namespace

std::string ResourceName(std::string_view token) {
    const auto normalized = NormalizeToken(token);
    if (normalized == "gemstones" || normalized == "gems") return "gems";
    if (normalized == "crystals" || normalized == "crystal") return "crystal";
    if (normalized == "dust" || normalized == "sulfur") return "sulfur";
    if (normalized == "gold" || normalized == "wood" || normalized == "ore" ||
        normalized == "mercury") {
        return normalized;
    }
    return {};
}

bool IsResourceToken(std::string_view token) {
    return !ResourceName(token).empty();
}

std::string ShortTokenName(std::string_view token, std::size_t max_len) {
    if (token.empty()) {
        return {};
    }

    const auto raw_lower = LowerCopy(Copy(token));
    if (const auto it = Aliases().find(raw_lower); it != Aliases().end()) {
        return Shorten(it->second, max_len);
    }

    const auto normalized = NormalizeToken(token);
    if (const auto it = Aliases().find(normalized); it != Aliases().end()) {
        return Shorten(it->second, max_len);
    }
    if (const auto resource = ResourceName(normalized); !resource.empty()) {
        return Shorten(resource, max_len);
    }

    std::string title = JoinTitle(SplitUnderscore(normalized));
    if (title.empty()) {
        title = Copy(token);
    }
    return Shorten(title, max_len);
}

std::string ShortObjectName(std::string_view kind, std::string_view type, std::size_t max_len) {
    const auto kind_text = LowerCopy(Copy(kind));
    const auto type_text = LowerCopy(Copy(type));

    if (kind_text == "res") {
        if (const auto resource = ResourceName(type); !resource.empty()) {
            return Shorten(resource, max_len);
        }
        return ShortTokenName(type, max_len);
    }
    if (kind_text == "mine") {
        return Shorten(ShortTokenName(type, max_len) + " mine", max_len);
    }
    if (kind_text == "chest") {
        if (type_text.find("pandora") != std::string::npos) return "Pandora";
        if (type_text.find("enchanted") != std::string::npos && type_text.find("scroll") != std::string::npos) return "Scroll+";
        if (type_text.find("scroll") != std::string::npos) return "Scroll";
        if (type_text.find("camp_fire") != std::string::npos) return "camp";
        return "chest";
    }
    if (kind_text == "event" || kind_text == "todo") {
        return type.empty() ? ShortTokenName("encounter", max_len) : ShortTokenName(type, max_len);
    }
    return ShortTokenName(type.empty() ? kind : type, max_len);
}

std::string ShortRewardName(std::string_view token, std::size_t max_len) {
    return ShortTokenName(token, max_len);
}

}
