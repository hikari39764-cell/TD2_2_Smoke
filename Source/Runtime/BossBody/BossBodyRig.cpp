#include "BossBodyRig.h"

#include <algorithm>
#include <cmath>

#include "BossBodyMath.h"
#include "BossBodySword.h"
#include "../Boss/BossRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;

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
    BossBodyPlacement Swinging(const BossBodyRigInput& in, Vec3 local, float yaw, float flareDegrees, int index) const {
        const Vec3 outward = Rotate(body.rotation, {std::sin(yaw), 0.0f, std::cos(yaw)});
        const Vec3 tangent = Rotate(body.rotation, {std::cos(yaw), 0.0f, -std::sin(yaw)});
        const Vec3 flat{in.sway.x, 0.0f, in.sway.z};
        const float flutter =
            in.settings.flutterDegrees * std::sin(2.0f * kPi * in.settings.flutterFrequency * in.clock +
                                                  kFlutterPhaseStep * static_cast<float>(index));
        const float flare =
            std::clamp(flareDegrees + Dot(flat, outward) + in.sway.y + flutter, kFlareMinDegrees, kFlareMaxDegrees);
        return Hanging(local, yaw, flare, in.settings.bodyScale, Dot(flat, tangent));
    }
};

// 角の丸い四角の輪郭の上の点（angle 0＝前、π/2＝左）。
Vec3 WaistPoint(float angle, float radiusSide, float radiusFront, float height) {
    const float s = std::sin(angle), c = std::cos(angle);
    const float e = 2.0f / kWaistOutlinePower;
    return {std::copysign(std::pow(std::abs(s), e), s) * radiusSide, height,
            std::copysign(std::pow(std::abs(c), e), c) * radiusFront};
}

void SolveArmor(const BossBodyRigInput& in, BossBodyPose& pose) {
    const BossBodySettings& body = in.settings;
    const BodyFrame frame{in.body, in.halfHeight};
    // 胴・兜・大袖は、刀の横の向きに付いて縦の軸まわりに捻る（BossBodySword が進める）。兜はさらにプレイヤーを向く。
    const float twist = in.twist;
    const float size = body.bodyScale;
    pose.torso = frame.Hanging(RotateY(BodyPoint(body, body.torsoCenter), twist), twist, 0.0f, size);
    pose.head =
        frame.Hanging(RotateY(BodyPoint(body, body.headCenter), twist), in.headYaw, 0.0f, body.headScale * size);
    const float sodeYaw = body.sodeYawDegrees * kRadiansPerDegree;
    // 右の袖は -X の側（外向きは -X 寄り）、左は +X の側。
    const Vec3 sodeHang = BodyPoint(body, body.sodeHang);
    pose.sode[0] = frame.Swinging(in, RotateY(sodeHang, twist), twist - sodeYaw, body.sodeFlareDegrees, 100);
    pose.sode[1] = frame.Swinging(in, RotateY({-sodeHang.x, sodeHang.y, sodeHang.z}, twist),
                                  twist + sodeYaw, body.sodeFlareDegrees, 101);
    pose.tassetCount = std::clamp(body.tassetCount, 0, kBossBodyTassetCapacity);
    // 草摺は腰に付くので、捻りの一部だけ。
    const float tassetTwist = twist * std::clamp(body.motion.tassetTwistShare, 0.0f, 1.0f);
    for (int k = 0; k < pose.tassetCount; ++k) {
        const float angle = 2.0f * kPi * static_cast<float>(k) / static_cast<float>(pose.tassetCount);
        pose.tassets[static_cast<size_t>(k)] = frame.Swinging(
            in,
            RotateY(BodyPoint(body, WaistPoint(angle, body.tassetRadiusSide, body.tassetRadiusFront,
                                               body.tassetHangHeight)),
                    tassetTwist),
            angle + tassetTwist, body.tassetFlareDegrees, k);
    }
    pose.eyesFire = in.phaseTwo;
    pose.eyesLit = in.eyesLit;
}

// 待機の構えの刀・握りの右手・右の籠手・左手（待機の置き場所）・左の籠手。本体の待機（BossBodySword・BossBodyLeftHand が
// 落ち着いた形）と同じ値で置く。分身は動きの状態を持たないので、ばね・釣り合いのずれは入れない。
void SolveIdleHands(const BossBodyRigInput& in, BossBodyPose& pose) {
    const BossBodySettings& s = in.settings;
    const BodyFrame frame{in.body, in.halfHeight};
    const auto direction = [&](Vec3 local) { return Normalize(Rotate(in.body.rotation, local), local); };
    Vec3 blade{}, grip{};
    IdleKamae(s.motion, in.phaseTwo, in.clock, blade, grip);
    grip = BodyPoint(s, grip);
    const Vec3 edge = Perpendicular(IdleShape(s.motion, in.phaseTwo).edge, blade);
    pose.sword.position = frame.Point(grip);
    pose.sword.rotation = AimZ(direction(blade), Mul(direction(edge), -1.0f));
    pose.sword.scale = in.swordScale;
    pose.rightHand = pose.sword;
    pose.rightHand.scale = in.handScale;
    const Vec3 rightShoulder = frame.Point(RotateY(BodyPoint(s, s.shoulder), in.twist));
    const Vec3 rightWrist = Add(pose.sword.position, Rotate(pose.sword.rotation, Mul(s.gripWrist, in.handScale)));
    const Vec3 gripElbow = Normalize(Rotate(pose.sword.rotation, s.gripElbow));
    const Vec3 rightElbow = Lerp(Normalize(Sub(rightShoulder, rightWrist), gripElbow), gripElbow, s.cuffGripBlend);
    pose.rightCuff = Cuff(rightWrist, rightElbow, Rotate(pose.sword.rotation, s.gripBack), in.handScale, s,
                          rightShoulder);
    const Vec3 back = direction(RotateY(s.leftRestBack, in.twist));
    pose.leftHand.position = frame.Point(RotateY(BodyPoint(s, s.leftRestWrist), in.twist));
    pose.leftHand.rotation = AimZ(direction(RotateY(s.leftRestFingers, in.twist)), back);
    pose.leftHand.scale = in.handScale;
    pose.leftShape = BossBodyHandShape::Open;
    const Vec3 shoulder = BodyPoint(s, s.shoulder);
    const Vec3 leftShoulder = frame.Point(RotateY({-shoulder.x, shoulder.y, shoulder.z}, in.twist));
    pose.leftCuff =
        Cuff(pose.leftHand.position, Sub(leftShoulder, pose.leftHand.position), back, in.handScale, s, leftShoulder);
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
    SolveArmor(InputOf(boss, bodyTransform), pose);
    pose.armorHot = boss.body.armorHot && PhaseTwoLook(boss);
    return pose;
}

BossBodyRigInput BossBodyRig::InputOf(const BossComponent& boss, const HIKARI::GameProjectTransform& body) {
    const BossBodyState& state = boss.bodyState;
    return {boss.body,
            body,
            boss.bodyHalfHeight,
            state.clock,
            state.sway,
            state.motion.twist,
            state.motion.headYaw,
            PhaseTwoLook(boss),
            boss.state != BossState::Stunned && boss.state != BossState::Dead,
            SwordScale(boss),
            HandScale(boss)};
}

BossBodyPose BossBodyRig::SolveIdle(const BossBodyRigInput& input) {
    BossBodyPose pose;
    // 分身は技を出さないので、刀が灼けるのはフェーズ 2 の常の分だけ。
    pose.bladeHot = input.phaseTwo;
    SolveIdleHands(input, pose);
    SolveArmor(input, pose);
    return pose;
}

} // namespace Project_TD2_2
