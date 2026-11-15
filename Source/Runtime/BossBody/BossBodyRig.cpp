#include "BossBodyRig.h"

#include <algorithm>
#include <cmath>

#include "BossBodySword.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 草摺を吊る腰の輪郭：角の丸い四角（模型を作るスクリプト bb_armor.superpoint と同じ指数）。
constexpr float kWaistOutlinePower = 2.6f;

Quat AroundY(float radians) { return {0.0f, std::sin(radians * 0.5f), 0.0f, std::cos(radians * 0.5f)}; }
Quat AroundX(float radians) { return {std::sin(radians * 0.5f), 0.0f, 0.0f, std::cos(radians * 0.5f)}; }
Quat AroundZ(float radians) { return {0.0f, 0.0f, std::sin(radians * 0.5f), std::cos(radians * 0.5f)}; }

// 揺れの計算で、瞬間移動（突進の出現・霞隠れ）を動きと見なさない速さの上限と、裾を開く角度の範囲。
constexpr float kSwayTeleportSpeed = 25.0f;
constexpr float kFlareMinDegrees = -12.0f;
constexpr float kFlareMaxDegrees = 80.0f;
// 揺らぎの位相を、部品ごとにずらす量（同じ動きに揃わないように）。
constexpr float kFlutterPhaseStep = 2.4f;

// 体の物体の変換で、体の局所（足元が原点）の点・向きを世界へ移す。体の物体の原点は体の中心。
struct BodyFrame {
    const HIKARI::GameProjectTransform& body;
    float halfHeight;

    Vec3 Point(Vec3 local) const {
        const Vec3 centered{local.x * body.scale.x, (local.y - halfHeight) * body.scale.y, local.z * body.scale.z};
        return Add(body.position, Rotate(body.rotation, centered));
    }
    // yaw で外へ向け、flare で裾を外へ開く部品（大袖・草摺）。roll は、吊る所を支点に裾を横へ振る角度。
    BossBodyPlacement Hanging(Vec3 local, float yaw, float flareDegrees, float scale = 1.0f,
                              float rollDegrees = 0.0f) const {
        BossBodyPlacement placement;
        placement.position = Point(local);
        placement.rotation =
            Multiply(body.rotation, Multiply(AroundY(yaw), Multiply(AroundZ(rollDegrees * kRadiansPerDegree),
                                                                    AroundX(-flareDegrees * kRadiansPerDegree))));
        placement.scale = scale * body.scale.x;
        return placement;
    }
    // 揺れを足した、垂れた部品。振れ（世界座標の度）のうち、外向きの成分は裾の開き、横の成分は横振りになる。
    BossBodyPlacement Swinging(const BossComponent& boss, Vec3 local, float yaw, float flareDegrees, int index) const {
        const BossBodyState& state = boss.bodyState;
        const Vec3 outward = Rotate(body.rotation, {std::sin(yaw), 0.0f, std::cos(yaw)});
        const Vec3 tangent = Rotate(body.rotation, {std::cos(yaw), 0.0f, -std::sin(yaw)});
        const Vec3 flat{state.sway.x, 0.0f, state.sway.z};
        const float flutter =
            boss.body.flutterDegrees * std::sin(2.0f * kPi * boss.body.flutterFrequency * state.clock +
                                                kFlutterPhaseStep * static_cast<float>(index));
        const float flare =
            std::clamp(flareDegrees + Dot(flat, outward) + state.sway.y + flutter, kFlareMinDegrees, kFlareMaxDegrees);
        return Hanging(local, yaw, flare, 1.0f, Dot(flat, tangent));
    }
};

// 角の丸い四角の輪郭の上の点（angle 0＝前、π/2＝左）。
Vec3 WaistPoint(float angle, float radiusSide, float radiusFront, float height) {
    const float s = std::sin(angle), c = std::cos(angle);
    const float e = 2.0f / kWaistOutlinePower;
    return {std::copysign(std::pow(std::abs(s), e), s) * radiusSide, height,
            std::copysign(std::pow(std::abs(c), e), c) * radiusFront};
}

void SolveArmor(const BossComponent& boss, const HIKARI::GameProjectTransform& bodyTransform, BossBodyPose& pose) {
    const BossBodySettings& body = boss.body;
    const BodyFrame frame{bodyTransform, boss.bodyHalfHeight};
    // 胴・兜・大袖は、刀の横の向きに付いて縦の軸まわりに捻る（BossBodySword が進める）。兜はさらにプレイヤーを向く。
    const float twist = boss.bodyState.motion.twist;
    pose.torso = frame.Hanging(RotateY(body.torsoCenter, twist), twist, 0.0f);
    pose.head = frame.Hanging(RotateY(body.headCenter, twist), boss.bodyState.motion.headYaw, 0.0f, body.headScale);
    const float sodeYaw = body.sodeYawDegrees * kRadiansPerDegree;
    // 右の袖は -X の側（外向きは -X 寄り）、左は +X の側。
    pose.sode[0] = frame.Swinging(boss, RotateY(body.sodeHang, twist), twist - sodeYaw, body.sodeFlareDegrees, 100);
    pose.sode[1] = frame.Swinging(boss, RotateY({-body.sodeHang.x, body.sodeHang.y, body.sodeHang.z}, twist),
                                  twist + sodeYaw, body.sodeFlareDegrees, 101);
    pose.tassetCount = std::clamp(body.tassetCount, 0, kBossBodyTassetCapacity);
    // 草摺は腰に付くので、捻りの一部だけ。
    const float tassetTwist = twist * std::clamp(body.motion.tassetTwistShare, 0.0f, 1.0f);
    for (int k = 0; k < pose.tassetCount; ++k) {
        const float angle = 2.0f * kPi * static_cast<float>(k) / static_cast<float>(pose.tassetCount);
        pose.tassets[static_cast<size_t>(k)] = frame.Swinging(
            boss,
            RotateY(WaistPoint(angle, body.tassetRadiusSide, body.tassetRadiusFront, body.tassetHangHeight),
                    tassetTwist),
            angle + tassetTwist, body.tassetFlareDegrees, k);
    }
    pose.eyesFire = boss.lastPhaseNumber >= 2;
    pose.eyesLit = boss.state != BossState::Stunned && boss.state != BossState::Dead;
}

} // namespace

void BossBodyRig::StepSway(BossComponent& boss, const HIKARI::GameProjectTransform& bodyTransform, float seconds) {
    const BossBodySettings& body = boss.body;
    BossBodyState& state = boss.bodyState;
    if (!state.swayReady || seconds <= 0.0f) {
        state.previousPosition = bodyTransform.position;
        state.seenHits = boss.brain.seenHits;
        state.swayReady = true;
        return;
    }
    state.clock += seconds;
    Vec3 velocity = Mul(Sub(bodyTransform.position, state.previousPosition), 1.0f / seconds);
    state.previousPosition = bodyTransform.position;
    if (Length(velocity) > kSwayTeleportSpeed)
        velocity = {};
    // 動きと反対へ振れる。落ちる時は裾が持ち上がり、昇る時は体へ寄る。
    Vec3 target = Mul(velocity, -body.swayDegreesPerSpeed);
    const float size = Length(target);
    if (size > body.swayMaxDegrees)
        target = Mul(target, body.swayMaxDegrees / size);
    // ばね：目標へ引かれ、行き過ぎて戻る。1 フレームが長い時も暴れないよう、短い刻みに分けて進める。
    const float omega = 2.0f * kPi * body.swayFrequency;
    const int steps = std::clamp(static_cast<int>(std::ceil(seconds * omega * 4.0f)), 1, 8);
    const float dt = seconds / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        const Vec3 pull =
            Sub(Mul(Sub(target, state.sway), omega * omega), Mul(state.swayVelocity, 2.0f * body.swayDamping * omega));
        state.swayVelocity = Add(state.swayVelocity, Mul(pull, dt));
        state.sway = Add(state.sway, Mul(state.swayVelocity, dt));
    }
    // 撃たれた：後ろへ弾く（弾は正面から来る事が多い）。
    if (boss.brain.seenHits > state.seenHits)
        state.swayVelocity = Add(state.swayVelocity, Mul(Forward(boss.yaw), -body.hitKickDegrees * omega));
    state.seenHits = boss.brain.seenHits;
}

BossBodyPose BossBodyRig::Solve(const BossComponent& boss, const HIKARI::GameProjectTransform& bodyTransform) {
    BossBodyPose pose;
    pose.bladeHot = BossRules::IsBladeLit(boss);
    // 刀・手・籠手は、見せる姿勢（BossBodySword が進める）から置く。
    BossBodySword::Solve(boss, bodyTransform, pose);
    SolveArmor(boss, bodyTransform, pose);
    return pose;
}

} // namespace Project_TD2_2
