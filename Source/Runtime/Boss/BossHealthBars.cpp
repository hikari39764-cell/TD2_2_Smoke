#include "BossHealthBars.h"

#include <algorithm>
#include <cstdio>

#include "BossMoveExecutor.h"
#include "BossRules.h"
#include "../Combat/DamageableRules.h"

namespace Project_TD2_2 {
namespace {

// 条の値の下限（0 にすると最大 HP が 0 になり、Flow が撃破と読む）。
constexpr float kMinBarHealth = 1.0f;

// 条の値。項目が 0 の時は、シーンで決めたボス全体の HP（Damageable の最大 HP）を割合で分ける。
float BarHealth(const BossComponent& boss, int phase) {
    const float set = phase >= 2 ? boss.phaseTwoHealth : boss.phaseOneHealth;
    if (set > 0.0f)
        return std::max(kMinBarHealth, set);
    const float share = std::clamp(boss.phaseOneShare, 0.05f, 0.95f);
    return std::max(kMinBarHealth, boss.authoredHealth * (phase >= 2 ? 1.0f - share : share));
}

} // namespace

void BossHealthBars::Prepare(BossComponent& boss, DamageableComponent& health) {
    if (boss.healthBarsReady)
        return;
    boss.healthBarsReady = true;
    boss.authoredHealth = health.maxHealth;
    health.maxHealth = BarHealth(boss, 1);
    DamageableRules::Initialize(health);
    // 先に初期化されていた（ボスより先に当たった）時も、1 本目の値を超えないようにする。
    health.currentHealth = std::min(health.currentHealth, health.maxHealth);
    boss.lastPhaseNumber = 1;
    boss.phaseShiftLeft = 0;
    boss.phaseStartSeconds = boss.clockSeconds;
    char message[160]{};
    std::snprintf(message, sizeof(message), "HealthBars tick=%llu first=%.3f second=%.3f shiftFrames=%d",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(BarHealth(boss, 1)),
                  static_cast<double>(BarHealth(boss, 2)), boss.phaseShiftFrames);
    BossLog::Write(message);
}

void BossHealthBars::Step(BossComponent& boss) {
    if (boss.phaseShiftLeft <= 0 || --boss.phaseShiftLeft > 0)
        return;
    char message[128]{};
    std::snprintf(message, sizeof(message), "PhaseShiftEnd tick=%llu phase=%d state=%d",
                  static_cast<unsigned long long>(boss.tick), boss.lastPhaseNumber, static_cast<int>(boss.state));
    BossLog::Write(message);
}

bool BossHealthBars::CheckEmptied(BossComponent& boss, DamageableComponent& health, double rawDamage) {
    if (boss.lastPhaseNumber >= 2 || DamageableRules::IsAlive(health) || boss.state == BossState::Dead)
        return false;
    // いまの技を打ち切って待機へ（気絶の入り方と同じ打ち切り。霞の大剣は消え、天降り斬り・跳躍の体はその場で地面へ
    // 戻り、炎走りのまだ放っていない前線は放たない）。変わり目の間は待機のまま技を頼まない（StepState）。
    // 場に残っている物の扱い：
    // - 飛んでいる剣気（斬撃・ナイフ・霞の輪）はそのまま飛ばす。放った後の物を消すと、当たる直前の刃が急に消えて見え、
    //   変わり目の演出（c-8dd296）が何を見せるかも決まっていないので、いまは触らない。
    // - 場の火はそのまま。フェーズ 1 は火を残さない（技の表の @フェーズ2）ので、1 本目の間に燃えている火は無い。
    // - 霞分身は消す（BossClones::Step が変わり目の間に霞にして消す）。分身は「HP60%以下」の技なのでフェーズ 1 には
    //   出ないが、出ていれば、変わり目で無敵の本体の代わりに撃たれる的が残るのを避ける。
    BossMoveExecutor::Interrupt(boss);
    BossRules::CancelRequest(boss);
    BossMoveExecutor::EnterIdle(boss);
    boss.brain.cloneStruck = false;
    // 気絶値は 0 に戻す（変わり目の後、溜まっていた分でいきなりひざまずかない）。
    boss.stun = 0.0f;
    boss.stunPending = false;
    const float before = boss.shownHealth;
    boss.lastPhaseNumber = 2;
    boss.phaseShiftLeft = std::max(1, boss.phaseShiftFrames);
    boss.phaseStartSeconds = boss.clockSeconds;
    // 2 本目を満たす。余ったダメージは持ち越さない（Damageable は 0 で止めているので、そのまま満たせばよい）。
    health.maxHealth = BarHealth(boss, 2);
    health.currentHealth = health.maxHealth;
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "PhaseChange tick=%llu phase=2 before=%.3f damage=%.3f lastHit=%.3f health=%.3f max=%.3f "
                  "shiftFrames=%d",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(before), rawDamage,
                  static_cast<double>(DamageableRules::GetLastDamage(health)),
                  static_cast<double>(DamageableRules::GetHealth(health)),
                  static_cast<double>(DamageableRules::GetMaxHealth(health)), boss.phaseShiftLeft);
    BossLog::Write(message);
    return true;
}

double BossHealthBars::TotalPercent(const BossComponent& boss, const DamageableComponent& health) {
    const double first = BarHealth(boss, 1);
    const double second = BarHealth(boss, 2);
    const double left =
        static_cast<double>(DamageableRules::GetHealth(health)) + (boss.lastPhaseNumber >= 2 ? 0.0 : second);
    // 先に 100 を掛ける（既定の 4 : 6 なら 2 本目が満ちた時がちょうど 60 % になり、「HP60%以下」が 2 本目から成り立つ）。
    return left * 100.0 / (first + second);
}

void BossHealthBars::Publish(BossComponent& boss, const DamageableComponent& health) {
    boss.shownHealth = DamageableRules::GetHealth(health);
    boss.shownMaxHealth = DamageableRules::GetMaxHealth(health);
    boss.shownPhase = boss.lastPhaseNumber;
    boss.shownPhaseSeconds = std::max(0.0f, boss.clockSeconds - boss.phaseStartSeconds);
}

} // namespace Project_TD2_2
