#include "BossBodyVisuals.h"

#include <algorithm>
#include <string_view>

#include "HIKARI/Project/PresentationViews.h"
#include "BossBodyAccents.h"
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
// 刀身に掛ける材質の効果（エンジンに組み込みの溶かす効果。溶かす量は 0 のまま、輪郭の発光だけを使う）と、その項目の名前。
constexpr std::string_view kBladeFxProfile = "HIKARI_Dissolve";
constexpr std::string_view kRimStrength = "rimStrength";
constexpr std::string_view kRimPower = "rimPower";
constexpr std::string_view kPulseSpeed = "pulseSpeed";
// 輪郭の光がこれより弱ければ、強さ 0 を送る。
constexpr float kBladeRimMinimum = 0.01f;

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
    // 技に合わせた量と熱さ（溜め・刀の火・振り終わりの吹き出し）は、演出の層（BossBodyAccents::StepSmoke）が決める。
    const BossBodyAccentState& accents = boss.bodyState.accents;
    VfxParameters::SetFloat(parameters, kAmountInput,
                            boss.body.smokeAmount * (stunned ? boss.body.smokeAmountStunned : accents.smokeAmount));
    VfxParameters::SetFloat(parameters, kSmokeHeatInput,
                            stunned                      ? 1.0f
                            : boss.lastPhaseNumber >= 2 ? boss.body.smokeHeatPhaseTwo
                                                         : accents.smokeHeat);
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
    request.worldTransform.scale = {placement.scale, placement.scale, placement.scale * placement.lengthScale};
    if (world.GetPresentationModels().SubmitExclusiveNode(request, part.node) || boss.bodyState.submitWarned)
        return;
    boss.bodyState.submitWarned = true;
    BossLog::Warn("[BossBody] 外形の模型を出せませんでした（Boss の「外形の模型」とノードの名前を確かめてください）");
}

// 刀身の輪郭の光の目標：両手の斬りの構え・溜め（前隙と、連撃の振りの間）で灯り、大きな溜めで増し、振る間は強い。
float BladeRimTarget(const BossComponent& boss, bool hot) {
    const BossBodyAccentSettings& a = boss.body.accents;
    if (boss.state != BossState::Act || boss.swordPose.twoHand < 0.5f)
        return 0.0f;
    float rim = 0.0f;
    if (boss.swordSmoke)
        rim = a.bladeRimSwing;
    else if (boss.phase != BossActPhase::Post)
        rim = a.bladeRimKamae + a.bladeRimCharge * std::max(0.0f, boss.chargeVisual);
    return rim * (hot ? a.bladeRimHotShare : 1.0f);
}

// 刀身（模型の刀身の番号）に材質の効果を掛け、輪郭の光を時間で寄せて送る。効果を掛ける前に同じフレームで刀身を頼んで
// あること（頼まれていない番号には掛けられない）。効果（プロファイル）は掛かっていない時だけ掛け、外さない：掛け外しや
// 掛け直しは描画の場面の作り直し（光の追跡の場面を含む）を毎フレーム起こして重い。光らない時は強さ 0 を送る。
void UpdateBladeShader(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, bool hot,
                       float seconds) {
    const BossBodyAccentSettings& a = boss.body.accents;
    float& rim = boss.bodyState.accents.bladeRim;
    const float target = BladeRimTarget(boss, hot);
    const float time = target > rim ? a.bladeRimRiseSeconds : a.bladeRimFallSeconds;
    rim += (target - rim) * (1.0f - std::exp(-std::max(0.0f, seconds) / std::max(1e-3f, time)));
    // 構えも振りも 0 なら効果を掛けない（材質のまま描く）。
    if (a.bladeRimKamae <= 0.0f && a.bladeRimSwing <= 0.0f)
        return;
    const HIKARI::MaterialFxView fx =
        world.GetPresentationModels().GetMaterialFx(object.GetHandle().value, kBlade.instance);
    if (!fx.IsValid())
        return;
    char current[64]{};
    if (fx.GetProfileIdSize() != kBladeFxProfile.size() || !fx.CopyProfileId(current, sizeof(current)) ||
        kBladeFxProfile != current) {
        if (!fx.SetProfileId(kBladeFxProfile))
            return;
        fx.SetFloat(kRimPower, a.bladeRimPower);
        fx.SetFloat(kPulseSpeed, a.bladeRimPulse);
    }
    fx.SetFloat(kRimStrength, rim < kBladeRimMinimum ? 0.0f : rim);
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
    BossBodyAccents::StepSmoke(boss, seconds);
    UpdateSmoke(world, boss, body);
    if (boss.body.model.empty())
        return;
    BossBodyRig::StepSway(boss, body, seconds);
    BossBodySword::Step(boss, seconds);
    const BossBodyPose pose = BossBodyRig::Solve(boss, body);
    BossBodySword::Trace(boss, body, pose);
    SubmitPose(world, object, boss, pose);
    UpdateBladeShader(world, object, boss, pose.bladeHot, seconds);
    BossBodyAccents::Update(world, boss, body, pose, seconds);
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
    grip = BossBodyMath::Mul(grip, boss.body.bodyScale);
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
    BossBodyAccents::Stop(world, boss);
}

} // namespace Project_TD2_2
