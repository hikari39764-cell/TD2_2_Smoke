#include "BurnFieldVisuals.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "../Common/FluidChannels.h"
#include "../Common/VfxPlayback.h"

namespace Project_TD2_2 {
namespace {

using Kind = HIKARI::GameProjectFluidInteractionKind;

// 同じ所有者の中で注入を識別する。燃料の点 1 つにつき、燃料 1 つと、霧を焼く力 1 つ。点は同じ番号で出し続ける
// （点がゆっくり動いた時は、エンジンが通った線に沿って燃料を広げる）。
constexpr uint64_t kFuelInstanceBase = 100;
constexpr uint64_t kFogInstanceBase = 140;
// 引爆で爆ぜた塊：燃料と、外へ押す衝撃。こちらは毎フレーム番号を替える（離れた 2 か所の間を掃かせない）。
constexpr uint64_t kBlastFuelBase = 160;
constexpr uint64_t kBlastPushBase = 180;
constexpr uint64_t kBlastSmokeBase = 200;
// 1 フレームに見せる塊の数。火の域が受け取れる注入は 32 個で、燃料の点・吸い込みの力と分け合う。残りは次のフレームへ回す。
constexpr uint32_t kBlastsPerFrame = 3;
constexpr float kMaximumStrength = 10000.0f;
// 点が足りなくて覆えないセルがある時に、1 フレームで受け持つ半径を広げる割合と、火が減った時に戻す割合。
constexpr float kCoverGrow = 1.08f;
constexpr float kCoverShrink = 0.995f;
// 点が、受け持つセルの重心へ寄る速さ（1 フレームの割合）。急に動かすと炎が跳ぶ。
constexpr float kEmitterFollow = 0.2f;
// 燃料の点は、受け持つかたまりの長い方へ毎フレーム行き来させる（エンジンが通った線に沿って燃料を広げるので、
// 球ではなく細長い形で入る）。エンジンは 60 m/s を超える移動を「瞬間移動」と見て線にしないので、それより遅くする
// （BurnField の fuelSweep）。
constexpr float kMinimumRadius = 0.45f;
constexpr float kMaximumRadius = 3.0f;

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// 燃えているセルを、いちばん近い点へ割り当てて、点を重心へ寄せる。どの点の半径にも入らないセルがあれば、
// そこへ新しい点を置く。置ける点が無ければ false（半径を広げる合図）。
bool PlaceEmitters(BurnFieldComponent& field, int limit) {
    const BurnMap& map = field.map;
    struct Sum {
        float x = 0.0f, z = 0.0f, remaining = 0.0f;
        float xx = 0.0f, zz = 0.0f, xz = 0.0f;
        int cells = 0;
    };
    std::array<Sum, kBurnEmitterCapacity> sums{};
    const float reach = field.cover * field.cover;
    bool covered = true;
    map.ForEachBurning([&](int ix, int iz, float remaining) {
        const HIKARI::GameProjectFloat3 cell = map.CellCenter(ix, iz);
        int nearest = -1;
        float best = reach;
        for (int i = 0; i < limit; ++i) {
            const BurnEmitter& emitter = field.emitters[static_cast<size_t>(i)];
            if (!emitter.active)
                continue;
            const float d = (cell.x - emitter.x) * (cell.x - emitter.x) + (cell.z - emitter.z) * (cell.z - emitter.z);
            if (d <= best) {
                best = d;
                nearest = i;
            }
        }
        if (nearest < 0) {
            // どの点にも入らないセル：空いている点をここへ置く（同じフレームの後のセルは、この点へ入る）。
            for (int i = 0; i < limit && nearest < 0; ++i)
                if (!field.emitters[static_cast<size_t>(i)].active)
                    nearest = i;
            if (nearest < 0) {
                covered = false;
                return;
            }
            field.emitters[static_cast<size_t>(nearest)] = {true, cell.x, cell.z, 0.0f, remaining};
        }
        // 点からの相対位置で足す（重心と、広がりの向き・大きさを出す）。
        const BurnEmitter& owner = field.emitters[static_cast<size_t>(nearest)];
        const float dx = cell.x - owner.x;
        const float dz = cell.z - owner.z;
        Sum& sum = sums[static_cast<size_t>(nearest)];
        sum.x += dx;
        sum.z += dz;
        sum.xx += dx * dx;
        sum.zz += dz * dz;
        sum.xz += dx * dz;
        sum.remaining = std::max(sum.remaining, remaining);
        ++sum.cells;
    });
    for (size_t i = 0; i < field.emitters.size(); ++i) {
        BurnEmitter& emitter = field.emitters[i];
        const Sum& sum = sums[i];
        if (!emitter.active)
            continue;
        if (static_cast<int>(i) >= limit || sum.cells == 0) {
            emitter = {};
            continue;
        }
        const float n = static_cast<float>(sum.cells);
        const float meanX = sum.x / n;
        const float meanZ = sum.z / n;
        // かたまりの広がり（分散）から、長い方の向きと、長さ・幅を出す。一様な帯の分散は 幅² ÷ 12。
        const float cxx = std::max(0.0f, sum.xx / n - meanX * meanX);
        const float czz = std::max(0.0f, sum.zz / n - meanZ * meanZ);
        const float cxz = sum.xz / n - meanX * meanZ;
        const float half = 0.5f * (cxx + czz);
        const float root = std::sqrt(0.25f * (cxx - czz) * (cxx - czz) + cxz * cxz);
        const float angle = 0.5f * std::atan2(2.0f * cxz, cxx - czz);
        float axisX = std::cos(angle);
        float axisZ = std::sin(angle);
        if (axisX * emitter.axisX + axisZ * emitter.axisZ < 0.0f) {
            axisX = -axisX;
            axisZ = -axisZ;
        }
        const float along = std::sqrt(12.0f * (half + root)) + map.CellSize();
        const float across = std::sqrt(12.0f * std::max(0.0f, half - root)) + map.CellSize();
        // 置いたばかりの点はそのまま、続いている点はゆっくり寄せる（急に変えると炎が跳ぶ）。
        const float follow = emitter.along <= 0.0f ? 1.0f : kEmitterFollow;
        emitter.x += meanX * kEmitterFollow;
        emitter.z += meanZ * kEmitterFollow;
        emitter.axisX += (axisX - emitter.axisX) * follow;
        emitter.axisZ += (axisZ - emitter.axisZ) * follow;
        const float length = std::sqrt(emitter.axisX * emitter.axisX + emitter.axisZ * emitter.axisZ);
        if (length > 1.0e-4f) {
            emitter.axisX /= length;
            emitter.axisZ /= length;
        } else {
            emitter.axisX = axisX;
            emitter.axisZ = axisZ;
        }
        emitter.along += (along - emitter.along) * follow;
        emitter.across += (across - emitter.across) * follow;
        // セルが 1〜2 個だけの点（火の端、吸われた残り）は燃料を絞る。
        emitter.share = std::clamp(n / 6.0f, 0.35f, 1.0f);
        emitter.remaining = sum.remaining;
    }
    return covered;
}

// 塊を、場の火の域の流体で見せる。引爆で爆ぜた火（規則の BurnFieldRules::Explode が積む）は火球と爆発のエフェクト、
// 当たった所や刀の点火は小さな火球、煙の刃が当たった所や霞隠れの出入りは暗い煙の塊（ボスの演出が AddBlast で積む）。
void RenderBlasts(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field) {
    const uint32_t count = std::min<uint32_t>(field.blastCount, kBlastsPerFrame);
    if (count == 0)
        return;
    const HIKARI::FluidView fluid = world.GetFluid();
    field.blastFlip ^= 1u;
    for (uint32_t index = 0; index < count; ++index) {
        const BurnBlast& blast = field.blasts[index];
        const bool detonation = blast.kind == BurnBlastKind::Detonation;
        const bool smoke = blast.kind == BurnBlastKind::Smoke;
        const float radius = std::max(0.3f, blast.radius * (detonation ? field.blastRadiusScale : 1.0f));
        // 引爆の塊は床の上の位置で来るので、火球の中心を少し浮かせる。ほかは渡された位置のまま。
        const HIKARI::GameProjectFloat3 center{
            blast.position.x, detonation ? field.map.FloorY() + radius * 0.4f : blast.position.y, blast.position.z};
        const uint64_t slot = static_cast<uint64_t>(index) * 2u + field.blastFlip;
        const float fuelStrength = smoke ? field.smokeBlastStrength * field.smokeBlastFuelShare
                                         : field.blastFuel * (detonation ? 1.0f : field.hitFuelScale);
        HIKARI::GameProjectFluidInteraction fuel{};
        fuel.kind = Kind::Fuel;
        fuel.position = center;
        fuel.extents = {radius, std::max(0.0f, field.blastTemperature), 0.0f};
        fuel.direction = {0.0f, field.blastRise, 0.0f};
        fuel.strength = std::clamp(fuelStrength, 0.0f, kMaximumStrength);
        fuel.channels = FluidChannels::kFire;
        if (fuel.strength > 0.0f)
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kBlastFuelBase + slot, fuel).Accepted() && field.fluidAccepted;
        if (smoke && field.smokeBlastStrength > 0.0f) {
            // 霞の塊：場の霧の域へ冷たい霧を 1 回入れる（霞の刃が当たって弾け、場の霧へ溶ける。火の域へ入れると暗い煤の塊になる）。
            HIKARI::GameProjectFluidInteraction puff{};
            puff.kind = Kind::Emit;
            puff.position = center;
            puff.extents = {radius, 0.0f, 0.0f};
            puff.direction = {0.0f, field.blastRise * 0.5f, 0.0f};
            puff.strength = std::min(kMaximumStrength, field.smokeBlastStrength);
            puff.channels = FluidChannels::kFog;
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kBlastSmokeBase + slot, puff).Accepted() && field.fluidAccepted;
        }
        // 向きの無い衝撃：火の域では塊を外へ膨らませ、引爆では場の霧も吹き飛ばす（当たった所の霧はボスの側が押す）。
        const float pushSpeed = smoke ? field.smokeBlastPush : field.blastPush * (detonation ? 1.0f : 0.6f);
        if (pushSpeed > 0.0f) {
            HIKARI::GameProjectFluidInteraction push{};
            push.kind = Kind::Impulse;
            push.position = center;
            push.extents = {radius * 1.5f, 0.0f, 0.0f};
            push.strength = pushSpeed;
            push.channels = detonation ? (FluidChannels::kFire | FluidChannels::kFog)
                            : smoke    ? FluidChannels::kFog
                                       : FluidChannels::kFire;
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kBlastPushBase + slot, push).Accepted() && field.fluidAccepted;
        }
        if (detonation && !field.blastEffect.IsEmpty()) {
            field.vfxAccepted =
                VfxPlayback::PlayOneShot(world, field.blastEffect, center, radius * field.blastEffectScale) &&
                field.vfxAccepted;
        }
    }
    // 見せた分を前から詰める。
    std::move(field.blasts.begin() + count, field.blasts.begin() + field.blastCount, field.blasts.begin());
    field.blastCount -= count;
}

} // namespace

void BurnFieldVisuals::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field,
                              float seconds) {
    const BurnMap& map = field.map;
    field.fluidAccepted = true;
    field.vfxAccepted = true;
    field.activeEmitters = 0;
    if (seconds <= 0.0f)
        return;
    ++field.renderFrame;
    field.visualTime += seconds;
    // 爆ぜた塊は、火のセルが全部消えた後のフレームにも残っているので、先に見せる。
    RenderBlasts(world, object, field);
    const float coverMin = std::max(0.5f, field.emitterCover);
    const float coverMax = std::max(coverMin, field.emitterCoverMax);
    if (map.BurningCells() == 0) {
        for (BurnEmitter& emitter : field.emitters)
            emitter = {};
        field.cover = coverMin;
        return;
    }
    // 点を燃えているセルのかたまりへ置く。点が足りなければ受け持つ半径を少しずつ広げ、余れば少しずつ戻す
    // （一度に変えると、点が一斉に動いて炎が跳ぶ）。
    field.cover = std::clamp(field.cover, coverMin, coverMax);
    const int limit = std::clamp(field.emitterCount, 1, static_cast<int>(field.emitters.size()));
    // 覆えている間は少しずつ戻すので、半径は「ちょうど覆える大きさ」の近くに留まる（炎の太さはこの半径では決めない）。
    if (PlaceEmitters(field, limit))
        field.cover = std::max(coverMin, field.cover * kCoverShrink);
    else
        field.cover = std::min(coverMax, field.cover * kCoverGrow);

    // 引爆の溜め：火そのものが熱く・細く・低くなる（白く締まって、内側へ縮んで見える）。地面や画面の予兆は出さない。
    const float charge = std::clamp(field.charge, 0.0f, 1.0f);
    const float shrink = Lerp(1.0f, field.chargeShrink, charge);
    const float sweepStep = std::clamp(field.fuelSweep, 0.0f, 55.0f) * seconds;
    const float rate = std::max(0.0f, field.fuelRate) * Lerp(1.0f, field.chargeFuelScale, charge);
    const float rise = field.fuelRise * Lerp(1.0f, field.chargeRiseScale, charge);
    const float temperature =
        Lerp(field.fuelTemperature, std::max(field.fuelTemperature, field.chargeTemperature), charge);
    const HIKARI::FluidView fluid = world.GetFluid();
    for (size_t index = 0; index < field.emitters.size(); ++index) {
        BurnEmitter& emitter = field.emitters[index];
        if (!emitter.active)
            continue;
        ++field.activeEmitters;
        // 燃えているセルの少ない点（火の端）と、消えかけの点は、燃料を絞る。
        const float fade = std::clamp(emitter.remaining / std::max(0.01f, field.fadeSeconds), 0.0f, 1.0f);
        const float share = emitter.share;
        // 炎の幅は、かたまりの幅に合わせる。球の中心が床より下にある分、床の高さでの切り口がその幅になる半径にする。
        const float halfWidth = 0.5f * emitter.across * field.fuelRadiusScale;
        const float radius = std::clamp(std::sqrt(halfWidth * halfWidth + field.fuelHeight * field.fuelHeight),
                                        kMinimumRadius, kMaximumRadius) *
                             shrink;
        // 長い方へ行き来させる距離（片側）。1 フレームに動ける距離より短ければ両端を交互に、長ければ端から端へ
        // 数フレームかけて往復する（どこも数フレームおきに燃料が入るので、炎は途切れない）。
        // 細長くした分、同じ濃さになるように燃料を足す（球に対する円柱の分）。
        const float reach = sweepStep > 0.0f ? std::max(0.0f, 0.5f * (emitter.along - emitter.across)) : 0.0f;
        if (2.0f * reach <= sweepStep) {
            emitter.sweepSign = -emitter.sweepSign;
            emitter.sweep = emitter.sweepSign * reach;
        } else {
            emitter.sweep += emitter.sweepSign * sweepStep;
            if (emitter.sweep >= reach) {
                emitter.sweep = reach;
                emitter.sweepSign = -1.0f;
            } else if (emitter.sweep <= -reach) {
                emitter.sweep = -reach;
                emitter.sweepSign = 1.0f;
            }
        }
        const float side = emitter.sweep;
        const float stretch = 1.0f + 1.25f * (2.0f * reach) / radius;
        // 炎の背のうねり：点ごとに位相をずらした 2 つのゆっくりした波で、燃料と上へ押す速さを増減させる
        // （どこも同じ高さの帯にならず、高い舌と低い所が入れ替わる。根元は消えない）。
        const float phase = static_cast<float>(index) * 2.399963f;
        const float turn = 6.2831853f * field.fuelWaveHz * field.visualTime;
        const float wave = 0.6f * std::sin(turn + phase) + 0.4f * std::sin(turn * 1.73f + phase * 2.3f);
        const float swell = 1.0f + std::clamp(field.fuelWave, 0.0f, 0.9f) * wave;
        HIKARI::GameProjectFluidInteraction fuel{};
        fuel.kind = Kind::Fuel;
        fuel.position = {emitter.x + emitter.axisX * side, map.FloorY() + field.fuelHeight,
                         emitter.z + emitter.axisZ * side};
        fuel.extents = {radius, std::max(0.0f, temperature), 0.0f};
        fuel.direction = {0.0f, rise * swell, 0.0f};
        fuel.strength = std::min(kMaximumStrength, rate * share * fade * swell * stretch);
        fuel.channels = FluidChannels::kFire;
        field.fluidAccepted =
            fluid.Submit(object.GetHandle(), kFuelInstanceBase + index, fuel).Accepted() && field.fluidAccepted;

        // 火の下の霧を焼く：場の霧（Default）だけに届く、吸収だけの力。注入の数を抑えるため、点を 1 つおきに交代で出す。
        if (field.fogBurnAbsorption > 0.0f && field.fogBurnRadiusScale > 0.0f &&
            (index + field.renderFrame) % 2u == 0u) {
            HIKARI::GameProjectFluidInteraction burn{};
            burn.kind = Kind::Force;
            burn.position = {emitter.x, map.FloorY() + field.fuelHeight, emitter.z};
            burn.extents = {0.5f * std::max(emitter.along, emitter.across) * field.fogBurnRadiusScale, 0.0f, 0.0f};
            burn.absorption = field.fogBurnAbsorption * 2.0f;
            burn.channels = FluidChannels::kFog;
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kFogInstanceBase + index, burn).Accepted() && field.fluidAccepted;
        }
    }
}

} // namespace Project_TD2_2
