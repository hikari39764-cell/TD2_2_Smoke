#include "BossSelection.h"

#include <algorithm>
#include <cmath>

#include "BossFrame.h"
#include "BossKasumiStep.h"
#include "BossRules.h"

namespace Project_TD2_2 {
namespace {

// 突進系：待機・移動中に撃たれるほど出やすくなる型。
bool IsRushType(BossMoveType type) { return type == BossMoveType::Dash || type == BossMoveType::Ambush; }

// 得意な距離（距離重み。0＝近い、1＝遠い）がいまの距離に近いほど大きい値。距離重みが空欄の行は 0（近い）として扱う。
float DistanceFit(const BossComponent& boss, const BossMove& move, const BossSituation& situation, float power) {
    const float reach = std::clamp(situation.distance / std::max(0.01f, boss.selectFarDistance), 0.0f, 1.0f);
    const float favored = std::max(0.0f, move.distanceWeight);
    return std::pow(std::max(boss.selectMinWeight, 1.0f - std::abs(favored - reach)), power);
}

bool IsInterruptCandidate(const BossComponent& boss, int row, const BossSituation& situation) {
    const BossMove* move = BossRules::GetMove(boss, row);
    return move != nullptr && move->when.pressure && BossSelection::CanPerform(boss, row, situation);
}

// ふつうのコンボでつなげる行：割り込み専用・特別・連続攻撃が条件の行は出さない。
bool IsComboCandidate(const BossComponent& boss, int row, const BossSituation& situation) {
    const BossMove* move = BossRules::GetMove(boss, row);
    return move != nullptr && move->use != BossMoveUse::Interrupt && move->use != BossMoveUse::Special &&
           !move->when.pressure && BossSelection::CanPerform(boss, row, situation);
}

bool HasComboTarget(const BossMove& move, int row) {
    return std::find(move.comboTargets.begin(), move.comboTargets.end(), row) != move.comboTargets.end();
}

} // namespace

bool BossSelection::CanPerform(const BossComponent& boss, int row, const BossSituation& situation) {
    const BossMove* move = BossRules::GetMove(boss, row);
    // 引爆は爆ぜさせる火が無ければ出せない（木の枝はここで断られて失敗し、次の枝へ進む）。霞歩は、かわさない間
    // （息が切れている）は出さない（Boss.kasumiStepRestWhileSpent）。
    return move != nullptr && move->executable && situation.distance >= 0.0f &&
           BossMoveConditionHolds(move->when, situation) &&
           (move->type != BossMoveType::Detonate || boss.watch.burningPercent > 0.0f) &&
           BossKasumiStep::Allows(boss, *move);
}

float BossSelection::NeutralWeight(const BossComponent& boss, int row, const BossSituation& situation) {
    const BossMove* move = BossRules::GetMove(boss, row);
    if (move == nullptr || move->use != BossMoveUse::Start || !CanPerform(boss, row, situation))
        return 0.0f;
    float weight = DistanceFit(boss, *move, situation, boss.selectPower);
    if (boss.brain.lastStarted == row)
        weight *= boss.selectRepeatScale;
    if (IsRushType(move->type) && boss.brain.moveHits > 0)
        weight *= 1.0f + static_cast<float>(std::min(boss.brain.moveHits, boss.moveHitMax)) * boss.moveHitBoost;
    return weight;
}

float BossSelection::InterruptWeight(const BossComponent& boss, int row, const BossSituation& situation,
                                     bool recovering) {
    if (!IsInterruptCandidate(boss, row, situation))
        return 0.0f;
    if (recovering) {
        // 後隙の間は、いまの技のコンボ先にある候補（表の上の行から）を先に出す。
        if (const BossMove* current = BossRules::GetMove(boss, BossRules::GetCurrentMove(boss))) {
            for (int other = 0; other < BossRules::GetMoveCount(boss); ++other) {
                if (HasComboTarget(*current, other) && IsInterruptCandidate(boss, other, situation))
                    return other == row ? 1.0f : 0.0f;
            }
        }
    }
    return DistanceFit(boss, *BossRules::GetMove(boss, row), situation, boss.interruptPower);
}

int BossSelection::PickCombo(const BossComponent& boss, const BossSituation& situation, float roll, float pick) {
    const BossMove* current = BossRules::GetMove(boss, BossRules::GetCurrentMove(boss));
    if (current == nullptr || BossRules::GetChain(boss) + 1 >= boss.comboMaxChain)
        return -1;
    // 候補ごとの重み（表の「名前:重み」。書いていなければ 1）。
    const auto weightOf = [&](size_t k) {
        return k < current->comboWeights.size() ? std::max(0.0f, current->comboWeights[k]) : 1.0f;
    };
    float total = 0.0f;
    int last = -1;
    for (size_t k = 0; k < current->comboTargets.size(); ++k) {
        if (IsComboCandidate(boss, current->comboTargets[k], situation) && weightOf(k) > 0.0f) {
            total += weightOf(k);
            last = current->comboTargets[k];
        }
    }
    if (last < 0)
        return -1;
    const float chance = current->comboChance >= 0.0f ? current->comboChance : boss.comboChancePercent;
    if (!(roll * 100.0f < chance))
        return -1;
    // 候補が何行もあれば、重みに比例した確率で 1 つ（重みが全部 1 なら同じ確率）。
    float left = std::clamp(pick, 0.0f, 1.0f) * total;
    for (size_t k = 0; k < current->comboTargets.size(); ++k) {
        if (!IsComboCandidate(boss, current->comboTargets[k], situation) || !(weightOf(k) > 0.0f))
            continue;
        left -= weightOf(k);
        if (left < 0.0f)
            return current->comboTargets[k];
    }
    return last;
}

bool BossSelection::IsComboOfCurrent(const BossComponent& boss, int row) {
    const BossMove* current = BossRules::GetMove(boss, BossRules::GetCurrentMove(boss));
    return current != nullptr && HasComboTarget(*current, row);
}

bool BossSelection::IsPressured(const BossComponent& boss, int hits, float seconds) {
    const BossBrainMemory& memory = boss.brain;
    if (boss.clockSeconds < memory.pressureReadySeconds)
        return false;
    int recent = 0;
    for (size_t i = 0; i < memory.hitCount; ++i)
        recent += boss.clockSeconds - memory.hitSeconds[i] <= seconds ? 1 : 0;
    return recent >= std::max(1, hits);
}

void BossSelection::UsePressure(BossComponent& boss, float cooldownSeconds) {
    boss.brain.hitCount = 0;
    boss.brain.hitNext = 0;
    boss.brain.pressureReadySeconds = boss.clockSeconds + std::max(0.0f, cooldownSeconds);
}

uint64_t BossSelection::NoteHits(BossComponent& boss, uint64_t totalHits) {
    BossBrainMemory& memory = boss.brain;
    const uint64_t fresh = totalHits > memory.seenHits ? totalHits - memory.seenHits : 0;
    memory.seenHits = totalHits;
    if (fresh == 0)
        return 0;
    for (uint64_t n = 0; n < std::min<uint64_t>(fresh, kBossHitMemory); ++n) {
        memory.hitSeconds[memory.hitNext] = boss.clockSeconds;
        memory.hitNext = (memory.hitNext + 1) % kBossHitMemory;
        memory.hitCount = std::min(kBossHitMemory, memory.hitCount + 1);
    }
    // 待機中・移動の技の間に撃たれた分は、突進系を出やすくする。
    const BossMove* current = BossRules::GetMove(boss, BossRules::GetCurrentMove(boss));
    if (BossRules::GetState(boss) != BossState::Idle &&
        (current == nullptr || !BossMoveTable::IsMoveType(current->type)))
        return fresh;
    memory.moveHits += static_cast<int>(std::min<uint64_t>(fresh, kBossHitMemory));
    char message[128]{};
    std::snprintf(message, sizeof(message), "MoveHit tick=%llu moveHits=%d", static_cast<unsigned long long>(boss.tick),
                  memory.moveHits);
    BossLog::Write(message);
    return fresh;
}

void BossSelection::NoteStarted(BossComponent& boss, int row, int chain) {
    const BossMove* move = BossRules::GetMove(boss, row);
    if (move == nullptr)
        return;
    if (chain == 0 && move->use == BossMoveUse::Start)
        boss.brain.lastStarted = row;
    // 反応の枝が頼んだ技か（コンボの続きは前の技のまま）。
    if (chain == 0) {
        boss.reactionMoveKey = boss.pendingReactionKey;
        boss.pendingReactionKey = 0;
    }
    if (IsRushType(move->type))
        boss.brain.moveHits = 0;
    // 分身を撃たれた後の本体の突進（瞬間突進の型）が始まった。
    if (move->type == BossMoveType::Dash)
        boss.brain.cloneStruck = false;
}

float BossSelection::SwitchChance(const BossComponent& boss, int row) {
    const BossMove* move = BossRules::GetMove(boss, row);
    if (move == nullptr)
        return 0.0f;
    if (!boss.punishSwitchEnabled || !(move->switchChance > 0.0f) || static_cast<size_t>(row) >= kBossPunishRows)
        return move->switchChance;
    const int punished = std::min(static_cast<int>(boss.watch.punished[static_cast<size_t>(row)]),
                                  std::max(0, boss.punishSwitchMaxHits));
    return std::min(100.0f, move->switchChance *
                                (1.0f + std::max(0.0f, boss.punishSwitchPerHit) * static_cast<float>(punished)));
}

} // namespace Project_TD2_2
