#include "GourdShotFogReturn.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/ProjectLog.h"
#include "../Common/VectorMath.h"

namespace Project_TD2_2 {
namespace {

using VectorMath::Add;
using VectorMath::Scale;
using Vec3 = HIKARI::GameProjectFloat3;

constexpr float kPi = 3.14159265358979323846f;
constexpr float kFramesPerSecond = 60.0f;
// 通り道に落とす点の上限（1 発あたり）。射程 15 m・間隔 2.5 m で 4〜5 点。
constexpr int kTrailPointLimit = 6;
// これより近い予約は 1 つにまとめる（連射で同じ所へ落ち続けても数を増やさない）。
constexpr float kMergeDistance = 1.5f;
// 1 フレームに注入する予約の上限。場の霧の域が受け取れる干渉は 32 個までで、吸い込みと押し広げにも枠を残す。
constexpr uint32_t kEmitBudget = 8;
// 流体への提出 ID。GourdShotVisuals は 100〜599 を使う。
constexpr uint64_t kEmitInstanceBase = 600;
// 終点の煙を降ろし始める高さの上限 (m)。高い所の命中でも、場の霧の域の中から始める。
constexpr float kDropHeightLimit = 2.5f;
// 落とす中心が回る速さ (rad／F) と、予約ごとに回り始めをずらす角度（黄金角。隣の予約と揃わない）。
constexpr float kWanderRadiansPerFrame = 0.35f;
constexpr float kWanderPhaseStep = 2.399963f;
constexpr float kSmokeEpsilon = 0.0001f;

float FlatDistance(Vec3 a, Vec3 b) {
    const float x = a.x - b.x, z = a.z - b.z;
    return std::sqrt(x * x + z * z);
}

// 注入（Emit）が 1 秒に足す量 [密度 × m³] ÷ 強さ。重みは中心から (1 - r/R)² で、床（中心から height 下）より下は域の外 なので数えない。球全体は 2πR³/15、中心より下の厚さ h の層は 2π(R²h/12 − h³/6 + h⁴/6R − h⁵/20R²)。
float EmitVolume(float radius, float height) {
    const float h = std::clamp(height, 0.0f, radius);
    const float upper = kPi * radius * radius * radius / 15.0f;
    const float lower = 2.0f * kPi *
                        (radius * radius * h / 12.0f - h * h * h / 6.0f + h * h * h * h / (6.0f * radius) -
                         h * h * h * h * h / (20.0f * radius * radius));
    return upper + lower;
}

bool NearFire(const GourdShotComponent& shot, Vec3 point) {
    for (const auto& zone : shot.fireZones) {
        if (zone.framesLeft > 0 && FlatDistance(zone.position, point) < shot.fogReturnClearance)
            return true;
    }
    for (const auto& projectile : shot.projectiles) {
        if ((projectile.active || projectile.fadeFramesLeft > 0) &&
            FlatDistance(projectile.position, point) < shot.fogReturnClearance)
            return true;
    }
    return false;
}

void AddDeposit(GourdShotComponent& shot, Vec3 ground, float dropHeight, float smoke) {
    if (smoke <= kSmokeEpsilon)
        return;
    const float perFrame = smoke / static_cast<float>(std::max(1, shot.fogReturnFrames));
    // 1 か所が戻す速さの上限は「1 発ぶんを fogReturnFrames で出し切る速さ」。まとめた分は時間を延ばして出す。
    const float perFrameLimit = shot.fogReturnSmoke / static_cast<float>(std::max(1, shot.fogReturnFrames));
    GourdFogDeposit* nearest = nullptr;
    GourdFogDeposit* free = nullptr;
    float nearestDistance = 0.0f;
    for (auto& deposit : shot.fogDeposits) {
        if (!deposit.active) {
            if (free == nullptr)
                free = &deposit;
            continue;
        }
        const float distance = FlatDistance(deposit.ground, ground);
        if (nearest == nullptr || distance < nearestDistance) {
            nearest = &deposit;
            nearestDistance = distance;
        }
    }
    shot.fogReturnQueued += smoke;
    // 空きが無い時も捨てず、一番近い予約へ足す（煙の総量を保つ）。
    if (nearest != nullptr && (nearestDistance < kMergeDistance || free == nullptr)) {
        nearest->smokeLeft += smoke;
        nearest->smokePerFrame = std::min(perFrameLimit, nearest->smokePerFrame + perFrame);
        return;
    }
    if (free == nullptr)
        return;
    *free = {};
    free->active = true;
    free->ground = ground;
    free->dropHeight = std::clamp(dropHeight - shot.fogReturnHeight, 0.0f, kDropHeightLimit);
    free->smokeLeft = smoke;
    free->smokePerFrame = std::min(perFrameLimit, perFrame);
    free->delayFrames = std::max(0, shot.fogReturnDelayFrames);
}

} // namespace

bool GourdShotFogReturn::FindGround(HIKARI::WorldView& world, Vec3 from, float reach, uint64_t ignoredObject,
                                    Vec3& ground) {
    HIKARI::GameProjectPhysicsRaycastQuery query{};
    query.origin = from;
    query.direction = {0.0f, -1.0f, 0.0f};
    query.maxDistance = reach;
    query.filter.ignoredObjectHandle = ignoredObject;
    query.filter.includeTriggers = 0;
    HIKARI::GameProjectPhysicsHit hit{};
    if (!world.GetPhysicsQueries().Raycast(query, hit))
        return false;
    ground = hit.position;
    return true;
}

void GourdShotFogReturn::MarkFire(GourdShotComponent& shot, Vec3 position, int frames) {
    if (frames <= 0)
        return;
    auto& zone = shot.fireZones[shot.fireZoneCursor % kGourdFireZoneCapacity];
    ++shot.fireZoneCursor;
    zone.framesLeft = frames;
    zone.position = position;
}

void GourdShotFogReturn::Queue(HIKARI::WorldView& world, GourdShotComponent& shot, const GourdProjectile& projectile,
                               uint64_t ignoredObject) {
    if (shot.fogReturnSmoke <= 0.0f)
        return;
    // 通り道：砲口の炎と終点の炎に重なる両端を除いて、等間隔に落とす。
    const float spacing = std::max(0.5f, shot.fogReturnTrailSpacing);
    const float last = projectile.distance - spacing;
    const int count =
        last >= spacing ? std::min(kTrailPointLimit, static_cast<int>((last - spacing) / spacing) + 1) : 0;
    const float trailSmoke = count > 0 ? shot.fogReturnSmoke * std::clamp(shot.fogReturnTrailShare, 0.0f, 1.0f) : 0.0f;
    float placedSmoke = 0.0f;
    for (int k = 0; k < count; ++k) {
        const float along = spacing * static_cast<float>(k + 1);
        Vec3 ground{};
        if (!FindGround(world, Add(projectile.origin, Scale(projectile.direction, along)), shot.impactGroundReach,
                        projectile.ignoredPlayer.value, ground))
            continue;
        const float smoke = trailSmoke / static_cast<float>(count);
        AddDeposit(shot, ground, 0.0f, smoke);
        placedSmoke += smoke;
    }
    // 終点：通り道に落とせなかった分もここへ回す。地面に届かない高さで終わった弾の煙は戻らない。
    Vec3 ground{};
    if (FindGround(world, projectile.position, shot.impactGroundReach, ignoredObject, ground))
        AddDeposit(shot, ground, projectile.position.y - ground.y, shot.fogReturnSmoke - placedSmoke);
}

void GourdShotFogReturn::Step(GourdShotComponent& shot) {
    for (auto& zone : shot.fireZones)
        zone.framesLeft = std::max(0, zone.framesLeft - 1);
    for (auto& deposit : shot.fogDeposits) {
        if (!deposit.active)
            continue;
        deposit.rate = 0.0f;
        // 出し切った次の tick で閉じる（最後の tick の間の描画フレームにも注入させる）。
        if (deposit.smokeLeft <= kSmokeEpsilon) {
            deposit.active = false;
            continue;
        }
        if (deposit.delayFrames > 0) {
            --deposit.delayFrames;
            continue;
        }
        if (NearFire(shot, deposit.ground))
            continue;
        const float amount = std::min(deposit.smokeLeft, deposit.smokePerFrame);
        deposit.smokeLeft -= amount;
        deposit.rate = amount * kFramesPerSecond;
        ++deposit.age;
        shot.fogReturned += amount;
    }
}

void GourdShotFogReturn::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                                uint32_t& fluidCount, float smokePerVolume) {
    uint32_t emitted = 0, waiting = 0;
    for (size_t i = 0; i < shot.fogDeposits.size(); ++i) {
        const auto& deposit = shot.fogDeposits[i];
        if (!deposit.active)
            continue;
        if (deposit.rate <= 0.0f || emitted >= kEmitBudget || smokePerVolume <= 0.0f) {
            ++waiting;
            continue;
        }
        // 終点の煙は爆ぜた高さから地面へ降ろす。動く注入は前のフレームからの線分に沿って広がるので、柱になって落ちる。
        const float fall =
            shot.fogReturnFallFrames > 0
                ? std::clamp(1.0f - static_cast<float>(deposit.age) / static_cast<float>(shot.fogReturnFallFrames),
                             0.0f, 1.0f)
                : 0.0f;
        const float height = shot.fogReturnHeight + deposit.dropHeight * fall;
        HIKARI::GameProjectFluidInteraction interaction{};
        interaction.kind = HIKARI::GameProjectFluidInteractionKind::Emit;
        // 中心を小さく回しながら、回る向きへ煙を流す。動く注入は線分に沿って広がるので、丸い塊ではなく渦の筋になる。
        const float phase =
            kWanderPhaseStep * static_cast<float>(i) + kWanderRadiansPerFrame * static_cast<float>(deposit.age);
        const Vec3 around{std::cos(phase), 0.0f, std::sin(phase)};
        interaction.position = {deposit.ground.x + around.x * shot.fogReturnWander, deposit.ground.y + height,
                                deposit.ground.z + around.z * shot.fogReturnWander};
        // extents.y は温度。冷たい煙（0）なので昇らず、光らない。
        interaction.extents = {shot.fogReturnRadius, 0.0f, 0.0f};
        interaction.direction = {-around.z * shot.fogReturnSwirl, -shot.fogReturnSink, around.x * shot.fogReturnSwirl};
        interaction.strength = deposit.rate / smokePerVolume / EmitVolume(shot.fogReturnRadius, height);
        shot.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), kEmitInstanceBase + i, interaction).Accepted() &&
            shot.fluidAccepted;
        ++emitted;
        ++fluidCount;
        ++shot.fluidSubmissions;
    }
    if (shot.traceEveryFrames > 0 && shot.processedTick % static_cast<uint64_t>(shot.traceEveryFrames) == 0 &&
        (emitted > 0 || waiting > 0)) {
        char message[192]{};
        std::snprintf(message, sizeof(message), "FogReturn tick=%llu emitting=%u waiting=%u queued=%.3f returned=%.3f",
                      static_cast<unsigned long long>(shot.processedTick), emitted, waiting,
                      static_cast<double>(shot.fogReturnQueued), static_cast<double>(shot.fogReturned));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdShot", message);
    }
}

void GourdShotFogReturn::Clear(GourdShotComponent& shot) {
    for (auto& deposit : shot.fogDeposits)
        deposit = {};
    for (auto& zone : shot.fireZones)
        zone = {};
}

} // namespace Project_TD2_2
