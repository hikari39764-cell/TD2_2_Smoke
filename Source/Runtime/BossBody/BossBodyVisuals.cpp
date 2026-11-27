#include "BossBodyVisuals.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

#include "HIKARI/Project/FluidView.h"
#include "BossAnimBody.h"
#include "BossBodyAccents.h"
#include "BossBodyMath.h"
#include "BossBodyRig.h"
#include "BossCinematicFx.h"
#include "BossIntroVisuals.h"
#include "BossPhaseShiftVisuals.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossPhaseShift.h"
#include "../Boss/BossRules.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossVfx.h"
#include "../Common/ComponentLookup.h"
#include "../Common/FluidChannels.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

// 胴の煙のエフェクトの入力（資産側の契約）：出す量の倍率と、源の温度の倍率。
constexpr const char* kSmokeHeatInput = "input.heat";
// 胴の煙の入力：火の粉・熱のゆがみの粒・裾と首の熾火の源の量（フェーズ 1 は 0）。
constexpr const char* kSmokeBurnInput = "input.burn";
// 胴の煙の入力：立ち姿の肩の横に固定の「両の袖口」の源の量。骨格の体が出ている間は 0（腕に付かず、空中に煙を置くので。
// 腕の煙は、骨に付いて行く煙が受け持つ）。
constexpr const char* kSmokeArmsInput = "input.arms";
// 火の舌の注入の番号（ボスの物体ごと）：襟の右 2 本・左 2 本、右・左の袖口、腰の周りの 3 本（+6..8）。
constexpr uint64_t kFireTongueBase = 1600;
// 骨に付いて行く煙（背骨 3・両腕 4 ずつ）の提出 ID（1620〜1630）、温度、背骨の煙が昇る速さ (m/秒)、骨の上の置き場所の割合。
constexpr uint64_t kBoneSmokeBase = 1620;
constexpr float kBoneSmokeHeat = 0.3f;
constexpr float kSpineRise = 0.3f;
constexpr std::array<float, 3> kSpineShares{0.2f, 0.55f, 0.9f};
constexpr std::array<float, 2> kUpperArmShares{0.3f, 0.75f};
constexpr std::array<float, 2> kForeArmShares{0.2f, 0.65f};
// 待機の姿（Idle_Shoulder）の、腰 → 胸の骨（Spine2）の向き（模型の空間。長さ 1。胴の煙の域を倒す角度の 0 の基準。
// 模型か待機の姿を作り直したら測り直す）。
constexpr HIKARI::GameProjectFloat3 kSpineRest{0.025f, 0.9962f, -0.0837f};
// 同じ待機の姿の、腰 → 頭の骨の向き（模型の空間。長さ 1。戦いの間、この向きからの回りで域を倒す。測り直しは同じ時に）。
constexpr HIKARI::GameProjectFloat3 kHeadRest{-0.0125f, 0.9999f, 0.003f};
// 胴の煙の域を倒す重みを上げ下げする速さ（1/秒。演出の始めと終わりで急に切り替えない）。
constexpr float kTiltWeightRate = 3.0f;
// 域の軸と向きの追い方：目標との差がここまでは細かい揺れ（ゆっくり寄せる）、ここからは体の動き（速く追う）。
// 軸は距離 (m)、向きは角度 (rad)。軸の速い側は、倒す向きの 2 倍の速さ（打ち退け・踏み込みで煙が体から離れ過ぎない）。
constexpr float kPivotCalm = 0.12f;
constexpr float kPivotBusy = 0.5f;
constexpr float kTurnCalm = 0.035f;
constexpr float kTurnBusy = 0.16f;
constexpr float kPivotFollowScale = 2.0f;

// 戦いの間、胴の煙の域の軸（腰）と向き（背骨）を、体（根）がこのフレームに動いた分だけ先に運ぶ（胴の煙を置く前に呼ぶ）。
// 踏み込み・かわし・向き変えで、軸が体から遅れない：なめらかに追うのは、体の中での腰と背骨の動きだけになる。変わり目の
// 演出で体を据えている間は運ばない（前のまま、世界の中でなめらかに追う）。body は、胴の煙を付ける体の置き場所。
void CarrySmokeTilt(BossBodyComponent& look, const BossPhaseShiftComponent* phaseShift,
                    const HIKARI::GameProjectTransform& body) {
    BossAnimBodyState& anim = look.anim;
    if (anim.smokePivotValid && anim.smokeBodyValid && !BossPhaseShift::Pinned(phaseShift)) {
        const HIKARI::GameProjectQuaternion turn =
            BossMath::Multiply(body.rotation, BossMath::Conjugate(anim.smokeBodyRotation));
        anim.smokePivot = BossMath::Add(body.position,
                                        BossMath::Rotate(turn, BossMath::Sub(anim.smokePivot, anim.smokeBodyPosition)));
        anim.smokeSpine = BossMath::Normalize(BossMath::Rotate(turn, anim.smokeSpine), {0.0f, 1.0f, 0.0f});
    }
    anim.smokeBodyPosition = body.position;
    anim.smokeBodyRotation = body.rotation;
    anim.smokeBodyValid = true;
}

// 胴の煙の域を倒す向きと重みを進める：向きは腰 → 胸をなめらかに追う。重みは、変わり目の演出で体を据えている間は 1、
// 戦いの間（骨格の体が出ている間）は smokeTiltCombat。
void StepSmokeTilt(BossBodyComponent& look, const BossPhaseShiftComponent* phaseShift, bool animated, float seconds) {
    BossAnimBodyState& anim = look.anim;
    const BossBodySettings& s = look.settings;
    const float dt = std::max(0.0f, seconds);
    const bool seen = animated && anim.hipsSeen;
    const bool pinned = BossPhaseShift::Pinned(phaseShift);
    const float step = dt * kTiltWeightRate;
    const float target = !seen ? 0.0f : pinned ? 1.0f : std::clamp(s.smokeTiltCombat, 0.0f, 1.0f);
    anim.smokeTiltWeight = anim.smokeTiltWeight < target ? std::min(target, anim.smokeTiltWeight + step)
                                                         : std::max(target, anim.smokeTiltWeight - step);
    if (!seen) {
        anim.smokePivotValid = false;
        return;
    }
    // 域が細かく動くと、エンジンがその加速度を煙へ逆向きに入れ、煙全体が揺れ続ける。動捕の細かい揺れは追わず、
    // 体が大きく動いた時だけ速く追う（戦いの間は、技の屈みに間に合うよう smokeTiltCombatFollow で）。
    const float calm = std::max(0.0f, s.smokeCalmFollow);
    const float busy = std::max(calm, pinned ? s.smokeTiltFollow : s.smokeTiltCombatFollow);
    const auto follow = [dt](float rate) { return 1.0f - std::exp(-dt * rate); };
    if (!anim.smokePivotValid) {
        anim.smokePivot = anim.hipsWorld;
        anim.smokePivotValid = true;
    }
    const HIKARI::GameProjectFloat3 toHips = BossMath::Sub(anim.hipsWorld, anim.smokePivot);
    const float pivotRate =
        BossMath::Lerp(calm, busy * kPivotFollowScale,
                       BossMath::Smooth((BossMath::Length(toHips) - kPivotCalm) / (kPivotBusy - kPivotCalm)));
    anim.smokePivot = BossMath::Add(anim.smokePivot, BossMath::Mul(toHips, follow(pivotRate)));
    HIKARI::GameProjectFloat3 spine =
        BossMath::Normalize(BossMath::Sub(anim.chestWorld, anim.hipsWorld), {0.0f, 1.0f, 0.0f});
    if (!pinned && anim.smokeBodyValid) {
        // 戦いの間：腰 → 胸（Spine2）は深く屈んでも 20° ほどしか倒れず、見えている鎧の上半身（胸当て・兜）に煙の柱が
        // 付いて行かない。腰 → 頭が待機の姿から回った分を、待機の背骨の向きに掛ける（待機の姿では倒さない）。
        const HIKARI::GameProjectFloat3 from = BossMath::Rotate(anim.smokeBodyRotation, kHeadRest);
        const HIKARI::GameProjectFloat3 to =
            BossMath::Normalize(BossMath::Sub(anim.headWorld, anim.hipsWorld), {0.0f, 1.0f, 0.0f});
        const HIKARI::GameProjectFloat3 axis = BossMath::Cross(from, to);
        const float sine = BossMath::Length(axis);
        spine = BossMath::Rotate(anim.smokeBodyRotation, kSpineRest);
        if (sine > 1.0e-4f) {
            const float half = 0.5f * BossMath::AngleBetween(from, to);
            const float scale = std::sin(half) / sine;
            spine = BossMath::Normalize(
                BossMath::Rotate({axis.x * scale, axis.y * scale, axis.z * scale, std::cos(half)}, spine),
                {0.0f, 1.0f, 0.0f});
        }
    }
    const float turnRate = BossMath::Lerp(
        calm, busy,
        BossMath::Smooth((BossMath::AngleBetween(anim.smokeSpine, spine) - kTurnCalm) / (kTurnBusy - kTurnCalm)));
    anim.smokeSpine = BossMath::Normalize(
        BossMath::Add(anim.smokeSpine, BossMath::Mul(BossMath::Sub(spine, anim.smokeSpine), follow(turnRate))),
        {0.0f, 1.0f, 0.0f});
}

// 胴の煙の域を、腰を軸にして背骨の向き（腰 → 胸）へ倒す。feet は倒す前の域の置き場所（足元。向きは体の向き）。
// 域の中の煙は域と一緒に回り（エンジンの局所空間の域）、浮力は世界の上向きのまま：後ろへ反る・屈む体に、煙の柱が
// 付いて行く。
void TiltSmoke(const BossBodyComponent& look, HIKARI::GameProjectTransform& feet) {
    const BossBodySettings& s = look.settings;
    const float weight = look.anim.smokeTiltWeight * std::clamp(s.smokeTilt, 0.0f, 1.0f);
    if (!(weight > 0.0f) || !look.anim.smokePivotValid)
        return;
    const HIKARI::GameProjectFloat3 up{0.0f, s.smokeHipsHeight, 0.0f};
    const HIKARI::GameProjectFloat3 rest = BossMath::Rotate(feet.rotation, kSpineRest);
    const HIKARI::GameProjectFloat3 axis = BossMath::Cross(rest, look.anim.smokeSpine);
    const float sine = BossMath::Length(axis);
    HIKARI::GameProjectQuaternion rotation = feet.rotation;
    if (sine > 1.0e-4f) {
        const float angle = std::min(BossMath::AngleBetween(rest, look.anim.smokeSpine),
                                     std::max(0.0f, s.smokeTiltMaxDegrees) * BossMath::kRadiansPerDegree) *
                            weight;
        const float half = std::sin(angle * 0.5f) / sine;
        rotation =
            BossMath::Multiply({axis.x * half, axis.y * half, axis.z * half, std::cos(angle * 0.5f)}, feet.rotation);
    }
    // 腰を軸に：資産の中の腰の高さの点が、なめらかに追った腰の所に重なる所へ（重みで、倒す前の置き場所から寄せる）。
    const HIKARI::GameProjectFloat3 pivot =
        BossMath::Lerp(BossMath::Add(feet.position, BossMath::Rotate(feet.rotation, up)), look.anim.smokePivot, weight);
    feet.position = BossMath::Sub(pivot, BossMath::Rotate(rotation, up));
    feet.rotation = rotation;
}

// フェーズ 2 の燃える胴の強さ（0〜1）を進める：フェーズ 2 で付き、撃破で消える。気絶の間は弱める。一定の速さで寄せる
// （火の大きさが急に跳ばない）。
void StepFire(const BossComponent& boss, BossBodyComponent& look, const BossPhaseShiftComponent* phaseShift,
              float seconds) {
    const BossBodySettings& s = look.settings;
    float target = 0.0f;
    if (BossBodyMath::PhaseTwoLook(boss) && boss.state != BossState::Dead)
        target = boss.state == BossState::Stunned ? std::clamp(s.fireStunnedShare, 0.0f, 1.0f) : 1.0f;
    // 変わり目の演出（c-8dd296）：閃光より前から、内から火が回る。
    target = std::max(target, BossPhaseShiftVisuals::FireFloor(phaseShift));
    float& fire = look.state.fire;
    const float step = s.fireRiseSeconds > 0.0f ? std::max(0.0f, seconds) / s.fireRiseSeconds : 1.0f;
    fire = fire < target ? std::min(target, fire + step) : std::max(target, fire - step);
}

// 胴の煙：体の足元に付けて出し続ける。量と熱さは、気絶・フェーズで変える。
void UpdateSmoke(HIKARI::WorldView& world, BossComponent& boss, BossBodyComponent& look,
                 const BossIntroComponent* intro, const BossPhaseShiftComponent* phaseShift,
                 const HIKARI::GameProjectTransform& body) {
    using namespace BossVfx;
    const BossBodySettings& s = look.settings;
    if (s.smokeEffect.IsEmpty())
        return;
    // 体の物体の原点は体の中心なので、足元まで下げる（エフェクトの原点＝足元）。域の大きさは拡大の影響を受けない。
    HIKARI::GameProjectTransform transform{};
    transform.rotation = body.rotation;
    transform.position = BossMath::Add(
        body.position, BossMath::Rotate(body.rotation, {0.0f, -boss.bodyHalfHeight * body.scale.y, 0.0f}));
    TiltSmoke(look, transform);
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    const bool stunned = boss.state == BossState::Stunned || boss.state == BossState::Dead;
    // 技に合わせた量と熱さ（溜め・刀の火・振り終わりの吹き出し）は、演出の層（BossBodyAccents::StepSmoke）が決める。
    const BossBodyAccentState& accents = look.state.accents;
    // 登場：組み上がる前は出さず（域も持たない）、組み上がりの始めは柱として濃く立つ。
    const float rise = BossIntroVisuals::SmokeScale(intro);
    if (rise <= 0.0f && !look.state.smokeVfx.IsValid())
        return;
    // フェーズ 2 の燃える胴（c-857ae3）：火の強さに合わせて、熱さを smokeHeatPhaseTwo へ寄せ、量を変え、火の粉と
    // 熱のゆがみを出す。フェーズ 1 は火の強さが 0 なので、いままでと同じ値を送る。
    const float fire = look.state.fire;
    const float amount = s.smokeAmount * rise * (stunned ? s.smokeAmountStunned : accents.smokeAmount) *
                         BossMath::Lerp(1.0f, s.smokeAmountPhaseTwo, fire) *
                         BossPhaseShiftVisuals::SmokeScale(phaseShift);
    VfxParameters::SetFloat(parameters, kAmountInput, amount);
    // 立ち姿の位置に固定の「両の袖口」の源は、骨格の体が出ている間は止める（腕の煙は、骨に付いて行く煙が出す）。
    VfxParameters::SetFloat(parameters, kSmokeArmsInput, look.anim.active && look.anim.hipsSeen ? 0.0f : amount);
    VfxParameters::SetFloat(parameters, kSmokeHeatInput,
                            BossMath::Lerp(stunned ? 1.0f : accents.smokeHeat, s.smokeHeatPhaseTwo, fire));
    VfxParameters::SetFloat(parameters, kSmokeBurnInput,
                            s.fireBurn * fire * BossPhaseShiftVisuals::BurnBoost(phaseShift));
    StartOrSend(world, boss, look.state.smokeVfx, s.smokeEffect, transform, parameters);
}

// 火の舌を 1 つ入れる：胴の煙の域だけに届く燃料（燃え始める温度より熱くして入れるので、すぐ燃える）。燃料は昇りながら
// 燃えて熱と煤になり、熱い煤が光る（燃焼の値は胴の煙の資産の域）。冷めた煤は薄い黒い煙になって昇る。
void EmitTongue(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, const BossBodyComponent& look,
                uint64_t id, HIKARI::GameProjectFloat3 at, float share, float radiusScale = 1.0f) {
    const BossBodySettings& s = look.settings;
    const float strength = s.fireTongueDensity * share * look.state.fire;
    if (strength <= 0.0f)
        return;
    HIKARI::GameProjectFluidInteraction emit{};
    emit.kind = HIKARI::GameProjectFluidInteractionKind::Fuel;
    emit.position = at;
    // extents.x は半径、extents.y は温度。
    emit.extents = {std::max(0.02f, s.fireTongueRadius * s.bodyScale * radiusScale), s.fireTongueHeat, 0.0f};
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

// フェーズ 2 の火の舌：襟の両脇と両の袖口から、燃料を上へ吹き出す（見える鎧の口に付けて動かす）。霧へ沈んでいる間は
// 入れない（地面の下で燃えない）。
void UpdateFireTongues(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                       const BossBodyComponent& look, const BossPhaseShiftComponent* phaseShift,
                       const BossBodyPose& pose) {
    const BossBodySettings& s = look.settings;
    const BossBodyState& state = look.state;
    // 変わり目の演出を飛ばしたフレームも入れない（籠手が一度に移るので、移った線に沿って火を引かない）。
    if (state.fire <= 0.0f || BossPhaseShiftVisuals::HoldTongues(phaseShift) ||
        BossPose::RootHeight(boss) < -BossBodyMath::kTiny || s.smokeEffect.IsEmpty())
        return;
    // 襟：左右に 2 本ずつ（前後へ fireCollarSpread ずらす）。左は右を左右に反転した所。
    const float spread = 0.5f * s.fireCollarSpread;
    for (uint64_t k = 0; k < 4u; ++k) {
        const float side = k < 2u ? 1.0f : -1.0f;
        const float along = (k % 2u) == 0u ? spread : -spread;
        const HIKARI::GameProjectFloat3 local =
            BossMath::Mul({s.fireCollar.x * side, s.fireCollar.y, s.fireCollar.z + along}, pose.torso.scale);
        EmitTongue(world, object, boss, look, kFireTongueBase + k,
                   BossMath::Add(pose.torso.position, BossMath::Rotate(pose.torso.rotation, local)), 1.0f);
    }
    EmitTongue(world, object, boss, look, kFireTongueBase + 4u, CuffMouth(s, pose.rightCuff, s.fireCuffAlong),
               s.fireCuffShare);
    EmitTongue(world, object, boss, look, kFireTongueBase + 5u, CuffMouth(s, pose.leftCuff, s.fireCuffAlong),
               s.fireCuffShare);
    // 腰の周り（前の左右と後ろ）：胴の煙の柱の中を、下から火が上がる。骨格の体が出ている間は腰の骨の所、そうでなければ
    // 胴の中心から下げた所。
    if (s.fireHemShare > 0.0f) {
        const HIKARI::GameProjectFloat3 hips =
            look.anim.active && look.anim.hipsSeen
                ? look.anim.hipsWorld
                : BossMath::Add(pose.torso.position, {0.0f, -0.9f * pose.torso.scale, 0.0f});
        const HIKARI::GameProjectFloat3 around[3] = {{0.75f, 0.0f, 0.65f}, {-0.75f, 0.0f, 0.65f}, {0.0f, 0.0f, -1.0f}};
        for (uint64_t k = 0; k < 3u; ++k) {
            const HIKARI::GameProjectFloat3 local = BossMath::Mul(
                {around[k].x * s.fireHemRing, -s.fireHemDrop, around[k].z * s.fireHemRing}, pose.torso.scale);
            EmitTongue(world, object, boss, look, kFireTongueBase + 6u + k,
                       BossMath::Add(hips, BossMath::Rotate(pose.torso.rotation, local)), s.fireHemShare,
                       s.fireHemRadius);
        }
    }
}

// 骨に付いて行く煙（c-271cba）：骨格の体が出ている間、背骨（腰 → 胸）と両腕（肩 → 肘 → 袖口）に沿う点から、胴の煙の域へ
// 煙を入れる。同じ ID の注入は、前のフレームの所から今の所まで掃いて入るので、腕を振っても煙が帯になって付いて行く。
// amount は胴の煙と同じ倍率（登場・気絶・変わり目の演出）。
void EmitBoneSmoke(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id,
                   HIKARI::GameProjectFloat3 at, HIKARI::GameProjectFloat3 velocity, float strength, float radius) {
    if (!(strength > 0.0f))
        return;
    HIKARI::GameProjectFluidInteraction emit{};
    emit.kind = HIKARI::GameProjectFluidInteractionKind::Emit;
    emit.position = at;
    // extents.x は半径、extents.y は温度（少しだけ温かい：ゆっくり昇る）。
    emit.extents = {std::max(0.02f, radius), kBoneSmokeHeat, 0.0f};
    emit.direction = velocity;
    emit.strength = std::min(10000.0f, strength);
    emit.channels = FluidChannels::kBodyFire;
    boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, emit).Accepted() && boss.fluidAccepted;
}

void UpdateBoneSmoke(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                     const BossBodyComponent& look, const BossPhaseShiftComponent* phaseShift,
                     const BossBodyPose& pose) {
    const BossBodySettings& s = look.settings;
    if (!look.anim.active || !look.anim.hipsSeen || s.smokeEffect.IsEmpty())
        return;
    const bool stunned = boss.state == BossState::Stunned;
    // 座り込みの間（胴の煙を薄くする間）も、骨の煙は半分までしか薄くしない（鎧と手をつなぐ煙は残す）。
    const float amount = (stunned ? s.smokeAmountStunned : 1.0f) *
                         BossMath::Lerp(0.5f, 1.0f, BossPhaseShiftVisuals::SmokeScale(phaseShift));
    using BossMath::Add;
    using BossMath::Lerp;
    using BossMath::Mul;
    using BossMath::Sub;
    // 背骨：腰から胸へ。
    const HIKARI::GameProjectFloat3 hips = look.anim.hipsWorld;
    const HIKARI::GameProjectFloat3 chest = pose.torso.position;
    const float spineRadius = s.spineSmokeRadius * s.bodyScale;
    uint64_t id = kBoneSmokeBase;
    for (float u : kSpineShares)
        EmitBoneSmoke(world, object, boss, id++, Lerp(hips, chest, u), {0.0f, kSpineRise, 0.0f}, s.spineSmoke * amount,
                      spineRadius);
    // 両腕：肩 → 肘 → 袖口。肘は、袖口（前腕 → 手の 3/4）と手から戻す。
    const float armRadius = s.armSmokeRadius * s.bodyScale;
    const auto arm = [&](const BossBodyPlacement& shoulder, const BossBodyPlacement& cuff,
                         const BossBodyPlacement& hand) {
        const HIKARI::GameProjectFloat3 elbow = Sub(Mul(cuff.position, 4.0f), Mul(hand.position, 3.0f));
        const HIKARI::GameProjectFloat3 upper = BossMath::Normalize(Sub(elbow, shoulder.position));
        const HIKARI::GameProjectFloat3 fore = BossMath::Normalize(Sub(cuff.position, elbow));
        for (float u : kUpperArmShares)
            EmitBoneSmoke(world, object, boss, id++, Lerp(shoulder.position, elbow, u), Mul(upper, s.armSmokeFlow),
                          s.armSmoke * amount, armRadius);
        for (float u : kForeArmShares)
            EmitBoneSmoke(world, object, boss, id++, Lerp(elbow, cuff.position, u), Mul(fore, s.armSmokeFlow),
                          s.armSmoke * amount, armRadius);
    };
    arm(pose.sode[0], pose.rightCuff, pose.rightHand);
    arm(pose.sode[1], pose.leftCuff, pose.leftHand);
}

// 左手の所作（手の光の演出 BossBodyAccents が読む）：投げナイフの間だけ Knife。
void StepLeftGesture(const BossComponent& boss, BossBodyComponent& look) {
    const BossMove* move = BossBodyMath::CurrentMove(boss);
    look.state.leftGesture = move != nullptr && move->type == BossMoveType::Knife ? BossBodyLeftGesture::Knife
                                                                                  : BossBodyLeftGesture::Balance;
}

} // namespace

void BossBodyVisuals::NoteTick(BossComponent& boss, HIKARI::GameProjectFloat3 position) {
    boss.tickFrom = boss.ticksSeen > 0 ? boss.tickTo : position;
    boss.tickTo = position;
    boss.ticksSeen = std::min(boss.ticksSeen + 1, 2);
}

void BossBodyVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                             BossBodyComponent* look, BossIntroComponent* intro, BossPhaseShiftComponent* phaseShift,
                             float seconds, float alpha) {
    // 体の見た目の部品が無い間と、撃破の後に体が縮んで消える間は、体も体に付く効果も出さない。
    const bool vanishing = boss.state == BossState::Dead && boss.stateFrame > boss.deadVanishDelayFrames;
    HIKARI::GameProjectTransform body{};
    if (look == nullptr || vanishing || !object.GetWorldTransform(body)) {
        Stop(world, boss, look, phaseShift);
        return;
    }
    // 体の位置は tick ごとに変わるので、描画のフレームでは前の tick との間を補間した所に置く（ナレッジ k-c2a8a4。
    // エンジンが体を描くのと同じ）。向きは tick の値のまま（1 tick に変わる角度は小さい）。
    HIKARI::GameProjectFloat3 shift{};
    if (look->settings.interpolate && boss.ticksSeen >= 2)
        shift = BossMath::Mul(BossMath::Sub(boss.tickFrom, boss.tickTo), 1.0f - std::clamp(alpha, 0.0f, 1.0f));
    body.position = BossMath::Add(body.position, shift);
    if (intro != nullptr)
        BossIntroVisuals::Advance(world, look->settings, *intro, seconds);
    // 体に付く効果（胴の煙・気配・衝撃の輪）は見た目の体に付ける（登場の間は演技の体の根と向き。c-e0c500。変わり目の演出の
    // 間は骨格の体の腰の真上。c-271cba）。
    const HIKARI::GameProjectTransform shown =
        BossPhaseShiftVisuals::SmokeBody(phaseShift, *look, BossIntroVisuals::BodyTransform(boss, intro, body));
    BossPhaseShiftVisuals::Advance(world, boss, *look, phaseShift, seconds);
    BossBodyAccents::StepSmoke(boss, *look, seconds);
    StepFire(boss, *look, phaseShift, seconds);
    CarrySmokeTilt(*look, phaseShift, shown);
    UpdateSmoke(world, boss, *look, intro, phaseShift, shown);
    StepLeftGesture(boss, *look);
    // 体は骨格の体（戦いの間）か登場の役者が描く。ここは体に付く効果の置き場所を、その骨から受け取る（c-db1a15）。
    BossBodyPose pose = BossBodyRig::Base(boss, look->settings, body);
    // 演出の効果の毎フレームの頼みを 0 に戻してから、登場と変わり目の演出が、このフレームの分を書く。
    BossCinematicFx::BeginFrame(boss);
    // 登場の間は、体に付く効果の置き場所（兜・刀・胴）を、演じている役者の骨のものに直す。
    if (intro != nullptr)
        BossIntroVisuals::ApplyPose(boss, *intro, pose, seconds);
    // フェーズの変わり目の演出の間の、刀身の灼け・刃口の光・気配。
    BossPhaseShiftVisuals::ApplyPose(boss, phaseShift, pose);
    // 登場の役者が出ている間は、骨格の体を出さない（役者が体を演じる）。
    const bool animated =
        !BossIntroVisuals::Showing(intro) && BossAnimBody::Update(world, boss, *look, body, alpha, seconds);
    if (animated)
        BossAnimBody::ApplyPose(world, boss, *look, alpha, seconds, pose);
    if (BossIntroVisuals::Showing(intro))
        BossAnimBody::Stop(world, boss, *look);
    BossPhaseShiftVisuals::Render(world, object, boss, *look, phaseShift, pose, seconds);
    StepSmokeTilt(*look, phaseShift, animated, seconds);
    if (animated)
        UpdateBoneSmoke(world, object, boss, *look, phaseShift, pose);
    UpdateFireTongues(world, object, boss, *look, phaseShift, pose);
    BossBodyAccents::Update(world, boss, *look, intro, shown, pose, seconds);
}

void BossBodyVisuals::UpdatePreviews(HIKARI::WorldView& world) {
    // 演出ワークスペースがボスの登場の資産を流している間だけ、役者をその時刻の姿にする（流していない時は、触った
    // ものを元へ戻す＝ふだんの編集の見た目）。胴の煙・効果は出さない。
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType), [&](HIKARI::ObjectView object, BossComponent& boss) {
            ComponentLookup::WithFirstOnObject<BossIntroComponent>(
                world, kBossIntroType, object, [&](BossIntroComponent* intro) {
                    if (intro == nullptr)
                        return;
                    ComponentLookup::WithFirstOnObject<BossBodyComponent>(
                        world, kBossBodyType, object, [&](const BossBodyComponent* look) {
                            BossIntroVisuals::Preview(world, BossBodySettingsOf(look), *intro);
                        });
                });
            // 変わり目の演出の資産のプレビュー（c-271cba）：骨格の体をその時刻の姿にする。
            ComponentLookup::WithFirstOnObject<BossPhaseShiftComponent>(
                world, kBossPhaseShiftType, object, [&](BossPhaseShiftComponent* shift) {
                    if (shift != nullptr)
                        BossPhaseShiftVisuals::Preview(world, boss, *shift);
                });
        });
}

void BossBodyVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss, BossBodyComponent* look,
                           BossPhaseShiftComponent* phaseShift) {
    BossPhaseShiftVisuals::Stop(world, phaseShift);
    if (look == nullptr)
        return;
    BossVfx::StopHandle(world, look->state.smokeVfx);
    BossBodyAccents::Stop(world, *look);
    BossAnimBody::Stop(world, boss, *look);
}

} // namespace Project_TD2_2
