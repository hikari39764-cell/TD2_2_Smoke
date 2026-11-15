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

} // namespace

BossBodyPose BossBodyRig::Solve(const BossComponent& boss) {
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
    // 体の向きだけで回す（剣の線と同じく、体の傾きは入れない）。
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
    return pose;
}

} // namespace Project_TD2_2
