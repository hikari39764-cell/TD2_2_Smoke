#include "BossBodyAccents.h"

#include <algorithm>

#include "HIKARI/Project/LightTypes.h"
#include "BossBodyMath.h"
#include "BossHelmGlowData.h"
#include "BossIntroActor.h"
#include "../Boss/BossIntro.h"
#include "../Boss/BossKasumiStep.h"
#include "../Boss/BossRules.h"
#include "../Boss/BossVfx.h"
#include "../Player/Feedback.h"
#include "../Common/VfxParameters.h"
#include "HIKARI/Extend/World.h"
#include "../Common/FieldValues.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;
using BossVfx::Placed;
using BossVfx::StartOrSend;
using BossVfx::StopHandle;

// エフェクトの入力（資産側の契約）。どれも Color で、0 にするとその飾りが出ない。
constexpr const char* kTrailInput = "input.trail"; // 刀光の色と明るさ
constexpr const char* kHeatInput = "input.heat";   // 刀身の炎の明るさ
constexpr const char* kHazeInput = "input.haze";   // 振りのゆがみ（R が強さ。G・B は 1）
constexpr const char* kEyesInput = "input.eyes";   // 目の光の色と明るさ
constexpr const char* kFlareInput = "input.flare"; // 目の閃きの色と明るさ
constexpr const char* kColorInput = "input.color"; // 刀身を走る光の色
constexpr const char* kAuraInput = "input.aura";   // 溜めの気配の色と強さ
constexpr const char* kDashInput = "input.dash";   // 突進の尾の色と明るさ
constexpr const char* kHandInput = "input.hand";   // 左手の光の色と明るさ
constexpr const char* kEdgeInput = "input.edge";   // 刃口の常時の光の色と明るさ
constexpr const char* kEdgeDarkInput = "input.edgeDark"; // 刃のまわりの暗い縁（A が濃さ）
constexpr const char* kFlowInput = "input.flow";         // 刃文に沿って流れる光の色と明るさ
constexpr const char* kMistInput = "input.mist";         // 刀身を包む霞の帯の色と明るさ
constexpr const char* kWakeInput = "input.wake";         // 剣先の残光の色と明るさ
constexpr const char* kEmbersInput = "input.embers";     // 刀身から立つ火の粉の量（Float。c-857ae3）
constexpr const char* kFlameInput = "input.flame";       // 刀身に沿って流れる炎（RGB と A に強さ。c-857ae3）
// 流体の刀身の炎のエフェクト（BossBladeFire）：燃料の量の入力と、資産が作られている刃の長さ (m。実寸で作ってあるので、
// 根の大きさは いまの刃の長さ ÷ これ）。
constexpr const char* kFireAmountInput = "input.amount";
constexpr float kBladeFireAuthoredSpan = 4.33f;
// 流体の炎を止める熱さ（これより下がったら域ごと止める。燃料は熱さに比例して先に細る）。
constexpr float kBladeFireStopHeat = 0.02f;
// 範囲爆発の溜めのエフェクトの入力：集める量（Float 0〜1）、掌の芯の光（Color）、押し縮める瞬間の色（Color）。
constexpr const char* kGatherInput = "input.gather";
constexpr const char* kCoreInput = "input.core";
constexpr const char* kPinchInput = "input.pinch";
// 叩きつけた後（または打ち切られた後）、手の域を消すまでの時間 (秒)。芯の光を追う時間 (秒)。明滅の周波数 (Hz)。
constexpr float kGatherLingerSeconds = 0.06f;
constexpr float kGatherCoreFollowSeconds = 0.03f;
constexpr float kGatherFlickerHz = 17.0f;
// 集める量が出そろうまでの、集める区間の割合。押し縮めてから芯が明るくなり切るまでの、押し縮める区間の割合。
constexpr float kGatherRampShare = 0.6f;
constexpr float kPinchBrightenShare = 0.45f;
// 突進の尾の明るさを追う時間 (秒)。
constexpr float kDashFollowSeconds = 0.06f;
// 刀光が立ち上がる・引く時間 (秒)。出ている帯は、エフェクト側の時間で消える。
constexpr float kTrailRiseSeconds = 0.02f;
// 胴の煙の量・熱さを追う時間 (秒)。
constexpr float kSmokeFollowSeconds = 0.15f;

HIKARI::GameProjectVfxParameterOverrides Color(const char* input, Vec3 color) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetColor(parameters, input, color);
    return parameters;
}

// 体の足元（世界座標）。
Vec3 Feet(const Boss& boss, const HIKARI::GameProjectTransform& body) {
    return Add(body.position, Rotate(body.rotation, {0.0f, -boss.bodyHalfHeight * body.scale.y, 0.0f}));
}

// 刀を振って当てる技か（刀身を走る光を出す技）。投げ・歩きなどは違う。
bool UsesSword(BossMoveType type) {
    switch (type) {
    case BossMoveType::Knife:
    case BossMoveType::WalkSide:
    case BossMoveType::WalkIn:
    case BossMoveType::Backstep:
    case BossMoveType::KasumiStep: return false;
    default: return true;
    }
}

// 歩き・下がり・霞歩のような、構えの合図を出さない技か。
bool IsFootwork(BossMoveType type) {
    return type == BossMoveType::WalkSide || type == BossMoveType::WalkIn || type == BossMoveType::Backstep ||
           type == BossMoveType::KasumiStep;
}

// 刀に付くエフェクトの置き場所：原点＝刃の真ん中、+X＝剣先、+Y＝刃口（振る時に進む向き）、大きさ＝刃の長さ。
HIKARI::GameProjectTransform BladeTransform(const Boss& boss, const BossBody& look,
                                            const BossBodyPose& pose) {
    const float reach = VisibleBladeReach(boss, look);
    const Vec3 direction = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const Vec3 edge = Rotate(pose.sword.rotation, {0.0f, -1.0f, 0.0f});
    const float from = reach * std::clamp(look.bodyBladeFromShare, 0.0f, 0.9f);
    const float span = reach - from;
    return Placed(Add(pose.sword.position, Mul(direction, from + span * 0.5f)), span, BasisRotation(direction, edge));
}

// 刃口の常時の光の目標（倍率。脈と燃える刀身の割合は掛ける前）：刀を見れば何をしているか分かる段階。待機は弱く、
// 前隙は進みにつれて強く、振る間（命中の窓）はいちばん強く、連撃の振りの間・投げる・蹴る間は構え切った
// 明るさ、後隙は待機へ。歩き・下がりは待機のまま。
float EdgeTarget(const Boss& boss, const BossBody& look, const BossMove* move, bool& pulsing) {
    const BossBody& s = look;
    pulsing = false;
    if (boss.state == BossState::Dead)
        return 0.0f;
    if (boss.state == BossState::Stunned)
        return s.bodyEdgeStunned;
    if (boss.state != BossState::Act || move == nullptr || IsFootwork(move->type) || boss.phase == BossActPhase::Post)
        return s.bodyEdgeIdle;
    if (boss.swordSmoke)
        return s.bodyEdgeSwing;
    pulsing = true;
    if (boss.phase == BossActPhase::Pre) {
        const float progress =
            move->pre > 0 ? std::clamp(static_cast<float>(boss.stateFrame) / static_cast<float>(move->pre), 0.0f, 1.0f)
                          : 1.0f;
        return Lerp(s.bodyEdgeIdle, s.bodyEdgeWindup, std::pow(progress, std::max(0.01f, s.bodyEdgeRampPower)));
    }
    return s.bodyEdgeWindup;
}

// 刃口の常時の光と暗い縁を、刀に付くエフェクトの入力へ入れる。
void SetEdge(HIKARI::GameProjectVfxParameterOverrides& parameters, Boss& boss, BossBody& look,
             const BossBodyPose& pose, float seconds) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    bool pulsing = false;
    float target = EdgeTarget(boss, look, CurrentMove(boss), pulsing);
    // 登場の一振りの溜めの間は、登場の演出が頼んだ強さ。
    target = std::max(target, boss.cinematicFxState.bladeEdge);
    a.edgeClock = pulsing ? a.edgeClock + seconds : 0.0f;
    if (pulsing)
        target *= 1.0f + s.bodyEdgePulseDepth * std::sin(2.0f * kPi * s.bodyEdgePulseFrequency * a.edgeClock);
    if (pose.bladeHot)
        target *= s.bodyEdgeHotShare;
    // 実体の刀の技の間は、刃口の光を落とす（暗い縁と刃文だけ。霞の技と見分ける）。
    if (BossBodyAccents::Kind(boss) == BossBodyAccents::BladeKind::Real)
        target *= s.bodyRealEdgeShare;
    const float time = target > a.edgeGain ? s.bodyEdgeRiseSeconds : s.bodyEdgeFallSeconds;
    a.edgeGain = Lerp(a.edgeGain, target, ExpFollow(seconds, time));
    const float dark = boss.state == BossState::Dead ? 0.0f : s.bodyEdgeDark;
    a.edgeDarkGain = Lerp(a.edgeDarkGain, dark, ExpFollow(seconds, s.bodyEdgeFallSeconds));
    VfxParameters::SetColor(parameters, kEdgeInput, Mul(pose.bladeHot ? ToGP(s.bodyEdgeFireColor) : ToGP(s.bodyEdgeMistColor), a.edgeGain));
    VfxParameters::SetColorAlpha(parameters, kEdgeDarkInput, {1.0f, 1.0f, 1.0f}, a.edgeDarkGain);
}

// 刀に付くエフェクト：見える刀の刃に付けて出し続け、振っている間は刀光を、刀が燃えている間は刀身の炎を入れる。
// 刃口の常時の光と暗い縁は、いつも入れる（段階で明るさが変わる）。
void UpdateBlade(HIKARI::WorldView& world, Boss& boss, BossBody& look, BossIntro* intro,
                 const BossBodyPose& pose, float seconds) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    // 登場の間は、共用の刀の光（刃口の光・刀光の帯）を出さない。役者の刀の刃口を、細い線として光らせる。
    if (intro != nullptr && (intro->state.phase == BossIntroPhase::Show || BossIntroRules::ActorLingers(intro))) {
        StopHandle(world, a.bladeVfx);
        StopHandle(world, a.bladeFireVfx);
        const Vec3 tint = ToGP(s.bodyGlintMistColor);
        BossIntroActor::SetBladeEdge(world, *intro, Mul(tint, 1.0f / std::max({tint.x, tint.y, tint.z, kTiny})),
                                     boss.introBladeEdgeGlow * boss.cinematicFxState.bladeEdge);
        return;
    }
    if (!s.bodyBladeEffect.IsSet()) {
        StopHandle(world, a.bladeVfx);
        StopHandle(world, a.bladeFireVfx);
        return;
    }
    // 刀光：命中の窓の間と、その後の振り抜き（剣先がまだ速い間）。
    a.trailHold = boss.swordSmoke ? s.bodyTrailFollowSeconds : std::max(0.0f, a.trailHold - seconds);
    // 見えている骨格の体の刀の剣先の速さ（c-7b1a09）。骨格の体が出ていなければ 0。
    const float tipSpeed = BossAnimBody::Showing(&look) ? look.anim.tipSpeed : 0.0f;
    // 剣先が地面に届く少し手前で切る（地面に止められた剣先は地面に沿って滑り、折れ線の帯を引く）。
    const bool grounded = pose.swordTipClearance < s.bodyTrailGroundCut;
    // 登場の一振りの間も出す（登場の演出が頼む）。
    const bool swinging =
        (s.bodyTrail && !grounded && (boss.swordSmoke || (a.trailHold > 0.0f && tipSpeed >= s.bodyTrailMinTipSpeed))) ||
        boss.cinematicFxState.bladeSwing;
    a.trailGain = grounded ? 0.0f : Lerp(a.trailGain, swinging ? 1.0f : 0.0f, ExpFollow(seconds, kTrailRiseSeconds));
    // 振りのゆがみ：刀光の間と、刀を振る技の打ち込みで剣先が速い間（命中の窓の少し前の振り出しから）。振りかぶりでは
    // 出さない。地面に沿う剣先では切る。
    const BossMove* move = CurrentMove(boss);
    const bool striking = boss.state == BossState::Act && boss.phase == BossActPhase::Act;
    const bool fast = s.bodySwingHazeMinTipSpeed > 0.0f && striking && move != nullptr && UsesSword(move->type) &&
                      tipSpeed >= s.bodySwingHazeMinTipSpeed;
    const bool hazing = s.bodyTrail && !grounded && (swinging || fast);
    a.hazeGain = grounded ? 0.0f : Lerp(a.hazeGain, hazing ? 1.0f : 0.0f, ExpFollow(seconds, kTrailRiseSeconds));
    // 刀身の炎：灼けた刀身（模型）が出ている間。
    const bool burning = pose.bladeHot;
    a.heatGain = Lerp(a.heatGain, burning ? 1.0f : 0.0f, ExpFollow(seconds, s.bodyBladeHeatSeconds));

    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetColor(parameters, kTrailInput,
                            Mul(pose.bladeHot ? ToGP(s.bodyTrailFireColor) : ToGP(s.bodyTrailMistColor), a.trailGain));
    // 流体の炎（c-3c908c）：エフェクトがあるシーンでは、炎は刃に付いて動く流体域で出し、シェーダーの炎の帯と刀身の光の層は
    // 控えめにする。
    const bool fluidFire = !!s.bodyBladeFireEffect.IsSet() && s.bodyBladeFireAmount > 0.0f;
    // 速く振る間は、流体の炎が刃から剥がれるので、刃に貼り付くシェーダーの炎の帯を戻す（すぐ上げ、ゆっくり下げる）。
    const float fastShare =
        s.bodyBladeFlameFastSpeed > 0.0f ? std::clamp(tipSpeed / s.bodyBladeFlameFastSpeed, 0.0f, 1.0f) : 0.0f;
    a.bladeFlameFast = fastShare > a.bladeFlameFast
                           ? fastShare
                           : Lerp(a.bladeFlameFast, fastShare, ExpFollow(seconds, s.bodyBladeFlameReturnSeconds));
    const float heat = s.bodyBladeHeat * a.heatGain * (fluidFire ? s.bodyBladeFireGlowShare : 1.0f);
    const float flame = s.bodyBladeHeat * a.heatGain * (fluidFire ? Lerp(s.bodyBladeFlameShare, 1.0f, a.bladeFlameFast) : 1.0f);
    VfxParameters::SetColor(parameters, kHeatInput, {heat, heat, heat});
    VfxParameters::SetFloat(parameters, kEmbersInput, s.bodyBladeEmbers * a.heatGain);
    VfxParameters::SetColorAlpha(parameters, kFlameInput, {flame, flame, flame}, std::clamp(flame, 0.0f, 1.0f));
    if (fluidFire && a.heatGain > kBladeFireStopHeat) {
        HIKARI::GameProjectTransform fire = BladeTransform(boss, look, pose);
        const float scale = fire.scale.x / kBladeFireAuthoredSpan;
        fire.scale = {scale, scale, scale};
        HIKARI::GameProjectVfxParameterOverrides fuel{};
        VfxParameters::SetFloat(fuel, kFireAmountInput, s.bodyBladeFireAmount * a.heatGain);
        StartOrSend(world, boss, a.bladeFireVfx, VfxPlayback::Reference(s.bodyBladeFireEffect), fire, fuel);
    } else {
        StopHandle(world, a.bladeFireVfx);
    }
    VfxParameters::SetColor(parameters, kHazeInput, {s.bodySwingHaze * a.hazeGain, 1.0f, 1.0f});
    SetEdge(parameters, boss, look, pose, seconds);
    // 常時の層：刃文の流れと霞の帯は、待機でも見える。霞の技の間は強め、実体の刀の技の間は弱める。
    const BossBodyAccents::BladeKind kind = BossBodyAccents::Kind(boss);
    const float stage = kind == BossBodyAccents::BladeKind::Haze   ? s.bodyHazeBoost
                        : kind == BossBodyAccents::BladeKind::Real ? s.bodyRealFlowShare
                                                                   : 1.0f;
    const float alive = boss.state == BossState::Dead ? 0.0f : 1.0f;
    const float time = stage * alive > a.flowGain ? s.bodyEdgeRiseSeconds : s.bodyEdgeFallSeconds;
    a.flowGain = Lerp(a.flowGain, stage * alive, ExpFollow(seconds, time));
    VfxParameters::SetColor(parameters, kFlowInput,
                            Mul(pose.bladeHot ? ToGP(s.bodyFlowFireColor) : ToGP(s.bodyFlowMistColor), s.bodyFlowIdle * a.flowGain));
    VfxParameters::SetColor(parameters, kMistInput,
                            Mul(pose.bladeHot ? ToGP(s.bodyMistFireColor) : ToGP(s.bodyMistMistColor), s.bodyMistIdle * a.flowGain));
    // 剣先の残光：剣先が動いている間はいつも（速いほど明るい）。地面に沿う剣先では切る。
    const float nearGround =
        std::clamp((pose.swordTipClearance - s.bodyTrailGroundCut) / std::max(kTiny, s.bodyWakeGroundFade), 0.0f, 1.0f);
    const float wake =
        grounded ? 0.0f
                 : std::clamp((tipSpeed - s.bodyWakeMinTipSpeed) / std::max(kTiny, s.bodyWakeFullTipSpeed - s.bodyWakeMinTipSpeed),
                              0.0f, 1.0f) *
                       nearGround;
    a.wakeGain = wake > a.wakeGain ? wake : Lerp(a.wakeGain, wake, ExpFollow(seconds, s.bodyEdgeFallSeconds));
    VfxParameters::SetColor(parameters, kWakeInput,
                            Mul(pose.bladeHot ? ToGP(s.bodyWakeFireColor) : ToGP(s.bodyWakeMistColor), s.bodyWakeGain * a.wakeGain));
    StartOrSend(world, boss, a.bladeVfx, VfxPlayback::Reference(s.bodyBladeEffect), BladeTransform(boss, look, pose), parameters);
}

// 技を出すと決めた瞬間（構えの合図が出た最初のフレーム）：目が閃き、刀を振る技なら鍔元から剣先へ光が走る。
void UpdateCommit(HIKARI::WorldView& world, Boss& boss, BossBody& look, const BossBodyPose& pose,
                  float seconds) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    const BossMove* move = CurrentMove(boss);
    const bool glint = move != nullptr && boss.glintFrames > 0 && !IsFootwork(move->type);
    const bool started = glint && !a.wasGlint;
    a.wasGlint = glint;
    // 登場・変わり目の演出が頼んだ目の閃き（BossCinematicFx::EyeFlare）を先に取り込む。
    a.flare = std::max(a.flare, boss.cinematicFxState.eyeFlareRequest);
    boss.cinematicFxState.eyeFlareRequest = 0.0f;
    a.flare = std::max(0.0f, a.flare - seconds / std::max(kTiny, s.bodyEyeFlareSeconds) * s.bodyEyeFlare);
    if (started)
        a.flare = s.bodyEyeFlare;

    // 刀身を走る光（1 回きり）。出ている間は、刃に付けて動かす。
    if (a.glintVfx.IsValid() && !world.GetVfx().IsActive(a.glintVfx))
        a.glintVfx = {};
    // 剣気を放った瞬間も、刀身を走る光をもう一度（明るく）出す。
    const bool released = boss.releaseCount != a.releaseSeen;
    a.releaseSeen = boss.releaseCount;
    // 放つ瞬間の全体の止め（項目で入れた時だけ。発射 310 より弱い優先度）。
    constexpr uint64_t kReleaseStopSource = 0x5444325245ull;
    if (released && s.bodyReleaseHoldTicks > 0)
        Feedback::RequestHitStop(world, kReleaseStopSource, 306, s.bodyReleaseHoldTicks, s.bodyReleaseHoldScale);
    // 登場の演出が頼んだ光（刀が地面を離れる・振り下ろす瞬間）。
    const float intro = boss.cinematicFxState.glint;
    boss.cinematicFxState.glint = 0.0f;
    const float bright = intro > 0.0f ? intro : released && s.bodyReleaseGlint > 0.0f ? s.bodyReleaseGlint : 1.0f;
    const auto color = Color(kColorInput, Mul(pose.bladeHot ? ToGP(s.bodyGlintFireColor) : ToGP(s.bodyGlintMistColor), bright));
    if (((started && UsesSword(move->type)) || (released && s.bodyReleaseGlint > 0.0f) || intro > 0.0f) &&
        !!s.bodyGlintEffect.IsSet()) {
        StopHandle(world, a.glintVfx);
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = VfxPlayback::Reference(s.bodyGlintEffect);
        play.transform = BladeTransform(boss, look, pose);
        play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
        play.parameters = color;
        a.glintVfx = BossVfx::Start(world, boss, play);
    } else if (a.glintVfx.IsValid()) {
        BossVfx::Send(world, boss, a.glintVfx, BladeTransform(boss, look, pose), color);
    }
}

// 目の光：兜に付けて出し続ける。頭が動くと光が尾を引き、技を出すと決めた瞬間に横へ閃く。
// 灯 1 つ：兜の置き場所から決めた所へ置き、色と明るさを入れる（明るさ 0 なら消す）。
void PlaceHelmLight(HIKARI::ObjectView light, const BossBodyPlacement& head, Vec3 local, Quat rotation, Vec3 color,
                    float intensity) {
    if (!light.IsValid())
        return;
    HIKARI::GameProjectTransform transform{};
    transform.position = Add(head.position, Rotate(head.rotation, Mul(local, head.scale)));
    transform.rotation = rotation;
    transform.scale = {1.0f, 1.0f, 1.0f};
    (void)light.SetWorldTransform(transform);
    HIKARI::GameProjectLightPatch patch{};
    patch.fieldMask = static_cast<uint32_t>(HIKARI::GameProjectLightPatchField::Enabled) |
                      static_cast<uint32_t>(HIKARI::GameProjectLightPatchField::Color) |
                      static_cast<uint32_t>(HIKARI::GameProjectLightPatchField::Intensity);
    patch.enabled = intensity > 0.001f ? 1u : 0u;
    patch.color = color;
    patch.intensity = intensity;
    (void)light.GetLight().ApplyPatch(patch);
}

// 兜の中の灯：前を向く影付きのスポットライトと、兜の中を満たす点光を、兜に合わせて置く。面頬の隙間から本物の光が
// 漏れ、頭が動けば光も動く。灯の物体がシーンに無ければ false（前の貼り付けの光を出す）。
// 目の熱さ（0〜1）：変わり目の演出が書いた値、無ければフェーズ 2 か。
float EyesHeat(const BossBodyPose& pose) {
    return pose.eyesHeat >= 0.0f ? Clamp01(pose.eyesHeat) : (pose.eyesFire ? 1.0f : 0.0f);
}

// 骨格の体（戦いの体。登場の役者と同じ模型）の面頬の隙間の光：目が熱い間（フェーズ 2・変わり目の演出）は、目の光の色と
// 明るさで上書きする（模型の材質のままだとふだんの色。c-271cba）。熱くない間は上書きを外す。
void UpdateBodyHelmGlow(HIKARI::WorldView& world, const Boss& boss, BossBody& look, float heat,
                        Vec3 color, float strength) {
    BossBodyAccentState& a = look.state.accents;
    if (!boss.animBodyObject.IsAlive())
        return;
    const HIKARI::ObjectView body = world.FindObject(HIKARI::Extend::HandleOf(boss.animBodyObject));
    if (!body.IsValid())
        return;
    if (heat > 0.001f && strength > 0.001f) {
        (void)body.GetModel().SetMaterialColorOverride(BossHelmGlowData::kActorMaterial, {0.0f, 0.0f, 0.0f}, 1.0f,
                                                       color, std::min(strength, 64.0f));
        a.bodyGlowOverridden = true;
    } else if (a.bodyGlowOverridden) {
        (void)body.GetModel().ClearMaterialColorOverride(BossHelmGlowData::kActorMaterial);
        a.bodyGlowOverridden = false;
    }
}

bool UpdateHelmLights(HIKARI::WorldView& world, Boss& boss, BossBody& look, BossIntro* intro,
                      const BossBodyPose& pose, float seconds) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    const HIKARI::ObjectView spot =
        s.bodyHelmSpotObject.IsAlive() ? world.FindObject(HIKARI::Extend::HandleOf(s.bodyHelmSpotObject)) : HIKARI::ObjectView{};
    const HIKARI::ObjectView fill =
        s.bodyHelmFillObject.IsAlive() ? world.FindObject(HIKARI::Extend::HandleOf(s.bodyHelmFillObject)) : HIKARI::ObjectView{};
    if (!spot.IsValid() && !fill.IsValid())
        return false;
    a.helmClock += std::max(0.0f, seconds);
    // 色は目の光の色（霞・火）の色合いだけを使い、明るさは灯の項目で決める。変わり目の演出の間は熱さで混ぜる（c-271cba）。
    const float heat = EyesHeat(pose);
    const Vec3 tint = Lerp(ToGP(s.bodyEyesMistColor), ToGP(s.bodyEyesFireColor), heat);
    const Vec3 color = Mul(tint, 1.0f / std::max({tint.x, tint.y, tint.z, kTiny}));
    // 明るさ：灯っている時の基準に、登場の演出が頼む倍率と閃きを足し、細かく揺らがせる。
    const float flicker = 1.0f + s.bodyHelmLightFlicker * (0.6f * std::sin(a.helmClock * 17.0f) +
                                                       0.4f * std::sin(a.helmClock * 31.0f + 1.3f));
    const float level = pose.eyesLit
                            ? (1.0f + boss.cinematicFxState.eyeGlow * s.helmGlowShare + a.flare * s.bodyHelmFlareGain) *
                                  std::max(0.0f, flicker)
                            : 0.0f;
    // X まわりの正の回転で下を向く。
    const float half = s.bodyHelmSpotPitchDegrees * kRadiansPerDegree * 0.5f;
    const Quat aim = Multiply(pose.head.rotation, Quat{std::sin(half), 0.0f, 0.0f, std::cos(half)});
    PlaceHelmLight(spot, pose.head, ToGP(s.bodyHelmSpotLocal), aim, color, s.bodyHelmSpotIntensity * level);
    PlaceHelmLight(fill, pose.head, ToGP(s.bodyHelmFillLocal), pose.head.rotation, color, s.bodyHelmFillIntensity * level);
    // 面頬の隙間の中の光（模型の中の自発光の面）：登場の間は役者の模型の明るさを毎フレーム入れる。戦いの間は
    // BossBodyVisuals が、灯っている時だけその部品を出す（明るさは模型の材質の値）。
    if (intro != nullptr)
        BossIntroActor::SetHelmGlow(world, *intro, color, s.bodyHelmGlowStrength * level);
    UpdateBodyHelmGlow(world, boss, look, heat, color, s.bodyHelmGlowStrength * level);
    return true;
}

void UpdateEyes(HIKARI::WorldView& world, Boss& boss, BossBody& look, BossIntro* intro,
                const BossBodyPose& pose, float seconds) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    // 兜の中の灯がある時は、貼り付けの光は出さない。
    if (UpdateHelmLights(world, boss, look, intro, pose, seconds)) {
        StopHandle(world, a.eyesVfx);
        return;
    }
    if (!s.bodyEyesEffect.IsSet()) {
        StopHandle(world, a.eyesVfx);
        return;
    }
    // 登場の演出は、目の光を普段より明るく灯す（BossIntroEffects が頼む倍率）。
    const Vec3 color =
        pose.eyesLit ? Mul(Lerp(ToGP(s.bodyEyesMistColor), ToGP(s.bodyEyesFireColor), EyesHeat(pose)), 1.0f + boss.cinematicFxState.eyeGlow)
                     : Vec3{};
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetColor(parameters, kEyesInput, color);
    VfxParameters::SetColor(parameters, kFlareInput, Mul(color, a.flare));
    StartOrSend(world, boss, a.eyesVfx, VfxPlayback::Reference(s.bodyEyesEffect), Placed(pose.head.position, pose.head.scale, pose.head.rotation),
                parameters);
}

// 溜めの強さ（0〜）：登場の演出が頼んだ強さ（立ち上がる瞬間は 1 を超える）。登場の外では 0。
float ChargeAmount(const Boss& boss) { return std::max(0.0f, boss.cinematicFxState.aura); }

// 溜めの気配：溜めている間、体を包んで立ちのぼる。体の足元に付けて出し続け、強さを入力で変える。
void UpdateAura(HIKARI::WorldView& world, Boss& boss, BossBody& look,
                const HIKARI::GameProjectTransform& body, const BossBodyPose& pose, float seconds) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    if (!s.bodyAuraEffect.IsSet()) {
        StopHandle(world, a.auraVfx);
        return;
    }
    a.aura = Lerp(a.aura, ChargeAmount(boss), ExpFollow(seconds, s.bodyAuraSeconds));
    // 資産は 1 倍の体（身長 4 m）で作ってあるので、体の大きさ（BossBody.bodyScale）を掛ける。
    StartOrSend(world, boss, a.auraVfx, VfxPlayback::Reference(s.bodyAuraEffect), Placed(Feet(boss, body), look.bodyScale, body.rotation),
                Color(kAuraInput, Mul(pose.bladeHot ? ToGP(s.bodyAuraFireColor) : ToGP(s.bodyAuraMistColor), a.aura)));
}

// 衝撃の輪を 1 つ出す（1 回きり）。at は地面の上の点、size は大きさの倍率。
void PlayShock(HIKARI::WorldView& world, Boss& boss, BossBody& look, const BossBodyPose& pose,
               Vec3 at, float size) {
    const BossBody& s = look;
    if (!(size > 0.0f) || !s.bodyShockEffect.IsSet())
        return;
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = VfxPlayback::Reference(s.bodyShockEffect);
    play.transform = Placed({at.x, boss.ground.y, at.z}, size);
    play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
    play.parameters = Color(kColorInput, pose.bladeHot ? ToGP(s.bodyShockFireColor) : ToGP(s.bodyShockMistColor));
    BossVfx::Start(world, boss, play);
}

// 重い一撃が当たった瞬間を、規則が書く様子の変わり目から見分けて、衝撃の輪を出す（当たった後だけ。予告はしない）。
void UpdateShock(HIKARI::WorldView& world, Boss& boss, BossBody& look,
                 const HIKARI::GameProjectTransform& body, const BossBodyPose& pose) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    const Vec3 feet = Feet(boss, body);
    // 登場の演出が頼んだ衝撃の輪（立ち上がる・刀が抜ける・振り下ろす瞬間）。
    for (BossCinematicFxState::Shock& shock : boss.cinematicFxState.shocks) {
        if (shock.size > 0.0f && shock.delay <= 0.0f) {
            PlayShock(world, boss, look, pose, shock.at, shock.size);
            shock.size = 0.0f;
        }
    }
    // 回し蹴り：霞の輪が出た（最初のフレームは、前の輪を覚えるだけ）。
    if (a.shockSeen && boss.ringVfxId != a.previousRing && boss.ringVfxId != 0)
        PlayShock(world, boss, look, pose, feet, s.bodyShockKick);
    a.shockSeen = true;
    a.previousRing = boss.ringVfxId;
}

// 突進の尾：体が速く動く間だけ、肩と背から流れる光の帯と、空気のゆがみの尾を引く。
void UpdateDash(HIKARI::WorldView& world, Boss& boss, BossBody& look,
                const HIKARI::GameProjectTransform& body, const BossBodyPose& pose, float seconds) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    if (!s.bodyDashEffect.IsSet()) {
        StopHandle(world, a.dashVfx);
        return;
    }
    const Vec3 feet = Feet(boss, body);
    float speed = a.feetSeen && seconds > kTiny ? Length(Sub(feet, a.previousFeet)) / seconds : 0.0f;
    // 霧へ沈んでいる間（体の根が地面より下）は出さない。
    if (BossPose::RootHeight(boss) < -kTiny)
        speed = 0.0f;
    a.previousFeet = feet;
    a.feetSeen = true;
    const float target =
        std::clamp((speed - s.bodyDashMinSpeed) / std::max(kTiny, s.bodyDashFullSpeed - s.bodyDashMinSpeed), 0.0f, 1.0f);
    a.dash = Lerp(a.dash, target, ExpFollow(seconds, kDashFollowSeconds));
    // 肩と背から流れる帯の置き場所は 1 倍の体で作ってあるので、体の大きさを掛ける。
    StartOrSend(world, boss, a.dashVfx, VfxPlayback::Reference(s.bodyDashEffect), Placed(feet, look.bodyScale, body.rotation),
                Color(kDashInput, Mul(pose.bladeHot ? ToGP(s.bodyDashFireColor) : ToGP(s.bodyDashMistColor), a.dash)));
}

// 左手の所作の演出：投げナイフの前隙の間、掌に光が集まり、投げる瞬間に手元で弾ける。
// 所作と手の形は、動きの層（BossBodyLeftHand）が決めたものを読むだけ。
void UpdateHand(HIKARI::WorldView& world, Boss& boss, BossBody& look, const BossBodyPose& pose,
                float seconds) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    const BossBodyLeftGesture gesture = look.state.leftGesture;
    const bool acting = boss.state == BossState::Act;
    const int phase = acting ? static_cast<int>(boss.phase) : -1;
    // 光の強さ。投げナイフは投げた後は消す。
    const float target =
        gesture == BossBodyLeftGesture::Knife && acting && boss.phase == BossActPhase::Pre ? s.bodyHandKnife : 0.0f;
    a.hand = Lerp(a.hand, target, ExpFollow(seconds, s.bodyHandSeconds));
    const Vec3 color = ToGP(s.bodyHandMistColor);
    const Vec3 point =
        Add(pose.leftHand.position, Rotate(pose.leftHand.rotation, Mul(ToGP(s.bodyHandPoint), pose.leftHand.scale)));

    // 弾ける瞬間：投げナイフの前隙が終わった。
    bool burst = false;
    if (a.previousGesture >= 0) {
        const auto previous = static_cast<BossBodyLeftGesture>(a.previousGesture);
        const bool released = a.previousHandPhase == static_cast<int>(BossActPhase::Pre) &&
                              phase == static_cast<int>(BossActPhase::Act);
        burst = previous == BossBodyLeftGesture::Knife && released;
    }
    a.previousGesture = static_cast<int>(gesture);
    a.previousHandPhase = phase;
    if (burst && s.bodyHandBurst > 0.0f && !!s.bodyHandBurstEffect.IsSet()) {
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = VfxPlayback::Reference(s.bodyHandBurstEffect);
        play.transform = Placed(point, s.bodyHandBurst * pose.leftHand.scale);
        play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
        play.parameters = Color(kColorInput, color);
        BossVfx::Start(world, boss, play);
    }
    if (!s.bodyHandEffect.IsSet()) {
        StopHandle(world, a.handVfx);
        return;
    }
    StartOrSend(world, boss, a.handVfx, VfxPlayback::Reference(s.bodyHandEffect), Placed(point, pose.leftHand.scale, pose.leftHand.rotation),
                Color(kHandInput, Mul(color, a.hand)));
}

// 範囲爆発の溜め（c-3c908c）：叩きつけ（輪を放つ tick）までの残りの F（規則の Boss.ringCountdown）に合わせて、
// 集める（まわりの霧・火が渦を巻いて掌へ流れ込む。量は終わりへ向けて増える）→ 押し縮める（集めるのを止め、残りが掌へ
// 吸われて縮み、内へ縮む輪と一閃が出て、芯が強く光る）→ 叩きつけ（縮んだ芯だけが手に付いて落ち、爆発へ渡す）。
void UpdateGather(HIKARI::WorldView& world, Boss& boss, BossBody& look, const BossBodyPose& pose,
                  float seconds) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    const BossMove* move = CurrentMove(boss);
    const bool kicking = boss.state == BossState::Act && move != nullptr && move->type == BossMoveType::Kick &&
                         boss.phase != BossActPhase::Post;
    const bool fire = pose.bladeHot;
    const HIKARI::GameProjectVfxAssetReference& asset =
        fire && !!s.bodyGatherFireEffect.IsSet() ? VfxPlayback::Reference(s.bodyGatherFireEffect) : VfxPlayback::Reference(s.bodyGatherMistEffect);
    const int lead = std::max(1, s.bodyGatherLeadFrames);
    const int pinch = std::clamp(s.bodyGatherPinchFrames, 0, lead - 1);
    const int frames = kicking && !asset.IsEmpty() ? boss.ringCountdown : -1;
    const bool live = frames >= 0 && frames <= lead;
    const Vec3 point =
        Add(pose.leftHand.position, Rotate(pose.leftHand.rotation, Mul(ToGP(s.bodyHandPoint), pose.leftHand.scale)));
    const HIKARI::GameProjectTransform at = Placed(point, 1.0f);

    float gather = 0.0f;
    float core = 0.0f;
    bool pinched = false;
    if (live && frames > pinch) {
        const float t = static_cast<float>(lead - frames) / static_cast<float>(lead - pinch);
        const float ramp = std::clamp(t / kGatherRampShare, 0.0f, 1.0f);
        gather = ramp * ramp * (3.0f - 2.0f * ramp);
        core = s.bodyGatherCore * t * t;
        // 掌へ流れ込む腕の回り：終わりへ向けて速める（拍が詰まっていく）。
        a.gatherPhase += seconds * Lerp(s.bodyGatherArmRate, s.bodyGatherArmRateEnd, t * t);
    } else if (live) {
        pinched = true;
        const float u = pinch > 0 ? 1.0f - static_cast<float>(frames) / static_cast<float>(pinch) : 1.0f;
        core = Lerp(s.bodyGatherCore, s.bodyGatherCorePinched, std::clamp(u / kPinchBrightenShare, 0.0f, 1.0f));
        a.gatherClock += seconds;
        core *= 1.0f + s.bodyGatherCoreFlicker * std::sin(2.0f * kPi * kGatherFlickerHz * a.gatherClock);
    }
    if (!pinched)
        a.gatherClock = 0.0f;
    if (!live)
        a.gatherPhase = 0.0f;
    a.gatherArm = gather;
    a.gatherPoint = point;
    a.gatherCoreGain = Lerp(a.gatherCoreGain, core, ExpFollow(seconds, kGatherCoreFollowSeconds));
    a.gatherPull = gather;

    // 押し縮める瞬間のエフェクトは、出ている間、手に付けて動かす。
    if (a.pinchVfx.IsValid()) {
        if (!world.GetVfx().IsActive(a.pinchVfx)) {
            a.pinchVfx = {};
        } else {
            HIKARI::GameProjectVfxUpdateRequest update{};
            update.updateTransform = 1u;
            update.transform = at;
            boss.vfxAccepted = world.GetVfx().Update(a.pinchVfx, update) && boss.vfxAccepted;
        }
    }
    const Vec3 tint = fire ? ToGP(s.bodyGatherFireColor) : ToGP(s.bodyGatherMistColor);
    if (live) {
        // 途中で刀の火が変わった時（フェーズの変わり目など）は、出し直す。
        if (a.gatherVfx.IsValid() && a.gatherFire != fire)
            StopHandle(world, a.gatherVfx);
        HIKARI::GameProjectVfxParameterOverrides parameters{};
        VfxParameters::SetFloat(parameters, kGatherInput, gather);
        VfxParameters::SetColor(parameters, kCoreInput, {a.gatherCoreGain, a.gatherCoreGain, a.gatherCoreGain});
        VfxParameters::SetBool(parameters, kPinchInput, pinched);
        StartOrSend(world, boss, a.gatherVfx, asset, at, parameters);
        a.gatherFire = fire;
        a.gatherLinger = kGatherLingerSeconds;
        if (pinched && !a.gatherPinched && !!s.bodyGatherPinchEffect.IsSet()) {
            HIKARI::GameProjectVfxPlayRequest play{};
            play.asset = VfxPlayback::Reference(s.bodyGatherPinchEffect);
            play.transform = at;
            play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
            play.parameters = Color(kColorInput, tint);
            a.pinchVfx = BossVfx::Start(world, boss, play);
        }
        a.gatherPinched = pinched;
        return;
    }
    // 叩きつけた（または技が打ち切られた）：芯を消し、手の域をすぐ止める（爆発が引き継ぐ）。
    a.gatherPinched = false;
    if (!a.gatherVfx.IsValid())
        return;
    a.gatherLinger -= seconds;
    if (a.gatherLinger <= 0.0f) {
        StopHandle(world, a.gatherVfx);
        return;
    }
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kGatherInput, 0.0f);
    VfxParameters::SetColor(parameters, kCoreInput, {a.gatherCoreGain, a.gatherCoreGain, a.gatherCoreGain});
    VfxParameters::SetBool(parameters, kPinchInput, true);
    StartOrSend(world, boss, a.gatherVfx, asset, at, parameters);
}

} // namespace

BossBodyAccents::BladeKind BossBodyAccents::Kind(const Boss& boss) {
    const BossMove* move = CurrentMove(boss);
    if (boss.state != BossState::Act || move == nullptr || boss.phase == BossActPhase::Post || !UsesSword(move->type) ||
        !BossMoveTable::IsSwingType(move->type))
        return BladeKind::None;
    if (move->delivery == BossDelivery::Blade)
        return BladeKind::Real;
    for (const BossStrikeData& strike : move->strikes) {
        if (strike.speed > 0.0f)
            return BladeKind::Haze;
    }
    return BladeKind::Real;
}

void BossBodyAccents::StepSmoke(Boss& boss, BossBody& look, float seconds) {
    const BossBody& s = look;
    BossBodyAccentState& a = look.state.accents;
    // 振り終わった瞬間（命中の窓が閉じた）に吹き出し、決めた時間で収まる。
    if (a.wasWindow && !boss.swordSmoke)
        a.puff = 1.0f;
    // 怯んだ瞬間も、胴の煙が散るように吹き出す（c-808641）。
    if (boss.flinchCount != a.flinchSeen) {
        a.flinchSeen = boss.flinchCount;
        a.puff = 1.0f;
    }
    a.wasWindow = boss.swordSmoke;
    a.puff = std::max(0.0f, a.puff - seconds / std::max(kTiny, s.bodySmokeSwingPuffSeconds));
    // 続けてかわせる回数を使い切った間（かわさない間）は、煙が薄く静かになる（息が切れている。地面や画面の印の代わり）。
    const float spent = BossKasumiStep::Spent(boss) ? std::max(0.0f, boss.kasumiStepSpentSmoke) : 1.0f;
    // プレイヤーが近い時は薄く（カメラの前を覆わない。c-66ec5e）。
    float nearShare = 1.0f;
    if (boss.lookTargetSeen) {
        const float distance = FlatLength(Sub(boss.lookTarget, boss.ground));
        const float span = std::max(kTiny, s.bodySmokeFarMeters - s.bodySmokeNearMeters);
        nearShare = Lerp(s.bodySmokeNearScale, 1.0f, Smooth(std::clamp((distance - s.bodySmokeNearMeters) / span, 0.0f, 1.0f)));
    }
    const float amount =
        Lerp(1.0f, s.bodySmokeChargeAmount, ChargeAmount(boss)) * Lerp(1.0f, s.bodySmokeSwingPuff, a.puff) * spent * nearShare;
    // 吹き出しはすぐ立ち上げ、ほかはなめらかに追う。
    a.smokeAmount =
        amount > a.smokeAmount ? amount : Lerp(a.smokeAmount, amount, ExpFollow(seconds, kSmokeFollowSeconds));
    // フェーズ 1 で刀が燃えている間（火を残す技の前隙〜後隙）は、胴の煙も中から灼ける。
    const float heat = BossRules::IsBladeLit(boss) ? s.bodySmokeLitHeat : 1.0f;
    a.smokeHeat = Lerp(a.smokeHeat, heat, ExpFollow(seconds, kSmokeFollowSeconds));
}

void BossBodyAccents::Update(HIKARI::WorldView& world, Boss& boss, BossBody& look,
                             BossIntro* intro, const HIKARI::GameProjectTransform& body,
                             const BossBodyPose& pose, float seconds) {
    UpdateBlade(world, boss, look, intro, pose, seconds);
    UpdateCommit(world, boss, look, pose, seconds);
    UpdateEyes(world, boss, look, intro, pose, seconds);
    UpdateAura(world, boss, look, body, pose, seconds);
    UpdateShock(world, boss, look, body, pose);
    UpdateDash(world, boss, look, body, pose, seconds);
    UpdateHand(world, boss, look, pose, seconds);
    UpdateGather(world, boss, look, pose, seconds);
}

void BossBodyAccents::Stop(HIKARI::WorldView& world, BossBody& look) {
    BossBodyAccentState& a = look.state.accents;
    StopHandle(world, a.bladeVfx);
    StopHandle(world, a.bladeFireVfx);
    StopHandle(world, a.eyesVfx);
    StopHandle(world, a.glintVfx);
    StopHandle(world, a.auraVfx);
    StopHandle(world, a.dashVfx);
    StopHandle(world, a.handVfx);
    StopHandle(world, a.gatherVfx);
    StopHandle(world, a.pinchVfx);
    a.gatherPull = 0.0f;
    a.gatherArm = 0.0f;
    // 兜の中の灯を消す（体を出さない間：撃破の後など）。
    BossBodyPlacement nowhere{};
    nowhere.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    for (const HIKARI::Entity id : {look.bodyHelmSpotObject, look.bodyHelmFillObject})
        if (id.IsAlive())
            PlaceHelmLight(world.FindObject(HIKARI::Extend::HandleOf(id)), nowhere, {}, nowhere.rotation, {1.0f, 1.0f, 1.0f}, 0.0f);
    a = {};
}

} // namespace Project_TD2_2
