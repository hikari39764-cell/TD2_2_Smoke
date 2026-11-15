#include "BossBodyRig.h"

#include <algorithm>
#include <cmath>

#include "../Boss/BossMath.h"
#include "../Boss/BossPose.h"
#include "../Boss/BossRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 棟（刃の反対）を向ける先：ボスの後ろ上。刃はその反対（前下）を向くので、縦の振りで刃が先に進む。
// 横の振りで刃を振る向きへ立てるのは、姿勢を作り直すカード（c-a08a28）でやる。
constexpr float kSpineBack = 1.0f;
constexpr float kSpineUp = 1.0f;
// 草摺を吊る腰の輪郭：角の丸い四角（模型を作るスクリプト bb_armor.superpoint と同じ指数）。
constexpr float kWaistOutlinePower = 2.6f;

// 局所 +Z を direction、+Y を spine（direction に直交する成分）へ向ける回転。
Quat AimZ(Vec3 direction, Vec3 spine) {
    const Vec3 side = Normalize(Cross(spine, direction), {1.0f, 0.0f, 0.0f});
    return BasisRotation(side, Cross(direction, side));
}

// 手首に付く籠手の筒：筒は手首から肘の向き（局所 -Z）へ伸び、腕の甲（局所 +Y）は手の甲の側を向く。
BossBodyPlacement Cuff(Vec3 wrist, Vec3 elbow, Vec3 back, float scale) {
    BossBodyPlacement cuff;
    cuff.position = wrist;
    cuff.rotation = AimZ(Mul(Normalize(elbow), -1.0f), back);
    cuff.scale = scale;
    return cuff;
}

Quat AroundY(float radians) { return {0.0f, std::sin(radians * 0.5f), 0.0f, std::cos(radians * 0.5f)}; }
Quat AroundX(float radians) { return {std::sin(radians * 0.5f), 0.0f, 0.0f, std::cos(radians * 0.5f)}; }

// 体の物体の変換で、体の局所（足元が原点）の点・向きを世界へ移す。体の物体の原点は体の中心。
struct BodyFrame {
    const HIKARI::GameProjectTransform& body;
    float halfHeight;

    Vec3 Point(Vec3 local) const {
        const Vec3 centered{local.x * body.scale.x, (local.y - halfHeight) * body.scale.y, local.z * body.scale.z};
        return Add(body.position, Rotate(body.rotation, centered));
    }
    // yaw で外へ向け、flare で裾を外へ開く部品（大袖・草摺）。
    BossBodyPlacement Hanging(Vec3 local, float yaw, float flareDegrees, float scale = 1.0f) const {
        BossBodyPlacement placement;
        placement.position = Point(local);
        placement.rotation =
            Multiply(body.rotation, Multiply(AroundY(yaw), AroundX(-flareDegrees * kRadiansPerDegree)));
        placement.scale = scale * body.scale.x;
        return placement;
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
    pose.torso = frame.Hanging(body.torsoCenter, 0.0f, 0.0f);
    pose.head = frame.Hanging(body.headCenter, 0.0f, 0.0f, body.headScale);
    const float sodeYaw = body.sodeYawDegrees * kRadiansPerDegree;
    // 右の袖は -X の側（外向きは -X 寄り）、左は +X の側。
    pose.sode[0] = frame.Hanging(body.sodeHang, -sodeYaw, body.sodeFlareDegrees);
    pose.sode[1] = frame.Hanging({-body.sodeHang.x, body.sodeHang.y, body.sodeHang.z}, sodeYaw, body.sodeFlareDegrees);
    pose.tassetCount = std::clamp(body.tassetCount, 0, kBossBodyTassetCapacity);
    for (int k = 0; k < pose.tassetCount; ++k) {
        const float angle = 2.0f * kPi * static_cast<float>(k) / static_cast<float>(pose.tassetCount);
        pose.tassets[static_cast<size_t>(k)] =
            frame.Hanging(WaistPoint(angle, body.tassetRadiusSide, body.tassetRadiusFront, body.tassetHangHeight),
                          angle, body.tassetFlareDegrees);
    }
    pose.eyesFire = boss.lastPhaseNumber >= 2;
}

} // namespace

BossBodyPose BossBodyRig::Solve(const BossComponent& boss, const HIKARI::GameProjectTransform& bodyTransform) {
    const BossBodySettings& body = boss.body;
    const BossSwordLine line = BossPose::SwordLine(boss, BossPose::Target(boss));
    const Vec3 forward = Forward(boss.yaw);
    const Vec3 spine = Add(Mul(forward, -kSpineBack), Vec3{0.0f, kSpineUp, 0.0f});
    const float scale =
        std::max(0.01f, boss.swordLength - body.swordGripAlong) / std::max(0.01f, body.swordModelLength);
    const float handScale = scale * body.handScale;
    BossBodyPose pose;
    pose.bladeHot = BossRules::IsBladeLit(boss);
    pose.sword.position = Add(line.root, Mul(line.direction, body.swordGripAlong));
    pose.sword.rotation = AimZ(line.direction, spine);
    pose.sword.scale = scale;
    pose.rightHand = pose.sword;
    pose.rightHand.scale = handScale;
    // 手は体の向きだけで回す（剣の線と同じく、体の傾きは入れない）。
    const Vec3 root{boss.ground.x, boss.ground.y + BossPose::RootHeight(boss), boss.ground.z};
    const Vec3 rightShoulder = Add(root, RotateY(body.shoulder, boss.yaw));
    const Vec3 leftShoulder = Add(root, RotateY({-body.shoulder.x, body.shoulder.y, body.shoulder.z}, boss.yaw));
    // 右の籠手の筒：握りの手の模型の手首（刀の座標）に付け、肩の方へ伸ばす。手の模型の肘の向きを少し混ぜて、
    // 手首が不自然に折れないようにする。
    const Vec3 rightWrist = Add(pose.sword.position, Rotate(pose.sword.rotation, Mul(body.gripWrist, handScale)));
    const Vec3 gripElbow = Normalize(Rotate(pose.sword.rotation, body.gripElbow));
    const Vec3 rightElbow = Lerp(Normalize(Sub(rightShoulder, rightWrist), gripElbow), gripElbow, body.cuffGripBlend);
    pose.rightCuff = Cuff(rightWrist, rightElbow, Rotate(pose.sword.rotation, body.gripBack), handScale);
    // 左手：何もしていない時の置き場所。
    pose.leftHand.position = Add(root, RotateY(body.leftRestWrist, boss.yaw));
    pose.leftHand.rotation =
        AimZ(Normalize(RotateY(body.leftRestFingers, boss.yaw)), RotateY(body.leftRestBack, boss.yaw));
    pose.leftHand.scale = handScale;
    pose.leftCuff = Cuff(pose.leftHand.position, Sub(leftShoulder, pose.leftHand.position),
                         RotateY(body.leftRestBack, boss.yaw), handScale);
    SolveArmor(boss, bodyTransform, pose);
    return pose;
}

} // namespace Project_TD2_2
