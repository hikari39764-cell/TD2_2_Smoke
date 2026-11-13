#include "BossSkyfallVisuals.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/VfxView.h"
#include "BossMath.h"
#include "BossVfx.h"
#include "BossWaves.h"
#include "../Burn/BurnFieldRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;
using namespace BossVfx;

// 着地・跳ぶ所の塊を、地面からこの高さに出す。
constexpr float kSkyfallPuffLift = 0.5f;
// 炎の輪の出始めの大きさ（広がり切った時に対する割合）。
constexpr float kSkyfallRingStart = 0.2f;

// 落ちる間：体と一緒に落ちる刃（斬撃のエフェクトを 1 つ、落ちる向きに向けて持って行く）。着地で砕けさせる。
void UpdateBlade(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds, bool fire) {
    const BossSkyfallState& sky = boss.skyfall;
    BossSlashEffect& blade = boss.skyfallVisual.blade;
    if (blade.handle.IsValid() && !world.GetVfx().IsActive(blade.handle))
        blade = {};
    HIKARI::GameProjectTransform body{};
    if (sky.phase == BossSkyfallPhase::Dive && boss.skyfallBladeWidth > 0.0f && !boss.slashEffect.IsEmpty() &&
        object.GetWorldTransform(body)) {
        if (!blade.handle.IsValid()) {
            const Vec3 direction = Normalize(Sub(sky.target, body.position), {0.0f, -1.0f, 0.0f});
            const Vec3 across = Normalize(Cross({0.0f, 1.0f, 0.0f}, direction), {1.0f, 0.0f, 0.0f});
            blade = {};
            blade.flying = true;
            blade.position = body.position;
            blade.rotation = BasisRotation(direction, across);
            blade.scale = boss.skyfallBladeWidth / std::max(0.01f, boss.slashEffectWidth);
            StartSlash(world, boss, blade, SlashAsset(boss, fire), 0u);
        } else {
            if (seconds > 0.0f)
                blade.velocity = Mul(Sub(body.position, blade.position), 1.0f / seconds);
            blade.position = body.position;
        }
    } else if (blade.handle.IsValid() && blade.flying) {
        if (fire) {
            // フェーズ 2 の着地は炎の輪と火球が後を引き受けるので、刃はすぐ止める（刃の域と輪の域が重なると、
            // 流体域の数の上限を超えて、砲口の炎などが出ないフレームができる）。
            StopHandle(world, blade.handle);
            blade = {};
        } else {
            // 着地した（か、打ち切られた）：刃を出すのをやめて砕けさせ、勢いを落としてその場で散らす
            // （砕けた霞が、着地の上に大きく立ちのぼる）。
            blade.flying = false;
            blade.impactSeconds = boss.slashImpactSeconds;
            blade.fadeSeconds = boss.slashFadeSeconds;
            blade.velocity = Mul(blade.velocity, boss.slashHitSpeedScale);
        }
    }
    if (blade.handle.IsValid() && !blade.flying) {
        blade.fadeSeconds -= seconds;
        blade.glow = std::max(0.0f, blade.glow - seconds / std::max(0.001f, boss.slashGlowFadeSeconds));
        blade.impactSeconds = std::max(0.0f, blade.impactSeconds - seconds);
        blade.position = Add(blade.position, Mul(blade.velocity, seconds));
        if (blade.fadeSeconds <= 0.0f) {
            StopHandle(world, blade.handle);
            blade = {};
        }
    }
    if (blade.handle.IsValid())
        SendSlash(world, boss, blade);
}

// フェーズ 2 の着地の炎の輪：着地の次のフレームに出し、広がり切るまで燃料を出し、その後は光を下げて消す
// （流体の炎は自分で燃え尽きる）。
void UpdateRing(HIKARI::WorldView& world, BossComponent& boss, float seconds, bool landedNow) {
    const BossSkyfallState& sky = boss.skyfall;
    BossSkyfallVisualState& look = boss.skyfallVisual;
    const float effectRadius = std::max(0.01f, boss.ringEffectRadius);
    if (landedNow) {
        look.ringPending = boss.lastPhaseNumber >= 2 && !boss.ringEffect.IsEmpty() && boss.skyfallRingRadius > 0.0f;
    } else if (look.ringPending) {
        // 着地の次のフレーム：炎の輪を出す（技が打ち切られていたら出さない）。
        look.ringPending = false;
        if (sky.phase == BossSkyfallPhase::Landed && !look.ringVfx.IsValid()) {
            look.ringAge = 0.0f;
            look.ringCenter = {sky.target.x, boss.ground.y + boss.ringHeight, sky.target.z};
            HIKARI::GameProjectVfxPlayRequest play{};
            play.asset = boss.ringEffect;
            play.transform = Placed(look.ringCenter, kSkyfallRingStart * boss.skyfallRingRadius / effectRadius);
            play.parameters = RingParameters(true, 1.0f);
            look.ringVfx = Start(world, boss, play);
        }
    }
    if (look.ringVfx.IsValid() && !world.GetVfx().IsActive(look.ringVfx))
        look.ringVfx = {};
    if (!look.ringVfx.IsValid())
        return;
    look.ringAge += seconds;
    const float fade = look.ringAge - boss.skyfallRingSeconds;
    if (fade >= boss.ringFadeSeconds) {
        StopHandle(world, look.ringVfx);
        return;
    }
    const float grow = std::clamp(look.ringAge / std::max(0.001f, boss.skyfallRingSeconds), 0.0f, 1.0f);
    const float glow = fade <= 0.0f ? 1.0f : 1.0f - fade / std::max(0.001f, boss.ringFadeSeconds);
    const float scale = Lerp(kSkyfallRingStart, 1.0f, grow) * boss.skyfallRingRadius / effectRadius;
    Send(world, boss, look.ringVfx, Placed(look.ringCenter, scale), RingParameters(fade <= 0.0f, glow));
}

} // namespace

void BossSkyfallVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                                float seconds) {
    const BossSkyfallState& sky = boss.skyfall;
    BossSkyfallVisualState& look = boss.skyfallVisual;
    const BossSkyfallPhase seen = look.seenPhase;
    look.seenPhase = sky.phase;
    const bool fire = boss.lastPhaseNumber >= 2;
    // 跳び上がる瞬間：足元の霧を押しのけ、霞の塊を巻き上げる（爆発の枠へ入れて、同じフレームの後ろで出す）。
    if (sky.phase == BossSkyfallPhase::Rise && seen != BossSkyfallPhase::Rise && boss.skyfallTakeoffBlastScale > 0.0f) {
        BossWaves::AddBlast(boss, {.pushPending = true,
                                   .effectPending = true,
                                   .puff = true,
                                   .scale = boss.skyfallTakeoffBlastScale,
                                   .position = {sky.takeoff.x, sky.takeoff.y + kSkyfallPuffLift, sky.takeoff.z}});
    }
    // 落ちる所を決めた瞬間：剣がひときわ光る（光そのものは剣先の霞のエフェクトが出す。ここは強さだけ）。
    if (sky.locked && !look.seenLock)
        look.flare = 1.0f;
    look.seenLock = sky.locked;
    look.flare = std::max(0.0f, look.flare - seconds / std::max(0.001f, boss.skyfallLockFlareSeconds));
    UpdateBlade(world, object, boss, seconds, fire);
    // 着地した瞬間：着地点の周りに霞（フェーズ 2 は火球）の塊を立て、フェーズ 2 は次のフレームに炎の輪を広げる。
    // 真ん中の爆発と場の霧の押しは、規則が入れた爆発の枠（skyfallBlastScale）が出す。
    const bool landedNow = sky.phase == BossSkyfallPhase::Landed && seen != BossSkyfallPhase::Landed;
    if (landedNow) {
        for (int i = 0; i < boss.skyfallLandPuffs; ++i) {
            const float angle = 2.0f * kPi * (static_cast<float>(i) + 0.5f) / static_cast<float>(boss.skyfallLandPuffs);
            const Vec3 at{sky.target.x + std::cos(angle) * boss.skyfallLandPuffDistance,
                          sky.target.y + kSkyfallPuffLift,
                          sky.target.z + std::sin(angle) * boss.skyfallLandPuffDistance};
            BurnFieldRules::AddBlast(world, at, boss.skyfallLandPuffRadius,
                                     fire ? BurnBlastKind::Hit : BurnBlastKind::Smoke);
        }
    }
    UpdateRing(world, boss, seconds, landedNow);
}

void BossSkyfallVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    StopHandle(world, boss.skyfallVisual.blade.handle);
    boss.skyfallVisual.blade = {};
    StopHandle(world, boss.skyfallVisual.ringVfx);
}

} // namespace Project_TD2_2
