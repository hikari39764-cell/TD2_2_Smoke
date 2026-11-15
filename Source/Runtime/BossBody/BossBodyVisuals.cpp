#include "BossBodyVisuals.h"

#include <algorithm>
#include <string_view>

#include "HIKARI/Project/PresentationViews.h"
#include "BossBodyRig.h"
#include "../Boss/BossFrame.h"

namespace Project_TD2_2 {
namespace {

// 模型のノードの名前（模型側の契約。作るスクリプト Tools/BossBody と同じ名前にする）と、表示専用の模型の番号（持ち主の物体ごと）。
struct BossBodyPart {
    std::string_view node;
    uint64_t instance;
};
constexpr BossBodyPart kHilt{"KatanaHilt", 1u};
constexpr BossBodyPart kBlade{"KatanaBlade", 2u};
// 灼けた刀身は、ふつうの刀身と同じ番号で出す（差し替え。同じフレームに両方は出ない）。
constexpr BossBodyPart kBladeHot{"KatanaBladeHot", 2u};
constexpr BossBodyPart kRightHand{"HandR", 3u};
constexpr BossBodyPart kRightCuff{"Cuff", 4u};
constexpr BossBodyPart kLeftCuff{"Cuff", 5u};
constexpr uint64_t kLeftHandInstance = 6u;

std::string_view LeftHandNode(BossBodyHandShape shape) {
    switch (shape) {
    case BossBodyHandShape::Fist: return "HandL_Fist";
    case BossBodyHandShape::Seal: return "HandL_Seal";
    default: return "HandL_Open";
    }
}

void Submit(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, const BossBodyPart& part,
            const BossBodyPlacement& placement) {
    HIKARI::GameProjectPresentationModelRequest request{};
    request.ownerObjectHandle = object.GetHandle().value;
    request.instanceId = part.instance;
    const std::string& guid = boss.body.model;
    std::copy_n(guid.data(), std::min(guid.size(), sizeof(request.model.assetGuid) - 1), request.model.assetGuid);
    request.worldTransform.position = placement.position;
    request.worldTransform.rotation = placement.rotation;
    request.worldTransform.scale = {placement.scale, placement.scale, placement.scale};
    if (world.GetPresentationModels().SubmitExclusiveNode(request, part.node) || boss.bodyState.submitWarned)
        return;
    boss.bodyState.submitWarned = true;
    BossLog::Warn("[BossBody] 外形の模型を出せませんでした（Boss の「外形の模型」とノードの名前を確かめてください）");
}

} // namespace

void BossBodyVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss) {
    // 撃破の後に体が縮んで消える間は、外形も出さない。
    const bool vanishing = boss.state == BossState::Dead && boss.stateFrame > boss.deadVanishDelayFrames;
    if (!boss.body.visible || boss.body.model.empty() || vanishing)
        return;
    const BossBodyPose pose = BossBodyRig::Solve(boss);
    Submit(world, object, boss, kHilt, pose.sword);
    Submit(world, object, boss, pose.bladeHot ? kBladeHot : kBlade, pose.sword);
    Submit(world, object, boss, kRightHand, pose.rightHand);
    Submit(world, object, boss, kRightCuff, pose.rightCuff);
    Submit(world, object, boss, {LeftHandNode(pose.leftShape), kLeftHandInstance}, pose.leftHand);
    Submit(world, object, boss, kLeftCuff, pose.leftCuff);
}

} // namespace Project_TD2_2
