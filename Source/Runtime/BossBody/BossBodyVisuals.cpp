#include "BossBodyVisuals.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "HIKARI/Project/FluidView.h"
#include "BossAnimBody.h"
#include "BossBodyAccents.h"
#include "BossBodyMath.h"
#include "BossBodyRig.h"
#include "BossIntroVisuals.h"
#include "BossPhaseShiftVisuals.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossRules.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossVfx.h"
#include "../Common/FluidChannels.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

// 胴の煙のエフェクトの入力（資産側の契約）：出す量の倍率と、源の温度の倍率。
constexpr const char* kSmokeHeatInput = "input.heat";
// 胴の煙の入力：火の粉・熱のゆがみの粒・裾と首の熾火の源の量（フェーズ 1 は 0）。
constexpr const char* kSmokeBurnInput = "input.burn";
// 火の舌の注入の番号（ボスの物体ごと）：襟の右 2 本・左 2 本、右・左の袖口。
constexpr uint64_t kFireTongueBase = 1600;

// フェーズ 2 の燃える胴の強さ（0〜1）を進める：フェーズ 2 で付き、撃破で消える。気絶の間は弱める。一定の速さで寄せる
// （火の大きさが急に跳ばない）。
void StepFire(BossComponent& boss, float seconds) {
    const BossBodySettings& s = boss.body;
    float target = 0.0f;
    if (BossBodyMath::PhaseTwoLook(boss) && boss.state != BossState::Dead)
        target = boss.state == BossState::Stunned ? std::clamp(s.fireStunnedShare, 0.0f, 1.0f) : 1.0f;
    // 変わり目の演出（c-8dd296）：閃光より前から、内から火が回る。
    target = std::max(target, BossPhaseShiftVisuals::FireFloor(boss));
    float& fire = boss.bodyState.fire;
    const float step = s.fireRiseSeconds > 0.0f ? std::max(0.0f, seconds) / s.fireRiseSeconds : 1.0f;
    fire = fire < target ? std::min(target, fire + step) : std::max(target, fire - step);
}

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
    // 登場：組み上がる前は出さず（域も持たない）、組み上がりの始めは柱として濃く立つ。
    const float intro = BossIntroVisuals::SmokeScale(boss);
    if (intro <= 0.0f && !boss.bodyState.smokeVfx.IsValid())
        return;
    // フェーズ 2 の燃える胴（c-857ae3）：火の強さに合わせて、熱さを smokeHeatPhaseTwo へ寄せ、量を変え、火の粉と
    // 熱のゆがみを出す。フェーズ 1 は火の強さが 0 なので、いままでと同じ値を送る。
    const float fire = boss.bodyState.fire;
    VfxParameters::SetFloat(parameters, kAmountInput,
                            boss.body.smokeAmount * intro *
                                (stunned ? boss.body.smokeAmountStunned : accents.smokeAmount) *
                                BossMath::Lerp(1.0f, boss.body.smokeAmountPhaseTwo, fire));
    VfxParameters::SetFloat(parameters, kSmokeHeatInput,
                            BossMath::Lerp(stunned ? 1.0f : accents.smokeHeat, boss.body.smokeHeatPhaseTwo, fire));
    VfxParameters::SetFloat(parameters, kSmokeBurnInput, boss.body.fireBurn * fire);
    StartOrSend(world, boss, boss.bodyState.smokeVfx, boss.body.smokeEffect, transform, parameters);
}

// 火の舌を 1 つ入れる：胴の煙の域だけに届く燃料（燃え始める温度より熱くして入れるので、すぐ燃える）。燃料は昇りながら
// 燃えて熱と煤になり、熱い煤が光る（燃焼の値は胴の煙の資産の域）。冷めた煤は薄い黒い煙になって昇る。
void EmitTongue(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id,
                HIKARI::GameProjectFloat3 at, float share) {
    const BossBodySettings& s = boss.body;
    const float strength = s.fireTongueDensity * share * boss.bodyState.fire;
    if (strength <= 0.0f)
        return;
    HIKARI::GameProjectFluidInteraction emit{};
    emit.kind = HIKARI::GameProjectFluidInteractionKind::Fuel;
    emit.position = at;
    // extents.x は半径、extents.y は温度。
    emit.extents = {std::max(0.02f, s.fireTongueRadius * s.bodyScale), s.fireTongueHeat, 0.0f};
    emit.direction = {0.0f, s.fireTongueRise, 0.0f};
    emit.strength = std::min(10000.0f, strength);
    emit.channels = FluidChannels::kBodyFire;
    boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, emit).Accepted() && boss.fluidAccepted;
}

// 籠手の筒の口（手首 0 〜 肘の側 1 の割合 along）。筒は手首から局所 -Z へ伸びる。
HIKARI::GameProjectFloat3 CuffMouth(const BossBodySettings& s, const BossBodyPlacement& cuff, float along) {
    const float length = s.cuffModelLength * cuff.scale * cuff.lengthScale * along;
    return BossMath::Add(cuff.position, BossMath::Rotate(cuff.rotation, {0.0f, 0.0f, -length}));
}

// フェーズ 2 の火の舌：襟の両脇と両の袖口から、燃料を上へ吹き出す（見える鎧の口に付けて動かす）。瞬間移動した
// フレームと、霧へ沈んでいる間は入れない（移った線に沿って火を引かない。地面の下で燃えない）。
void UpdateFireTongues(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                       const BossBodyPose& pose) {
    const BossBodySettings& s = boss.body;
    BossBodyState& state = boss.bodyState;
    const bool teleported = boss.teleportCount != state.fireTeleports;
    state.fireTeleports = boss.teleportCount;
    // 変わり目の演出を飛ばしたフレームも瞬間移動と同じに扱う（膝をついた姿から構えへ一度に移るので）。
    if (state.fire <= 0.0f || teleported || BossPhaseShiftVisuals::HoldTongues(boss) || boss.hidden ||
        BossPose::RootHeight(boss) < -BossBodyMath::kTiny || s.smokeEffect.IsEmpty())
        return;
    // 襟：左右に 2 本ずつ（前後へ fireCollarSpread ずらす）。左は右を左右に反転した所。
    const float spread = 0.5f * s.fireCollarSpread;
    for (uint64_t k = 0; k < 4u; ++k) {
        const float side = k < 2u ? 1.0f : -1.0f;
        const float along = (k % 2u) == 0u ? spread : -spread;
        const HIKARI::GameProjectFloat3 local =
            BossMath::Mul({s.fireCollar.x * side, s.fireCollar.y, s.fireCollar.z + along}, pose.torso.scale);
        EmitTongue(world, object, boss, kFireTongueBase + k,
                   BossMath::Add(pose.torso.position, BossMath::Rotate(pose.torso.rotation, local)), 1.0f);
    }
    EmitTongue(world, object, boss, kFireTongueBase + 4u, CuffMouth(s, pose.rightCuff, s.fireCuffAlong),
               s.fireCuffShare);
    EmitTongue(world, object, boss, kFireTongueBase + 5u, CuffMouth(s, pose.leftCuff, s.fireCuffAlong),
               s.fireCuffShare);
}

// 左手の所作（手の光の演出 BossBodyAccents が読む）：いまの技の型から決める。手の形はいつも開いた手。
void StepLeftGesture(BossComponent& boss) {
    struct Row {
        BossMoveType type;
        BossBodyLeftGesture gesture;
    };
    constexpr Row kGestures[] = {
        {BossMoveType::Detonate, BossBodyLeftGesture::Detonate},  {BossMoveType::Knife, BossBodyLeftGesture::Knife},
        {BossMoveType::Clone, BossBodyLeftGesture::Seal},         {BossMoveType::Ambush, BossBodyLeftGesture::Seal},
        {BossMoveType::FlameRun, BossBodyLeftGesture::FlameWipe}, {BossMoveType::Retreat, BossBodyLeftGesture::Aim},
    };
    BossBodyMotionState& m = boss.bodyState.motion;
    m.leftGesture = BossBodyLeftGesture::Balance;
    m.leftShape = BossBodyHandShape::Open;
    const BossMove* move = boss.state == BossState::Act ? BossRules::GetMove(boss, boss.move) : nullptr;
    if (move == nullptr)
        return;
    for (const Row& row : kGestures) {
        if (row.type == move->type)
            m.leftGesture = row.gesture;
    }
}

} // namespace

void BossBodyVisuals::NoteTick(BossComponent& boss, HIKARI::GameProjectFloat3 position) {
    BossBodyState& state = boss.bodyState;
    // 瞬間移動した tick（霞隠れの現れる・分身との入れ替え）は補間しない。
    const bool jumped = boss.teleportCount != state.tickTeleports;
    state.tickTeleports = boss.teleportCount;
    state.tickFrom = state.ticksSeen > 0 && !jumped ? state.tickTo : position;
    state.tickTo = position;
    state.ticksSeen = std::min(state.ticksSeen + 1, 2);
}

void BossBodyVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds,
                             float alpha) {
    // 撃破の後に体が縮んで消える間は、外形も出さない。
    const bool vanishing = boss.state == BossState::Dead && boss.stateFrame > boss.deadVanishDelayFrames;
    HIKARI::GameProjectTransform body{};
    if (!boss.body.visible || vanishing || !object.GetWorldTransform(body)) {
        Stop(world, boss);
        return;
    }
    // 体の位置は tick ごとに変わるので、描画のフレームでは前の tick との間を補間した所に置く（ナレッジ k-c2a8a4。
    // エンジンが体を描くのと同じ）。向きは tick の値のまま（1 tick に変わる角度は小さい）。
    const BossBodyState& state = boss.bodyState;
    boss.bodyState.presentShift = {};
    if (boss.body.interpolate && state.ticksSeen >= 2)
        boss.bodyState.presentShift =
            BossMath::Mul(BossMath::Sub(state.tickFrom, state.tickTo), 1.0f - std::clamp(alpha, 0.0f, 1.0f));
    body.position = BossMath::Add(body.position, boss.bodyState.presentShift);
    BossIntroVisuals::Advance(world, boss, seconds);
    // 体に付く効果（胴の煙・気配・衝撃の輪）は見た目の体に付ける（登場の間は演技の体の根と向き。c-e0c500）。
    const HIKARI::GameProjectTransform shown = BossIntroVisuals::BodyTransform(boss, body);
    BossPhaseShiftVisuals::Advance(boss, seconds);
    BossBodyAccents::StepSmoke(boss, seconds);
    StepFire(boss, seconds);
    UpdateSmoke(world, boss, shown);
    StepLeftGesture(boss);
    // 体は骨格の体（戦いの間）か登場の役者が描く。ここは体に付く効果の置き場所を、その骨から受け取る（c-db1a15）。
    BossBodyPose pose = BossBodyRig::Base(boss, body);
    // 登場の間は、体に付く効果の置き場所（兜・刀・胴）を、演じている役者の骨のものに直す。
    BossIntroVisuals::ApplyPose(boss, pose, seconds);
    // フェーズの変わり目の演出の間の、刀身の灼け・刃口の光・気配。
    BossPhaseShiftVisuals::ApplyPose(boss, pose);
    // 登場の役者が出ている間は、骨格の体を出さない（役者が体を演じる）。
    const bool animated = !BossIntroVisuals::Showing(boss) && BossAnimBody::Update(world, boss, body, alpha, seconds);
    if (animated)
        BossAnimBody::ApplyPose(world, boss, alpha, seconds, pose);
    if (BossIntroVisuals::Showing(boss))
        BossAnimBody::Stop(world, boss);
    BossPhaseShiftVisuals::Render(world, object, boss, pose, seconds);
    UpdateFireTongues(world, object, boss, pose);
    BossBodyAccents::Update(world, boss, shown, pose, seconds);
}

void BossBodyVisuals::UpdatePreviews(HIKARI::WorldView& world) {
    // 演出ワークスペースがボスの登場の資産を流している間だけ、役者をその時刻の姿にする（流していない時は、触った
    // ものを元へ戻す＝ふだんの編集の見た目）。胴の煙・効果は出さない。
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType),
        [&](HIKARI::ObjectView, BossComponent& boss) { BossIntroVisuals::Preview(world, boss); });
}

void BossBodyVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    BossVfx::StopHandle(world, boss.bodyState.smokeVfx);
    BossBodyAccents::Stop(world, boss);
    BossPhaseShiftVisuals::Stop(world, boss);
    BossAnimBody::Stop(world, boss);
}

} // namespace Project_TD2_2
