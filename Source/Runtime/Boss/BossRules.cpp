#include "BossRules.h"

#include <algorithm>

#include "../Combat/DamageableRules.h"

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

int BossRules::GetPhaseNumber(const BossComponent& boss, const DamageableComponent& health) {
    // 倍精度で比べる（単精度だと 900 / 1500 × 100 が 60 をわずかに超え、ちょうど 60% でフェーズ 2 にならない）。
    const double maximum = DamageableRules::GetMaxHealth(health);
    const double percent = maximum > 0.0 ? DamageableRules::GetHealth(health) / maximum * 100.0 : 0.0;
    return percent <= static_cast<double>(boss.phaseTwoHealthPercent) ? 2 : 1;
}

size_t BossRules::GetFluidDomainDemand(const BossComponent& boss) {
    size_t live = 0;
    for (const auto& effect : boss.slashEffects)
        live += effect.handle.IsValid() ? 1 : 0;
    // 剣を振っている間は、振り抜いた瞬間に出す斬撃の分を先に取っておく（出すフレームに枠が空いているように）。
    return std::min(kBossSlashEffectCapacity, live + (boss.swordSmoke ? 1 : 0));
}

float BossRules::GetStun(const BossComponent& boss) { return boss.stun; }

int BossRules::GetStunCount(const BossComponent& boss) { return boss.stunCount; }

bool BossRules::IsStunned(const BossComponent& boss) { return boss.state == BossState::Stunned; }

HIKARI::GameProjectFloat3 BossRules::GetGroundPosition(const BossComponent& boss) { return boss.ground; }

float BossRules::GetYaw(const BossComponent& boss) { return boss.yaw; }

} // namespace Project_TD2_2
