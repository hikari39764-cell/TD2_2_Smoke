#include "BossRules.h"

#include <algorithm>

namespace Project_TD2_2 {

bool BossRules::RequestMove(BossComponent& boss, int index, bool asCombo) {
    const BossMove* move = GetMove(boss, index);
    if (move == nullptr || !move->executable || boss.state == BossState::Dead)
        return false;
    boss.requestedMove = index;
    boss.requestedAsCombo = asCombo;
    return true;
}

bool BossRules::RequestMoveByName(BossComponent& boss, std::string_view name, bool asCombo) {
    return RequestMove(boss, FindMove(boss, name), asCombo);
}

void BossRules::CancelRequest(BossComponent& boss) {
    boss.requestedMove = -1;
    boss.requestedAsCombo = false;
}

void BossRules::SetHeld(BossComponent& boss, bool held) { boss.held = held; }

int BossRules::FindMove(const BossComponent& boss, std::string_view name) { return boss.table.Find(name); }

int BossRules::GetMoveCount(const BossComponent& boss) { return static_cast<int>(boss.table.GetMoves().size()); }

const BossMove* BossRules::GetMove(const BossComponent& boss, int index) {
    const auto& moves = boss.table.GetMoves();
    return index >= 0 && static_cast<size_t>(index) < moves.size() ? &moves[static_cast<size_t>(index)] : nullptr;
}

BossState BossRules::GetState(const BossComponent& boss) { return boss.state; }

bool BossRules::IsIdle(const BossComponent& boss) { return boss.state == BossState::Idle && boss.requestedMove < 0; }

int BossRules::GetCurrentMove(const BossComponent& boss) { return boss.state == BossState::Act ? boss.move : -1; }

BossActPhase BossRules::GetActPhase(const BossComponent& boss) { return boss.phase; }

int BossRules::GetChain(const BossComponent& boss) { return boss.chain; }

int BossRules::GetPhaseNumber(const BossComponent& boss) { return boss.lastPhaseNumber; }

bool BossRules::IsPhaseShifting(const BossComponent& boss) { return boss.phaseShiftLeft > 0; }

size_t BossRules::GetFluidDomainDemand(const BossComponent& boss) {
    size_t live = boss.ringVfx.IsValid() ? 1 : 0;
    for (const auto& effect : boss.slashEffects)
        live += effect.handle.IsValid() ? 1 : 0;
    // 剣を振っている間は、振り抜いた瞬間に出す斬撃の分を先に取っておく（出すフレームに枠が空いているように）。
    // 回し蹴りも、炎の輪を出す前（前隙・攻撃中）から先に取っておく。
    const BossMove* move = GetMove(boss, boss.move);
    const bool kicking =
        boss.state == BossState::Act && move != nullptr && move->type == BossMoveType::Kick && !boss.ringVfx.IsValid();
    // 胴の煙は出し続ける域なので、斬撃の枠の上限とは別に数える。
    return std::min(kBossSlashEffectCapacity + 1, live + (boss.swordSmoke ? 1 : 0) + (kicking ? 1 : 0)) +
           (boss.bodyState.smokeVfx.IsValid() ? 1 : 0);
}

bool BossRules::IsBladeLit(const BossComponent& boss) {
    const bool acting = boss.state == BossState::Act;
    return (boss.bladeLit && acting) || (boss.lastPhaseNumber >= 2 && (acting || boss.state == BossState::Idle) &&
                                         !boss.hidden && !boss.shiftState.lookPending);
}

float BossRules::GetStun(const BossComponent& boss) { return boss.stun; }

float BossRules::GetStunThreshold(const BossComponent& boss) {
    return boss.stunMax * (1.0f + std::max(0.0f, boss.stunMaxGrowth) * static_cast<float>(boss.stunCount));
}

int BossRules::GetStunCount(const BossComponent& boss) { return boss.stunCount; }

bool BossRules::IsStunned(const BossComponent& boss) { return boss.state == BossState::Stunned; }

HIKARI::GameProjectFloat3 BossRules::GetGroundPosition(const BossComponent& boss) { return boss.ground; }

float BossRules::GetYaw(const BossComponent& boss) { return boss.yaw; }

} // namespace Project_TD2_2
