#include "BossBodyAccents.h"

#include <algorithm>

#include "BossBodyMath.h"
#include "../Boss/BossRules.h"
#include "../Boss/BossVfx.h"
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
constexpr const char* kEyesInput = "input.eyes";   // 目の光の色と明るさ
constexpr const char* kFlareInput = "input.flare"; // 目の閃きの色と明るさ
constexpr const char* kColorInput = "input.color"; // 刀身を走る光の色
constexpr const char* kAuraInput = "input.aura";   // 溜めの気配の色と強さ
constexpr const char* kDashInput = "input.dash";   // 突進の尾の色と明るさ
constexpr const char* kHandInput = "input.hand";   // 左手の光の色と明るさ
// これより速い 1 フレームの動きは、瞬間移動と見て速さに数えない (m/秒)。
constexpr float kTeleportSpeed = 300.0f;
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
    case BossMoveType::Detonate:
    case BossMoveType::Greatsword:
    case BossMoveType::GreatswordCleave: return false;
    default: return true;
    }
}

// 歩き・下がりのような、構えの合図を出さない技か。
bool IsFootwork(BossMoveType type) {
    return type == BossMoveType::WalkSide || type == BossMoveType::WalkIn || type == BossMoveType::Backstep;
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

// 刀に付くエフェクト：見える刀の刃に付けて出し続け、振っている間は刀光を、刀が燃えている間は刀身の炎を入れる。
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
    // 刀身の炎：灼けた刀身（模型）が出ている間。
    const bool burning = pose.bladeHot && !great;
    a.heatGain = Lerp(a.heatGain, burning ? 1.0f : 0.0f, Follow(seconds, s.bladeHeatSeconds));

    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetColor(parameters, kTrailInput,
                            Mul(pose.bladeHot ? s.trailFireColor : s.trailMistColor, a.trailGain));
    const float heat = s.bladeHeat * a.heatGain;
    VfxParameters::SetColor(parameters, kHeatInput, {heat, heat, heat});
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
    const auto color = Color(kColorInput, pose.bladeHot ? s.glintFireColor : s.glintMistColor);
    if (started && UsesSword(move->type) && !s.glintEffect.IsEmpty()) {
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
    StartOrSend(world, boss, a.auraVfx, s.auraEffect, Placed(Feet(boss, body), 1.0f, body.rotation),
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
    // 瞬間移動（霞隠れ・分身）と、霧へ沈んでいる間は出さない。
    if (speed > kTeleportSpeed || BossPose::RootHeight(boss) < -kTiny)
        speed = 0.0f;
    a.previousFeet = feet;
    a.feetSeen = true;
    const float target =
        std::clamp((speed - s.dashMinSpeed) / std::max(kTiny, s.dashFullSpeed - s.dashMinSpeed), 0.0f, 1.0f);
    a.dash = Lerp(a.dash, target, Follow(seconds, kDashFollowSeconds));
    StartOrSend(world, boss, a.dashVfx, s.dashEffect, Placed(feet, 1.0f, body.rotation),
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

void BossBodyAccents::StepSmoke(BossComponent& boss, float seconds) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    // 振り終わった瞬間（命中の窓が閉じた）に吹き出し、決めた時間で収まる。
    if (a.wasWindow && !boss.swordSmoke)
        a.puff = 1.0f;
    a.wasWindow = boss.swordSmoke;
    a.puff = std::max(0.0f, a.puff - seconds / std::max(kTiny, s.smokeSwingPuffSeconds));
    const float amount = Lerp(1.0f, s.smokeChargeAmount, ChargeAmount(boss)) * Lerp(1.0f, s.smokeSwingPuff, a.puff);
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
