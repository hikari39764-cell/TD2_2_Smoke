#include "BossRules.h"

#include <algorithm>

namespace Project_TD2_2 {

bool BossRules::RequestMove(Boss& boss, int index, bool asCombo) {
    const BossMove* move = GetMove(boss, index);
    if (move == nullptr || !move->executable || boss.state == BossState::Dead)
        return false;
    boss.requestedMove = index;
    boss.requestedAsCombo = asCombo;
    return true;
}

bool BossRules::RequestMoveByName(Boss& boss, std::string_view name, bool asCombo) {
    return RequestMove(boss, FindMove(boss, name), asCombo);
}

void BossRules::CancelRequest(Boss& boss) {
    boss.requestedMove = -1;
    boss.requestedAsCombo = false;
}

void BossRules::SetHeld(Boss& boss, bool held) { boss.held = held; }

int BossRules::FindMove(const Boss& boss, std::string_view name) { return boss.table.Find(name); }

int BossRules::GetMoveCount(const Boss& boss) { return static_cast<int>(boss.table.GetMoves().size()); }

const BossMove* BossRules::GetMove(const Boss& boss, int index) {
    const auto& moves = boss.table.GetMoves();
    return index >= 0 && static_cast<size_t>(index) < moves.size() ? &moves[static_cast<size_t>(index)] : nullptr;
}

BossState BossRules::GetState(const Boss& boss) { return boss.state; }

float BossRules::GetShownHealth(const Boss& boss) { return boss.shownHealth; }

float BossRules::GetShownMaxHealth(const Boss& boss) { return boss.shownMaxHealth; }

int BossRules::GetCurrentMove(const Boss& boss) { return boss.state == BossState::Act ? boss.move : -1; }

int BossRules::GetChain(const Boss& boss) { return boss.chain; }

bool BossRules::IsPhaseShifting(const Boss& boss) { return boss.phaseShiftLeft > 0; }

bool BossRules::IsBladeLit(const Boss& boss) {
    const bool acting = boss.state == BossState::Act;
    return (boss.bladeLit && acting) ||
           (boss.lastPhaseNumber >= 2 && (acting || boss.state == BossState::Idle) && !boss.phaseTwoLookPending);
}

float BossRules::GetStunThreshold(const Boss& boss) {
    return boss.stunMax * (1.0f + std::max(0.0f, boss.stunMaxGrowth) * static_cast<float>(boss.stunCount));
}

} // namespace Project_TD2_2
