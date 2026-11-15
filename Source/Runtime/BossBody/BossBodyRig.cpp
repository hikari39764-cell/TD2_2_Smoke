#include "BossBodyRig.h"

#include <algorithm>
#include <cmath>

#include "../Boss/BossMath.h"
#include "../Boss/BossPose.h"

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

} // namespace

BossBodyPose BossBodyRig::Solve(const BossComponent& boss) {
    const BossBodySettings& body = boss.body;
    const BossSwordLine line = BossPose::SwordLine(boss, BossPose::Target(boss));
    const Vec3 forward = Forward(boss.yaw);
    const Vec3 spine = Add(Mul(forward, -kSpineBack), Vec3{0.0f, kSpineUp, 0.0f});
    const float scale =
        std::max(0.01f, boss.swordLength - body.swordGripAlong) / std::max(0.01f, body.swordModelLength);
    BossBodyPose pose;
    pose.sword.position = Add(line.root, Mul(line.direction, body.swordGripAlong));
    pose.sword.rotation = AimZ(line.direction, spine);
    pose.sword.scale = scale;
    pose.rightHand = pose.sword;
    pose.rightHand.scale = scale * body.handScale;
    return pose;
}

} // namespace Project_TD2_2
