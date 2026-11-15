#include "BossBodyVisuals.h"

#include <algorithm>
#include <string_view>

#include "HIKARI/Project/PresentationViews.h"
#include "BossBodyRig.h"
#include "../Boss/BossFrame.h"

namespace Project_TD2_2 {
namespace {

// 模型のノードの名前（模型側の契約。作るスクリプトと同じ名前にする）と、表示専用の模型の番号（持ち主の物体ごと）。
struct BossBodyPart {
    std::string_view node;
    uint64_t instance;
};
constexpr BossBodyPart kSword{"Katana", 1u};
constexpr BossBodyPart kRightHand{"HandR", 2u};

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
    Submit(world, object, boss, kSword, pose.sword);
    Submit(world, object, boss, kRightHand, pose.rightHand);
}

} // namespace Project_TD2_2
