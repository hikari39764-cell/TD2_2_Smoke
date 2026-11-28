#include "BossSmoke.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "../Common/FireObstacle.h"
#include "../Common/FluidChannels.h"
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
// 範囲爆発の溜め（c-3c908c）：足元の霧と場の火を、左手の真下へ寄せる力の提出 ID と、輪に並べる数。
constexpr uint64_t kGatherPullBase = 330;
constexpr int kGatherPullCount = 6;
// …掌へ渦を巻いて流れ込む腕（手の域へ入れる細い注入）の提出 ID と本数の上限、掌のそばで終わる距離 (m)、掌へ近づく速さの
// 曲がり（1 より小さいと、掌の近くほど速い＝吸い込まれる）、立ちのぼり始める高さ（地面から m）。
constexpr uint64_t kGatherArmBase = 340;
constexpr int kGatherArmMax = 8;
constexpr float kGatherArmInner = 0.35f;
constexpr float kGatherArmCurve = 0.6f;
constexpr float kGatherArmFloor = 0.15f;
// …立ちのぼり始めを薄く入れる区間（1 周のうちの割合）と、腕ごとの濃さのばらつき（0〜1。その分だけ薄い腕が混ざる）。
constexpr float kGatherArmFadeIn = 0.12f;
constexpr float kGatherArmVary = 0.35f;
constexpr float kLeftHandMistTurnsPerSecond = 1.5f;
// （c-3c908c：6 → 12。6 方向だと、押された霧の流れが六角になり、後から置く残り煙が六角形に流された。）
constexpr int kRingPushCount = 12;
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

// 霧の弾の残り煙のむら：弾が kTrailPatch (m) 進むごとに変わる 0〜1 の乱数（弾の id と進んだ距離から。フレームの刻みに
// 依らない）。
constexpr float kTrailPatch = 0.6f;
float TrailNoise(uint64_t id, float travelled, uint32_t salt) {
    uint32_t n = static_cast<uint32_t>(id) * 747796405u + static_cast<uint32_t>(travelled / kTrailPatch) * 2891336453u +
                 salt * 277803737u;
    n = (n ^ (n >> 16u)) * 0x7feb352du;
    n = (n ^ (n >> 15u)) * 0x846ca68bu;
    n ^= n >> 16u;
    return static_cast<float>(n & 0xffffffu) / 16777215.0f;
}

struct Emitter {
    HIKARI::WorldView& world;
    HIKARI::ObjectView object;
    Boss& boss;
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

void BossSmoke::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, Boss& boss,
                       const BossBody* look, float smokePerVolume) {
    Emitter emitter{world, object, boss, smokePerVolume};
    boss.fluidAccepted =
        FireObstacle::Submit(world, object, kFireObstacleId, boss.fireObstacleRadius, boss.fireObstacleHeight) &&
        boss.fluidAccepted;
    if (boss.swordSmoke) {
        emitter.Emit(kSwordOuterId, boss.swordOuter, boss.swordSmokeRadius, boss.swordSmokeRate * kSwordOuterShare, {});
        emitter.Emit(kSwordInnerId, boss.swordInner, boss.swordSmokeRadius,
                     boss.swordSmokeRate * (1.0f - kSwordOuterShare), {});
        // 振りの空気（c-4be37e）：刃が進む向き（規則の剣の構えの斬る向き）へ、刀の外側・内側の所で場の霧を押す。
        // 向きは見えている体の向き（体の回転・狙いのずれのねじり込み。刀の点 swordOuter・swordInner と同じ）。
        const Vec3 edge = RotateY(boss.swordPose.edge, BossPose::BodyYaw(boss));
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
    if (const float hold = BossAnimBody::TwoHand(boss); hold > 0.01f && boss.animBodyTwoHandMist > 0.0f) {
        const BossSwordPoints blade = BossPose::SwordPoints(boss, boss.swordPose);
        for (uint64_t k = 0; k < kTwoHandMistPoints; ++k) {
            const float u = (static_cast<float>(k) + 0.5f) / static_cast<float>(kTwoHandMistPoints);
            emitter.Emit(kTwoHandMistBase + k, Lerp(blade.base, blade.outer, u), boss.animBodyTwoHandMistRadius,
                         boss.animBodyTwoHandMist * hold / static_cast<float>(kTwoHandMistPoints), {});
        }
    }
    // 左手にまとう霧（c-40f6c1）：骨格の体が出ている間、見えている左手の所から、手の周りを回る向きに吹き出す。
    // 変わり目の演出のクリップの間は出さない（太陽へ伸ばす手・拳の火の所に、白い霧が掛からないように。c-271cba）。
    if (look != nullptr && look->anim.leftHandSeen && !look->anim.cinematicLayered &&
        boss.animBodyLeftHandMist > 0.0f) {
        const float angle = kTau * kLeftHandMistTurnsPerSecond * boss.floatClock;
        const Vec3 swirl{std::cos(angle) * boss.animBodyLeftHandMistSwirl, 0.0f,
                         std::sin(angle) * boss.animBodyLeftHandMistSwirl};
        emitter.Emit(kLeftHandMistId, look->anim.leftHandWorld, boss.animBodyLeftHandMistRadius,
                     boss.animBodyLeftHandMist, swirl);
    }
    // 範囲爆発の溜め（c-3c908c）：左手へ霧・火を集めている間、足元の霧と場の火を、手の真下へ地面に沿って寄せる
    // （叩きつけの爆発が、それを外へ吹き飛ばす）。手の真下を囲む輪の上から、内向きの力を並べる。
    if (look != nullptr && look->anim.leftHandSeen && look->state.accents.gatherPull > 0.0f &&
        look->bodyGatherFogPull > 0.0f) {
        const float radius = std::max(0.5f, look->bodyGatherFogPullRadius);
        const Vec3 center{look->anim.leftHandWorld.x, boss.ground.y + kBlastPushHeight, look->anim.leftHandWorld.z};
        for (int k = 0; k < kGatherPullCount; ++k) {
            const float angle = kTau * static_cast<float>(k) / static_cast<float>(kGatherPullCount);
            const Vec3 outward{std::cos(angle), 0.0f, std::sin(angle)};
            HIKARI::GameProjectFluidInteraction pull{};
            pull.kind = HIKARI::GameProjectFluidInteractionKind::Force;
            pull.position = Add(center, Mul(outward, radius));
            pull.direction = Mul(outward, -1.0f);
            pull.extents = {radius, 0.0f, 0.0f};
            pull.strength = look->bodyGatherFogPull * look->state.accents.gatherPull;
            pull.channels = FluidChannels::kFog | FluidChannels::kFire;
            boss.fluidAccepted = world.GetFluid()
                                     .Submit(object.GetHandle(), kGatherPullBase + static_cast<uint64_t>(k), pull)
                                     .Accepted() &&
                                 boss.fluidAccepted;
        }
    }
    // 範囲爆発の溜め（c-3c908c）：掌へ渦を巻いて流れ込む腕。細い注入の点を、手の真下の地面の輪から、体のまわりを巻いて
    // 掌へ滑らせる（エンジンが動いた線に沿って入れるので、球の塊ではなく細い筋になる。掲げた手は画面の上端に掛かる
    // ので、地面から昇る筋で画面の中ほどに見せる）。掌へ着いたら地面へ戻り（瞬間移動なので線は引かれない）、周ごとに
    // 始まりの向きを変える。手の域（Channel4）だけに届く。
    if (look != nullptr && look->state.accents.gatherArm > 0.0f) {
        const auto& gs = *look;
        const auto& ga = look->state.accents;
        const int arms = std::clamp(gs.bodyGatherArms, 1, kGatherArmMax);
        const float reach = std::max(kGatherArmInner + 0.1f, gs.bodyGatherArmReach);
        const float heat = ga.gatherFire ? gs.bodyGatherArmHeatFire : gs.bodyGatherArmHeatMist;
        const float floorY = boss.ground.y + kGatherArmFloor;
        const float height = std::max(0.0f, ga.gatherPoint.y - floorY);
        const float lift = std::max(0.2f, gs.bodyGatherArmLift);
        const float spread = std::clamp(gs.bodyGatherArmSpread, 0.0f, 1.0f);
        for (int k = 0; k < arms; ++k) {
            const float turn = ga.gatherPhase + static_cast<float>(k) / static_cast<float>(arms);
            const float cycle = std::floor(turn);
            const float u = turn - cycle;
            const float seed = static_cast<float>(k) * 2.399963f + cycle * 1.7f;
            const float ring =
                std::max(kGatherArmInner, reach * (1.0f - spread * (0.5f + 0.5f * std::sin(seed * 2.1f))));
            const float angle = seed + kTau * gs.bodyGatherArmTurns * u;
            const float r = kGatherArmInner + (ring - kGatherArmInner) * std::pow(1.0f - u, kGatherArmCurve);
            HIKARI::GameProjectFluidInteraction emit{};
            emit.kind = HIKARI::GameProjectFluidInteractionKind::Emit;
            emit.position = {ga.gatherPoint.x + std::cos(angle) * r, floorY + height * std::pow(u, lift),
                             ga.gatherPoint.z + std::sin(angle) * r};
            emit.extents = {std::max(0.05f, gs.bodyGatherArmRadius), std::max(0.0f, heat), 0.0f};
            const float weight = std::clamp(u / kGatherArmFadeIn, 0.0f, 1.0f) *
                                 (1.0f - kGatherArmVary * (0.5f + 0.5f * std::sin(seed * 3.7f)));
            emit.strength = std::min(kStrengthLimit, gs.bodyGatherArmStrength * ga.gatherArm * weight);
            emit.channels = FluidChannels::kHandGather;
            boss.fluidAccepted = world.GetFluid()
                                     .Submit(object.GetHandle(), kGatherArmBase + static_cast<uint64_t>(k), emit)
                                     .Accepted() &&
                                 boss.fluidAccepted;
        }
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
        // 叩きつけの爆発（c-3c908c）は、自分の速さと半径で押す（爆発の大きさ scale は、残す霧の量にだけ使う）。
        const float speed = blast.burst ? boss.burstPush : boss.blastPush;
        if (!(speed > 0.0f))
            continue;
        // 爆発：当たった所の真下から、地面に沿って六方へ 1 回押す。半径の半分ずつで輪を作り、全体で blastPushRadius まで届く。
        const float reach =
            (blast.burst ? boss.burstPushRadius * blast.burstScale : boss.blastPushRadius * blast.scale) * 0.5f;
        const Vec3 center{blast.position.x, boss.ground.y + kBlastPushHeight, blast.position.z};
        for (int k = 0; k < kBlastPushCount; ++k) {
            const float angle = kTau * static_cast<float>(k) / static_cast<float>(kBlastPushCount);
            HIKARI::GameProjectFluidInteraction push{};
            push.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
            push.direction = {std::cos(angle), 0.0f, std::sin(angle)};
            push.position = Add(center, Mul(push.direction, reach));
            push.extents = {reach, 0.0f, 0.0f};
            push.strength = speed;
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
        // 押すたびに、向きを半分ずらす（同じ 12 方向だけを押し続けない）。
        const float turn = static_cast<int>(wave.radius / std::max(0.1f, boss.ringPushRadius)) % 2 == 0 ? 0.5f : 0.0f;
        for (int k = 0; k < kRingPushCount; ++k) {
            const float angle = kTau * (static_cast<float>(k) + turn) / static_cast<float>(kRingPushCount);
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
    // ナイフ・霧の弾の残り煙（c-e0a9c3 の続き）：弾の真下の地面近くへ 1 点だけ入れる。弾そのもの（細い霧の尾）は、弾の
    // エフェクトの域が見せる。提出 ID は斬撃と同じ枠の 1 つ目（同じ枠に斬撃とナイフが同時に居ることは無い）。
    for (size_t i = 0; i < boss.waves.size() && boss.knifeTrailRadius > 0.0f; ++i) {
        const auto& wave = boss.waves[i];
        if (!wave.active || !wave.knife || wave.smokeRate <= 0.0f)
            continue;
        // むら：入れる所を左右へ振り、量を 0.3〜1.7 倍に強弱させる（平均は 1 倍。まっすぐで一様な白い帯にしない）。
        const Vec3 forward = FlatNormalize(wave.direction);
        const Vec3 side{forward.z, 0.0f, -forward.x};
        const float sway = (TrailNoise(wave.id, wave.travelled, 1u) * 2.0f - 1.0f) * boss.knifeTrailJitter;
        const float amount = 0.3f + 1.4f * TrailNoise(wave.id, wave.travelled, 2u);
        const Vec3 at = Add({wave.position.x, boss.ground.y + kResidueFloor, wave.position.z}, Mul(side, sway));
        emitter.Emit(kWaveBase + i * kWavePoints, at, boss.knifeTrailRadius, wave.smokeRate * amount,
                     {0.0f, -boss.waveSmokeSink, 0.0f});
    }
}

} // namespace Project_TD2_2
