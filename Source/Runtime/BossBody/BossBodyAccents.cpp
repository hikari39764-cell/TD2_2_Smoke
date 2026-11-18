#include "BossBodyAccents.h"

#include <algorithm>

#include "BossBodyMath.h"
#include "../Boss/BossKasumiStep.h"
#include "../Boss/BossRules.h"
#include "../Boss/BossVfx.h"
#include "../Combat/Feedback.h"
#include "../Common/VfxParameters.h"

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
// 溜めた一撃を放った、と見る溜めの進み（前のフレームがこれ以上で、いまが 0）。
constexpr float kReleaseCharge = 0.85f;
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
Vec3 Feet(const BossComponent& boss, const HIKARI::GameProjectTransform& body) {
    return Add(body.position, Rotate(body.rotation, {0.0f, -boss.bodyHalfHeight * body.scale.y, 0.0f}));
}

// 刀を振って当てる技か（刀身を走る光を出す技）。蹴り・投げ・分身・歩きなどは違う。
bool UsesSword(BossMoveType type) {
    switch (type) {
    case BossMoveType::Kick:
    case BossMoveType::Knife:
    case BossMoveType::Ambush:
    case BossMoveType::Clone:
    case BossMoveType::WalkSide:
    case BossMoveType::WalkIn:
    case BossMoveType::Backstep:
    case BossMoveType::KasumiStep:
    case BossMoveType::Detonate:
    case BossMoveType::Greatsword:
    case BossMoveType::GreatswordCleave: return false;
    default: return true;
    }
}

// 歩き・下がり・霞歩のような、構えの合図を出さない技か。
bool IsFootwork(BossMoveType type) {
    return type == BossMoveType::WalkSide || type == BossMoveType::WalkIn || type == BossMoveType::Backstep ||
           type == BossMoveType::KasumiStep;
}

// 大技か（目の閃きを強くする）。
bool IsBigMove(BossMoveType type) {
    return type == BossMoveType::Detonate || type == BossMoveType::Skyfall || type == BossMoveType::Retreat ||
           type == BossMoveType::Greatsword || type == BossMoveType::GreatswordCleave;
}

// 刀に付くエフェクトの置き場所：原点＝刃の真ん中、+X＝剣先、+Y＝刃口（振る時に進む向き）、大きさ＝刃の長さ。
HIKARI::GameProjectTransform BladeTransform(const BossComponent& boss, const BossBodyPose& pose) {
    const float reach = VisibleBladeReach(boss);
    const Vec3 direction = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const Vec3 edge = Rotate(pose.sword.rotation, {0.0f, -1.0f, 0.0f});
    const float from = reach * std::clamp(boss.body.accents.bladeFromShare, 0.0f, 0.9f);
    const float span = reach - from;
    return Placed(Add(pose.sword.position, Mul(direction, from + span * 0.5f)), span, BasisRotation(direction, edge));
}

// 刃口の常時の光の目標（倍率。脈と燃える刀身の割合は掛ける前）：刀を見れば何をしているか分かる段階。待機は弱く、
// 前隙は進みにつれて強く（溜めで足す）、振る間（命中の窓）はいちばん強く、連撃の振りの間・投げる・蹴る間は構え切った
// 明るさ、後隙は待機へ。歩き・下がりは待機のまま。
float EdgeTarget(const BossComponent& boss, const BossMove* move, bool& pulsing) {
    const BossBodyAccentSettings& s = boss.body.accents;
    pulsing = false;
    if (boss.state == BossState::Dead)
        return 0.0f;
    if (boss.state == BossState::Stunned)
        return s.edgeStunned;
    if (boss.state != BossState::Act || move == nullptr || IsFootwork(move->type) || boss.phase == BossActPhase::Post)
        return s.edgeIdle;
    if (boss.swordSmoke)
        return s.edgeSwing;
    pulsing = true;
    const float charge = s.edgeCharge * std::max(0.0f, boss.chargeVisual);
    if (boss.phase == BossActPhase::Pre) {
        const float progress =
            move->pre > 0 ? std::clamp(static_cast<float>(boss.stateFrame) / static_cast<float>(move->pre), 0.0f, 1.0f)
                          : 1.0f;
        return Lerp(s.edgeIdle, s.edgeWindup, std::pow(progress, std::max(0.01f, s.edgeRampPower))) + charge;
    }
    return s.edgeWindup + charge;
}

// 刃口の常時の光と暗い縁を、刀に付くエフェクトの入力へ入れる。霞の大剣の間は、霧の大剣が刀を包むので出さない。
void SetEdge(HIKARI::GameProjectVfxParameterOverrides& parameters, BossComponent& boss, const BossBodyPose& pose,
             bool great, float seconds) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    bool pulsing = false;
    float target = great ? 0.0f : EdgeTarget(boss, CurrentMove(boss), pulsing);
    a.edgeClock = pulsing ? a.edgeClock + seconds : 0.0f;
    if (pulsing)
        target *= 1.0f + s.edgePulseDepth * std::sin(2.0f * kPi * s.edgePulseFrequency * a.edgeClock);
    if (pose.bladeHot)
        target *= s.edgeHotShare;
    // 実体の刀の技の間は、刃口の光を落とす（暗い縁と刃文だけ。霞の技と見分ける）。
    if (BossBodyAccents::Kind(boss) == BossBodyAccents::BladeKind::Real)
        target *= s.realEdgeShare;
    const float time = target > a.edgeGain ? s.edgeRiseSeconds : s.edgeFallSeconds;
    a.edgeGain = Lerp(a.edgeGain, target, Follow(seconds, time));
    const float dark = great || boss.state == BossState::Dead ? 0.0f : s.edgeDark;
    a.edgeDarkGain = Lerp(a.edgeDarkGain, dark, Follow(seconds, s.edgeFallSeconds));
    VfxParameters::SetColor(parameters, kEdgeInput, Mul(pose.bladeHot ? s.edgeFireColor : s.edgeMistColor, a.edgeGain));
    VfxParameters::SetColorAlpha(parameters, kEdgeDarkInput, {1.0f, 1.0f, 1.0f}, a.edgeDarkGain);
}

// 刀に付くエフェクト：見える刀の刃に付けて出し続け、振っている間は刀光を、刀が燃えている間は刀身の炎を入れる。
// 刃口の常時の光と暗い縁は、いつも入れる（段階で明るさが変わる）。
void UpdateBlade(HIKARI::WorldView& world, BossComponent& boss, const BossBodyPose& pose, float seconds) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    if (s.bladeEffect.IsEmpty()) {
        StopHandle(world, a.bladeVfx);
        return;
    }
    // 霞の大剣の間は、霧の大剣が刀を包んでいるので、刀光も刀身の炎も出さない。
    const bool great = boss.greatsword.phase != BossGreatswordPhase::None;
    // 刀光：命中の窓の間と、その後の振り抜き（剣先がまだ速い間）。
    a.trailHold = boss.swordSmoke ? s.trailFollowSeconds : std::max(0.0f, a.trailHold - seconds);
    const float tipSpeed = Length(boss.bodyState.motion.velocity) * VisibleBladeReach(boss);
    // 剣先が地面に届く少し手前で切る（地面に止められた剣先は地面に沿って滑り、折れ線の帯を引く）。
    const bool grounded = pose.swordTipClearance < s.trailGroundCut;
    const bool swinging = s.trail && !great && !grounded &&
                          (boss.swordSmoke || (a.trailHold > 0.0f && tipSpeed >= s.trailMinTipSpeed));
    a.trailGain = grounded ? 0.0f : Lerp(a.trailGain, swinging ? 1.0f : 0.0f, Follow(seconds, kTrailRiseSeconds));
    // 振りのゆがみ：刀光の間と、刀を振る技の打ち込みで剣先が速い間（命中の窓の少し前の振り出しから）。振りかぶりでは
    // 出さない。地面に沿う剣先では切る。
    const BossMove* move = CurrentMove(boss);
    const bool striking = boss.state == BossState::Act && boss.phase == BossActPhase::Act;
    const bool fast = s.swingHazeMinTipSpeed > 0.0f && striking && move != nullptr && UsesSword(move->type) &&
                      tipSpeed >= s.swingHazeMinTipSpeed;
    const bool hazing = s.trail && !great && !grounded && (swinging || fast);
    a.hazeGain = grounded ? 0.0f : Lerp(a.hazeGain, hazing ? 1.0f : 0.0f, Follow(seconds, kTrailRiseSeconds));
    // 刀身の炎：灼けた刀身（模型）が出ている間。
    const bool burning = pose.bladeHot && !great;
    a.heatGain = Lerp(a.heatGain, burning ? 1.0f : 0.0f, Follow(seconds, s.bladeHeatSeconds));

    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetColor(parameters, kTrailInput,
                            Mul(pose.bladeHot ? s.trailFireColor : s.trailMistColor, a.trailGain));
    const float heat = s.bladeHeat * a.heatGain;
    VfxParameters::SetColor(parameters, kHeatInput, {heat, heat, heat});
    VfxParameters::SetColor(parameters, kHazeInput, {s.swingHaze * a.hazeGain, 1.0f, 1.0f});
    SetEdge(parameters, boss, pose, great, seconds);
    // 常時の層：刃文の流れと霞の帯は、待機でも見える。霞の技の間は強め、実体の刀の技の間は弱める。大剣の間は出さない。
    const BossBodyAccents::BladeKind kind = BossBodyAccents::Kind(boss);
    const float stage = kind == BossBodyAccents::BladeKind::Haze   ? s.hazeBoost
                        : kind == BossBodyAccents::BladeKind::Real ? s.realFlowShare
                                                                   : 1.0f;
    const float alive = great || boss.state == BossState::Dead ? 0.0f : 1.0f;
    const float time = stage * alive > a.flowGain ? s.edgeRiseSeconds : s.edgeFallSeconds;
    a.flowGain = Lerp(a.flowGain, stage * alive, Follow(seconds, time));
    VfxParameters::SetColor(parameters, kFlowInput,
                            Mul(pose.bladeHot ? s.flowFireColor : s.flowMistColor, s.flowIdle * a.flowGain));
    VfxParameters::SetColor(parameters, kMistInput,
                            Mul(pose.bladeHot ? s.mistFireColor : s.mistMistColor, s.mistIdle * a.flowGain));
    // 剣先の残光：剣先が動いている間はいつも（速いほど明るい）。地面に沿う剣先・大剣の間は切る。
    const float nearGround =
        std::clamp((pose.swordTipClearance - s.trailGroundCut) / std::max(kTiny, s.wakeGroundFade), 0.0f, 1.0f);
    const float wake =
        great || grounded
            ? 0.0f
            : std::clamp((tipSpeed - s.wakeMinTipSpeed) / std::max(kTiny, s.wakeFullTipSpeed - s.wakeMinTipSpeed), 0.0f,
                         1.0f) *
                  nearGround;
    a.wakeGain = wake > a.wakeGain ? wake : Lerp(a.wakeGain, wake, Follow(seconds, s.edgeFallSeconds));
    VfxParameters::SetColor(parameters, kWakeInput,
                            Mul(pose.bladeHot ? s.wakeFireColor : s.wakeMistColor, s.wakeGain * a.wakeGain));
    StartOrSend(world, boss, a.bladeVfx, s.bladeEffect, BladeTransform(boss, pose), parameters);
}

// 技を出すと決めた瞬間（構えの合図が出た最初のフレーム）：目が閃き、刀を振る技なら鍔元から剣先へ光が走る。
void UpdateCommit(HIKARI::WorldView& world, BossComponent& boss, const BossBodyPose& pose, float seconds) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    const BossMove* move = CurrentMove(boss);
    const bool glint = move != nullptr && boss.glintFrames > 0 && !IsFootwork(move->type);
    const bool started = glint && !a.wasGlint;
    a.wasGlint = glint;
    a.flare = std::max(0.0f, a.flare - seconds / std::max(kTiny, s.eyeFlareSeconds) * std::max(s.eyeFlare, s.eyeFlareBig));
    if (started)
        a.flare = IsBigMove(move->type) ? s.eyeFlareBig : s.eyeFlare;

    // 刀身を走る光（1 回きり）。出ている間は、刃に付けて動かす。
    if (a.glintVfx.IsValid() && !world.GetVfx().IsActive(a.glintVfx))
        a.glintVfx = {};
    // 剣気を放った瞬間も、刀身を走る光をもう一度（明るく）出す。
    const bool released = boss.releaseCount != a.releaseSeen;
    a.releaseSeen = boss.releaseCount;
    // 放つ瞬間の全体の止め（項目で入れた時だけ。発射 310 より弱い優先度）。
    constexpr uint64_t kReleaseStopSource = 0x5444325245ull;
    if (released && s.releaseHoldTicks > 0)
        Feedback::RequestHitStop(world, kReleaseStopSource, 306, s.releaseHoldTicks, s.releaseHoldScale);
    const float bright = released && s.releaseGlint > 0.0f ? s.releaseGlint : 1.0f;
    const auto color = Color(kColorInput, Mul(pose.bladeHot ? s.glintFireColor : s.glintMistColor, bright));
    if (((started && UsesSword(move->type)) || (released && s.releaseGlint > 0.0f)) && !s.glintEffect.IsEmpty()) {
        StopHandle(world, a.glintVfx);
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = s.glintEffect;
        play.transform = BladeTransform(boss, pose);
        play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
        play.parameters = color;
        a.glintVfx = BossVfx::Start(world, boss, play);
    } else if (a.glintVfx.IsValid()) {
        BossVfx::Send(world, boss, a.glintVfx, BladeTransform(boss, pose), color);
    }
}

// 目の光：兜に付けて出し続ける。頭が動くと光が尾を引き、技を出すと決めた瞬間に横へ閃く。
void UpdateEyes(HIKARI::WorldView& world, BossComponent& boss, const BossBodyPose& pose) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    if (s.eyesEffect.IsEmpty()) {
        StopHandle(world, a.eyesVfx);
        return;
    }
    const Vec3 color = pose.eyesLit ? (pose.eyesFire ? s.eyesFireColor : s.eyesMistColor) : Vec3{};
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetColor(parameters, kEyesInput, color);
    VfxParameters::SetColor(parameters, kFlareInput, Mul(color, a.flare));
    StartOrSend(world, boss, a.eyesVfx, s.eyesEffect, Placed(pose.head.position, pose.head.scale, pose.head.rotation),
                parameters);
}

// 溜めの強さ（0〜1）：規則が書く溜めの進み、天降り斬りで落ちる間、霞の大剣を作り出す間。
float ChargeAmount(const BossComponent& boss) {
    float charge = std::clamp(boss.chargeVisual, 0.0f, 1.0f);
    if (boss.skyfall.phase == BossSkyfallPhase::Dive)
        charge = 1.0f;
    if (boss.greatsword.phase == BossGreatswordPhase::Conjure || boss.greatsword.phase == BossGreatswordPhase::Raise)
        charge = std::max(charge, boss.body.accents.auraConjure * std::clamp(boss.greatsword.fill, 0.0f, 1.0f));
    return charge;
}

// 溜めの気配：溜めている間、体を包んで立ちのぼる。体の足元に付けて出し続け、強さを入力で変える。
void UpdateAura(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body,
                const BossBodyPose& pose, float seconds) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    if (s.auraEffect.IsEmpty()) {
        StopHandle(world, a.auraVfx);
        return;
    }
    a.aura = Lerp(a.aura, ChargeAmount(boss), Follow(seconds, s.auraSeconds));
    // 資産は 1 倍の体（身長 4 m）で作ってあるので、外形の体の大きさ（Boss.bodyScale）を掛ける。
    StartOrSend(world, boss, a.auraVfx, s.auraEffect, Placed(Feet(boss, body), boss.body.bodyScale, body.rotation),
                Color(kAuraInput, Mul(pose.bladeHot ? s.auraFireColor : s.auraMistColor, a.aura)));
}

// 衝撃の輪を 1 つ出す（1 回きり）。at は地面の上の点、size は大きさの倍率。
void PlayShock(HIKARI::WorldView& world, BossComponent& boss, const BossBodyPose& pose, Vec3 at, float size) {
    const BossBodyAccentSettings& s = boss.body.accents;
    if (!(size > 0.0f) || s.shockEffect.IsEmpty())
        return;
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = s.shockEffect;
    play.transform = Placed({at.x, boss.ground.y, at.z}, size);
    play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
    play.parameters = Color(kColorInput, pose.bladeHot ? s.shockFireColor : s.shockMistColor);
    BossVfx::Start(world, boss, play);
}

// 重い一撃が当たった瞬間を、規則が書く様子の変わり目から見分けて、衝撃の輪を出す（当たった後だけ。予告はしない）。
void UpdateShock(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body,
                 const BossBodyPose& pose) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    const Vec3 feet = Feet(boss, body);
    const BossMove* move = CurrentMove(boss);
    const int skyfall = static_cast<int>(boss.skyfall.phase);
    const int great = static_cast<int>(boss.greatsword.phase);
    const int phase = move != nullptr ? static_cast<int>(boss.phase) : -1;
    if (a.previousSkyfall >= 0) {
        // 天降り斬り：落ちて来て着地した。
        if (a.previousSkyfall == static_cast<int>(BossSkyfallPhase::Dive) && boss.skyfall.phase == BossSkyfallPhase::Landed)
            PlayShock(world, boss, pose, feet, s.shockSkyfall);
        // 霞の大剣の縦割り：叩きつけた剣が地面に刺さった。
        if (a.previousGreatsword == static_cast<int>(BossGreatswordPhase::Slam) &&
            boss.greatsword.phase == BossGreatswordPhase::Stuck)
            PlayShock(world, boss, pose,
                      Add(boss.greatsword.hilt, Mul(boss.greatsword.direction, boss.greatsword.length)), s.shockCleave);
        // 回し蹴り：霞の輪が出た。
        if (boss.ringVfxId != a.previousRing && boss.ringVfxId != 0)
            PlayShock(world, boss, pose, feet, s.shockKick);
        if (move != nullptr && a.previousActPhase == static_cast<int>(BossActPhase::Pre) &&
            boss.phase == BossActPhase::Act) {
            // 引爆：爆ぜ始め。
            if (move->type == BossMoveType::Detonate)
                PlayShock(world, boss, pose, feet, s.shockDetonate);
            // 退き溜め：溜め切った一撃を放つ。
            if (move->type == BossMoveType::Retreat && a.previousCharge >= kReleaseCharge)
                PlayShock(world, boss, pose, feet, s.shockRelease);
        }
    }
    a.previousSkyfall = skyfall;
    a.previousGreatsword = great;
    a.previousActPhase = phase;
    a.previousRing = boss.ringVfxId;
    a.previousCharge = boss.chargeVisual;
}

// 突進の尾：体が速く動く間だけ、肩と背から流れる光の帯と、空気のゆがみの尾を引く。
void UpdateDash(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body,
                const BossBodyPose& pose, float seconds) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    if (s.dashEffect.IsEmpty()) {
        StopHandle(world, a.dashVfx);
        return;
    }
    const Vec3 feet = Feet(boss, body);
    float speed = a.feetSeen && seconds > kTiny ? Length(Sub(feet, a.previousFeet)) / seconds : 0.0f;
    // 瞬間移動（霞隠れ・分身。規則の層が数える）と、霧へ沈んでいる間は出さない。
    const bool teleported = boss.teleportCount != a.teleportSeen;
    a.teleportSeen = boss.teleportCount;
    if (teleported || BossPose::RootHeight(boss) < -kTiny)
        speed = 0.0f;
    a.previousFeet = feet;
    a.feetSeen = true;
    const float target =
        std::clamp((speed - s.dashMinSpeed) / std::max(kTiny, s.dashFullSpeed - s.dashMinSpeed), 0.0f, 1.0f);
    a.dash = Lerp(a.dash, target, Follow(seconds, kDashFollowSeconds));
    // 肩と背から流れる帯の置き場所は 1 倍の体で作ってあるので、体の大きさを掛ける。
    StartOrSend(world, boss, a.dashVfx, s.dashEffect, Placed(feet, boss.body.bodyScale, body.rotation),
                Color(kDashInput, Mul(pose.bladeHot ? s.dashFireColor : s.dashMistColor, a.dash)));
}

// 左手の所作の演出：所作の間、掌に光が集まり、握る・印を切る・投げる・刀身をなぞり終える瞬間に手元で弾ける。
// 所作と手の形は、動きの層（BossBodyLeftHand）が決めたものを読むだけ。
void UpdateHand(HIKARI::WorldView& world, BossComponent& boss, const BossBodyPose& pose, float seconds) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    const BossBodyMotionState& m = boss.bodyState.motion;
    const BossBodyLeftGesture gesture = m.leftGesture;
    const bool acting = boss.state == BossState::Act;
    const int phase = acting ? static_cast<int>(boss.phase) : -1;
    // 所作ごとの光の強さと色。引爆は握った後、投げナイフは投げた後は消す。
    float target = 0.0f;
    bool fire = a.handFire;
    switch (gesture) {
    case BossBodyLeftGesture::Detonate:
        target = pose.leftShape == BossBodyHandShape::Fist ? 0.0f : s.handDetonate;
        fire = true;
        break;
    case BossBodyLeftGesture::Seal:
        target = s.handSeal;
        fire = false;
        break;
    case BossBodyLeftGesture::Knife:
        target = acting && boss.phase == BossActPhase::Pre ? s.handKnife : 0.0f;
        fire = false;
        break;
    case BossBodyLeftGesture::FlameWipe:
        target = s.handWipe;
        fire = true;
        break;
    case BossBodyLeftGesture::Aim:
        target = s.handAim * std::clamp(boss.chargeVisual, 0.0f, 1.0f);
        fire = pose.bladeHot;
        break;
    default: break;
    }
    a.handFire = fire;
    a.hand = Lerp(a.hand, target, Follow(seconds, s.handSeconds));
    const Vec3 color = fire ? s.handFireColor : s.handMistColor;
    const Vec3 point =
        Add(pose.leftHand.position, Rotate(pose.leftHand.rotation, Mul(s.handPoint, pose.leftHand.scale)));

    // 弾ける瞬間：引爆で拳を握った、刀印・投げナイフの前隙が終わった、刀身をなぞり終えた。
    const int shape = static_cast<int>(pose.leftShape);
    bool burst = false;
    if (a.previousGesture >= 0) {
        const auto previous = static_cast<BossBodyLeftGesture>(a.previousGesture);
        const bool released = a.previousHandPhase == static_cast<int>(BossActPhase::Pre) &&
                              phase == static_cast<int>(BossActPhase::Act);
        burst = (gesture == BossBodyLeftGesture::Detonate && pose.leftShape == BossBodyHandShape::Fist &&
                 a.previousShape != static_cast<int>(BossBodyHandShape::Fist)) ||
                ((previous == BossBodyLeftGesture::Seal || previous == BossBodyLeftGesture::Knife) && released) ||
                (previous == BossBodyLeftGesture::FlameWipe && gesture != BossBodyLeftGesture::FlameWipe);
    }
    a.previousGesture = static_cast<int>(gesture);
    a.previousShape = shape;
    a.previousHandPhase = phase;
    if (burst && s.handBurst > 0.0f && !s.handBurstEffect.IsEmpty()) {
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = s.handBurstEffect;
        play.transform = Placed(point, s.handBurst * pose.leftHand.scale);
        play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
        play.parameters = Color(kColorInput, color);
        BossVfx::Start(world, boss, play);
    }
    if (s.handEffect.IsEmpty()) {
        StopHandle(world, a.handVfx);
        return;
    }
    StartOrSend(world, boss, a.handVfx, s.handEffect, Placed(point, pose.leftHand.scale, pose.leftHand.rotation),
                Color(kHandInput, Mul(color, a.hand)));
}

} // namespace

BossBodyAccents::BladeKind BossBodyAccents::Kind(const BossComponent& boss) {
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

void BossBodyAccents::StepSmoke(BossComponent& boss, float seconds) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    // 振り終わった瞬間（命中の窓が閉じた）に吹き出し、決めた時間で収まる。
    if (a.wasWindow && !boss.swordSmoke)
        a.puff = 1.0f;
    // 怯んだ瞬間も、胴の煙が散るように吹き出す（c-808641）。
    if (boss.flinchCount != a.flinchSeen) {
        a.flinchSeen = boss.flinchCount;
        a.puff = 1.0f;
    }
    a.wasWindow = boss.swordSmoke;
    a.puff = std::max(0.0f, a.puff - seconds / std::max(kTiny, s.smokeSwingPuffSeconds));
    // 続けてかわせる回数を使い切った間（かわさない間）は、煙が薄く静かになる（息が切れている。地面や画面の印の代わり）。
    const float spent = BossKasumiStep::Spent(boss) ? std::max(0.0f, boss.kasumiStep.spentSmoke) : 1.0f;
    const float amount =
        Lerp(1.0f, s.smokeChargeAmount, ChargeAmount(boss)) * Lerp(1.0f, s.smokeSwingPuff, a.puff) * spent;
    // 吹き出しはすぐ立ち上げ、ほかはなめらかに追う。
    a.smokeAmount = amount > a.smokeAmount ? amount : Lerp(a.smokeAmount, amount, Follow(seconds, kSmokeFollowSeconds));
    // フェーズ 1 で刀が燃えている間（火を残す技の前隙〜後隙）は、胴の煙も中から灼ける。
    const float heat = BossRules::IsBladeLit(boss) ? s.smokeLitHeat : 1.0f;
    a.smokeHeat = Lerp(a.smokeHeat, heat, Follow(seconds, kSmokeFollowSeconds));
}

void BossBodyAccents::Update(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body,
                             const BossBodyPose& pose, float seconds) {
    UpdateBlade(world, boss, pose, seconds);
    UpdateCommit(world, boss, pose, seconds);
    UpdateEyes(world, boss, pose);
    UpdateAura(world, boss, body, pose, seconds);
    UpdateShock(world, boss, body, pose);
    UpdateDash(world, boss, body, pose, seconds);
    UpdateHand(world, boss, pose, seconds);
}

void BossBodyAccents::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    BossBodyAccentState& a = boss.bodyState.accents;
    StopHandle(world, a.bladeVfx);
    StopHandle(world, a.eyesVfx);
    StopHandle(world, a.glintVfx);
    StopHandle(world, a.auraVfx);
    StopHandle(world, a.dashVfx);
    StopHandle(world, a.handVfx);
    a = {};
}

} // namespace Project_TD2_2
