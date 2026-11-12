#include "BossSmoke.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "BossMath.h"
#include "BossPose.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 1 フレームにボスが出す注入の上限。場の霧の域が受け取れる干渉は 32 個までで、プレイヤー・ヒョウタン側の枠を残す。
constexpr uint32_t kEmitBudget = 20;
// 流体への提出 ID（持ち主はボスの物体なので、ヒョウタンの ID とは重ならない）。
constexpr uint64_t kSwordOuterId = 1, kSwordInnerId = 2, kKamaeId = 3, kBodyId = 4, kDepositBase = 10, kWaveBase = 32;
// 爆発が足元の霧を押す力の提出 ID と、押す向きの数、地面からの高さ (m)。
// 向きのある押しを輪に並べる（向きの無い膨張は霧を上へも持ち上げ、連撃でカメラが霧の中に入った）。
constexpr uint64_t kBlastPushBase = 110;
constexpr int kBlastPushCount = 6;
constexpr float kBlastPushHeight = 0.3f;
constexpr float kTau = 6.28318530718f;
// 斬撃 1 つが残り煙を入れる点の数と並び（判定の幅に対する位置）。
// 場の霧の域が受け取れる干渉は 32 個までなので、1 本 4 点に抑える。
constexpr int kWavePoints = 4;
constexpr float kWavePointOffsets[kWavePoints] = {-0.375f, -0.125f, 0.125f, 0.375f};
// 残り煙を入れる高さの下限（地面から m）。縦の斬撃の、地面より下の点を地面の上へ寄せる。
constexpr float kResidueFloor = 0.4f;
// 剣の霞を外側（剣先）と内側に分ける割合。剣先ほど濃い。
constexpr float kSwordOuterShare = 0.65f;
constexpr float kStrengthLimit = 10000.0f;
constexpr float kFramesPerSecond = 60.0f;

// 注入（Emit）が 1 秒に足す量 [密度 × m³] ÷ 強さ。重みは中心から (1 - r/R)²、床より下（中心から height 下）は
// 域の外なので数えない。GourdShotFogReturn の EmitVolume と同じ式（エンジンの注入の式が変わったら両方直す）。
float EmitVolume(float radius, float height) {
    const float h = std::clamp(height, 0.0f, radius);
    const float upper = kPi * radius * radius * radius / 15.0f;
    const float lower = 2.0f * kPi *
                        (radius * radius * h / 12.0f - h * h * h / 6.0f + h * h * h * h / (6.0f * radius) -
                         h * h * h * h * h / (20.0f * radius * radius));
    return upper + lower;
}

struct Emitter {
    HIKARI::WorldView& world;
    HIKARI::ObjectView object;
    BossComponent& boss;
    float smokePerVolume;
    uint32_t used = 0;

    // smokePerSecond は煙の単位（ヒョウタンの煙と同じ）。
    void Emit(uint64_t id, Vec3 position, float radius, float smokePerSecond, Vec3 velocity) {
        if (smokePerSecond <= 0.0f || used >= kEmitBudget || smokePerVolume <= 0.0f)
            return;
        Submit(id, position, radius, smokePerSecond / smokePerVolume / EmitVolume(radius, position.y - boss.ground.y),
               velocity);
    }

    void Submit(uint64_t id, Vec3 position, float radius, float strength, Vec3 velocity) {
        HIKARI::GameProjectFluidInteraction interaction{};
        interaction.kind = HIKARI::GameProjectFluidInteractionKind::Emit;
        interaction.position = position;
        // extents.y は温度。冷たい煙（0）なので昇らず、光らない。
        interaction.extents = {radius, 0.0f, 0.0f};
        interaction.direction = velocity;
        interaction.strength = std::min(kStrengthLimit, strength);
        boss.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), id, interaction).Accepted() && boss.fluidAccepted;
        ++used;
    }
};

} // namespace

void BossSmoke::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float smokePerVolume) {
    Emitter emitter{world, object, boss, smokePerVolume};
    if (boss.swordSmoke) {
        emitter.Emit(kSwordOuterId, boss.swordOuter, boss.swordSmokeRadius, boss.swordSmokeRate * kSwordOuterShare, {});
        emitter.Emit(kSwordInnerId, boss.swordInner, boss.swordSmokeRadius,
                     boss.swordSmokeRate * (1.0f - kSwordOuterShare), {});
    }
    if (boss.glintFrames > 0) {
        // 構え：剣が止まり、剣先に霞が集まる（地面や画面の予告の代わり）。
        const BossSwordPoints sword = BossPose::SwordPoints(boss, BossPose::Target(boss));
        emitter.Emit(kKamaeId, sword.outer, boss.swordSmokeRadius,
                     boss.kamaeSmoke / static_cast<float>(std::max(1, boss.kamaeSmokeFrames)) * kFramesPerSecond, {});
    }
    emitter.Emit(kBodyId, {boss.ground.x, boss.ground.y + boss.depositHeight, boss.ground.z}, boss.bodySmokeRadius,
                 boss.bodySmokeRate, {});
    for (size_t i = 0; i < boss.deposits.size(); ++i) {
        const auto& deposit = boss.deposits[i];
        if (deposit.active)
            emitter.Emit(kDepositBase + i, deposit.position, deposit.radius, deposit.rate, {});
    }
    for (size_t i = 0; i < boss.blasts.size(); ++i) {
        auto& blast = boss.blasts[i];
        if (!blast.pushPending)
            continue;
        blast.pushPending = false;
        if (!(boss.blastPush > 0.0f))
            continue;
        // 爆発：当たった所の真下から、地面に沿って六方へ 1 回押す。半径の半分ずつで輪を作り、全体で blastPushRadius まで届く。
        const float reach = boss.blastPushRadius * blast.scale * 0.5f;
        const Vec3 center{blast.position.x, boss.ground.y + kBlastPushHeight, blast.position.z};
        for (int k = 0; k < kBlastPushCount; ++k) {
            const float angle = kTau * static_cast<float>(k) / static_cast<float>(kBlastPushCount);
            HIKARI::GameProjectFluidInteraction push{};
            push.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
            push.direction = {std::cos(angle), 0.0f, std::sin(angle)};
            push.position = Add(center, Mul(push.direction, reach));
            push.extents = {reach, 0.0f, 0.0f};
            push.strength = boss.blastPush;
            const uint64_t id = kBlastPushBase + i * kBlastPushCount + static_cast<uint64_t>(k);
            boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, push).Accepted() && boss.fluidAccepted;
        }
    }
    for (size_t i = 0; i < boss.waves.size(); ++i) {
        const auto& wave = boss.waves[i];
        if (!wave.active || wave.knife || wave.smokeRate <= 0.0f)
            continue;
        // 斬撃の本体はエフェクト（BossVisuals）が見せる。ここは通った跡に残る煙（residuePerTwoMeters）だけを、
        // 刃の waveResidueLag 後ろの地面近くへ入れる。刃のすぐ近くへ入れると、斬撃のエフェクトの域にも入って三日月が崩れる。
        const Vec3 velocity =
            Add(Mul(wave.direction, wave.speed * boss.waveSmokeDrag), {0.0f, -boss.waveSmokeSink, 0.0f});
        const Vec3 behind = Sub(wave.position, Mul(wave.direction, boss.waveResidueLag));
        for (int p = 0; p < kWavePoints; ++p) {
            Vec3 position = Add(behind, Mul(wave.across, kWavePointOffsets[p] * wave.width));
            position.y = std::clamp(position.y, boss.ground.y + kResidueFloor,
                                    boss.ground.y + std::max(kResidueFloor, boss.waveResidueHeight));
            emitter.Emit(kWaveBase + i * kWavePoints + static_cast<uint64_t>(p), position, boss.waveSmokeRadius,
                         wave.smokeRate / kWavePoints, velocity);
        }
    }
}

} // namespace Project_TD2_2
