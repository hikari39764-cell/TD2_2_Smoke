#include "GourdAbsorbField.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "GourdRules.h"
#include "../Common/FluidChannels.h"
#include "../Common/VectorMath.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using VectorMath::Add;
using VectorMath::Difference;
using VectorMath::Normalize;
using VectorMath::Scale;
using Vec3 = HIKARI::GameProjectFloat3;
using Kind = HIKARI::GameProjectFluidInteractionKind;

// 同じ所有者の中で力を識別する。1 は口元の回収、2 からが霧を運ぶ力（どちらも消した量を ReadRemoved が読む）。
constexpr uint64_t kSinkInstance = 1;
constexpr uint64_t kGatherInstanceBase = 2;
// 消した量を読む枠：0 は口元、1 は口の上、2 からは霧を運ぶ力（輪 4 つで 3 + 5 × 3 + 足元 + 持ち上げ = 20 個）。
constexpr size_t kRemovedFixedSlots = 2;
constexpr float kTau = 6.28318530718f;
constexpr float kRadiansPerDegree = kTau / 360.0f;
// 奥の輪ほど、運ぶ流れを少し太くする（輪 1 つごとの増し分）。
constexpr float kRingGrowth = 0.15f;
// いちばん手前の輪の筋の数（片側）と、それより奥の輪の筋の数（片側）。奥ほど扇が広いので筋を増やす。
constexpr int kNearLanes = 1;
constexpr int kFarLanes = 2;
// 足元の全周から寄せる力：中心をプレイヤーの後ろへ寄せる割合と、前方の筋に対する強さ。
constexpr float kNearBackShare = 0.4f;
constexpr float kNearStrengthShare = 0.6f;
// 満杯で吸えない時に、霧を寄せる流れを弱める倍率（そよ風だけ残す）。
constexpr float kFullFlowScale = 0.15f;
// 渦を温める注入の ID（力は 2 から数える。重ならない番号にする）と、奥の 1 か所の半径・温度の割合。
constexpr uint64_t kGlowInstanceBase = 60;
// 口の上で霧を回収する力の ID（回読の対象）。
constexpr uint64_t kCatchInstance = 50;
// 地面に沿って温める場所の数。口から遠いほど広く、ぬるくする（1 か所ごとの半径の増し分と、温度の減り分）。
constexpr int kGlowGroundSpots = 3;
constexpr float kGlowRadiusGrowth = 0.5f;
constexpr float kGlowHeatDrop = 0.26f;

struct Field {
    HIKARI::WorldView& world;
    HIKARI::ObjectView object;
    GourdComponent& gourd;
    uint32_t channels = FluidChannels::kFog;
    uint64_t instance = kGatherInstanceBase;

    // absorption は、この力の半径の中の霧が運ばれながら消える率 [1/s]（消した量は ReadRemoved が数える）。
    void Push(Vec3 position, float radius, Vec3 direction, float strength, float absorption) {
        if (strength <= 0.0f || radius <= 0.0f)
            return;
        HIKARI::GameProjectFluidInteraction force{};
        force.kind = Kind::Force;
        force.position = position;
        force.extents = {radius, 0.0f, 0.0f};
        force.direction = direction;
        force.strength = strength;
        // 消した量を読めない番号の力では、霧を消さない（数えられない霧を場から減らさない）。
        const size_t slot = kRemovedFixedSlots + static_cast<size_t>(instance - kGatherInstanceBase);
        force.absorption = slot < gourd.removedByForce.size() ? absorption : 0.0f;
        force.channels = channels;
        gourd.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), instance++, force).Accepted() && gourd.fluidAccepted;
        ++gourd.fluidSubmissions;
    }
};

} // namespace

bool GourdAbsorbField::ReadRemoved(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdComponent& gourd,
                                   double& outTotal) {
    const HIKARI::FluidView fluid = world.GetFluid();
    outTotal = gourd.removedSum;
    for (size_t slot = 0; slot < gourd.removedByForce.size(); ++slot) {
        const uint64_t instance = slot == 0   ? kSinkInstance
                                  : slot == 1 ? kCatchInstance
                                              : kGatherInstanceBase + (slot - kRemovedFixedSlots);
        double total = 0.0;
        if (!fluid.GetRemovedSmoke(object.GetHandle(), instance, total).Succeeded())
            return false;
        // エンジンは、出さなくなって 2 秒ほどたった力の累計を捨てて 0 から数え直す。減っていたら、その値が新しい分。
        double& last = gourd.removedByForce[slot];
        // 口元と口の上で消した霧は全部、扇の中で運びながら消した霧は gatherSmokeShare の割合だけを煙に数える。
        const double share =
            slot < kRemovedFixedSlots ? 1.0 : static_cast<double>(std::clamp(gourd.gatherSmokeShare, 0.0f, 1.0f));
        gourd.removedSum += share * (total >= last ? total - last : total);
        last = total;
    }
    outTotal = gourd.removedSum;
    return true;
}

void GourdAbsorbField::Submit(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdComponent& gourd,
                              float seconds) {
    // 満杯の時は霧を消さない（吸っても増えない煙を場から減らさない）。流れもそよ風まで弱める。
    const bool blocked = GourdRules::IsFull(gourd) && !gourd.absorbWhenFull;
    const float flow = blocked ? kFullFlowScale : 1.0f;
    // 扇の中の霧は、口へ運ばれながら消える（吸った所に穴が見える）。満杯で吸わない設定の時は消さない。
    const float fade = blocked ? 0.0f : gourd.gatherAbsorption;
    // 場の霧にはいつも届く。煙が入る間は場の火にも届き、火の舌が口へ曲がって吸われる
    // （満杯では火を吸えない規則に合わせて、満杯の間は火に触らない）。
    const uint32_t channels = FluidChannels::kFog | (GourdRules::IsFull(gourd) ? 0u : FluidChannels::kFire);
    Field field{world, object, gourd, channels};
    if (gourd.gatherStrength > 0.0f) {
        const Vec3 forward = PlayerRules::FlatNormalize(gourd.aimDirection, {0.0f, 0.0f, 1.0f});
        const Vec3 right{forward.z, 0.0f, -forward.x};
        // 霧の集まる点（口の前の足元）と、ヒョウタンの真下の同じ高さの点。扇はこの点を中心に広げる。
        const Vec3 foot = gourd.sinkPosition;
        const Vec3 base{gourd.position.x, foot.y, gourd.position.z};
        const float fan = gourd.gatherFanDegrees * kRadiansPerDegree;
        const float start = gourd.absorbForwardDistance;
        const float range = std::max(start, gourd.gatherRange);
        const int rings = std::max(1, gourd.gatherRings);
        Vec3 inner = foot;
        for (int ring = 1; ring <= rings; ++ring) {
            const float fraction = static_cast<float>(ring) / static_cast<float>(rings);
            const float distance = start + (range - start) * fraction;
            // 奥から手前へ進む脈：位相を距離で遅らせるので、濃淡の帯が口へ向かって流れて見える。
            const float pulse =
                1.0f + gourd.gatherPulse * std::sin(kTau * (gourd.gatherPulseFrequency * seconds + distance / range));
            const float strength =
                gourd.gatherStrength * flow * pulse * (1.0f + (gourd.gatherFarScale - 1.0f) * fraction);
            const float radius = gourd.gatherRadius * (1.0f + kRingGrowth * static_cast<float>(ring));
            const int lanes = ring == 1 ? kNearLanes : kFarLanes;
            for (int lane = -lanes; lane <= lanes; ++lane) {
                const float angle = fan * static_cast<float>(lane) / static_cast<float>(lanes);
                const Vec3 outward = Add(Scale(forward, std::cos(angle)), Scale(right, std::sin(angle)));
                const Vec3 position = Add(base, Scale(outward, distance));
                // 一つ手前の輪の中心線へ向けて絞り込み、横向きの成分で渦を巻かせる。
                const Vec3 inward = Normalize(Difference(inner, position), Scale(outward, -1.0f));
                const Vec3 tangent{inward.z, 0.0f, -inward.x};
                field.Push(position, radius, Normalize(Add(inward, Scale(tangent, gourd.gatherSwirl)), inward),
                           strength, fade);
            }
            inner = Add(base, Scale(forward, distance));
        }
        // 足元の全周：プレイヤーの周りの霧も、集まる点へ寄せる。
        if (gourd.gatherNearRadius > 0.0f) {
            const Vec3 position = Add(base, Scale(forward, -gourd.gatherNearRadius * kNearBackShare));
            field.Push(position, gourd.gatherNearRadius, Normalize(Difference(foot, position), forward),
                       gourd.gatherStrength * kNearStrengthShare * flow, fade);
        }
        // 集まった霧を、足元から口へ持ち上げる。
        field.Push(foot, gourd.gatherRadius, Normalize(Difference(gourd.mouthPosition, foot), {0.0f, 1.0f, 0.0f}),
                   gourd.gatherStrength * gourd.gatherLift * flow, 0.0f);
    }
    // 吸われていく霧を温める。場の霧（ArenaGroundFog）と口のエフェクトの域は、温度が上がった煙が青緑に光るので、
    // 本物の霧が口へ近づくほど明るく光って見える。注入の強さ（足す煙）は 0 にして、温度だけを使う。
    if (gourd.absorbGlowHeat > 0.0f && !blocked) {
        const Vec3 forward = PlayerRules::FlatNormalize(gourd.aimDirection, {0.0f, 0.0f, 1.0f});
        const Vec3 base{gourd.position.x, gourd.sinkPosition.y, gourd.position.z};
        for (int i = 0; i <= kGlowGroundSpots; ++i) {
            // 0 は口元。1 以降は地面に沿って、absorbGlowReach ごとに奥へ置く。
            const float step = static_cast<float>(i);
            HIKARI::GameProjectFluidInteraction heat{};
            heat.kind = Kind::Emit;
            heat.position = i == 0 ? gourd.mouthPosition : Add(base, Scale(forward, gourd.absorbGlowReach * step));
            heat.extents = {gourd.absorbGlowRadius * (1.0f + kGlowRadiusGrowth * step),
                            gourd.absorbGlowHeat * std::max(0.0f, 1.0f - kGlowHeatDrop * step), 0.0f};
            gourd.fluidAccepted = world.GetFluid()
                                      .Submit(object.GetHandle(), kGlowInstanceBase + static_cast<uint64_t>(i), heat)
                                      .Accepted() &&
                                  gourd.fluidAccepted;
            ++gourd.fluidSubmissions;
        }
    }
    // 口を通り過ぎて上へ抜ける霧は、口の少し上で消して回収する（流れは作らない）。
    if (gourd.absorbCatchRadius > 0.0f && gourd.absorbCatchRate > 0.0f && !blocked) {
        HIKARI::GameProjectFluidInteraction above{};
        above.kind = Kind::Force;
        above.position = {gourd.mouthPosition.x, gourd.mouthPosition.y + gourd.absorbCatchHeight,
                          gourd.mouthPosition.z};
        above.extents = {gourd.absorbCatchRadius, 0.0f, 0.0f};
        above.absorption = gourd.absorbCatchRate;
        above.channels = channels;
        gourd.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), kCatchInstance, above).Accepted() && gourd.fluidAccepted;
        ++gourd.fluidSubmissions;
    }
    HIKARI::GameProjectFluidInteraction sink{};
    sink.kind = Kind::Force;
    sink.position = gourd.mouthPosition;
    sink.extents = {gourd.absorbRadius, 0.0f, 0.0f};
    sink.strength = blocked ? 0.0f : -gourd.absorbStrength;
    sink.absorption = blocked ? 0.0f : gourd.absorption;
    sink.channels = channels;
    gourd.fluidAccepted =
        world.GetFluid().Submit(object.GetHandle(), kSinkInstance, sink).Accepted() && gourd.fluidAccepted;
    ++gourd.fluidSubmissions;
}

} // namespace Project_TD2_2
