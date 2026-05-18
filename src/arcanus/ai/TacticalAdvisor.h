#pragma once

#include "arcanus/core/GameState.h"

#include <string>
#include <vector>

namespace arcanus {

enum class AdvicePriority {
    Info,
    Recommend,
    Important,
    Critical
};

struct AdviceLine {
    AdvicePriority priority = AdvicePriority::Info;
    std::string title;
    std::string body;
};

struct Advice {
    std::string headline;
    float win_probability = 0.0f;
    std::vector<AdviceLine> lines;
};

class TacticalAdvisor {
public:
    Advice Evaluate(const GameState& state) const;

private:
    Advice EvaluateMap(const GameState& state) const;
    Advice EvaluateCombat(const GameState& state) const;
    Advice EvaluateTown(const GameState& state) const;
    static float EstimateArmyHealthRatio(const GameState& state);
};

std::string ToString(AdvicePriority priority);

}

