#include "GourdShotVisuals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/VfxView.h"
#include "GourdShotRules.h"
#include "../Common/VectorMath.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

using Vec3 = HIKARI::GameProjectFloat3;
// SDK の上限から地面霧と他のゲーム表現の枠を引いた、安全上の予算。
constexpr uint32_t kSourceBudget = 8;
constexpr uint32_t kFluidBudget = 24;
// GourdShotTrail の入力契約。資産の基準注入率・半径と組にする。
constexpr float kSmokeSourceRate = 24.0f;
constexpr float kSmokeSourceRadius = 0.25f;
// 足元の霧を外へ押す向きの数。向きのある押しを輪に並べる（向きの無い膨張は、上にある炎の域まで持ち上げて潰す）。
constexpr int kGroundPushCount = 6;
constexpr float kTau = 6.28318530718f;

void StopHandle(HIKARI::WorldView& world, GourdShotComponent& shot, HIKARI::GameProjectVfxPlaybackHandle& handle) {
    if (handle.IsValid()) {
        const auto vfx = world.GetVfx();
        if (vfx.IsActive(handle))
            shot.vfxAccepted = vfx.Stop(handle) && shot.vfxAccepted;
        handle = {};
    }
}

void SmokeParameters(HIKARI::GameProjectVfxParameterOverrides& parameters, float amount, Vec3 color) {
    VfxParameters::SetFloat(parameters, "input.amount", amount);
    VfxParameters::SetColor(parameters, "input.color", color);
}

void UpdateEffect(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                  HIKARI::GameProjectVfxPlaybackHandle& handle, const HIKARI::GameProjectVfxAssetReference& asset,
                  const HIKARI::GameProjectTransform& transform,
                  const HIKARI::GameProjectVfxParameterOverrides& parameters) {
    const auto vfx = world.GetVfx();
    if (handle.IsValid() && !vfx.IsActive(handle))
        handle = {};
    if (!handle.IsValid()) {
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = asset;
        play.ownerObjectHandle = object.GetHandle().value;
        play.transform = transform;
        play.loopMode = HIKARI::GameProjectVfxLoopMode::Loop;
        play.parameters = parameters;
        handle = vfx.Play(play);
        shot.vfxAccepted = handle.IsValid() && shot.vfxAccepted;
    }
    if (handle.IsValid()) {
        HIKARI::GameProjectVfxUpdateRequest update{};
        update.updateTransform = 1;
        update.updateParameters = 1;
        update.transform = transform;
        update.parameters = parameters;
        // 固定弾道が位置の唯一の持ち主。速度による二重移動をさせない。
        const bool velocityAccepted = vfx.SetVelocity(handle, {});
        const bool updateAccepted = vfx.Update(handle, update);
        shot.vfxAccepted = velocityAccepted && updateAccepted && shot.vfxAccepted;
    }
}

void UpdateTrail(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                 GourdProjectile& projectile, uint32_t& sourceCount) {
    if ((!projectile.active && !projectile.finishTrailPending) || shot.trailEffect.IsEmpty() ||
        shot.trailSmokeScale <= 0.0f || sourceCount >= kSourceBudget) {
        StopHandle(world, shot, projectile.trail);
        projectile.finishTrailPending = false;
        return;
    }
    ++sourceCount;
    const float scale = projectile.radius / std::max(shot.projectileRadius, 0.01f);
    HIKARI::GameProjectTransform transform{};
    transform.position = projectile.position;
    transform.scale = {scale, scale, scale};
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    SmokeParameters(parameters, projectile.densityScale * shot.trailSmokeScale, shot.smokeColor);
    UpdateEffect(world, object, shot, projectile.trail, shot.trailEffect, transform, parameters);
    // 終点の区間を VFX 更新へ渡した次のフレームで停止する。
    if (!projectile.active)
        projectile.finishTrailPending = false;
}

void UpdateShell(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                 GourdProjectile& projectile) {
    StopHandle(world, shot, projectile.staleShell);
    const bool lingering = !projectile.active && projectile.fadeFramesLeft > 0;
    if ((!projectile.active && !lingering) || shot.shellEffect.IsEmpty() || shot.shellScale <= 0.0f) {
        StopHandle(world, shot, projectile.shell);
        return;
    }
    // 射程の後半で弾が大きく薄くなる規則（MasterData）を、そのまま炎の源の大きさと燃料の量へ渡す。
    const float scale = shot.shellScale * projectile.radius / std::max(shot.projectileRadius, 0.01f);
    HIKARI::GameProjectTransform transform{};
    transform.position = projectile.position;
    transform.rotation = VectorMath::RotationFromXAxis(projectile.direction);
    transform.scale = {scale, scale, scale};
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    // 止まった後は燃料を切り、命中なら数 F だけ熱い閃きを弾く。域はすぐ止めるので、明るい塊が残らない。
    VfxParameters::SetFloat(parameters, "input.fly", projectile.active ? projectile.densityScale : 0.0f);
    VfxParameters::SetFloat(parameters, "input.flash", lingering && projectile.flashFramesLeft > 0 ? 1.0f : 0.0f);
    UpdateEffect(world, object, shot, projectile.shell, shot.shellEffect, transform, parameters);
}

void PlayBurst(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
               const GourdShotBurst& burst) {
    const auto& asset = burst.muzzle ? shot.muzzleEffect : shot.impactEffect;
    if (asset.IsEmpty() || burst.scale <= 0.0f)
        return;
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = asset;
    play.ownerObjectHandle = object.GetHandle().value;
    play.transform.position = burst.position;
    // 砲口の火の輪は撃つ向き（資産の +X）。爆発は世界の向きのまま（炎と煤が上へ昇る）。
    if (burst.muzzle)
        play.transform.rotation = VectorMath::RotationFromXAxis(burst.direction);
    play.transform.scale = {burst.scale, burst.scale, burst.scale};
    play.randomSeed = static_cast<uint32_t>(shot.burstsPlayed);
    play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
    // 一度きりの再生は資産の時間で自分から終わるので、ハンドルを持ち越さない。
    shot.vfxAccepted = world.GetVfx().Play(play).IsValid() && shot.vfxAccepted;
    ++shot.burstsPlayed;
}

void UpdateResidue(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                   GourdShotResidue& residue, uint32_t& sourceCount) {
    if (!residue.active || residue.framesLeft <= 0 || shot.residueSmokeRate <= 0.0f || shot.trailEffect.IsEmpty() ||
        sourceCount >= kSourceBudget) {
        StopHandle(world, shot, residue.smoke);
        return;
    }
    ++sourceCount;
    HIKARI::GameProjectTransform transform{};
    transform.position = residue.position;
    const float scale = shot.residueRadius / kSmokeSourceRadius;
    transform.scale = {scale, scale, scale};
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    const float fade = static_cast<float>(residue.framesLeft) / static_cast<float>(std::max(1, shot.residueFrames));
    SmokeParameters(parameters, shot.residueSmokeRate / kSmokeSourceRate * fade, shot.smokeColor);
    UpdateEffect(world, object, shot, residue.smoke, shot.trailEffect, transform, parameters);
}

void SubmitFluid(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot, uint32_t& total,
                 uint64_t id, HIKARI::GameProjectFluidInteractionKind kind, Vec3 position, float radius, float strength,
                 Vec3 direction = {}) {
    if (strength <= 0.0f)
        return;
    if (total >= kFluidBudget) {
        ++shot.droppedVisuals;
        return;
    }
    HIKARI::GameProjectFluidInteraction interaction{};
    interaction.kind = kind;
    interaction.position = position;
    interaction.extents = {radius, 0.0f, 0.0f};
    interaction.strength = strength;
    interaction.direction = direction;
    shot.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, interaction).Accepted() && shot.fluidAccepted;
    ++total;
    ++shot.fluidSubmissions;
}

} // namespace

void GourdShotVisuals::QueueBurst(GourdShotComponent& shot, bool muzzle, Vec3 position, Vec3 direction,
                                  float scale) {
    // 予約は輪で持つ。あふれたら一番古い予約を捨てる（同時 8 弾の命中まで収まる）。
    auto& burst = shot.bursts[shot.burstCursor % kGourdBurstCapacity];
    ++shot.burstCursor;
    if (burst.pending)
        ++shot.droppedVisuals;
    burst.pending = true;
    burst.muzzle = muzzle;
    burst.position = position;
    burst.direction = VectorMath::Normalize(direction, {0.0f, 1.0f, 0.0f});
    burst.scale = scale;
}

void GourdShotVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                              uint32_t& sourceCount) {
    for (auto& projectile : shot.projectiles) {
        UpdateTrail(world, object, shot, projectile, sourceCount);
        UpdateShell(world, object, shot, projectile);
    }
    for (auto& residue : shot.residues)
        UpdateResidue(world, object, shot, residue, sourceCount);
    for (auto& burst : shot.bursts) {
        if (!burst.pending)
            continue;
        PlayBurst(world, object, shot, burst);
        burst.pending = false;
    }
    if (shot.traceEveryFrames > 0 && shot.processedTick % static_cast<uint64_t>(shot.traceEveryFrames) == 0) {
        char message[192]{};
        std::snprintf(message, sizeof(message), "Effects tick=%llu sourceCount=%u bursts=%llu vfxAccepted=%d",
                      static_cast<unsigned long long>(shot.processedTick), sourceCount,
                      static_cast<unsigned long long>(shot.burstsPlayed), static_cast<int>(shot.vfxAccepted));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdShot", message);
    }
}

void GourdShotVisuals::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                              uint32_t& total) {
    using Kind = HIKARI::GameProjectFluidInteractionKind;
    shot.fluidSubmissions = 0;
    if (shot.muzzlePending) {
        SubmitFluid(world, object, shot, total, 100, Kind::Impulse, shot.muzzlePosition, shot.muzzleRadius,
                    shot.muzzleStrength, shot.muzzleDirection);
        SubmitFluid(world, object, shot, total, 101, Kind::Emit, shot.muzzlePosition, shot.muzzleRadius,
                    shot.muzzleSmokeRate);
        shot.muzzlePending = false;
    }
    for (size_t i = 0; i < shot.projectiles.size(); ++i) {
        const auto& projectile = shot.projectiles[i];
        if (projectile.active && !projectile.trail.IsValid() && shot.trailSmokeScale > 0.0f)
            SubmitFluid(world, object, shot, total, 200 + i, Kind::Emit, projectile.position, projectile.radius,
                        shot.fallbackTrailRate * projectile.densityScale * shot.trailSmokeScale);
    }
    for (size_t i = 0; i < shot.residues.size(); ++i) {
        auto& residue = shot.residues[i];
        if (!residue.active)
            continue;
        if (residue.burstPending) {
            // 爆ぜた点の真下から、地面に沿って六方へ押す。半径の半分ずつで輪を作り、全体で impactRadius まで届く。
            const float reach = shot.impactRadius * 0.5f;
            for (int k = 0; k < kGroundPushCount; ++k) {
                const float angle = kTau * static_cast<float>(k) / static_cast<float>(kGroundPushCount);
                const Vec3 outward{std::cos(angle), 0.0f, std::sin(angle)};
                SubmitFluid(world, object, shot, total, 300 + i * kGroundPushCount + static_cast<uint64_t>(k),
                            Kind::Impulse, VectorMath::Add(residue.position, VectorMath::Scale(outward, reach)),
                            reach, shot.impactStrength * residue.burstScale, outward);
            }
            residue.burstPending = false;
        }
        if (residue.framesLeft > 0 && !residue.smoke.IsValid()) {
            const float fade =
                static_cast<float>(residue.framesLeft) / static_cast<float>(std::max(1, shot.residueFrames));
            SubmitFluid(world, object, shot, total, 500 + i, Kind::Emit, residue.position, shot.residueRadius,
                        shot.residueSmokeRate * fade);
        }
    }
    if (shot.traceEveryFrames > 0 && shot.processedTick % static_cast<uint64_t>(shot.traceEveryFrames) == 0) {
        char message[256]{};
        std::snprintf(message, sizeof(message),
                      "Visual tick=%llu active=%u fluidSubmissions=%u totalShotFluid=%u dropped=%u vfxAccepted=%d "
                      "fluidAccepted=%d",
                      static_cast<unsigned long long>(shot.processedTick),
                      GourdShotRules::GetActiveProjectileCount(shot), shot.fluidSubmissions, total, shot.droppedVisuals,
                      static_cast<int>(shot.vfxAccepted), static_cast<int>(shot.fluidAccepted));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdShot", message);
    }
}

void GourdShotVisuals::Stop(HIKARI::WorldView& world, GourdShotComponent& shot) {
    for (auto& projectile : shot.projectiles) {
        StopHandle(world, shot, projectile.trail);
        StopHandle(world, shot, projectile.shell);
        StopHandle(world, shot, projectile.staleShell);
    }
    for (auto& residue : shot.residues)
        StopHandle(world, shot, residue.smoke);
    for (auto& burst : shot.bursts)
        burst.pending = false;
}

} // namespace Project_TD2_2
