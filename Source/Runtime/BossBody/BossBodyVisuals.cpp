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
constexpr BossBodyPart kTorso{"Do", 7u};
constexpr BossBodyPart kHead{"Kabuto", 8u};
// 目の光は、霞の色と火の色を同じ番号で差し替える。
constexpr BossBodyPart kEyesMist{"EyesMist", 9u};
constexpr BossBodyPart kEyesFire{"EyesFire", 9u};
constexpr std::string_view kSodeNode = "Sode";
constexpr uint64_t kSodeInstance = 10u;
// 草摺は、同じノードを枚数ぶん、この番号から続けて出す。
constexpr std::string_view kTassetNode = "KusazuriPanel";
constexpr uint64_t kTassetInstance = 20u;

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
    HIKARI::GameProjectTransform body{};
    if (!object.GetWorldTransform(body))
        return;
    const BossBodyPose pose = BossBodyRig::Solve(boss, body);
    Submit(world, object, boss, kHilt, pose.sword);
    Submit(world, object, boss, pose.bladeHot ? kBladeHot : kBlade, pose.sword);
    Submit(world, object, boss, kRightHand, pose.rightHand);
    Submit(world, object, boss, kRightCuff, pose.rightCuff);
    Submit(world, object, boss, {LeftHandNode(pose.leftShape), kLeftHandInstance}, pose.leftHand);
    Submit(world, object, boss, kLeftCuff, pose.leftCuff);
    Submit(world, object, boss, kTorso, pose.torso);
    Submit(world, object, boss, kHead, pose.head);
    Submit(world, object, boss, pose.eyesFire ? kEyesFire : kEyesMist, pose.head);
    for (size_t side = 0; side < pose.sode.size(); ++side)
        Submit(world, object, boss, {kSodeNode, kSodeInstance + side}, pose.sode[side]);
    for (int k = 0; k < pose.tassetCount; ++k)
        Submit(world, object, boss, {kTassetNode, kTassetInstance + static_cast<uint64_t>(k)},
               pose.tassets[static_cast<size_t>(k)]);
}

} // namespace Project_TD2_2
