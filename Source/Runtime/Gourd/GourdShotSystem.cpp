#include "GourdShotSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/VfxView.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "GourdShotRules.h"
#include "GourdRules.h"
#include "../Combat/DamageableRules.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using Vec3 = HIKARI::GameProjectFloat3;
constexpr float kPi = 3.14159265358979323846f;
// 地面の注入源 1 個と他の演出に余裕を残す。値ではなく SDK の提出予算を守る規則。
constexpr uint32_t kTrailBudget = 8;
constexpr uint32_t kFluidBudget = 24;
constexpr uint64_t kMuzzleImpulseId = 100;
constexpr uint64_t kMuzzleEmitId = 101;
constexpr uint64_t kTrailEmitBaseId = 200;
constexpr uint64_t kResidueImpulseBaseId = 300;
constexpr uint64_t kResidueEmitBaseId = 400;

bool IsPlaying(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    return world.GetGameSession() != nullptr && !frame.paused;
}

Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Scale(Vec3 a, float value) { return {a.x * value, a.y * value, a.z * value}; }
Vec3 Difference(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Normalize(Vec3 a, Vec3 fallback) {
    const float length = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return length > 0.0f ? Scale(a, 1.0f / length) : fallback;
}

template <class Callback>
bool WithShooter(HIKARI::WorldView& world, HIKARI::ObjectView shotObject, Callback&& callback) {
    bool found = false;
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView gourdObject, GourdComponent& gourd) {
            if (gourdObject.GetHandle().value != shotObject.GetHandle().value)
                return;
            const auto playerObject = world.FindSceneObject(gourd.playerId);
            if (!playerObject.IsValid())
                return;
            world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                                    [&](HIKARI::ObjectView object, PlayerComponent& player) {
                                                        if (object.GetHandle().value != playerObject.GetHandle().value)
                                                            return;
                                                        found = true;
                                                        callback(gourd, object, player);
                                                    });
        });
    return found;
}

Vec3 Aim(HIKARI::WorldView& world, const GourdComponent& gourd, const PlayerComponent& player) {
    const auto fallback = PlayerRules::GetFacing(player);
    const auto target = world.FindObject(PlayerRules::GetLockTarget(player));
    HIKARI::GameProjectTransform transform{};
    if (target.IsValid() && target.GetWorldTransform(transform))
        return Normalize(Difference(transform.position, gourd.position), fallback);
    return PlayerRules::FlatNormalize(fallback);
}

void Log(const GourdShotComponent& shot, const char* message) {
    if (shot.traceEveryFrames > 0)
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdShot", message);
}

void LogPhase(const GourdShotComponent& shot, const char* event) {
    char message[384]{};
    std::snprintf(
        message, sizeof(message),
        "%s tick=%llu phase=%d beginTick=%llu nextFireTick=%llu lastShotTick=%llu recoveryEnd=%llu shots=%llu", event,
        static_cast<unsigned long long>(shot.processedTick), static_cast<int>(shot.phase),
        static_cast<unsigned long long>(shot.beginTick), static_cast<unsigned long long>(shot.nextFireTick),
        static_cast<unsigned long long>(shot.lastShotTick), static_cast<unsigned long long>(shot.recoveryEndTick),
        static_cast<unsigned long long>(shot.shotsFired));
    Log(shot, message);
}

int BlockedReason(const GourdShotComponent& shot, const GourdComponent& gourd, const PlayerComponent& player) {
    if (shot.staminaCost > 0.0f && PlayerRules::GetStamina(player) <= 0.0f)
        return 1;
    if (shot.smokeCost > 0.0f && GourdRules::GetSmoke(gourd) < shot.smokeCost)
        return 2;
    const auto free =
        std::count_if(shot.projectiles.begin(), shot.projectiles.end(), [](const GourdProjectile& projectile) {
            return !projectile.active && !projectile.finishTrailPending && !projectile.trail.IsValid();
        });
    return free < std::clamp(shot.pelletsPerShot, 1, static_cast<int>(kGourdProjectileCapacity)) ? 3 : 0;
}

bool CanPay(GourdShotComponent& shot, const GourdComponent& gourd, const PlayerComponent& player) {
    const int reason = BlockedReason(shot, gourd, player);
    if (reason != 0 && reason != shot.blockedReason) {
        char message[256]{};
        std::snprintf(message, sizeof(message), "Blocked tick=%llu reason=%d stamina=%.3f smoke=%.6f",
                      static_cast<unsigned long long>(shot.processedTick), reason,
                      static_cast<double>(PlayerRules::GetStamina(player)),
                      static_cast<double>(GourdRules::GetSmoke(gourd)));
        Log(shot, message);
    }
    shot.blockedReason = reason;
    return reason == 0;
}

void SpawnPellets(GourdShotComponent& shot, HIKARI::GameProjectObjectHandle player, Vec3 origin, Vec3 direction) {
    const int count = std::clamp(shot.pelletsPerShot, 1, static_cast<int>(kGourdProjectileCapacity));
    int pellet = 0;
    for (auto& projectile : shot.projectiles) {
        if (projectile.active || projectile.finishTrailPending || projectile.trail.IsValid() || pellet >= count)
            continue;
        const float fraction = count > 1 ? static_cast<float>(pellet) / static_cast<float>(count - 1) - 0.5f : 0.0f;
        const float angle = fraction * shot.spreadDegrees * kPi / 180.0f;
        const float cosine = std::cos(angle), sine = std::sin(angle);
        projectile = {};
        projectile.active = true;
        projectile.id = shot.nextProjectileId++;
        projectile.spawnedTick = shot.processedTick;
        projectile.position = origin;
        projectile.direction = {direction.x * cosine + direction.z * sine, direction.y,
                                direction.z * cosine - direction.x * sine};
        projectile.ignoredPlayer = player;
        projectile.speed = shot.initialSpeed;
        projectile.radius = shot.projectileRadius;
        ++pellet;
    }
}

bool Fire(HIKARI::WorldView& world, GourdShotComponent& shot, GourdComponent& gourd, HIKARI::ObjectView playerObject,
          PlayerComponent& player) {
    if (!CanPay(shot, gourd, player))
        return false;
    const float staminaBefore = PlayerRules::GetStamina(player), smokeBefore = GourdRules::GetSmoke(gourd);
    // 両方を先に照合する。煙不足でスタミナだけを失わず、0 の費用は制限そのものを外す。
    if (shot.staminaCost > 0.0f && !PlayerRules::TryConsumeStamina(player, shot.staminaCost, true))
        return false;
    if (shot.smokeCost > 0.0f && !GourdRules::TryConsumeSmoke(gourd, shot.smokeCost))
        return false;
    const Vec3 direction = Aim(world, gourd, player);
    const Vec3 origin = Add(gourd.position, Scale(direction, gourd.height * 0.5f));
    SpawnPellets(shot, playerObject.GetHandle(), origin, direction);
    PlayerRules::LockActions(player, shot.recoveryFrames);
    PlayerRules::AddRecoil(player, Scale(direction, -1.0f), shot.recoilDistance, shot.recoilFrames);
    GourdRules::AddRecoil(gourd, Scale(direction, -1.0f), shot.gourdRecoilDistance, shot.gourdRecoilFrames);
    shot.phase = GourdShotPhase::Recovery;
    shot.lastShotTick = shot.processedTick;
    shot.nextFireTick = shot.processedTick + static_cast<uint64_t>(shot.repeatFrames);
    shot.recoveryEndTick = shot.processedTick + static_cast<uint64_t>(shot.recoveryFrames);
    ++shot.shotsFired;
    shot.attackBuffer = 0;
    shot.muzzlePending = true;
    shot.muzzlePosition = origin;
    shot.muzzleDirection = direction;
    char message[512]{};
    std::snprintf(message, sizeof(message),
                  "Shot tick=%llu ordinal=%llu pellets=%d staminaBefore=%.3f stamina=%.3f smokeBefore=%.6f smoke=%.6f "
                  "nextFireTick=%llu recoveryEnd=%llu position=(%.6f,%.6f,%.6f) direction=(%.6f,%.6f,%.6f)",
                  static_cast<unsigned long long>(shot.processedTick), static_cast<unsigned long long>(shot.shotsFired),
                  shot.pelletsPerShot, static_cast<double>(staminaBefore),
                  static_cast<double>(PlayerRules::GetStamina(player)), static_cast<double>(smokeBefore),
                  static_cast<double>(GourdRules::GetSmoke(gourd)), static_cast<unsigned long long>(shot.nextFireTick),
                  static_cast<unsigned long long>(shot.recoveryEndTick), static_cast<double>(origin.x),
                  static_cast<double>(origin.y), static_cast<double>(origin.z), static_cast<double>(direction.x),
                  static_cast<double>(direction.y), static_cast<double>(direction.z));
    Log(shot, message);
    return true;
}

void StepPhase(HIKARI::WorldView& world, GourdShotComponent& shot, GourdComponent& gourd,
               HIKARI::ObjectView playerObject, PlayerComponent& player) {
    // 自分の動作ロック中は CanStartAction が false になるので、継続は通常状態と生存を確認する。
    if (!shot.enabled || !gourd.enabled || !PlayerRules::IsAlive(player) || player.state != PlayerState::Normal) {
        if (shot.phase != GourdShotPhase::Idle)
            LogPhase(shot, "Interrupted");
        shot.phase = GourdShotPhase::Idle;
        return;
    }
    if (shot.phase == GourdShotPhase::Recovery && shot.processedTick >= shot.recoveryEndTick) {
        shot.phase = GourdShotPhase::Idle;
        LogPhase(shot, "End");
    }
    if (shot.phase == GourdShotPhase::Idle && (shot.attackHeld || shot.attackBuffer > 0) &&
        PlayerRules::CanStartAction(player) && CanPay(shot, gourd, player)) {
        shot.phase = GourdShotPhase::Windup;
        shot.beginTick = shot.processedTick;
        shot.nextFireTick = shot.beginTick + static_cast<uint64_t>(shot.windupFrames);
        shot.attackBuffer = 0;
        // 発射 tick の Player::PreFixedUpdate が先に走るため、その境界までロックをつなぐ。
        // 発射時刻は増やさず、同じ tick の Fire が後隙のロックに置き換える。
        PlayerRules::LockActions(player, shot.windupFrames + 1);
        LogPhase(shot, "Begin");
    }
    if (shot.phase == GourdShotPhase::Windup && shot.processedTick >= shot.nextFireTick) {
        if (!Fire(world, shot, gourd, playerObject, player))
            shot.phase = GourdShotPhase::Idle;
    } else if (shot.phase == GourdShotPhase::Recovery && shot.processedTick >= shot.nextFireTick &&
               (shot.attackHeld || shot.attackBuffer > 0)) {
        Fire(world, shot, gourd, playerObject, player);
    }
}

void AddResidue(GourdShotComponent& shot, Vec3 position) {
    auto& residue = shot.residues[shot.residueCursor % kGourdResidueCapacity];
    ++shot.residueCursor;
    residue.active = true;
    residue.burstPending = true;
    residue.framesLeft = std::max(1, shot.residueFrames);
    residue.position = position;
}

bool ApplyHit(HIKARI::WorldView& world, uint64_t target, float amount, Vec3 direction, float& health) {
    bool accepted = false;
    world.ForEachComponent<DamageableComponent>(
        HIKARI::MakeComponentTypeId(kDamageableType), [&](HIKARI::ObjectView object, DamageableComponent& damageable) {
            if (object.GetHandle().value == target) {
                accepted = DamageableRules::ApplyDamage(damageable, amount, direction);
                health = DamageableRules::GetHealth(damageable);
            }
        });
    return accepted;
}

void LogProjectile(const GourdShotComponent& shot, const GourdProjectile& projectile, const char* event,
                   uint64_t hitObject = 0, bool damageAccepted = false, float health = 0.0f) {
    char message[512]{};
    std::snprintf(message, sizeof(message),
                  "%s tick=%llu id=%llu distance=%.6f speed=%.6f radius=%.6f density=%.6f damage=%.6f "
                  "hitObject=%llu damageAccepted=%d health=%.6f position=(%.6f,%.6f,%.6f)",
                  event, static_cast<unsigned long long>(shot.processedTick),
                  static_cast<unsigned long long>(projectile.id), static_cast<double>(projectile.distance),
                  static_cast<double>(projectile.speed), static_cast<double>(projectile.radius),
                  static_cast<double>(projectile.densityScale),
                  static_cast<double>(GourdShotRules::GetDamageAtDistance(shot, projectile.distance)),
                  static_cast<unsigned long long>(hitObject), static_cast<int>(damageAccepted),
                  static_cast<double>(health), static_cast<double>(projectile.position.x),
                  static_cast<double>(projectile.position.y), static_cast<double>(projectile.position.z));
    Log(shot, message);
}

void StepProjectile(HIKARI::WorldView& world, HIKARI::ObjectView owner, GourdShotComponent& shot,
                    GourdProjectile& projectile, float dt) {
    if (projectile.spawnedTick == shot.processedTick) {
        // 同じ描画フレームで砲口に源を作り、次の移動から最初の区間も掃掠する。
        LogProjectile(shot, projectile, "ProjectileSpawn");
        return;
    }
    const float speed = GourdShotRules::GetSpeedAtDistance(shot, projectile.distance);
    const float step = std::min(speed * dt, std::max(0.0f, shot.range - projectile.distance));
    HIKARI::GameProjectPhysicsShapeCastQuery query{};
    query.shape.type = HIKARI::GameProjectPhysicsShapeType::Sphere;
    query.shape.radius = GourdShotRules::GetRadiusAtDistance(shot, projectile.distance);
    query.startPosition = projectile.position;
    query.direction = projectile.direction;
    query.maxDistance = step;
    query.filter.ignoredObjectHandle = projectile.ignoredPlayer.value;
    query.filter.includeTriggers = 0;
    HIKARI::GameProjectPhysicsHit hit{};
    const bool didHit = step > 0.0f && world.GetPhysicsQueries().ShapeCast(query, hit) &&
                        hit.objectHandle != owner.GetHandle().value &&
                        hit.objectHandle != projectile.ignoredPlayer.value;
    const float traveled = didHit ? std::clamp(hit.distance, 0.0f, step) : step;
    projectile.distance = std::min(shot.range, projectile.distance + traveled);
    projectile.position = Add(projectile.position, Scale(projectile.direction, traveled));
    projectile.speed = GourdShotRules::GetSpeedAtDistance(shot, projectile.distance);
    projectile.radius = GourdShotRules::GetRadiusAtDistance(shot, projectile.distance);
    projectile.densityScale = GourdShotRules::GetDensityAtDistance(shot, projectile.distance);
    if (didHit) {
        float health = 0.0f;
        const bool accepted =
            ApplyHit(world, hit.objectHandle, GourdShotRules::GetDamageAtDistance(shot, projectile.distance),
                     projectile.direction, health);
        LogProjectile(shot, projectile, "ProjectileHit", hit.objectHandle, accepted, health);
        projectile.active = false;
        projectile.finishTrailPending = true;
        AddResidue(shot, projectile.position);
    } else if (projectile.distance >= shot.range) {
        LogProjectile(shot, projectile, "Dissipate");
        projectile.active = false;
        projectile.finishTrailPending = true;
        AddResidue(shot, projectile.position);
    } else if (shot.traceEveryFrames > 0 && shot.processedTick % static_cast<uint64_t>(shot.traceEveryFrames) == 0) {
        LogProjectile(shot, projectile, "ProjectileStep");
    }
}

void SetTrailParameters(HIKARI::GameProjectVfxParameterOverrides& parameters, float density) {
    parameters.count = 1;
    constexpr char kAmountInput[] = "input.amount";
    std::memcpy(parameters.values[0].inputId, kAmountInput, sizeof(kAmountInput));
    parameters.values[0].value.numeric[0] = density;
}

void UpdateTrail(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                 GourdProjectile& projectile, uint32_t& trailCount) {
    const auto vfx = world.GetVfx();
    if ((!projectile.active && !projectile.finishTrailPending) || shot.trailEffect.IsEmpty() ||
        trailCount >= kTrailBudget) {
        if (projectile.trail.IsValid()) {
            shot.vfxAccepted = vfx.Stop(projectile.trail) && shot.vfxAccepted;
            projectile.trail = {};
        }
        projectile.finishTrailPending = false;
        return;
    }
    ++trailCount;
    const float scale = projectile.radius / std::max(shot.projectileRadius, 0.01f);
    HIKARI::GameProjectTransform transform{};
    transform.position = projectile.position;
    transform.scale = {scale, scale, scale};
    if (!projectile.trail.IsValid()) {
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = shot.trailEffect;
        play.ownerObjectHandle = object.GetHandle().value;
        play.transform = transform;
        play.loopMode = HIKARI::GameProjectVfxLoopMode::Loop;
        SetTrailParameters(play.parameters, projectile.densityScale);
        projectile.trail = vfx.Play(play);
        shot.vfxAccepted = projectile.trail.IsValid() && shot.vfxAccepted;
    }
    if (projectile.trail.IsValid()) {
        HIKARI::GameProjectVfxUpdateRequest update{};
        update.updateTransform = 1;
        update.updateParameters = 1;
        update.transform = transform;
        SetTrailParameters(update.parameters, projectile.densityScale);
        // 固定 tick が唯一の弾道。自動速度は 0 にし、絶対位置の更新を源の掃掠注入へ渡す。
        shot.vfxAccepted =
            vfx.SetVelocity(projectile.trail, {}) && vfx.Update(projectile.trail, update) && shot.vfxAccepted;
    }
    // 全 LateUpdate の後で VFX が進むため、終点を渡したこのフレームは止めない。
    // 次の LateUpdate で Stop するまでスロットも再利用しない。
    if (!projectile.active)
        projectile.finishTrailPending = false;
}

void SubmitFluid(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot, uint32_t& total,
                 uint64_t id, HIKARI::GameProjectFluidInteractionKind kind, Vec3 position, float radius, float strength,
                 Vec3 direction = {}) {
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

void RenderSmoke(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot, uint32_t& total) {
    using Kind = HIKARI::GameProjectFluidInteractionKind;
    shot.fluidSubmissions = 0;
    if (shot.muzzlePending) {
        SubmitFluid(world, object, shot, total, kMuzzleImpulseId, Kind::Impulse, shot.muzzlePosition, shot.muzzleRadius,
                    shot.muzzleStrength, shot.muzzleDirection);
        SubmitFluid(world, object, shot, total, kMuzzleEmitId, Kind::Emit, shot.muzzlePosition, shot.muzzleRadius,
                    shot.muzzleSmokeRate);
        shot.muzzlePending = false;
    }
    for (size_t i = 0; i < shot.projectiles.size(); ++i) {
        const auto& projectile = shot.projectiles[i];
        if (projectile.active && !projectile.trail.IsValid())
            SubmitFluid(world, object, shot, total, kTrailEmitBaseId + i, Kind::Emit, projectile.position,
                        projectile.radius, shot.fallbackTrailRate * projectile.densityScale);
    }
    for (size_t i = 0; i < shot.residues.size(); ++i) {
        auto& residue = shot.residues[i];
        if (!residue.active)
            continue;
        if (residue.burstPending) {
            SubmitFluid(world, object, shot, total, kResidueImpulseBaseId + i, Kind::Impulse, residue.position,
                        shot.impactRadius, shot.impactStrength);
            residue.burstPending = false;
        }
        SubmitFluid(world, object, shot, total, kResidueEmitBaseId + i, Kind::Emit, residue.position,
                    shot.residueRadius, shot.residueSmokeRate);
    }
    if (shot.traceEveryFrames > 0 && shot.processedTick % static_cast<uint64_t>(shot.traceEveryFrames) == 0) {
        char message[256]{};
        std::snprintf(message, sizeof(message),
                      "Visual tick=%llu active=%u fluidSubmissions=%u totalShotFluid=%u dropped=%u vfxAccepted=%d "
                      "fluidAccepted=%d",
                      static_cast<unsigned long long>(shot.processedTick),
                      GourdShotRules::GetActiveProjectileCount(shot), shot.fluidSubmissions, total, shot.droppedVisuals,
                      static_cast<int>(shot.vfxAccepted), static_cast<int>(shot.fluidAccepted));
        Log(shot, message);
    }
}

} // namespace

void GourdShotSystem::Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    HIKARI::GameProjectInputActionState action{};
    const bool valid = world.GetInput().GetAction("Gameplay.Attack", action);
    world.ForEachComponent<GourdShotComponent>(HIKARI::MakeComponentTypeId(kGourdShotType),
                                               [&](HIKARI::ObjectView, GourdShotComponent& shot) {
                                                   shot.attackHeld = valid && action.down;
                                                   if (valid && action.pressed)
                                                       shot.attackBuffer = shot.inputBufferFrames;
                                               });
}

void GourdShotSystem::PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<GourdShotComponent>(
        HIKARI::MakeComponentTypeId(kGourdShotType), [&](HIKARI::ObjectView object, GourdShotComponent& shot) {
            shot.processedTick = frame.fixedTickIndex;
            const bool found = WithShooter(
                world, object, [&](GourdComponent& gourd, HIKARI::ObjectView playerObject, PlayerComponent& player) {
                    StepPhase(world, shot, gourd, playerObject, player);
                });
            if (!found)
                shot.phase = GourdShotPhase::Idle;
            shot.attackBuffer = std::max(0, shot.attackBuffer - 1);
        });
}

void GourdShotSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    world.ForEachComponent<GourdShotComponent>(
        HIKARI::MakeComponentTypeId(kGourdShotType), [&](HIKARI::ObjectView object, GourdShotComponent& shot) {
            for (auto& residue : shot.residues) {
                if (residue.active && --residue.framesLeft <= 0)
                    residue.active = false;
            }
            for (auto& projectile : shot.projectiles) {
                if (projectile.active)
                    StepProjectile(world, object, shot, projectile, frame.fixedDt);
            }
        });
}

void GourdShotSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    uint32_t trails = 0;
    world.ForEachComponent<GourdShotComponent>(
        HIKARI::MakeComponentTypeId(kGourdShotType), [&](HIKARI::ObjectView object, GourdShotComponent& shot) {
            if (GourdShotRules::IsFiring(shot)) {
                WithShooter(world, object, [&](GourdComponent& gourd, HIKARI::ObjectView, PlayerComponent& player) {
                    GourdRules::SetAimOverride(gourd, Aim(world, gourd, player));
                });
            }
            for (auto& projectile : shot.projectiles)
                UpdateTrail(world, object, shot, projectile, trails);
        });
}

void GourdShotSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    uint32_t submitted = 0;
    world.ForEachComponent<GourdShotComponent>(
        HIKARI::MakeComponentTypeId(kGourdShotType),
        [&](HIKARI::ObjectView object, GourdShotComponent& shot) { RenderSmoke(world, object, shot, submitted); });
}

void GourdShotSystem::OnWorldDetached(HIKARI::WorldView& world) {
    world.ForEachComponent<GourdShotComponent>(HIKARI::MakeComponentTypeId(kGourdShotType),
                                               [&](HIKARI::ObjectView, GourdShotComponent& shot) {
                                                   for (auto& projectile : shot.projectiles) {
                                                       if (projectile.trail.IsValid())
                                                           world.GetVfx().Stop(projectile.trail);
                                                       projectile.trail = {};
                                                   }
                                               });
}

} // namespace Project_TD2_2
