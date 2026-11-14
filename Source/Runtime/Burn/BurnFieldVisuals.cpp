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

// 同じ所有者の中で注入を識別する。升目（タイル）1 つにつき、燃料 1 つと、霧を焼く力 1 つ。升目ごとに決まった番号で出す
// （毎フレーム入れている升目では、エンジンが、点の動いた線に沿って燃料を広げる。番号を升目の間で使い回すと、
// 隣の升目との間まで掃かれて、火の無い所に燃料の線が入る）。
constexpr uint64_t kFuelInstanceBase = 10000;
constexpr uint64_t kFogInstanceBase = 20000;
// 引爆で爆ぜた塊：燃料と、外へ押す衝撃。こちらは毎フレーム番号を替える（離れた 2 か所の間を掃かせない）。
constexpr uint64_t kBlastFuelBase = 160;
constexpr uint64_t kBlastPushBase = 180;
constexpr uint64_t kBlastSmokeBase = 200;
// 1 フレームに見せる塊の数。火の域が受け取れる注入は 32 個で、燃料の点・吸い込みの力と分け合う。残りは次のフレームへ回す。
constexpr uint32_t kBlastsPerFrame = 3;
constexpr float kMaximumStrength = 10000.0f;
// 燃料の点は、升目の中のかたまりの長い方へ毎フレーム行き来させる（エンジンが通った線に沿って燃料を広げるので、
// 球ではなく細長い形で入る）。エンジンは 60 m/s を超える移動を「瞬間移動」と見て線にしないので、それより遅くする
// （BurnField の fuelSweep）。
constexpr float kMinimumRadius = 0.45f;
constexpr float kMaximumRadius = 3.0f;

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// 升目（タイル）の大きさを決め、持ち越しと作業場所を、その数に合わせる（大きさが変わった時だけ確保し直す）。
void LayoutTiles(BurnFieldComponent& field) {
    const BurnMap& map = field.map;
    const int side = std::max(map.Width(), map.Height());
    const int wanted = std::max(1, static_cast<int>(std::lround(field.emitterTile / map.CellSize())));
    const int tileCells = std::max(wanted, (side + kBurnTilesPerSideMax - 1) / kBurnTilesPerSideMax);
    const int tilesPerSide = (side + tileCells - 1) / tileCells;
    if (tileCells == field.tileCells && tilesPerSide == field.tilesPerSide)
        return;
    field.tileCells = tileCells;
    field.tilesPerSide = tilesPerSide;
    const size_t count = static_cast<size_t>(tilesPerSide) * static_cast<size_t>(tilesPerSide);
    field.tiles.assign(count, {});
    field.tileSums.assign(count, {});
}

// 燃えているセルを、入っている升目ごとに足し合わせる（升目の中心からの相対位置で）。燃えている升目の数を返す。
int SumTiles(BurnFieldComponent& field) {
    const BurnMap& map = field.map;
    std::fill(field.tileSums.begin(), field.tileSums.end(), BurnTileSum{});
    const float tileSize = static_cast<float>(field.tileCells) * map.CellSize();
    int burning = 0;
    map.ForEachBurning([&](int ix, int iz, float remaining) {
        const int tx = ix / field.tileCells;
        const int tz = iz / field.tileCells;
        BurnTileSum& sum = field.tileSums[static_cast<size_t>(tz * field.tilesPerSide + tx)];
        // 升目の中でのセルの位置（中心が 0）。
        const float dx = (static_cast<float>(ix - tx * field.tileCells) + 0.5f) * map.CellSize() - 0.5f * tileSize;
        const float dz = (static_cast<float>(iz - tz * field.tileCells) + 0.5f) * map.CellSize() - 0.5f * tileSize;
        sum.x += dx;
        sum.z += dz;
        sum.xx += dx * dx;
        sum.zz += dz * dz;
        sum.xz += dx * dz;
        sum.remaining = std::max(sum.remaining, remaining);
        burning += sum.cells++ == 0 ? 1 : 0;
    });
    return burning;
}

// 升目 1 つの、そのフレームの燃えているかたまり。
struct TileShape {
    // かたまりの重心（世界座標）と、長い方の向き・長さ・幅 (m)。
    float x = 0.0f;
    float z = 0.0f;
    float axisX = 1.0f;
    float axisZ = 0.0f;
    float along = 0.0f;
    float across = 0.0f;
};

// 足し合わせから、かたまりの重心と、広がり（分散）の長い方の向き・長さ・幅を出す。一様な帯の分散は 幅² ÷ 12。
// 向きの符号は前のフレーム（tile の軸）に揃える（行き来の向きがフレームごとにひっくり返らないように）。
TileShape ShapeOf(const BurnFieldComponent& field, const BurnTileSum& sum, const BurnTile& tile, int tx, int tz) {
    const BurnMap& map = field.map;
    const float tileSize = static_cast<float>(field.tileCells) * map.CellSize();
    const HIKARI::GameProjectFloat3 corner = map.CellCenter(tx * field.tileCells, tz * field.tileCells);
    const float n = static_cast<float>(sum.cells);
    const float meanX = sum.x / n;
    const float meanZ = sum.z / n;
    const float cxx = std::max(0.0f, sum.xx / n - meanX * meanX);
    const float czz = std::max(0.0f, sum.zz / n - meanZ * meanZ);
    const float cxz = sum.xz / n - meanX * meanZ;
    const float half = 0.5f * (cxx + czz);
    const float root = std::sqrt(0.25f * (cxx - czz) * (cxx - czz) + cxz * cxz);
    const float angle = 0.5f * std::atan2(2.0f * cxz, cxx - czz);
    TileShape shape;
    // 升目の角のセルの中心から、升目の中心までは (升目 − セル) ÷ 2。
    shape.x = corner.x + 0.5f * (tileSize - map.CellSize()) + meanX;
    shape.z = corner.z + 0.5f * (tileSize - map.CellSize()) + meanZ;
    shape.axisX = std::cos(angle);
    shape.axisZ = std::sin(angle);
    if (shape.axisX * tile.axisX + shape.axisZ * tile.axisZ < 0.0f) {
        shape.axisX = -shape.axisX;
        shape.axisZ = -shape.axisZ;
    }
    shape.along = std::sqrt(12.0f * (half + root)) + map.CellSize();
    shape.across = std::sqrt(12.0f * std::max(0.0f, half - root)) + map.CellSize();
    return shape;
}

// 直近 window 秒の間に出した爆発のエフェクトの数（cap まで数えれば足りる）。
uint32_t RecentBlastEffects(const BurnFieldComponent& field, uint32_t cap, float window) {
    const uint32_t limit = std::min({field.blastEffectsPlayed, static_cast<uint32_t>(kBlastEffectCapMax), cap});
    const float since = field.visualTime - window;
    uint32_t recent = 0;
    while (recent < limit &&
           field.blastEffectTimes[(field.blastEffectsPlayed - recent - 1u) % kBlastEffectCapMax] > since)
        ++recent;
    return recent;
}

// 引爆の塊に爆発のエフェクトを出すか（間引いた・止めた数も数える）。重なった粒子のエフェクトが多すぎると描画が
// 失敗して戻らないので、出す数を抑える。見込んだ塊の数が上限を超える引爆は、塊の番号を見込みに対して均等に間引く
// （order 番目までに出す数 = ⌈order × 上限 ÷ 見込み⌉。最初の塊には必ず出る）。上限以下の引爆は今までどおり全部に出す。
// どの場合も、直近の窓の間に出した数が上限に届いていれば出さない（見込みより塊が多かった時の歯止め）。
bool TakeBlastEffect(BurnFieldComponent& field, const BurnBlast& blast) {
    const uint32_t cap =
        static_cast<uint32_t>(std::clamp(field.blastEffectCap, 1, static_cast<int>(kBlastEffectCapMax)));
    const uint64_t plan = field.blastPlan;
    if (plan > cap && blast.order > 0) {
        const uint64_t before = ((blast.order - 1u) * uint64_t{cap} + plan - 1u) / plan;
        const uint64_t through = (blast.order * uint64_t{cap} + plan - 1u) / plan;
        if (through == before) {
            ++field.blastEffectsThinned;
            return false;
        }
    }
    const uint32_t recent = RecentBlastEffects(field, cap, std::max(0.0f, field.blastEffectWindow));
    if (recent >= cap) {
        ++field.blastEffectsHeld;
        return false;
    }
    field.blastEffectTimes[field.blastEffectsPlayed % kBlastEffectCapMax] = field.visualTime;
    ++field.blastEffectsPlayed;
    field.blastEffectPeak = std::max(field.blastEffectPeak, recent + 1u);
    return true;
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
                                         : field.blastFuel * (detonation ? blast.power : field.hitFuelScale);
        HIKARI::GameProjectFluidInteraction fuel{};
        fuel.kind = Kind::Fuel;
        fuel.position = center;
        const float blastTemperature =
            detonation ? Lerp(field.fuelTemperature, field.blastTemperature, std::clamp(blast.power, 0.0f, 1.0f))
                       : field.blastTemperature;
        fuel.extents = {radius, std::max(0.0f, blastTemperature), 0.0f};
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
        const float pushSpeed = smoke ? field.smokeBlastPush : field.blastPush * (detonation ? blast.power : 0.6f);
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
        if (detonation && !field.blastEffect.IsEmpty() && TakeBlastEffect(field, blast)) {
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
    field.burningTiles = 0;
    field.emitterStride = 1;
    if (map.BurningCells() == 0)
        return;
    // 燃えているセルを、場所の決まった升目ごとにまとめ、升目ごとに 1 か所へ燃料を入れる。升目の炎の位置と太さは、
    // その升目のセルだけで決まる（ほかの所で火が増えても減っても動かない）。燃えている升目が 1 フレームに出せる数より
    // 多い時は、stride フレームに 1 回ずつ順番に入れ、その分だけ燃料を増やす（どの升目も同じ間隔で入る）。
    LayoutTiles(field);
    field.burningTiles = SumTiles(field);
    const int limit = std::clamp(field.emitterCount, 1, static_cast<int>(kBurnEmitterCapacity));
    const int stride = (field.burningTiles + limit - 1) / limit;
    field.emitterStride = stride;
    const int turn = static_cast<int>(field.renderFrame % static_cast<uint32_t>(stride));

    // 引爆の溜め：火そのものが熱く・細く・低くなる（白く締まって、内側へ縮んで見える）。地面や画面の予兆は出さない。
    // 一か所だけの溜め（天降り斬りの着地の火）は、その円の中の升目だけを同じ見た目にする。
    const float fieldCharge = std::clamp(field.charge, 0.0f, 1.0f);
    const float hotspotCharge = std::clamp(field.hotspotCharge, 0.0f, 1.0f);
    const float sweepSpeed = std::clamp(field.fuelSweep, 0.0f, 55.0f);
    const HIKARI::FluidView fluid = world.GetFluid();
    int rank = 0;
    for (size_t index = 0; index < field.tileSums.size(); ++index) {
        const BurnTileSum& sum = field.tileSums[index];
        if (sum.cells == 0)
            continue;
        if (rank++ % stride != turn)
            continue;
        BurnTile& tile = field.tiles[index];
        const TileShape shape = ShapeOf(field, sum, tile, static_cast<int>(index) % field.tilesPerSide,
                                        static_cast<int>(index) / field.tilesPerSide);
        tile.axisX = shape.axisX;
        tile.axisZ = shape.axisZ;
        if (field.activeEmitters++ == 0) {
            field.traceAlong = shape.along;
            field.traceAcross = shape.across;
        }
        // 一か所だけの溜めは、白く締めずに、炎を濃く・高く・少し熱くする（燃え盛ってから爆ぜる）。
        const float hotX = shape.x - field.hotspotCenter.x, hotZ = shape.z - field.hotspotCenter.z;
        const float hot = hotX * hotX + hotZ * hotZ <= field.hotspotRadius * field.hotspotRadius ? hotspotCharge : 0.0f;
        const float shrink = Lerp(1.0f, field.chargeShrink, fieldCharge);
        const float rate = std::max(0.0f, field.fuelRate) * Lerp(1.0f, field.chargeFuelScale, fieldCharge) *
                           Lerp(1.0f, field.hotspotFuelScale, hot);
        const float rise = field.fuelRise * Lerp(1.0f, field.chargeRiseScale, fieldCharge) + field.hotspotRise * hot;
        const float charged =
            Lerp(field.fuelTemperature, std::max(field.fuelTemperature, field.chargeTemperature), fieldCharge);
        const float temperature = Lerp(charged, std::max(charged, field.hotspotTemperature), hot);
        // 燃えているセルの少ない升目（火の端、吸われた残り）と、消えかけの升目は、燃料を絞る。
        const float fade = std::clamp(sum.remaining / std::max(0.01f, field.fadeSeconds), 0.0f, 1.0f);
        const float share = std::clamp(static_cast<float>(sum.cells) / 6.0f, 0.35f, 1.0f);
        // 炎の幅は、かたまりの幅に合わせる。球の中心が床より下にある分、床の高さでの切り口がその幅になる半径にする。
        const float halfWidth = 0.5f * shape.across * field.fuelRadiusScale;
        const float radius = std::clamp(std::sqrt(halfWidth * halfWidth + field.fuelHeight * field.fuelHeight),
                                        kMinimumRadius, kMaximumRadius) *
                             shrink;
        // 長い方へ行き来させる距離（片側）。毎フレーム入れている升目は、1 フレームに動ける距離（瞬間移動と見られない速さ）
        // より短ければ両端を交互に、長ければ端から端へ数フレームかけて往復する（エンジンが通った線に沿って燃料を広げる）。
        // 順番待ちの升目は、入れるたびに半径ぶん進めて往復する（線にはならないので、かたまりを端から端まで順に埋める）。
        // 細長くした分、同じ濃さになるように燃料を足す（球に対する円柱の分）。
        const float reach = sweepSpeed > 0.0f ? std::max(0.0f, 0.5f * (shape.along - shape.across)) : 0.0f;
        const float sweepStep = stride == 1 ? sweepSpeed * seconds : radius;
        if (2.0f * reach <= sweepStep) {
            tile.sweepSign = -tile.sweepSign;
            tile.sweep = tile.sweepSign * reach;
        } else {
            tile.sweep += tile.sweepSign * sweepStep;
            if (tile.sweep >= reach) {
                tile.sweep = reach;
                tile.sweepSign = -1.0f;
            } else if (tile.sweep <= -reach) {
                tile.sweep = -reach;
                tile.sweepSign = 1.0f;
            }
        }
        const float stretch = 1.0f + 1.25f * (2.0f * reach) / radius;
        // 炎の背のうねり：升目ごとに位相をずらした 2 つのゆっくりした波で、燃料と上へ押す速さを増減させる
        // （どこも同じ高さの帯にならず、高い舌と低い所が入れ替わる。根元は消えない）。
        const float phase = static_cast<float>(index) * 2.399963f;
        const float wheel = 6.2831853f * field.fuelWaveHz * field.visualTime;
        const float wave = 0.6f * std::sin(wheel + phase) + 0.4f * std::sin(wheel * 1.73f + phase * 2.3f);
        const float swell = 1.0f + std::clamp(field.fuelWave, 0.0f, 0.9f) * wave;
        HIKARI::GameProjectFluidInteraction fuel{};
        fuel.kind = Kind::Fuel;
        fuel.position = {shape.x + shape.axisX * tile.sweep, map.FloorY() + field.fuelHeight,
                         shape.z + shape.axisZ * tile.sweep};
        fuel.extents = {radius, std::max(0.0f, temperature), 0.0f};
        fuel.direction = {0.0f, rise * swell, 0.0f};
        fuel.strength = std::min(kMaximumStrength, rate * share * fade * swell * stretch * static_cast<float>(stride));
        fuel.channels = FluidChannels::kFire;
        field.fluidAccepted =
            fluid.Submit(object.GetHandle(), kFuelInstanceBase + index, fuel).Accepted() && field.fluidAccepted;

        // 火のそばの霧：場の霧（Default）だけに届く力で、火の熱で上へ持ち上げながら焼いて薄くする（閉じた域なので、
        // 持ち上がった分の霧が横から火へ流れ込む）。注入の数を抑えるため、入れる升目を 1 つおきに交代で出す。
        if ((field.fogBurnAbsorption > 0.0f || field.fogUpdraft > 0.0f) && field.fogBurnRadiusScale > 0.0f &&
            (static_cast<uint32_t>(field.activeEmitters) + field.renderFrame / static_cast<uint32_t>(stride)) % 2u ==
                0u) {
            HIKARI::GameProjectFluidInteraction burn{};
            burn.kind = Kind::Force;
            burn.position = {shape.x, map.FloorY() + field.fuelHeight, shape.z};
            burn.extents = {0.5f * std::max(shape.along, shape.across) * field.fogBurnRadiusScale, 0.0f, 0.0f};
            burn.absorption = field.fogBurnAbsorption * 2.0f;
            // 交代で出す分、加速度も 2 倍にして平均を項目の値に合わせる。向きの無い Force は湧き出しになるので、
            // 上昇が 0 の時は向きも強さも入れない（吸収だけ）。
            if (field.fogUpdraft > 0.0f) {
                burn.direction = {0.0f, 1.0f, 0.0f};
                burn.strength = std::min(kMaximumStrength, field.fogUpdraft * 2.0f * fade);
            }
            burn.channels = FluidChannels::kFog;
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kFogInstanceBase + index, burn).Accepted() && field.fluidAccepted;
        }
    }
}

} // namespace Project_TD2_2
