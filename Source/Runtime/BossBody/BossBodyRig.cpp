#include "BossBodyRig.h"

#include <algorithm>

#include "BossBodyMath.h"
#include "../Boss/BossRules.h"

namespace Project_TD2_2 {

BossBodyPose BossBodyRig::Base(const BossComponent& boss, const HIKARI::GameProjectTransform& body) {
    using namespace BossBodyMath;
    BossBodyPose pose;
    pose.bladeHot = BossRules::IsBladeLit(boss);
    pose.armorHot = boss.body.armorHot && PhaseTwoLook(boss);
    pose.eyesFire = PhaseTwoLook(boss);
    pose.eyesLit = boss.state != BossState::Stunned && boss.state != BossState::Dead;
    const float size = boss.body.bodyScale * body.scale.x;
    const auto at = [&](float scale) {
        BossBodyPlacement placement;
        placement.position = body.position;
        placement.rotation = body.rotation;
        placement.scale = scale;
        return placement;
    };
    pose.sword = at(SwordScale(boss));
    pose.rightHand = pose.leftHand = pose.rightCuff = pose.leftCuff = at(HandScale(boss));
    pose.torso = at(size);
    pose.head = at(boss.body.headScale * size);
    pose.sode = {at(size), at(size)};
    pose.tassetCount = std::clamp(boss.body.tassetCount, 0, kBossBodyTassetCapacity);
    for (int k = 0; k < pose.tassetCount; ++k)
        pose.tassets[static_cast<size_t>(k)] = at(size);
    return pose;
}

} // namespace Project_TD2_2
