#include "FlowRules.h"

#include <algorithm>
#include <cmath>

namespace Project_TD2_2 {
namespace {

bool HasFallen(const Flow& flow, const FlowObservation& observation) {
    return flow.startHeightKnown && !observation.playerRiding &&
           observation.playerHeight < flow.startHeight - flow.fallDepth;
}

bool IsPlayerLost(const Flow& flow, const FlowObservation& observation) {
    return observation.playerPresent && (observation.playerDead || HasFallen(flow, observation));
}

bool IsBossDown(const FlowObservation& observation) {
    return observation.bossPresent && observation.bossMaxHealth > 0.0f && observation.bossHealth <= 0.0f;
}

// 近づいた、または最初の一撃を当てた。
bool IsEngaged(const Flow& flow, const FlowObservation& observation) {
    if (!observation.bossPresent || observation.bossMaxHealth <= 0.0f)
        return false;
    const bool isNear = observation.distanceToBoss >= 0.0f && observation.distanceToBoss <= flow.engageDistance;
    return isNear || observation.bossHealth < observation.bossMaxHealth;
}

void Enter(Flow& flow, FlowPhase phase) {
    flow.phase = phase;
    flow.phaseTicks = 0;
}

} // namespace

const char* FlowRules::Step(Flow& flow, const FlowObservation& observation) {
    ++flow.tick;
    ++flow.phaseTicks;
    // 落下の基準は、プレイヤーが初めて見えた時の高さ。
    if (!flow.startHeightKnown && observation.playerPresent) {
        flow.startHeightKnown = true;
        flow.startHeight = observation.playerHeight;
    }
    switch (flow.phase) {
    case FlowPhase::Waiting:
        if (IsPlayerLost(flow, observation)) {
            Enter(flow, FlowPhase::Lost);
            return observation.playerDead ? "PlayerDead" : "PlayerFell";
        }
        if (IsEngaged(flow, observation)) {
            Enter(flow, FlowPhase::Fighting);
            return observation.bossHealth < observation.bossMaxHealth ? "FirstHit" : "PlayerNear";
        }
        return nullptr;
    case FlowPhase::Fighting:
        if (IsBossDown(observation)) {
            Enter(flow, FlowPhase::Won);
            return "BossDefeated";
        }
        if (IsPlayerLost(flow, observation)) {
            Enter(flow, FlowPhase::Lost);
            return observation.playerDead ? "PlayerDead" : "PlayerFell";
        }
        return nullptr;
    case FlowPhase::Lost:
    case FlowPhase::Won: return nullptr;
    }
    return nullptr;
}

int FlowRules::SecondsToTicks(float seconds, float fixedDt) {
    if (fixedDt <= 0.0f || seconds <= 0.0f)
        return 0;
    return static_cast<int>(std::ceil(seconds / fixedDt));
}

bool FlowRules::IsRestartDue(const Flow& flow, float fixedDt) {
    if (flow.phase == FlowPhase::Lost)
        return flow.phaseTicks >= static_cast<uint64_t>(SecondsToTicks(flow.loseRestartSeconds, fixedDt));
    if (flow.phase == FlowPhase::Won)
        return flow.phaseTicks >= static_cast<uint64_t>(SecondsToTicks(flow.winRestartSeconds, fixedDt));
    return false;
}

float FlowRules::GetFade(const Flow& flow, float fixedDt) {
    const float seconds = flow.phase == FlowPhase::Lost  ? flow.loseFadeSeconds
                          : flow.phase == FlowPhase::Won ? flow.winFadeSeconds
                                                         : -1.0f;
    if (seconds < 0.0f)
        return 0.0f;
    const int ticks = SecondsToTicks(seconds, fixedDt);
    if (ticks <= 0)
        return 1.0f;
    return std::clamp(static_cast<float>(flow.phaseTicks) / static_cast<float>(ticks), 0.0f, 1.0f);
}

} // namespace Project_TD2_2
