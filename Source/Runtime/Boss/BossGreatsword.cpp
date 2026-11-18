#include "BossGreatsword.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossBurnMarks.h"
#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossRules.h"
#include "BossWaves.h"
#include "../Common/TraceInterval.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

const char* PhaseName(BossGreatswordPhase phase) {
    switch (phase) {
    case BossGreatswordPhase::Conjure: return "conjure";
    case BossGreatswordPhase::Sweep: return "sweep";
    case BossGreatswordPhase::Dissolve: return "dissolve";
    case BossGreatswordPhase::Ready: return "ready";
    case BossGreatswordPhase::Raise: return "raise";
    case BossGreatswordPhase::Slam: return "slam";
    case BossGreatswordPhase::Stuck: return "stuck";
    default: return "none";
    }
}

bool IsGreatswordType(BossMoveType type) {
    return type == BossMoveType::Greatsword || type == BossMoveType::GreatswordCleave;
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
    char message[448]{};
    std::snprintf(
        message, sizeof(message),
        "%s tick=%llu phase=%s fill=%.3f wind=%.3f sweep=%.3f raise=%.3f slam=%.3f dissolve=%.3f "
        "angle=%.1f elev=%.1f hilt=(%.3f,%.3f,%.3f) dir=(%.3f,%.3f,%.3f) chained=%d cleave=%d "
        "erupting=%d front=(%.3f,%.3f)%s%s",
        event, static_cast<unsigned long long>(boss.tick), PhaseName(sword.phase), static_cast<double>(sword.fill),
        static_cast<double>(sword.wind), static_cast<double>(sword.sweep), static_cast<double>(sword.raise),
        static_cast<double>(sword.slam), static_cast<double>(sword.dissolve), static_cast<double>(AngleOf(boss)),
        static_cast<double>(ElevationOf(boss)), static_cast<double>(sword.hilt.x), static_cast<double>(sword.hilt.y),
        static_cast<double>(sword.hilt.z), static_cast<double>(sword.direction.x),
        static_cast<double>(sword.direction.y), static_cast<double>(sword.direction.z), static_cast<int>(sword.chained),
        static_cast<int>(sword.cleave), static_cast<int>(sword.erupting), static_cast<double>(sword.eruptionFront.x),
        static_cast<double>(sword.eruptionFront.z), reason ? " reason=" : "", reason ? reason : "");
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
    if (TraceInterval::Due(boss.traceEveryFrames, boss.tick))
        LogShape(boss, "GreatswordTrace");
}

// ボスの中心（足元の真上。高さは足元から height m）。
Vec3 Center(const BossComponent& boss, float height) {
    return {boss.ground.x, boss.ground.y + BossPose::RootHeight(boss) + height, boss.ground.z};
}

// 地面の高さの、ボスの中心から yaw の向きへ distance m の所。
Vec3 GroundAhead(const BossComponent& boss, Vec3 origin, float yaw, float distance) {
    const Vec3 at = Add(origin, Mul(Forward(yaw), distance));
    return {at.x, boss.ground.y, at.z};
}

// 作り出す間：右の胸の前（greatswordRaiseSide・greatswordRaiseForward、足元から greatswordRaiseHeight m）に柄を置き、
// 真上から後ろへ少し倒して立てる。
void RaisePose(const BossComponent& boss, Vec3& hilt, Vec3& direction) {
    const Vec3 shoulder{boss.greatswordRaiseSide, boss.greatswordRaiseHeight, boss.greatswordRaiseForward};
    hilt = Add(Center(boss, 0.0f), RotateY(shoulder, boss.yaw));
    const float tilt = boss.greatswordRaiseTilt * kRadiansPerDegree;
    direction = Normalize(Add(Vec3{0.0f, std::cos(tilt), 0.0f}, Mul(Forward(boss.yaw), -std::sin(tilt))));
}

// 縦割りの振りかぶり：頭上（足元から greatswordOverheadHeight m、前へ greatswordOverheadForward m）に柄を置き、
// 真上から後ろへ greatswordOverheadTilt 度倒して立てる。向きはいまのボスの向き。
void OverheadPose(const BossComponent& boss, Vec3& hilt, Vec3& direction) {
    hilt = Add(Center(boss, boss.greatswordOverheadHeight), Mul(Forward(boss.yaw), boss.greatswordOverheadForward));
    const float tilt = boss.greatswordOverheadTilt * kRadiansPerDegree;
    direction = Normalize(Add(Vec3{0.0f, std::cos(tilt), 0.0f}, Mul(Forward(boss.yaw), -std::sin(tilt))));
}

// 薙ぐ間：基準の向き yaw から angle 度（右が負）の向きに、ボスの中心から greatswordHiltOffset m の所に柄。剣先は
// greatswordSweepTiltDegrees だけ下げる（当たりは角度と距離だけなので、高さと傾きは見た目）。
void SweepPose(const BossComponent& boss, float yaw, float angle, Vec3& hilt, Vec3& direction) {
    const Vec3 flat = Forward(yaw + angle * kRadiansPerDegree);
    const float tilt = boss.greatswordSweepTiltDegrees * kRadiansPerDegree;
    direction = Add(Mul(flat, std::cos(tilt)), Vec3{0.0f, -std::sin(tilt), 0.0f});
    hilt = Add(Center(boss, boss.greatswordSweepHeight), Mul(flat, boss.greatswordHiltOffset));
}

float StartAngle(const BossComponent& boss) { return -boss.greatswordSweepDegrees * 0.5f; }

// 0〜1 の進みを、peak で速さが最も大きくなる 0〜1 の進みへ（両端で速さ 0。振りの型の速さの曲線と同じ形）。
float HeavyCurve(float u, float peak) {
    const float x = std::pow(std::clamp(u, 0.0f, 1.0f), std::log(0.5f) / std::log(std::clamp(peak, 0.1f, 0.9f)));
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

// 叩きつけ終えた形：柄はボスの中心から縦割りの向きへ greatswordHiltOffset m・足元から greatswordCleaveHiltHeight m、
// 剣先が地面に着く角度（rad。水平から下向きが負）。
Vec3 LandHilt(const BossComponent& boss) {
    return Add(Center(boss, boss.greatswordCleaveHiltHeight),
               Mul(Forward(boss.greatsword.cleaveYaw), boss.greatswordHiltOffset));
}
float LandAngle(const BossComponent& boss, Vec3 landHilt) {
    const float drop = landHilt.y - boss.ground.y;
    return -std::asin(std::clamp(drop / std::max(0.01f, boss.greatsword.length), -1.0f, 1.0f));
}

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

// プレイヤーの中心の、origin から yaw の向きへの距離（along）と、その線から右への距離（side）。
void Project(const BossFrame& frame, Vec3 origin, float yaw, float& along, float& side) {
    const Vec3 offset{frame.playerCenter.x - origin.x, 0.0f, frame.playerCenter.z - origin.z};
    const Vec3 forward = Forward(yaw);
    along = Dot(offset, forward);
    side = Dot(offset, Vec3{-forward.z, 0.0f, forward.x});
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
    const float reach = boss.greatswordHiltOffset + sword.length + boss.playerHitRadius;
    const float low = std::min(sword.previousAngle, sword.angle) - boss.greatswordHitMarginDegrees;
    const float high = std::max(sword.previousAngle, sword.angle) + boss.greatswordHitMarginDegrees;
    // 回転薙ぎ（c-9143b5）は剣の角度が 1 周を越えるので、プレイヤーの角度を 1 周ずつずらして見る。
    bool within = false;
    for (int turn = -1; turn <= 2 && !within; ++turn) {
        const float a = playerAngle + 360.0f * static_cast<float>(turn);
        within = a >= low && a <= high;
    }
    const bool inside = distance >= boss.greatswordInnerRadius && distance <= reach && within;
    if (inside) {
        sword.caught = true;
        const BossStrikeData& strike = BossMoveTable::Strike(move, 0);
        sword.hit =
            BossWaves::HitPlayer(frame, strike.damage, FlatNormalize(offset, Forward(sword.yaw)), strike.blow,
                                 boss.move, 0, "greatsword", PlayerDamageType::Slash, sword.fire, &sword.dodged);
    }
    // 当たった・よけられた tick と、最後まで当たらなかった時の薙ぎ終わりに知らせる。
    if (inside || last)
        LogHit(frame, distance, playerAngle);
}

// 縦割りの刃の当たり：ボスの正面（縦割りの向き）の線の上、懐（greatswordInnerRadius）〜剣先（reach）、左右へ
// greatswordCleaveHalfWidth（どちらもプレイヤーの体の半径を足す）にいれば、1 回の縦割りで 1 回だけ当てる。
void StepCleaveHit(BossFrame& frame, const BossMove& move, float reach, bool last) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    if (sword.caught || frame.player == nullptr)
        return;
    float along = 0.0f, side = 0.0f;
    Project(frame, boss.ground, sword.cleaveYaw, along, side);
    const bool inside = along >= boss.greatswordInnerRadius && along <= reach + boss.playerHitRadius &&
                        std::abs(side) <= boss.greatswordCleaveHalfWidth + boss.playerHitRadius;
    if (inside) {
        sword.caught = true;
        const BossStrikeData& strike = BossMoveTable::Strike(move, 0);
        sword.hit = BossWaves::HitPlayer(frame, strike.damage, Forward(sword.cleaveYaw), strike.blow, boss.move, 0,
                                         "cleave", PlayerDamageType::Slash, sword.fire, &sword.dodged);
        sword.cleaveStruck = sword.hit;
    }
    if (!inside && !last)
        return;
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "GreatswordCleaveHit tick=%llu hit=%d dodged=%d caught=%d slam=%.3f along=%.3f side=%.3f reach=%.3f",
                  static_cast<unsigned long long>(boss.tick), static_cast<int>(sword.hit),
                  static_cast<int>(sword.dodged), static_cast<int>(sword.caught), static_cast<double>(sword.slam),
                  static_cast<double>(along), static_cast<double>(side), static_cast<double>(reach));
    BossLog::Write(message);
}

void LogEruption(const BossComponent& boss, const char* state) {
    const BossGreatswordState& sword = boss.greatsword;
    char message[320]{};
    std::snprintf(message, sizeof(message),
                  "GreatswordEruption tick=%llu state=%s distance=%.3f from=(%.3f,%.3f) front=(%.3f,%.3f) yaw=%.4f "
                  "caught=%d hit=%d dodged=%d",
                  static_cast<unsigned long long>(boss.tick), state, static_cast<double>(sword.eruptionDistance),
                  static_cast<double>(sword.eruptionFrom.x), static_cast<double>(sword.eruptionFrom.z),
                  static_cast<double>(sword.eruptionFront.x), static_cast<double>(sword.eruptionFront.z),
                  static_cast<double>(sword.cleaveYaw), static_cast<int>(sword.eruptionCaught),
                  static_cast<int>(sword.eruptionHit), static_cast<int>(sword.eruptionDodged));
    BossLog::Write(message);
}

// 剣先が地面に着いた tick：噴き上がりを剣先の真下から走らせ、フェーズ 2 なら刃の下（懐〜剣先）に「線」の火を塗る。
void Land(BossFrame& frame, const BossMove& move, float reach) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    sword.landTick = boss.tick;
    sword.landed = true;
    sword.eruptionOrigin = {boss.ground.x, boss.ground.y, boss.ground.z};
    sword.eruptionDistance = reach;
    sword.eruptionFrom = sword.eruptionFront = GroundAhead(boss, sword.eruptionOrigin, sword.cleaveYaw, reach);
    sword.eruptionMove = boss.move;
    sword.eruptionCaught = sword.eruptionHit = sword.eruptionDodged = false;
    sword.erupting = reach < boss.greatswordEruptionReach;
    sword.eruptionBurnWidth = sword.eruptionBurnSeconds = 0.0f;
    sword.eruptionPainted = reach;
    if (const BossBurnMark* mark = BossBurnMarks::Find(move, BossBurnShape::Line, 0, boss.lastPhaseNumber)) {
        const Vec3 inner = GroundAhead(boss, sword.eruptionOrigin, sword.cleaveYaw, boss.greatswordInnerRadius);
        BossBurnMarks::PaintCleaveLine(frame, move, inner, sword.eruptionFrom, *mark, &inner);
        sword.eruptionBurnWidth = mark->width;
        sword.eruptionBurnSeconds = mark->seconds;
    }
    LogShape(boss, "GreatswordLand");
    if (sword.erupting)
        LogEruption(boss, "begin");
}

// 跳び割り（c-9143b5。縦割りの行で「前隙の移動 m」が正）：前隙の終わりの greatswordLeapFrames F で、プレイヤーから
// greatswordLeapStop m の所へ（最長は「前隙の移動 m」。近すぎれば跳ばずにその場で跳ね上がる）弧を描いて跳ぶ。
// 行き先は跳び始めの tick に決め（dashFrom・dashTo）、前隙の終わりに着地する。
void Leap(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    if (move.type != BossMoveType::GreatswordCleave || !(move.preShift > 0.0f))
        return;
    const int leap = std::clamp(boss.greatswordLeapFrames, 1, std::max(1, move.pre - 1));
    const int start = move.pre - leap;
    const int t = boss.stateFrame;
    if (t <= start)
        return;
    if (t == start + 1) {
        const Vec3 offset = Sub(PlayerGround(frame), boss.ground);
        const float distance = FlatLength(offset);
        const float length = std::clamp(distance - boss.greatswordLeapStop, 0.0f, move.preShift);
        boss.dashFrom = boss.ground;
        boss.dashTo = Add(boss.ground, Mul(FlatNormalize(offset, Forward(boss.yaw)), length));
        BossMoveExecutor::ClampToArena(boss, boss.dashTo);
        char extra[64]{};
        std::snprintf(extra, sizeof(extra), "distance=%.3f", static_cast<double>(length));
        LogShape(boss, "GreatswordLeap", extra);
    }
    // 跳び上がり（だんだん遅く）→ 頂きで留まって溜める（少し浮く）→ 一気に落ちる（だんだん速く）。前へは跳び上がる間に
    // ほとんど進み、落ちる間に残りを詰める。
    const float k = Share(t - start, leap);
    const float rise = std::clamp(boss.greatswordLeapRiseShare, 0.05f, 0.9f);
    const float hang = std::clamp(boss.greatswordLeapHangShare, 0.0f, 0.9f - rise);
    const float drop = std::max(0.05f, 1.0f - rise - hang);
    const float top = boss.greatswordLeapHeight;
    if (k < rise) {
        boss.height = top * EaseOut(k / rise);
        boss.greatswordApexBurst = false;
    } else if (k < rise + hang) {
        // 頂きで溜め始めた瞬間、振りかぶった剣先が閃く。
        if (!boss.greatswordApexBurst && boss.greatswordApexBurstScale > 0.0f) {
            boss.greatswordApexBurst = true;
            const BossGreatswordState& sword = boss.greatsword;
            BossWaves::AddBlast(boss, {.effectPending = true,
                                       .fire = sword.fire,
                                       .scale = boss.greatswordApexBurstScale,
                                       .position = Add(sword.hilt, Mul(sword.direction, sword.length))});
        }
        boss.height = top + boss.greatswordLeapHover * std::sin(kPi * (k - rise) / std::max(0.01f, hang));
    } else {
        const float u = (k - rise - hang) / drop;
        boss.height = top * (1.0f - u * u);
    }
    const float travel = k < rise          ? 0.75f * EaseOut(k / rise)
                         : k < rise + hang ? 0.75f
                                           : 0.75f + 0.25f * Smooth((k - rise - hang) / drop);
    boss.ground = Lerp(boss.dashFrom, boss.dashTo, travel);
}

} // namespace

void BossGreatsword::Begin(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    // コンボ：大剣の技の攻撃中が終わった tick に（後隙を出さずに）大剣の技を始めた。剣は作り直さずに続ける。
    const bool chained = IsGreatswordType(move.type) && boss.chain > 0 &&
                         (sword.phase == BossGreatswordPhase::Sweep || sword.phase == BossGreatswordPhase::Slam);
    if (!chained) {
        Reset(boss, "next");
        if (!IsGreatswordType(move.type))
            return;
        sword.length = boss.greatswordLength;
        sword.fire = boss.bladeLit;
        RaisePose(boss, sword.hilt, sword.direction);
        return;
    }
    // 一振りごとの値を戻す（剣の長さ・炎・走っている噴き上がり・縦割りの向きは続ける）。
    sword.chained = true;
    sword.cleave = move.type == BossMoveType::GreatswordCleave;
    // つなぎの柄はボスの中心からの差で覚える（跳び割りで体が動いても、柄が置き去りにならない。c-9143b5）。
    sword.fromHilt = Sub(sword.hilt, Center(boss, 0.0f));
    sword.fromDirection = sword.direction;
    sword.fill = 1.0f;
    sword.wind = sword.cleave ? 1.0f : 0.0f;
    sword.sweep = sword.raise = sword.slam = sword.dissolve = 0.0f;
    sword.caught = sword.hit = sword.dodged = false;
    sword.firePainted = 0;
    LogShape(boss, "GreatswordChain");
}

void BossGreatsword::Pre(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    const int t = boss.stateFrame;
    const bool cleave = move.type == BossMoveType::GreatswordCleave;
    // 跳び割りの跳び（体を先に動かし、剣はその tick の体の所から構える）。
    Leap(frame, move);
    Vec3 hilt{}, direction{};
    if (sword.chained) {
        // 連撃：前隙の全部で、前の一振りの終わりの形から、頭上へ振りかぶる（縦割り）・薙ぎ始めの構えへ回す（横薙ぎ）。
        const float share = Share(t, move.pre);
        if (cleave) {
            SetPhase(boss, BossGreatswordPhase::Raise);
            sword.raise = share;
            sword.cleaveYaw = boss.yaw;
            OverheadPose(boss, hilt, direction);
        } else {
            SetPhase(boss, BossGreatswordPhase::Ready);
            sword.wind = share;
            SweepPose(boss, boss.yaw, StartAngle(boss), hilt, direction);
        }
        // 柄は体の縦の軸のまわりを回って構え直す（まっすぐ結ぶと、左後ろから右後ろへ体の中を通った。c-9143b5）。
        const float e = Smooth(share);
        const Vec3 center = Center(boss, 0.0f);
        sword.hilt = Add(center, LerpAroundY(sword.fromHilt, Sub(hilt, center), e));
        sword.direction = Normalize(Lerp(sword.fromDirection, direction, e), direction);
        Trace(boss);
        return;
    }
    // 前隙の最後の bendFrames で、立てた剣を薙ぎ始めの構え（右後ろ・水平）へ下ろす・頭上へ振りかぶる。
    // 向きは追尾を止めるまでボスと一緒に回る。剣はそれより前に満ちきる。
    const int bendFrames =
        std::clamp(cleave ? boss.greatswordCleaveLiftFrames : boss.greatswordWindFrames, 1, std::max(1, move.pre));
    const int bendStart = move.pre - bendFrames;
    sword.fill = Share(t, std::min(boss.greatswordFillFrames, std::max(1, bendStart)));
    // 剣を作り終えた瞬間に、握りの上で爆ぜる（c-9143b5）。
    if (sword.fill >= 1.0f && !sword.burst && boss.greatswordConjureBurstScale > 0.0f) {
        sword.burst = true;
        BossWaves::AddBlast(boss, {.effectPending = true,
                                   .fire = sword.fire,
                                   .scale = boss.greatswordConjureBurstScale,
                                   .position = Add(sword.hilt, Mul(sword.direction, sword.length * 0.5f))});
        LogShape(boss, "GreatswordForged");
    }
    RaisePose(boss, hilt, direction);
    if (t > bendStart) {
        Vec3 bendHilt{}, bendDirection{};
        if (cleave) {
            SetPhase(boss, BossGreatswordPhase::Raise);
            sword.cleave = true;
            sword.raise = Share(t - bendStart, bendFrames);
            sword.cleaveYaw = boss.yaw;
            OverheadPose(boss, bendHilt, bendDirection);
        } else {
            SetPhase(boss, BossGreatswordPhase::Conjure);
            if (sword.wind <= 0.0f)
                LogShape(boss, "GreatswordWind");
            sword.wind = Share(t - bendStart, bendFrames);
            SweepPose(boss, boss.yaw, StartAngle(boss), bendHilt, bendDirection);
        }
        const float e = Smooth(cleave ? sword.raise : sword.wind);
        hilt = Lerp(hilt, bendHilt, e);
        direction = Normalize(Lerp(direction, bendDirection, e), bendDirection);
    } else {
        SetPhase(boss, BossGreatswordPhase::Conjure);
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
    sword.cleave = false;
    SweepPose(boss, sword.yaw, sword.angle, sword.hilt, sword.direction);
    SetPhase(boss, BossGreatswordPhase::Sweep);
}

void BossGreatsword::BeginSlam(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    sword.cleaveYaw = boss.yaw;
    sword.fill = sword.raise = 1.0f;
    sword.slam = 0.0f;
    sword.cleave = true;
    sword.cleaveStruck = false;
    // 叩きつけは、振りかぶり終えた形から始める。
    sword.fromHilt = sword.hilt;
    sword.fromDirection = sword.direction;
    SetPhase(boss, BossGreatswordPhase::Slam);
}

void BossGreatsword::Act(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    const int frames = move.segments[static_cast<size_t>(g.segment)].length;
    if (sword.phase == BossGreatswordPhase::Slam) {
        // 振りかぶった角度から、剣先が地面に着く角度まで、縦割りの向きの縦の面の中で振り下ろす。
        // 振り下ろしは加速する（進みの 2 乗。地面に着く直前が一番速い）。柄も同じ進みで叩きつけた所へ下りる。
        sword.slam = Share(g.tIn, frames);
        const float u = sword.slam * sword.slam;
        const Vec3 forward = Forward(sword.cleaveYaw);
        const Vec3 landHilt = LandHilt(boss);
        const float landAngle = LandAngle(boss, landHilt);
        const float fromAngle = std::atan2(sword.fromDirection.y, Dot(sword.fromDirection, forward));
        const float angle = Lerp(fromAngle, landAngle, u);
        sword.hilt = Lerp(sword.fromHilt, landHilt, u);
        sword.direction = Add(Mul(forward, std::cos(angle)), Vec3{0.0f, std::sin(angle), 0.0f});
        const float reach = boss.greatswordHiltOffset + sword.length * std::cos(landAngle);
        const bool last = g.tIn >= frames;
        if (sword.slam >= boss.greatswordCleaveHitFrom)
            StepCleaveHit(frame, move, reach, last);
        if (last)
            Land(frame, move, reach);
        Trace(boss);
        return;
    }
    if (sword.phase != BossGreatswordPhase::Sweep)
        return;
    // 薙ぐ角度は、ゆっくり振り出して中ほどで一番速く、最後はゆっくり止める。
    sword.previousAngle = sword.angle;
    sword.sweep = Share(g.tIn, frames);
    // 重い振り（c-9143b5）：ゆっくり振り出し、greatswordSweepPeak で一番速く、振り終わりへ重く止まる。
    // 回転薙ぎ（表の「体の回転 °」）は体ごと回り、剣の角度にその回転を足す（1 周を越えて薙ぐ）。
    const float turn = HeavyCurve(sword.sweep, boss.greatswordSweepPeak);
    boss.spin = move.actSpin * turn;
    sword.angle = StartAngle(boss) + boss.greatswordSweepDegrees * turn + boss.spin / kRadiansPerDegree;
    SweepPose(boss, sword.yaw, sword.angle, sword.hilt, sword.direction);
    StepHit(frame, move, g.tIn >= frames);
    BossBurnMarks::SweptArc(frame, move, Center(boss, 0.0f), sword.yaw, StartAngle(boss),
                            std::min(sword.angle, StartAngle(boss) + 360.0f), sword.firePainted);
    Trace(boss);
}

void BossGreatsword::Post(BossComponent& boss) {
    BossGreatswordState& sword = boss.greatsword;
    if (sword.phase == BossGreatswordPhase::None)
        return;
    // 縦割りの後は、剣が叩きつけた所に刺さったまま greatswordCleaveStuckFrames 待ってからほどける。
    const int stuck = sword.cleave ? std::max(0, boss.greatswordCleaveStuckFrames) : 0;
    if (boss.stateFrame > stuck + boss.greatswordDissolveFrames) {
        Reset(boss, "dissolved");
        return;
    }
    if (boss.stateFrame <= stuck) {
        SetPhase(boss, BossGreatswordPhase::Stuck);
        Trace(boss);
        return;
    }
    // ほどける間も、剣は薙ぎ終えた所に留まる（ボスの浮き沈みには付いて行く）。縦割りの剣は刺さった所に留まる。
    SetPhase(boss, BossGreatswordPhase::Dissolve);
    sword.dissolve = Share(boss.stateFrame - stuck, boss.greatswordDissolveFrames);
    if (!sword.cleave)
        SweepPose(boss, sword.yaw, sword.angle, sword.hilt, sword.direction);
    Trace(boss);
}

// 噴き上がりの前線を 1 tick 進める。前線が通った距離（1 tick 前〜いま）にプレイヤーの中心があり、線から左右へ
// greatswordEruptionHalfWidth（＋体の半径）以内なら、1 回の噴き上がりで 1 回だけ当てる（無敵でよけた時も、もう当たらない）。
void BossGreatsword::StepEruption(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    BossGreatswordState& sword = boss.greatsword;
    sword.landed = false;
    if (!sword.erupting)
        return;
    const float before = sword.eruptionDistance;
    sword.eruptionDistance =
        std::min(boss.greatswordEruptionReach, before + std::max(0.0f, boss.greatswordEruptionSpeed) * frame.dt);
    sword.eruptionFront = GroundAhead(boss, sword.eruptionOrigin, sword.cleaveYaw, sword.eruptionDistance);
    if (!sword.eruptionCaught && frame.player != nullptr) {
        float along = 0.0f, side = 0.0f;
        Project(frame, sword.eruptionOrigin, sword.cleaveYaw, along, side);
        if (along > before && along <= sword.eruptionDistance &&
            std::abs(side) <= boss.greatswordEruptionHalfWidth + boss.playerHitRadius) {
            sword.eruptionCaught = true;
            sword.eruptionHit = BossWaves::HitPlayer(frame, boss.greatswordEruptionDamage, Forward(sword.cleaveYaw),
                                                     boss.greatswordEruptionBlow, sword.eruptionMove, 0, "eruption",
                                                     PlayerDamageType::Explosion, sword.fire, &sword.eruptionDodged);
            sword.cleaveStruck = sword.cleaveStruck || sword.eruptionHit;
            char message[192]{};
            std::snprintf(message, sizeof(message),
                          "GreatswordEruptionHit tick=%llu hit=%d dodged=%d along=%.3f side=%.3f landTick=%llu",
                          static_cast<unsigned long long>(boss.tick), static_cast<int>(sword.eruptionHit),
                          static_cast<int>(sword.eruptionDodged), static_cast<double>(along), static_cast<double>(side),
                          static_cast<unsigned long long>(sword.landTick));
            BossLog::Write(message);
        }
    }
    const bool done = sword.eruptionDistance >= boss.greatswordEruptionReach;
    const BossMove* row = BossRules::GetMove(boss, sword.eruptionMove);
    if (sword.eruptionBurnWidth > 0.0f && row != nullptr) {
        // フェーズ 2：前線が通った所に、刃の下と同じ「線」の火を続けて塗る。
        BossBurnMark mark;
        mark.shape = BossBurnShape::Line;
        mark.width = sword.eruptionBurnWidth;
        mark.seconds = sword.eruptionBurnSeconds;
        const Vec3 from = GroundAhead(boss, sword.eruptionOrigin, sword.cleaveYaw, sword.eruptionPainted);
        BossBurnMarks::PaintCleaveLine(frame, *row, from, sword.eruptionFront, mark,
                                       done ? &sword.eruptionFrom : nullptr);
        sword.eruptionPainted = sword.eruptionDistance;
    }
    if (done) {
        sword.erupting = false;
        LogEruption(boss, "end");
    }
}

void BossGreatsword::Reset(BossComponent& boss, const char* reason) {
    if (boss.greatsword.erupting)
        LogEruption(boss, "cut");
    if (boss.greatsword.phase != BossGreatswordPhase::None) {
        boss.greatsword.phase = BossGreatswordPhase::None;
        LogShape(boss, "Greatsword", reason);
    }
    boss.greatsword = {};
}

} // namespace Project_TD2_2
