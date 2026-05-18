#include "arcanus/ai/TacticalAdvisor.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace arcanus {
namespace {

bool FactionUnknown(const std::string& faction) {
    return faction.empty() || faction == "Unknown" || faction == "undefined";
}

struct RouteCandidate {
    const MapObjectState* object = nullptr;
    int distance = 0;
    int score = 0;
    std::string reason;
};

int ObjectBaseScore(const MapObjectState& object, std::string& reason) {
    const auto& type = object.type;
    const auto& kind = object.kind;
    if (kind == "block" || kind == "garrison" || kind == "city") {
        return 0;
    }
    if (kind == "mine" || type.find("res_mine") != std::string::npos || type.find("mine") != std::string::npos) {
        reason = "шахта: долгосрочный доход";
        return 110;
    }
    if (kind == "chest" || type.find("chest") != std::string::npos) {
        reason = "сундук: быстрый опыт/золото";
        return 95;
    }
    if (kind == "res" || type.find("res") != std::string::npos || type.find("gold") != std::string::npos) {
        reason = "ресурс: быстрый прирост экономики";
        return 80;
    }
    if (kind == "item" || type.find("item") != std::string::npos || type.find("artifact") != std::string::npos) {
        reason = "предмет: возможный буст героя";
        return 75;
    }
    if (kind == "hire" || type.find("hire") != std::string::npos) {
        reason = "найм: усиление армии";
        return 70;
    }
    if (type.find("city") != std::string::npos) {
        reason = "город: стратегическая цель";
        return 65;
    }
    reason = "низкий приоритет без распознанного типа";
    return 25;
}

std::vector<RouteCandidate> BuildRouteCandidates(const GameState& state) {
    std::vector<RouteCandidate> candidates;
    candidates.reserve(std::min<std::size_t>(state.visible_objects.size(), 256));

    for (const auto& object : state.visible_objects) {
        if (object.x == state.hero.x && object.y == state.hero.y) {
            continue;
        }

        std::string reason;
        const int base_score = ObjectBaseScore(object, reason);
        if (base_score < 50) {
            continue;
        }

        const int distance = std::abs(object.x - state.hero.x) + std::abs(object.y - state.hero.y);
        if (distance <= 0 || distance > 80) {
            continue;
        }

        candidates.push_back({&object, distance, base_score - distance, reason});
    }

    std::ranges::sort(candidates, [](const RouteCandidate& left, const RouteCandidate& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        return left.distance < right.distance;
    });

    if (candidates.size() > 3) {
        candidates.resize(3);
    }
    return candidates;
}

}

Advice TacticalAdvisor::Evaluate(const GameState& state) const {
    if (!state.live_data) {
        return {
            "LIVE DATA | not connected",
            0.0f,
            {
                {AdvicePriority::Important, "Scanner", "Живое чтение игры пока не подключено. Показаны только статус оверлея и база Core.zip."},
                {AdvicePriority::Info, "Next", "Нужно поймать валидный Data* через hook/cache/heap-scan, затем читать карту и героя."}
            }
        };
    }

    switch (state.mode) {
    case GameMode::Map:
        return EvaluateMap(state);
    case GameMode::Combat:
        return EvaluateCombat(state);
    case GameMode::Town:
        return EvaluateTown(state);
    default:
        return {"LIVE DATA | partial", 0.0f, {{AdvicePriority::Info, "Scanner", "Живые данные частично читаются, но режим игры ещё не определён."}}};
    }
}

Advice TacticalAdvisor::EvaluateMap(const GameState& state) const {
    Advice advice;
    advice.headline = "MAP | live facts";

    if (FactionUnknown(state.faction)) {
        advice.lines.push_back({AdvicePriority::Info, "Faction", "Фракция читается как unknown/undefined. Фракционные законы и контр-советы отключены."});
    } else if (IsGroveLikeFaction(state.faction)) {
        advice.lines.push_back({AdvicePriority::Important, "Woodland Resistance", "Оценки армии Grove/Sylvan могут быть недостоверны."});
    }

    if (state.turn.day == 1) {
        advice.lines.push_back({AdvicePriority::Important, "Новая неделя", "Проверь прирост существ в городе перед рискованными боями."});
    }

    if (state.resources.ore <= 2) {
        advice.lines.push_back({AdvicePriority::Important, "Руда", "Руды почти нет. Не планируй дорогие постройки, пока не захватишь шахту или не подберёшь руду."});
    }

    if (state.resources.wood <= 2) {
        advice.lines.push_back({AdvicePriority::Important, "Дерево", "Дерева почти нет. Стройка может упереться в базовые здания и апгрейды."});
    }

    if (state.resources.gold < 1500) {
        advice.lines.push_back({AdvicePriority::Recommend, "Золото", "Золота мало: приоритет золото/сундуки/доход, найм только ключевых стеков."});
    } else if (state.resources.gold >= 5000 && (state.resources.ore <= 2 || state.resources.wood <= 2)) {
        advice.lines.push_back({AdvicePriority::Recommend, "Экономика", "Золото есть, но стройка ограничена базовыми ресурсами. Приоритет: руда/дерево."});
    }

    if (state.visible_objects.empty()) {
        advice.lines.push_back({AdvicePriority::Info, "Маршрут", "Объекты карты ещё не читаются. Маршруты отключены."});
    } else {
        const auto candidates = BuildRouteCandidates(state);
        if (candidates.empty()) {
            advice.lines.push_back({AdvicePriority::Info, "Маршрут", "Кандидаты есть в debug all-map, но рядом нет объектов с распознанным приоритетом. Автомаршрут не предлагаю."});
        } else {
            int index = 1;
            for (const auto& candidate : candidates) {
                std::ostringstream body;
                body << candidate.object->type
                     << " [" << candidate.object->x << "," << candidate.object->y << "]"
                     << " | dist=" << candidate.distance
                     << " | score=" << candidate.score
                     << " | " << candidate.reason
                     << " | debug all-map, без pathfinding.";
                advice.lines.push_back({
                    index == 1 ? AdvicePriority::Recommend : AdvicePriority::Info,
                    index == 1 ? "Маршрут A" : (index == 2 ? "Маршрут B" : "Маршрут C"),
                    body.str()
                });
                ++index;
            }
        }
    }

    if (advice.lines.empty()) {
        advice.lines.push_back({AdvicePriority::Info, "Статус", "Критичных предупреждений по прочитанным данным нет."});
    }

    return advice;
}

Advice TacticalAdvisor::EvaluateCombat(const GameState& state) const {
    Advice advice;
    advice.headline = "COMBAT | partial";
    advice.win_probability = state.combat.has_value() ? std::clamp(EstimateArmyHealthRatio(state) * 100.0f, 5.0f, 95.0f) : 0.0f;

    if (!state.combat.has_value()) {
        advice.lines.push_back({AdvicePriority::Info, "Combat", "Боевые стеки ещё не читаются. Тактические цели и шанс победы отключены."});
        return advice;
    }

    if (!state.combat->initiative_queue.empty()) {
        advice.lines.push_back({AdvicePriority::Important, "Ход", state.combat->initiative_queue.front()});
    }

    advice.lines.push_back({AdvicePriority::Info, "Combat", "Нужны HP, координаты и эффекты стеков для нормального расчёта боя."});
    return advice;
}

Advice TacticalAdvisor::EvaluateTown(const GameState& state) const {
    Advice advice;
    advice.headline = "TOWN | partial";
    advice.lines.push_back({AdvicePriority::Important, "Законы", "Проверь доступные Faction Laws перед выходом из города."});

    if (state.resources.ore <= 2 || state.resources.wood <= 2) {
        advice.lines.push_back({AdvicePriority::Recommend, "Стройка", "Не начинай дорогую цепочку зданий, пока не закроешь дефицит дерева/руды."});
    } else if (state.resources.gold >= 2500) {
        advice.lines.push_back({AdvicePriority::Recommend, "Стройка", "Ресурсы позволяют строить: приоритет доход или открытие ключевого тира существ."});
    } else {
        advice.lines.push_back({AdvicePriority::Recommend, "Найм", "Золото ограничено: покупай ключевой урон, слабые стеки можно отложить."});
    }

    return advice;
}

float TacticalAdvisor::EstimateArmyHealthRatio(const GameState& state) {
    int current = 0;
    int maximum = 0;
    for (const auto& stack : state.army) {
        current += std::max(0, stack.hp_current);
        maximum += std::max(0, stack.hp_max);
    }
    if (maximum == 0) {
        return 1.0f;
    }
    return static_cast<float>(current) / static_cast<float>(maximum);
}

std::string ToString(AdvicePriority priority) {
    switch (priority) {
    case AdvicePriority::Critical: return "CRITICAL";
    case AdvicePriority::Important: return "IMPORTANT";
    case AdvicePriority::Recommend: return "RECOMMEND";
    default: return "INFO";
    }
}

}
