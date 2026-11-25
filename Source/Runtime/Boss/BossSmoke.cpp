#include "BossSmoke.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "../Common/FireObstacle.h"
#include "../Common/FluidEmit.h"
#include "BossMath.h"
#include "BossPose.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 1 フレームにボスが出す注入の上限。場の霧の域が受け取れる干渉は 32 個までで、プレイヤー・ヒョウタン側の枠を残す。
constexpr uint32_t kEmitBudget = 20;
// 流体への提出 ID（持ち主はボスの物体なので、ヒョウタンの ID とは重ならない）。
constexpr uint64_t kSwordOuterId = 1, kSwordInnerId = 2, kKamaeId = 3, kBodyId = 4, kDepositBase = 10, kWaveBase = 32;
// 残り煙の枠（爆発の跡の煙も含む）の ID は kDepositBase から枠の数だけ。斬撃の ID と重ねない。
static_assert(kDepositBase + kBossDepositCapacity <= kWaveBase);
// 爆発が足元の霧を押す力の提出 ID と、押す向きの数、地面からの高さ (m)。
// 向きのある押しを輪に並べる（向きの無い膨張は霧を上へも持ち上げ、連撃でカメラが霧の中に入った）。
constexpr uint64_t kBlastPushBase = 110;
constexpr uint64_t kFireObstacleId = 5;
constexpr int kBlastPushCount = 6;
constexpr float kBlastPushHeight = 0.3f;
// 回し蹴りの霞の輪が足元の霧を外へ押す力の提出 ID と、輪に並べる向きの数（爆発と同じ、向きのある押しを輪に並べる）。
constexpr uint64_t kRingPushBase = 160;
// 飛ぶ斬撃が、真下の場の霧を左右へ割る押しの提出 ID（斬撃 1 つにつき左右 2 つ）。
constexpr uint64_t kWavePartBase = 200;
// 振りで場の霧を揺らす力（刀の外側・内側の 2 つ。c-4be37e）。
constexpr uint64_t kSwingPushBase = 300;
// 両手の霧の刀（c-40f6c1）：刀身に沿って霧を入れる点の数と、その提出 ID。
constexpr uint64_t kTwoHandMistPoints = 4;
constexpr uint64_t kTwoHandMistBase = 310;
// 左手にまとう霧（c-40f6c1）の提出 ID と、吹き出す向きが左手の周りを 1 秒に回る回数。
constexpr uint64_t kLeftHandMistId = 320;
constexpr float kLeftHandMistTurnsPerSecond = 1.5f;
constexpr int kRingPushCount = 6;
// 斬撃 1 つが残り煙を入れる点の数と並び（判定の幅に対する位置）。
// 場の霧の域が受け取れる干渉は 32 個までなので、1 本 4 点に抑える。
constexpr int kWavePoints = 4;
constexpr float kWavePointOffsets[kWavePoints] = {-0.375f, -0.125f, 0.125f, 0.375f};
// 残り煙を入れる高さの下限（地面から m）。縦の斬撃の、地面より下の点を地面の上へ寄せる。
constexpr float kResidueFloor = 0.4f;
// 剣の霞を外側（剣先）と内側に分ける割合。剣先ほど濃い。
constexpr float kSwordOuterShare = 0.65f;
constexpr float kStrengthLimit = 10000.0f;

using FluidEmit::EmitVolume;

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

void BossSmoke::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                       const BossBodyComponent* look, float smokePerVolume) {
    Emitter emitter{world, object, boss, smokePerVolume};
    boss.fluidAccepted =
        FireObstacle::Submit(world, object, kFireObstacleId, boss.fireObstacleRadius, boss.fireObstacleHeight) &&
        boss.fluidAccepted;
    if (boss.swordSmoke) {
        emitter.Emit(kSwordOuterId, boss.swordOuter, boss.swordSmokeRadius, boss.swordSmokeRate * kSwordOuterShare, {});
        emitter.Emit(kSwordInnerId, boss.swordInner, boss.swordSmokeRadius,
                     boss.swordSmokeRate * (1.0f - kSwordOuterShare), {});
        // 振りの空気（c-4be37e）：刃が進む向き（規則の剣の構えの斬る向き）へ、刀の外側・内側の所で場の霧を押す。
        const Vec3 edge = RotateY(boss.swordPose.edge, boss.yaw);
        if (boss.swingPushForce > 0.0f && Length(edge) > 0.5f) {
            const Vec3 at[2] = {boss.swordOuter, boss.swordInner};
            for (uint64_t k = 0; k < 2; ++k) {
                HIKARI::GameProjectFluidInteraction push{};
                push.kind = HIKARI::GameProjectFluidInteractionKind::Force;
                push.direction = Normalize(edge);
                push.position = at[k];
                push.extents = {boss.swingPushRadius, 0.0f, 0.0f};
                push.strength = boss.swingPushForce;
                boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), kSwingPushBase + k, push).Accepted() &&
                                     boss.fluidAccepted;
            }
        }
    }
    // 両手の霧の刀（c-40f6c1）：左手が柄に重なっている間、刀身に沿って（当たりの伸びた先まで）場の霧へ霧を入れる。
    if (const float hold = BossAnimBody::TwoHand(boss); hold > 0.01f && boss.animBody.twoHandMist > 0.0f) {
        const BossSwordPoints blade = BossPose::SwordPoints(boss, boss.swordPose);
        for (uint64_t k = 0; k < kTwoHandMistPoints; ++k) {
            const float u = (static_cast<float>(k) + 0.5f) / static_cast<float>(kTwoHandMistPoints);
            emitter.Emit(kTwoHandMistBase + k, Lerp(blade.base, blade.outer, u), boss.animBody.twoHandMistRadius,
                         boss.animBody.twoHandMist * hold / static_cast<float>(kTwoHandMistPoints), {});
        }
    }
    // 左手にまとう霧（c-40f6c1）：骨格の体が出ている間、見えている左手の所から、手の周りを回る向きに吹き出す。
    if (look != nullptr && look->anim.leftHandSeen && boss.animBody.leftHandMist > 0.0f) {
        const float angle = kTau * kLeftHandMistTurnsPerSecond * boss.floatClock;
        const Vec3 swirl{std::cos(angle) * boss.animBody.leftHandMistSwirl, 0.0f,
                         std::sin(angle) * boss.animBody.leftHandMistSwirl};
        emitter.Emit(kLeftHandMistId, look->anim.leftHandWorld, boss.animBody.leftHandMistRadius,
                     boss.animBody.leftHandMist, swirl);
    }
    if (boss.glintFrames > 0) {
        // 構え：剣が止まり、剣先に霞が集まる（地面や画面の予告の代わり）。
        const BossSwordPoints sword = BossPose::SwordPoints(boss, boss.swordPose);
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
    for (size_t i = 0; i < boss.waves.size() && boss.ringPush > 0.0f; ++i) {
        auto& wave = boss.waves[i];
        if (!wave.active || !wave.ring ||
            (wave.pushedRadius >= 0.0f && wave.radius - wave.pushedRadius < boss.ringPushRadius))
            continue;
        // 霞の輪：広がっている輪の線の上で、足元の霧を地面に沿って外へ押す（霧そのものが輪になって広がる）。
        // 押しは 1 回きりの速さなので、輪が押す半径ぶん広がるごとに 1 回だけ入れる（毎フレーム入れると強すぎる）。
        wave.pushedRadius = wave.radius;
        for (int k = 0; k < kRingPushCount; ++k) {
            const float angle = kTau * (static_cast<float>(k) + 0.5f) / static_cast<float>(kRingPushCount);
            HIKARI::GameProjectFluidInteraction push{};
            push.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
            push.direction = {std::sin(angle), 0.0f, std::cos(angle)};
            push.position = Add({wave.position.x, boss.ground.y + kBlastPushHeight, wave.position.z},
                                Mul(push.direction, wave.radius));
            push.extents = {boss.ringPushRadius, 0.0f, 0.0f};
            push.strength = boss.ringPush;
            boss.fluidAccepted = world.GetFluid()
                                     .Submit(object.GetHandle(), kRingPushBase + static_cast<uint64_t>(k), push)
                                     .Accepted() &&
                                 boss.fluidAccepted;
        }
    }
    // 斬撃が場の霧を割る：刃の真下の霧を、飛ぶ向きの左右へ毎フレーム 1 回ずつ押す（通った跡に溝が残り、霧が戻って埋まる）。
    for (size_t i = 0; i < boss.waves.size() && boss.wavePartPush > 0.0f && boss.wavePartRadius > 0.0f; ++i) {
        const auto& wave = boss.waves[i];
        if (!wave.active || wave.knife || wave.ring)
            continue;
        const Vec3 forward = FlatNormalize(wave.direction, {0.0f, 0.0f, 1.0f});
        const Vec3 side{forward.z, 0.0f, -forward.x};
        for (int k = 0; k < 2; ++k) {
            const Vec3 outward = Mul(side, k == 0 ? 1.0f : -1.0f);
            HIKARI::GameProjectFluidInteraction push{};
            push.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
            push.direction = outward;
            push.position = Add({wave.position.x, boss.ground.y + kBlastPushHeight, wave.position.z},
                                Mul(outward, boss.wavePartRadius * 0.6f));
            push.extents = {boss.wavePartRadius, 0.0f, 0.0f};
            push.strength = boss.wavePartPush;
            const uint64_t id = kWavePartBase + i * 2 + static_cast<uint64_t>(k);
            boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, push).Accepted() && boss.fluidAccepted;
        }
    }
    for (size_t i = 0; i < boss.waves.size(); ++i) {
        const auto& wave = boss.waves[i];
        if (!wave.active || wave.knife || wave.ring || wave.smokeRate <= 0.0f)
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

void BossSmoke::Describe(HIKARI::PropertyFields& fields, BossComponent& boss) {
    if (fields.Section("残る煙（場の霧へ入れる）")) {
        fields.Float("residuePerTwoMeters", "斬撃が 2 m 進むごとに残す煙", boss.residuePerTwoMeters, 0.0f, 1000.0f);
        fields.Float("knifeEndResidueScale", "ナイフが消えた所に残す煙（斬撃の何倍）", boss.knifeEndResidueScale, 0.0f,
                     10.0f);
        fields.Float("bossResidue", "ボスが動いた跡に残す煙（1 回ぶん）", boss.bossResidue, 0.0f, 1000.0f);
        fields.Float("bossResidueStep", "…この距離動くごと (m)", boss.bossResidueStep, 0.1f, 100.0f);
        fields.Float("swingSmoke", "剣の一振りが描く煙（仮）", boss.swingSmoke, 0.0f, 1000.0f);
        fields.Float("kamaeSmoke", "構えで剣先に集まる煙（仮）", boss.kamaeSmoke, 0.0f, 1000.0f);
        fields.Int("kamaeSmokeFrames", "構えの煙を出す長さ (F)", boss.kamaeSmokeFrames, 1, 120);
        fields.Note(
            "斬撃の残り煙は、刃が通り過ぎた後ろの地面近くへ入れます（吸える霧）。斬撃そのものの見た目はエフェクトです。");
        fields.Float("waveSmokeRadius", "斬撃の残り煙の 1 点の半径 (m)", boss.waveSmokeRadius, 0.05f, 10.0f);
        fields.Float("waveResidueLag", "残り煙を入れる位置（刃の何 m 後ろか。斬撃のエフェクトの域より後ろにする）",
                     boss.waveResidueLag, 0.0f, 20.0f);
        fields.Float("waveResidueHeight", "残り煙を入れる高さの上限（地面から m）", boss.waveResidueHeight, 0.0f,
                     10.0f);
        fields.Float("waveSmokeDrag", "残り煙を押す速さ（斬撃の速さに対する割合）", boss.waveSmokeDrag, 0.0f, 2.0f);
        fields.Float("waveSmokeSink", "残り煙が下へ落ちる速さ (m/秒)", boss.waveSmokeSink, 0.0f, 20.0f);
        fields.Float("swordSmokeRadius", "剣が煙を入れる半径 (m)", boss.swordSmokeRadius, 0.05f, 10.0f);
        fields.Float("depositRadius", "斬撃が消えた所の煙の半径 (m)", boss.depositRadius, 0.05f, 10.0f);
        fields.Float("depositHeight", "残す煙の高さ（地面から m）", boss.depositHeight, 0.0f, 5.0f);
        fields.Int("depositFrames", "消えた所の煙を出し切る長さ (F)", boss.depositFrames, 1, 600);
        fields.Float("blastFogSmoke", "爆発の跡に残す煙（爆発の大きさ 1 あたり。0 は残さない）", boss.blastFogSmoke,
                     0.0f, 1000.0f);
        fields.Float("blastFogRadius", "…その半径 (m。爆発の大きさ 1 の時)", boss.blastFogRadius, 0.05f, 10.0f);
        fields.Int("blastFogDelayFrames", "…爆発から出し始めるまで (F。爆発の炎と光が消えてから出す)",
                   boss.blastFogDelayFrames, 0, 600);
        fields.Int("blastFogFrames", "…出し切る長さ (F)", boss.blastFogFrames, 1, 600);
        fields.Float("bodySmokeRadius", "ボスの通った跡の煙の半径 (m)", boss.bodySmokeRadius, 0.05f, 10.0f);
        fields.Float("fireObstacleRadius", "場の火が体をよける半径 (m。0 でよけない)", boss.fireObstacleRadius, 0.0f,
                     10.0f);
        fields.Float("fireObstacleHeight", "場の火が体をよける高さ (m)", boss.fireObstacleHeight, 0.0f, 20.0f);
        fields.Float("waveAbsorbSmoke", "吸い込み：斬撃 1 つを吸った時にヒョウタンへ入る煙", boss.waveAbsorbSmoke, 0.0f,
                     1000.0f);
        fields.Float("knifeAbsorbSmoke", "吸い込み：ナイフ 1 本を吸った時にヒョウタンへ入る煙", boss.knifeAbsorbSmoke,
                     0.0f, 1000.0f);
        fields.Note("煙の単位はヒョウタンの煙と同じ。場の霧の量への換算はヒョウタンの「霧の量の換算」を使います。"
                    "残った煙は場の霧に混ざるので、ヒョウタンの吸い込みでそのまま吸えます。");
    }
}

} // namespace Project_TD2_2
