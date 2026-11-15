#include "BossBodyAccents.h"

#include <algorithm>

#include "BossBodyMath.h"
#include "../Boss/BossRules.h"
#include "../Boss/BossVfx.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;

// 刀に付くエフェクトの入力（資産側の契約）：刀光の色と明るさ、刀身の炎の色と明るさ（どちらも Color。0 で出さない）。
constexpr const char* kTrailInput = "input.trail";
constexpr const char* kHeatInput = "input.heat";
// 刀光が立ち上がる・引く時間 (秒)。出ている帯は、エフェクト側の時間で消える。
constexpr float kTrailRiseSeconds = 0.02f;

// 刀に付くエフェクト：見える刀の刃に付けて出し続け、振っている間は刀光を、刀が燃えている間は刀身の炎を入れる。
// エフェクトの局所：原点＝刃の真ん中、+X＝剣先、+Y＝刃口（振る時に進む向き）、大きさ＝刃の長さ。
void UpdateBlade(HIKARI::WorldView& world, BossComponent& boss, const BossBodyPose& pose, float seconds) {
    const BossBodyAccentSettings& s = boss.body.accents;
    BossBodyAccentState& a = boss.bodyState.accents;
    if (s.bladeEffect.IsEmpty()) {
        BossVfx::StopHandle(world, a.bladeVfx);
        return;
    }
    const float reach = VisibleBladeReach(boss);
    // 霞の大剣の間は、霧の大剣が刀を包んでいるので、刀光も刀身の炎も出さない。
    const bool great = boss.greatsword.phase != BossGreatswordPhase::None;
    // 刀光：命中の窓の間と、その後の振り抜き（剣先がまだ速い間）。
    a.trailHold = boss.swordSmoke ? s.trailFollowSeconds : std::max(0.0f, a.trailHold - seconds);
    const float tipSpeed = Length(boss.bodyState.motion.velocity) * reach;
    // 剣先が地面に届く少し手前で切る（地面に止められた剣先は地面に沿って滑り、折れ線の帯を引く）。
    const bool grounded = pose.swordTipClearance < s.trailGroundCut;
    const bool swinging = s.trail && !great && !grounded &&
                          (boss.swordSmoke || (a.trailHold > 0.0f && tipSpeed >= s.trailMinTipSpeed));
    a.trailGain = grounded ? 0.0f : Lerp(a.trailGain, swinging ? 1.0f : 0.0f, Follow(seconds, kTrailRiseSeconds));
    // 刀身の炎：灼けた刀身（模型）が出ている間。
    const bool burning = pose.bladeHot && !great;
    a.heatGain = Lerp(a.heatGain, burning ? 1.0f : 0.0f, Follow(seconds, s.bladeHeatSeconds));

    const Vec3 direction = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const Vec3 edge = Rotate(pose.sword.rotation, {0.0f, -1.0f, 0.0f});
    const float from = reach * std::clamp(s.bladeFromShare, 0.0f, 0.9f);
    const float span = reach - from;
    const HIKARI::GameProjectTransform transform = BossVfx::Placed(
        Add(pose.sword.position, Mul(direction, from + span * 0.5f)), span, BasisRotation(direction, edge));
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetColor(parameters, kTrailInput,
                            Mul(pose.bladeHot ? s.trailFireColor : s.trailMistColor, a.trailGain));
    const float heat = s.bladeHeat * a.heatGain;
    VfxParameters::SetColor(parameters, kHeatInput, {heat, heat, heat});
    BossVfx::StartOrSend(world, boss, a.bladeVfx, s.bladeEffect, transform, parameters);
}

} // namespace

void BossBodyAccents::Update(HIKARI::WorldView& world, HIKARI::ObjectView, BossComponent& boss,
                             const BossBodyPose& pose, float seconds) {
    UpdateBlade(world, boss, pose, seconds);
}

void BossBodyAccents::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    BossVfx::StopHandle(world, boss.bodyState.accents.bladeVfx);
    boss.bodyState.accents = {};
}

} // namespace Project_TD2_2
