#include "BossGreatsword.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossBurnMarks.h"
#include "BossMath.h"
#include "BossWaves.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

const char* PhaseName(BossGreatswordPhase phase) {
    switch (phase) {
    case BossGreatswordPhase::Conjure: return "conjure";
    case BossGreatswordPhase::Sweep: return "sweep";
    case BossGreatswordPhase::Dissolve: return "dissolve";
    default: return "none";
    }
}

float Share(int frame, int frames) {
    return std::clamp(static_cast<float>(frame) / static_cast<float>(std::max(1, frames)), 0.0f, 1.0f);
}

// 剣の向きの、ボスの向きから見た水平の角度（度。右が負）と、水平からの仰ぎ（度）。
float AngleOf(const BossComponent& boss) {
    return WrapPi(YawOf(boss.greatsword.direction) - boss.yaw) / kRadiansPerDegree;
}
float ElevationOf(const BossComponent& boss) {
    return std::asin(std::clamp(boss.greatsword.direction.y, -1.0f, 1.0f)) / kRadiansPerDegree;
}

void LogShape(const BossComponent& boss, const char* event, const char* reason = nullptr) {
    const BossGreatswordState& sword = boss.greatsword;
    char message[288]{};
    std::snprintf(message, sizeof(message),
                  "%s tick=%llu phase=%s fill=%.3f wind=%.3f sweep=%.3f dissolve=%.3f angle=%.1f elev=%.1f "
                  "hilt=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f,%.3f)%s%s",
                  event, static_cast<unsigned long long>(boss.tick), PhaseName(sword.phase),
                  static_cast<double>(sword.fill), static_cast<double>(sword.wind), static_cast<double>(sword.sweep),
                  static_cast<double>(sword.dissolve), static_cast<double>(AngleOf(boss)),
                  static_cast<double>(ElevationOf(boss)), static_cast<double>(sword.hilt.x),
                  static_cast<double>(sword.hilt.y), static_cast<double>(sword.hilt.z),
                  static_cast<double>(sword.direction.x), static_cast<double>(sword.direction.y),
                  static_cast<double>(sword.direction.z), reason ? " reason=" : "", reason ? reason : "");
    BossLog::Write(message);
}

void SetPhase(BossComponent& boss, BossGreatswordPhase phase) {
    if (boss.greatsword.phase == phase)
        return;
    boss.greatsword.phase = phase;
    LogShape(boss, "Greatsword");
}

// 確かめ用の項目 traceEveryFrames が入っている時だけ、毎 tick の形を出す（満ちる曲線・薙ぐ角度の並び）。
void Trace(const BossComponent& boss) {
    if (boss.traceEveryFrames > 0 && boss.tick % static_cast<uint64_t>(boss.traceEveryFrames) == 0)
        LogShape(boss, "GreatswordTrace");
}

// ボスの中心（足元の真上。高さは足元から height m）。
Vec3 Center(const BossComponent& boss, float height) {
    return {boss.ground.x, boss.ground.y + BossPose::RootHeight(boss) + height, boss.ground.z};
}

// 作り出す間：右肩の上（肩の横・前後の位置、足元から greatswordRaiseHeight m）に柄を置き、真上から後ろへ少し倒して立てる。
void RaisePose(const BossComponent& boss, Vec3& hilt, Vec3& direction) {
    const Vec3 shoulder{boss.swordShoulder.x, boss.greatswordRaiseHeight, boss.swordShoulder.z};
    hilt = Add(Center(boss, 0.0f), RotateY(shoulder, boss.yaw));
    const float tilt = boss.greatswordRaiseTilt * kRadiansPerDegree;
    direction = Normalize(Add(Vec3{0.0f, std::cos(tilt), 0.0f}, Mul(Forward(boss.yaw), -std::sin(tilt))));
}

// 薙ぐ間：剣は水平。基準の向き yaw から angle 度（右が負）の向きに、ボスの中心から greatswordHiltOffset m の所に柄。
void SweepPose(const BossComponent& boss, float yaw, float angle, Vec3& hilt, Vec3& direction) {
    direction = Forward(yaw + angle * kRadiansPerDegree);
    hilt = Add(Center(boss, boss.greatswordSweepHeight), Mul(direction, boss.greatswordHiltOffset));
}

float StartAngle(const BossComponent& boss) { return -boss.greatswordSweepDegrees * 0.5f; }

void LogHit(const BossFrame& frame, float playerDistance, float playerAngle) {
    const BossComponent& boss = frame.boss;
    const BossGreatswordState& sword = boss.greatsword;
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "GreatswordHit tick=%llu hit=%d dodged=%d caught=%d playerDistance=%.3f playerAngle=%.1f "
                  "bladeAngle=%.1f from=%.1f",
                  static_cast<unsigned long long>(boss.tick), static_cast<int>(sword.hit),
                  static_cast<int>(sword.dodged), static_cast<int>(sword.caught), static_cast<double>(playerDistance),
                  static_cast<double>(playerAngle), static_cast<double>(sword.angle),
                  static_cast<double>(sword.previousAngle));
    BossLog::Write(message);
}

// 薙ぎの当たり：この tick に剣が通った角度（1 tick 前の角度〜いまの角度。両側へゆとり）の中にプレイヤーがいて、
// ボスの中心からの距離が懐〜剣先（＋プレイヤーの体の半径）なら、1 回の薙ぎで 1 回だけ当てる。
// 無敵でよけた時も、その薙ぎはもう当たらない（斬撃と同じ）。
void StepHit(BossFrame& frame, const BossMove& move, bool last) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    if (sword.caught || frame.player == nullptr)
        return;
    const Vec3 offset{frame.playerCenter.x - boss.ground.x, 0.0f, frame.playerCenter.z - boss.ground.z};
    const float distance = FlatLength(offset);
    const float playerAngle = WrapPi(YawOf(offset) - sword.yaw) / kRadiansPerDegree;
    const float reach = boss.greatswordHiltOffset + boss.greatswordLength + boss.playerHitRadius;
    const float low = std::min(sword.previousAngle, sword.angle) - boss.greatswordHitMarginDegrees;
    const float high = std::max(sword.previousAngle, sword.angle) + boss.greatswordHitMarginDegrees;
    const bool inside =
        distance >= boss.greatswordInnerRadius && distance <= reach && playerAngle >= low && playerAngle <= high;
    if (inside) {
        sword.caught = true;
        bool dodged = false;
        const BossStrikeData& strike = BossMoveTable::Strike(move, 0);
        sword.hit = BossWaves::HitPlayer(frame, strike.damage, FlatNormalize(offset, Forward(sword.yaw)), strike.blow,
                                         boss.move, 0, "greatsword", dodged);
        sword.dodged = dodged;
    }
    // 当たった・よけられた tick と、最後まで当たらなかった時の薙ぎ終わりに知らせる。
    if (inside || last)
        LogHit(frame, distance, playerAngle);
}

} // namespace

void BossGreatsword::Begin(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    Reset(boss, "next");
    if (move.type != BossMoveType::Greatsword)
        return;
    BossGreatswordState& sword = boss.greatsword;
    sword.length = boss.greatswordLength;
    sword.fire = boss.bladeLit;
    RaisePose(boss, sword.hilt, sword.direction);
}

void BossGreatsword::Pre(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    const int t = boss.stateFrame;
    const int windFrames = std::clamp(boss.greatswordWindFrames, 1, std::max(1, move.pre));
    const int windStart = move.pre - windFrames;
    SetPhase(boss, BossGreatswordPhase::Conjure);
    sword.fill = Share(t, boss.greatswordFillFrames);
    Vec3 hilt{}, direction{};
    RaisePose(boss, hilt, direction);
    if (t > windStart) {
        if (sword.wind <= 0.0f)
            LogShape(boss, "GreatswordWind");
        // 立てた剣を、薙ぎ始めの構え（右後ろ・水平）へ下ろす。向きは追尾を止めるまでボスと一緒に回る。
        sword.wind = Share(t - windStart, windFrames);
        Vec3 startHilt{}, startDirection{};
        SweepPose(boss, boss.yaw, StartAngle(boss), startHilt, startDirection);
        const float e = Smooth(sword.wind);
        hilt = Lerp(hilt, startHilt, e);
        direction = Normalize(Lerp(direction, startDirection, e), startDirection);
    }
    sword.hilt = hilt;
    sword.direction = direction;
    Trace(boss);
}

void BossGreatsword::BeginSweep(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    sword.yaw = boss.yaw;
    sword.angle = sword.previousAngle = StartAngle(boss);
    sword.fill = sword.wind = 1.0f;
    SweepPose(boss, sword.yaw, sword.angle, sword.hilt, sword.direction);
    SetPhase(boss, BossGreatswordPhase::Sweep);
}

void BossGreatsword::Act(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    if (sword.phase != BossGreatswordPhase::Sweep)
        return;
    const int frames = move.segments[static_cast<size_t>(g.segment)].length;
    // 薙ぐ角度は、ゆっくり振り出して中ほどで一番速く、最後はゆっくり止める。
    sword.previousAngle = sword.angle;
    sword.sweep = Share(g.tIn, frames);
    sword.angle = StartAngle(boss) + boss.greatswordSweepDegrees * Smooth(sword.sweep);
    SweepPose(boss, sword.yaw, sword.angle, sword.hilt, sword.direction);
    StepHit(frame, move, g.tIn >= frames);
    BossBurnMarks::SweptArc(frame, move, Center(boss, 0.0f), sword.yaw, StartAngle(boss), sword.angle,
                            sword.firePainted);
    Trace(boss);
}

void BossGreatsword::Post(BossComponent& boss) {
    BossGreatswordState& sword = boss.greatsword;
    if (sword.phase == BossGreatswordPhase::None)
        return;
    if (boss.stateFrame > boss.greatswordDissolveFrames) {
        Reset(boss, "dissolved");
        return;
    }
    // ほどける間も、剣は薙ぎ終えた所に留まる（ボスの浮き沈みには付いて行く）。
    SetPhase(boss, BossGreatswordPhase::Dissolve);
    sword.dissolve = Share(boss.stateFrame, boss.greatswordDissolveFrames);
    SweepPose(boss, sword.yaw, sword.angle, sword.hilt, sword.direction);
    Trace(boss);
}

void BossGreatsword::Reset(BossComponent& boss, const char* reason) {
    if (boss.greatsword.phase != BossGreatswordPhase::None) {
        boss.greatsword.phase = BossGreatswordPhase::None;
        LogShape(boss, "Greatsword", reason);
    }
    boss.greatsword = {};
}

} // namespace Project_TD2_2
