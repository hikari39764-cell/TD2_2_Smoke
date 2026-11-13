#include "BossGreatswordVisuals.h"

#include <algorithm>

#include "HIKARI/Project/VfxView.h"
#include "BossMath.h"
#include "BossVfx.h"
#include "BossWaves.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;
using namespace BossVfx;

// 場の霧を押す所を、地面からこの高さに置く。
constexpr float kGreatswordPushLift = 0.5f;

// 薙ぎ終わった瞬間：剣が通った扇に沿って、場の霧を外へ押す（爆発の枠の「押し」だけを使う。光は出さない）。
void PushSweptFog(BossComponent& boss) {
    if (boss.greatswordSweepPushes <= 0 || !(boss.greatswordSweepPushDistance > 0.0f))
        return;
    const float yawEnd = YawOf(boss.greatsword.direction);
    const float swept = boss.greatswordSweepDegrees * kRadiansPerDegree;
    for (int i = 0; i < boss.greatswordSweepPushes; ++i) {
        const float back = swept * (static_cast<float>(i) + 0.5f) / static_cast<float>(boss.greatswordSweepPushes);
        const Vec3 at = Add(boss.ground, Mul(Forward(yawEnd - back), boss.greatswordSweepPushDistance));
        BossWaves::AddBlast(boss, {.pushPending = true, .position = {at.x, boss.ground.y + kGreatswordPushLift, at.z}});
    }
}

} // namespace

// 満ちる割合（柄から剣先へ）は規則が書く boss.greatsword.fill をそのまま送る。
void BossGreatswordVisuals::Update(HIKARI::WorldView& world, BossComponent& boss, float seconds) {
    const BossGreatswordState& sword = boss.greatsword;
    BossGreatswordVisualState& look = boss.greatswordVisual;
    const BossGreatswordPhase seen = look.seenPhase;
    look.seenPhase = sword.phase;
    if (look.vfx.IsValid() && !world.GetVfx().IsActive(look.vfx))
        look.vfx = {};
    const bool held = sword.phase != BossGreatswordPhase::None;
    if (held && seen == BossGreatswordPhase::None && look.vfx.IsValid()) {
        // 前の大剣がまだ消えかけている：止めて、次のフレームに出す（止めた域もそのフレームの間は流体域の数に入る）。
        StopHandle(world, look.vfx);
        look.seenPhase = BossGreatswordPhase::None;
        return;
    }
    if (held) {
        // 剣の平らな面：作る間はボスの正面へ向け（プレイヤーから剣の形が見える）、薙ぐ時は上へ向ける（刃が進む向きに立つ）。
        // 薙ぐ時の面は真上から少しボスの正面へ傾け、前にいるプレイヤーからも面が見えるようにする。
        const Vec3 front = Forward(boss.yaw);
        const Vec3 swing =
            Normalize(Add(Vec3{0.0f, 1.0f, 0.0f}, Mul(front, boss.greatswordSweepFaceTilt)), {0.0f, 1.0f, 0.0f});
        const Vec3 normal = Normalize(Lerp(front, swing, sword.wind), {0.0f, 1.0f, 0.0f});
        look.transform = Placed(sword.hilt, 1.0f, BasisRotation(Cross(sword.direction, normal), sword.direction));
        look.fade = boss.greatswordFadeSeconds;
    } else if (look.vfx.IsValid()) {
        // 技が終わった・崩された：出すのをやめ、残った霞が消えるまで待ってから止める（域ごと急に消さない）。
        look.fade -= seconds;
        if (look.fade <= 0.0f) {
            StopHandle(world, look.vfx);
            return;
        }
    } else {
        return;
    }
    const auto& asset = sword.fire ? boss.greatswordFireEffect : boss.greatswordMistEffect;
    if (!look.vfx.IsValid() && asset.IsEmpty())
        return;
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kFillInput, held ? sword.fill : 1.0f);
    // 薙ぐ間は多めに流し込む。ほどける間と終わった後は、もう出さない（残った霞が自分で消える）。
    const float amount = sword.phase == BossGreatswordPhase::Conjure ? 1.0f
                         : sword.phase == BossGreatswordPhase::Sweep ? boss.greatswordSweepAmount
                                                                     : 0.0f;
    VfxParameters::SetFloat(parameters, kAmountInput, amount);
    StartOrSend(world, boss, look.vfx, asset, look.transform, parameters);
    if (sword.phase == BossGreatswordPhase::Dissolve && seen == BossGreatswordPhase::Sweep)
        PushSweptFog(boss);
}

void BossGreatswordVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    StopHandle(world, boss.greatswordVisual.vfx);
}

} // namespace Project_TD2_2
