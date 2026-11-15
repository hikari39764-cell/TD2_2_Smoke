#include "BossBodyVisuals.h"

#include <algorithm>
#include <string_view>

#include "HIKARI/Project/PresentationViews.h"
#include "BossBodyMath.h"
#include "BossBodyRig.h"
#include "BossBodySword.h"
#include "../Boss/BossCloneComponent.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossVfx.h"
#include "../Common/VfxParameters.h"

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
// 胴の煙のエフェクトの入力（資産側の契約）：出す量の倍率と、源の温度の倍率。
constexpr const char* kSmokeHeatInput = "input.heat";

// 胴の煙：体の足元に付けて出し続ける。量と熱さは、気絶・フェーズで変える。
void UpdateSmoke(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body) {
    using namespace BossVfx;
    if (boss.body.smokeEffect.IsEmpty())
        return;
    // 体の物体の原点は体の中心なので、足元まで下げる（エフェクトの原点＝足元）。域の大きさは拡大の影響を受けない。
    HIKARI::GameProjectTransform transform{};
    transform.rotation = body.rotation;
    transform.position = BossMath::Add(
        body.position, BossMath::Rotate(body.rotation, {0.0f, -boss.bodyHalfHeight * body.scale.y, 0.0f}));
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    const bool stunned = boss.state == BossState::Stunned || boss.state == BossState::Dead;
    VfxParameters::SetFloat(parameters, kAmountInput,
                            boss.body.smokeAmount * (stunned ? boss.body.smokeAmountStunned : 1.0f));
    VfxParameters::SetFloat(parameters, kSmokeHeatInput,
                            boss.lastPhaseNumber >= 2 && !stunned ? boss.body.smokeHeatPhaseTwo : 1.0f);
    StartOrSend(world, boss, boss.bodyState.smokeVfx, boss.body.smokeEffect, transform, parameters);
}

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
    request.castShadow = boss.body.castShadow ? 1u : 0u;
    request.worldTransform.position = placement.position;
    request.worldTransform.rotation = placement.rotation;
    request.worldTransform.scale = {placement.scale, placement.scale, placement.scale};
    if (world.GetPresentationModels().SubmitExclusiveNode(request, part.node) || boss.bodyState.submitWarned)
        return;
    boss.bodyState.submitWarned = true;
    BossLog::Warn("[BossBody] 外形の模型を出せませんでした（Boss の「外形の模型」とノードの名前を確かめてください）");
}

// 置き場所の一式を、持ち主の物体（本体・分身）の表示専用の模型として頼む。番号は持ち主ごとなので、本体と分身で
// 同じ番号を使っても重ならない。
void SubmitPose(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, const BossBodyPose& pose) {
    Submit(world, object, boss, kHilt, pose.sword);
    Submit(world, object, boss, pose.bladeHot ? kBladeHot : kBlade, pose.sword);
    Submit(world, object, boss, kRightHand, pose.rightHand);
    Submit(world, object, boss, kRightCuff, pose.rightCuff);
    Submit(world, object, boss, {LeftHandNode(pose.leftShape), kLeftHandInstance}, pose.leftHand);
    Submit(world, object, boss, kLeftCuff, pose.leftCuff);
    Submit(world, object, boss, kTorso, pose.torso);
    Submit(world, object, boss, kHead, pose.head);
    if (pose.eyesLit)
        Submit(world, object, boss, pose.eyesFire ? kEyesFire : kEyesMist, pose.head);
    for (size_t side = 0; side < pose.sode.size(); ++side)
        Submit(world, object, boss, {kSodeNode, kSodeInstance + side}, pose.sode[side]);
    for (int k = 0; k < pose.tassetCount; ++k)
        Submit(world, object, boss, {kTassetNode, kTassetInstance + static_cast<uint64_t>(k)},
               pose.tassets[static_cast<size_t>(k)]);
}

} // namespace

void BossBodyVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds) {
    // 撃破の後に体が縮んで消える間は、外形も出さない。
    const bool vanishing = boss.state == BossState::Dead && boss.stateFrame > boss.deadVanishDelayFrames;
    HIKARI::GameProjectTransform body{};
    if (!boss.body.visible || vanishing || !object.GetWorldTransform(body)) {
        Stop(world, boss);
        return;
    }
    UpdateSmoke(world, boss, body);
    if (boss.body.model.empty())
        return;
    BossBodyRig::StepSway(boss, body, seconds);
    BossBodySword::Step(boss, seconds);
    const BossBodyPose pose = BossBodyRig::Solve(boss, body);
    BossBodySword::Trace(boss, pose);
    SubmitPose(world, object, boss, pose);
}

void BossBodyVisuals::UpdateClone(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                                  BossCloneComponent& clone, float seconds) {
    // 出ていない分身は地面の下にしまってあり、何も頼まない（頼まなかったフレームは消える）。
    HIKARI::GameProjectTransform body{};
    if (!clone.active || !boss.body.visible || boss.body.model.empty() || !object.GetWorldTransform(body))
        return;
    clone.bodyClock += std::max(0.0f, seconds);
    // 待機の構えの入力：分身の物体の変換（霧から浮かび上がる高さ・向き）と、分身の時計。振れは持たず、
    // 大袖・草摺は揺らぎだけ。胴の捻りは本体の待機と同じく刀の横の向きから。
    HIKARI::GameProjectFloat3 blade{}, grip{};
    BossBodyMath::IdleKamae(boss.body.motion, clone.bodyClock, blade, grip);
    const BossBodyRigInput input{boss.body,
                                 body,
                                 boss.bodyHalfHeight,
                                 clone.bodyClock,
                                 {},
                                 BossBodyMath::TwistTarget(boss.body.motion, blade),
                                 0.0f,
                                 boss.lastPhaseNumber >= 2,
                                 true,
                                 BossBodyMath::SwordScale(boss),
                                 BossBodyMath::HandScale(boss)};
    SubmitPose(world, object, boss, BossBodyRig::SolveIdle(input));
}

void BossBodyVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    BossVfx::StopHandle(world, boss.bodyState.smokeVfx);
}

} // namespace Project_TD2_2
