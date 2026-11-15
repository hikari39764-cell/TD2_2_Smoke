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
// 刀光が立ち上がる・引く時間 (秒)。出ている帯は、エフェクト側の時間で消える。
constexpr float kTrailRiseSeconds = 0.02f;
// 胴の煙の量・熱さを追う時間 (秒)。
constexpr float kSmokeFollowSeconds = 0.15f;

HIKARI::GameProjectVfxParameterOverrides Color(const char* input, Vec3 color) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetColor(parameters, input, color);
    return parameters;
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
    const Vec3 feet = Add(body.position, Rotate(body.rotation, {0.0f, -boss.bodyHalfHeight * body.scale.y, 0.0f}));
    StartOrSend(world, boss, a.auraVfx, s.auraEffect, Placed(feet, 1.0f, body.rotation),
                Color(kAuraInput, Mul(pose.bladeHot ? s.auraFireColor : s.auraMistColor, a.aura)));
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
}

void BossBodyAccents::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    BossBodyAccentState& a = boss.bodyState.accents;
    StopHandle(world, a.bladeVfx);
    StopHandle(world, a.eyesVfx);
    StopHandle(world, a.glintVfx);
    StopHandle(world, a.auraVfx);
    a = {};
}

} // namespace Project_TD2_2
