#include "BossObserver.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "BossFrame.h"
#include "BossMath.h"
#include "BossRules.h"
#include "BossSelection.h"
#include "../Burn/BurnField.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 立ち位置の輪に入れる時の、距離の 1 m あたりの数（cm で持つ）と上限。
constexpr float kStanceDistanceScale = 100.0f;
constexpr float kStanceDistanceMax = 65535.0f;

const char* DirectionName(BossRollDirection direction) {
    switch (direction) {
    case BossRollDirection::Away: return "away";
    case BossRollDirection::Toward: return "toward";
    case BossRollDirection::Left: return "left";
    default: return "right";
    }
}

// SeeHabit のログに並べる 1 文字（A＝後ろ、T＝前、L＝左、R＝右）。
char DirectionLetter(BossRollDirection direction) {
    switch (direction) {
    case BossRollDirection::Away: return 'A';
    case BossRollDirection::Toward: return 'T';
    case BossRollDirection::Left: return 'L';
    default: return 'R';
    }
}

// 転がる向きを、プレイヤーからボスへの向きと比べて分ける。前後から外れた向きは、ボスがプレイヤーへ向いているとみなした
// 時のボスの左右（ボスの右＝ボスの前を右へ 90 度回した向き。右手系で前 f の右は (−f.z, 0, f.x)）。
BossRollDirection RollDirectionOf(const Boss& boss, const Player& player, Vec3 playerCenter) {
    const Vec3 toBoss = FlatNormalize(Sub(boss.ground, playerCenter));
    const Vec3 roll = FlatNormalize(PlayerRules::GetActionDirection(player));
    const float along = Dot(roll, toBoss);
    const float axis = std::cos(std::clamp(boss.rollAxisDegrees, 0.0f, 90.0f) * kRadiansPerDegree);
    if (along >= axis)
        return BossRollDirection::Toward;
    if (along <= -axis)
        return BossRollDirection::Away;
    const Vec3 bossForward = Mul(toBoss, -1.0f);
    const Vec3 bossRight{-bossForward.z, 0.0f, bossForward.x};
    return Dot(roll, bossRight) >= 0.0f ? BossRollDirection::Right : BossRollDirection::Left;
}

void LogHabit(const Boss& boss) {
    const BossPlayerWatch& watch = boss.watch;
    char recent[kBossRollHabitCapacity + 1]{};
    const size_t count = watch.rollHabitCount;
    for (size_t i = 0; i < count; ++i) {
        // 古い順に並べる（最後が今のローリング）。
        const size_t back = count - 1 - i;
        recent[i] = DirectionLetter(
            watch.rollHabit[(watch.rollHabitNext + kBossRollHabitCapacity - 1 - back) % kBossRollHabitCapacity]);
    }
    int counts[4]{};
    const int samples = BossObserver::CountRecentRolls(watch, static_cast<int>(kBossRollHabitCapacity), counts);
    char message[224]{};
    std::snprintf(message, sizeof(message), "SeeHabit tick=%llu recent=%s rolls=%d away=%d toward=%d left=%d right=%d",
                  static_cast<unsigned long long>(boss.tick), recent, samples, counts[0], counts[1], counts[2],
                  counts[3]);
    BossLog::Write(message);
}

// ローリングの始まり：転がっていなかった所から転がった、または続けてもう一度転がった（経過 F が戻った）。
void ObserveRoll(Boss& boss, const Player& player, Vec3 playerCenter) {
    BossPlayerWatch& watch = boss.watch;
    const bool rolling = PlayerRules::GetState(player) == PlayerState::Roll;
    if (rolling && (!watch.wasRolling || PlayerRules::GetStateFrame(player) < watch.rollFrame)) {
        watch.rollSeconds = boss.clockSeconds;
        watch.rollDirection = RollDirectionOf(boss, player, playerCenter);
        watch.rollHabit[watch.rollHabitNext] = watch.rollDirection;
        watch.rollHabitNext = (watch.rollHabitNext + 1) % kBossRollHabitCapacity;
        watch.rollHabitCount = std::min(kBossRollHabitCapacity, watch.rollHabitCount + 1);
        char message[160]{};
        std::snprintf(message, sizeof(message), "SeeRoll tick=%llu direction=%s distance=%.3f",
                      static_cast<unsigned long long>(boss.tick), DirectionName(watch.rollDirection),
                      static_cast<double>(FlatLength(Sub(boss.ground, playerCenter))));
        BossLog::Write(message);
        LogHabit(boss);
    }
    watch.wasRolling = rolling;
    watch.rollFrame = PlayerRules::GetStateFrame(player);
}

// 狙いの線（水平）が、ボスの体の軸からどれだけ外れて通るか (m)。後ろや真上・真下へ向いていれば float の最大。
float AimMiss(const Boss& boss, const BossGearSight& gear) {
    const Vec3 toBoss = Sub(boss.ground, gear.aimOrigin);
    const float flat = FlatLength(gear.aimDirection);
    if (flat < 1e-3f)
        return std::numeric_limits<float>::max();
    const Vec3 aim{gear.aimDirection.x / flat, 0.0f, gear.aimDirection.z / flat};
    if (Dot(aim, {toBoss.x, 0.0f, toBoss.z}) <= 0.0f)
        return std::numeric_limits<float>::max();
    return std::abs(aim.x * toBoss.z - aim.z * toBoss.x);
}

// streak は撃った弾（fire）か次に来る弾（windup）が続けて何発目か、interval は前の弾からの F（続けていなければ 0）。
void LogShot(const Boss& boss, const char* sign, int streak, int interval, Vec3 playerCenter) {
    const BossPlayerWatch& watch = boss.watch;
    char message[224]{};
    std::snprintf(message, sizeof(message),
                  "SeeShot tick=%llu sign=%s streak=%d interval=%d aimed=%d miss=%.3f distance=%.3f",
                  static_cast<unsigned long long>(boss.tick), sign, streak, interval, static_cast<int>(watch.shotAimed),
                  static_cast<double>(std::min(watch.shotMiss, 999.0f)),
                  static_cast<double>(FlatLength(Sub(boss.ground, playerCenter))));
    BossLog::Write(message);
}

// 射撃：構えに入った tick と、撃った弾（続けて撃った数・間）。狙いは構えの間と押し続けている間だけ読む。
void ObserveShot(Boss& boss, const BossGearSight& gear, Vec3 playerCenter) {
    BossPlayerWatch& watch = boss.watch;
    const bool pending = gear.shotWindup || gear.shotChaining;
    if (pending) {
        watch.shotMiss = AimMiss(boss, gear);
        watch.shotAimed = watch.shotMiss <= boss.bodyRadius + std::max(0.0f, boss.watchShotAimMargin);
        watch.shotLineSeen = true;
        watch.shotOrigin = gear.aimOrigin;
        watch.shotDirection = gear.aimDirection;
    }
    if (gear.shotsFired > watch.shotsSeen) {
        const uint64_t gap = boss.tick - watch.shotLastTick;
        const bool chained =
            watch.shotStreak > 0 && gap <= static_cast<uint64_t>(std::max(1, boss.watchShotChainFrames));
        watch.shotStreak = chained ? watch.shotStreak + 1 : 1;
        watch.shotInterval = chained ? static_cast<int>(gap) : 0;
        watch.shotLastTick = boss.tick;
        // 押し続けていれば、次の弾を待ち始めた。
        if (gear.shotChaining)
            watch.shotIntentTick = boss.tick;
        LogShot(boss, "fire", watch.shotStreak, watch.shotInterval, playerCenter);
    }
    watch.shotsSeen = gear.shotsFired;
    if (gear.shotWindup && !watch.shotWindup) {
        watch.shotIntentTick = boss.tick;
        const int next = BossObserver::NextShotOrdinal(boss);
        LogShot(boss, "windup", next, next > 1 ? static_cast<int>(boss.tick - watch.shotLastTick) : 0, playerCenter);
    }
    watch.shotWindup = gear.shotWindup;
    watch.shotPending = pending;
}

void PushStance(BossPlayerWatch& watch, float angle, float distance) {
    watch.angle = angle;
    watch.distance = distance;
    watch.stanceAngle[watch.stanceNext] = static_cast<uint8_t>(std::lround(std::clamp(angle, 0.0f, 180.0f)));
    watch.stanceDistance[watch.stanceNext] =
        static_cast<uint16_t>(std::lround(std::clamp(distance * kStanceDistanceScale, 0.0f, kStanceDistanceMax)));
    watch.stanceNext = (watch.stanceNext + 1) % kBossStanceHistory;
    watch.stanceCount = std::min(kBossStanceHistory, watch.stanceCount + 1);
}

// 立ち位置：ボスの正面（いまの向き）からプレイヤーへの水平の角度と距離。
void ObserveStance(Boss& boss, Vec3 playerCenter) {
    const Vec3 offset = Sub(playerCenter, boss.ground);
    const float distance = FlatLength(offset);
    const float angle = AngleBetween(Forward(boss.yaw), FlatNormalize(offset, Forward(boss.yaw))) / kRadiansPerDegree;
    PushStance(boss.watch, angle, distance);
    if (boss.watchLogFrames <= 0 || boss.tick % static_cast<uint64_t>(boss.watchLogFrames) != 0)
        return;
    char message[192]{};
    std::snprintf(message, sizeof(message), "SeeStance tick=%llu angle=%.1f distance=%.3f behind=%.2f",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(angle), static_cast<double>(distance),
                  static_cast<double>(BossObserver::SecondsBehind(boss.watch, 90.0f)));
    BossLog::Write(message);
}

// プレイヤーの残り：ヒョウタンの煙・満杯、精力、HP、残りの回復の数。満杯・回復の数・HP（1 % 刻み）が変わったらログ。
void ObserveRemaining(Boss& boss, const Player& player, const BossGearSight& gear) {
    BossPlayerWatch& watch = boss.watch;
    watch.gourdPercent = gear.hasGourd ? gear.smokePercent : -1.0f;
    watch.gourdFull = gear.hasGourd && gear.gourdFull;
    watch.shotCostPercent = gear.hasGourd && gear.capacity > 0.0f ? gear.shotCost / gear.capacity * 100.0f : 0.0f;
    const float maxStamina = PlayerRules::GetMaxStamina(player);
    const float maxHealth = PlayerRules::GetMaxHealth(player);
    watch.staminaPercent = maxStamina > 0.0f ? PlayerRules::GetStamina(player) / maxStamina * 100.0f : 0.0f;
    watch.healthPercent = maxHealth > 0.0f ? PlayerRules::GetHealth(player) / maxHealth * 100.0f : 0.0f;
    watch.healCharges = PlayerRules::GetHealCharges(player);
    const int key = static_cast<int>(watch.gourdFull) + std::clamp(watch.healCharges, 0, 127) * 2 +
                    static_cast<int>(std::floor(std::clamp(watch.healthPercent, 0.0f, 100.0f))) * 256;
    if (key == watch.loggedRemain)
        return;
    watch.loggedRemain = key;
    char message[224]{};
    std::snprintf(message, sizeof(message), "SeeRemain tick=%llu gourd=%.1f full=%d stamina=%.1f health=%.1f heals=%d",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(watch.gourdPercent),
                  static_cast<int>(watch.gourdFull), static_cast<double>(watch.staminaPercent),
                  static_cast<double>(watch.healthPercent), watch.healCharges);
    BossLog::Write(message);
}

// 新しい方から、条件を満たす立ち位置が続いている tick の数。
template <typename Holds>
float SecondsWhile(const BossPlayerWatch& watch, Holds holds) {
    size_t run = 0;
    for (; run < watch.stanceCount; ++run) {
        const size_t at = (watch.stanceNext + kBossStanceHistory - 1 - run) % kBossStanceHistory;
        if (!holds(at))
            break;
    }
    return static_cast<float>(run) * watch.tickSeconds;
}

} // namespace

void BossObserver::Observe(HIKARI::WorldView& world, Boss& boss, const Player* player,
                           HIKARI::GameProjectFloat3 playerCenter, const BossGearSight& gear, float tickSeconds) {
    BossPlayerWatch& watch = boss.watch;
    const float now = boss.clockSeconds;
    watch.tickSeconds = std::max(1e-4f, tickSeconds);
    watch.burningPercent = BurnFieldAccess::GetBurningFraction(world) * 100.0f;
    watch.present = player != nullptr && PlayerRules::GetState(*player) != PlayerState::Dead;
    if (!watch.present) {
        watch.healing = watch.absorbing = watch.inFire = watch.wasRolling = false;
        watch.shotPending = watch.shotWindup = false;
        PushStance(watch, 0.0f, 0.0f);
        return;
    }
    watch.healing = PlayerRules::GetState(*player) == PlayerState::Heal;
    if (gear.absorbing && !watch.absorbing)
        watch.absorbSince = now;
    watch.absorbing = gear.absorbing;
    const bool inFire = BurnFieldAccess::IsBurning(world, playerCenter);
    if (inFire && !watch.inFire)
        watch.fireSince = now;
    watch.inFire = inFire;
    ObserveRoll(boss, *player, playerCenter);
    ObserveShot(boss, gear, playerCenter);
    ObserveStance(boss, playerCenter);
    ObserveRemaining(boss, *player, gear);
}

void BossObserver::NotePunished(Boss& boss, int recoveryRow, uint64_t hits) {
    if (recoveryRow < 0 || static_cast<size_t>(recoveryRow) >= kBossPunishRows || hits == 0)
        return;
    uint16_t& count = boss.watch.punished[static_cast<size_t>(recoveryRow)];
    count = static_cast<uint16_t>(std::min<uint64_t>(std::numeric_limits<uint16_t>::max(), count + hits));
    const BossMove* move = BossRules::GetMove(boss, recoveryRow);
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "Punished tick=%llu move=%s count=%d hits=%llu stateFrame=%d switchChance=%.1f",
                  static_cast<unsigned long long>(boss.tick), move != nullptr ? move->name.c_str() : "?",
                  static_cast<int>(count), static_cast<unsigned long long>(hits), boss.stateFrame,
                  static_cast<double>(BossSelection::SwitchChance(boss, recoveryRow)));
    BossLog::Write(message);
}

int BossObserver::CountRecentRolls(const BossPlayerWatch& watch, int rolls, int (&counts)[4]) {
    const size_t wanted = std::min(watch.rollHabitCount, static_cast<size_t>(std::max(0, rolls)));
    for (size_t i = 0; i < wanted; ++i) {
        const size_t at = (watch.rollHabitNext + kBossRollHabitCapacity - 1 - i) % kBossRollHabitCapacity;
        ++counts[static_cast<int>(watch.rollHabit[at])];
    }
    return static_cast<int>(wanted);
}

float BossObserver::SecondsBehind(const BossPlayerWatch& watch, float atLeastDegrees) {
    return SecondsWhile(watch, [&](size_t at) { return static_cast<float>(watch.stanceAngle[at]) >= atLeastDegrees; });
}

float BossObserver::SecondsFar(const BossPlayerWatch& watch, float atLeastMeters) {
    const float threshold = atLeastMeters * kStanceDistanceScale;
    return SecondsWhile(watch, [&](size_t at) { return static_cast<float>(watch.stanceDistance[at]) >= threshold; });
}

int BossObserver::NextShotOrdinal(const Boss& boss) {
    const BossPlayerWatch& watch = boss.watch;
    const bool chained = watch.shotStreak > 0 && boss.tick - watch.shotLastTick <=
                                                     static_cast<uint64_t>(std::max(1, boss.watchShotChainFrames));
    return chained ? watch.shotStreak + 1 : 1;
}

} // namespace Project_TD2_2
