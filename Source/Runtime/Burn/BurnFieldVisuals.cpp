#include "BurnFieldVisuals.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/VfxView.h"
#include "../Common/FluidChannels.h"
#include "../Common/VectorMath.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

using Kind = HIKARI::GameProjectFluidInteractionKind;

// 同じ所有者の中で注入を識別する。升目（タイル）1 つにつき、燃料 1 つと、霧を焼く力 1 つ。升目ごとに決まった番号で出す
// （毎フレーム入れている升目では、エンジンが、点の動いた線に沿って燃料を広げる。番号を升目の間で使い回すと、
// 隣の升目との間まで掃かれて、火の無い所に燃料の線が入る）。
constexpr uint64_t kFuelInstanceBase = 10000;
constexpr uint64_t kFogInstanceBase = 20000;
// 見た目だけの塊（当たった所・刀の点火・霞の塊）：燃料と、外へ押す衝撃。こちらは毎フレーム番号を替える
// （離れた 2 か所の間を掃かせない）。
constexpr uint64_t kBlastFuelBase = 160;
constexpr uint64_t kBlastPushBase = 180;
constexpr uint64_t kBlastSmokeBase = 200;
// 当たった所を突き抜ける炎（燃料と、同じ向きの衝撃）と、体の炎（体の周りを回る 2 つの燃料と、消えた瞬間に散らす衝撃）。
constexpr uint64_t kJetFuelId = 220;
constexpr uint64_t kJetPushId = 221;
constexpr uint64_t kBodyFuelBase = 222;
constexpr uint64_t kBodyScatterId = 224;
// 1 フレームに見せる塊の数。火の域が受け取れる注入は 32 個で、燃料の点（16）・吸い込みの力と分け合う。残りは次の
// フレームへ回す（c-e759cf で 3→6）。
constexpr uint32_t kBlastsPerFrame = 6;
constexpr float kMaximumStrength = 10000.0f;
// 燃料の点は、升目の中のかたまりの長い方へ毎フレーム行き来させる（エンジンが通った線に沿って燃料を広げるので、
// 球ではなく細長い形で入る）。エンジンは 60 m/s を超える移動を「瞬間移動」と見て線にしないので、それより遅くする
// （BurnField の fuelSweep）。
constexpr float kMinimumRadius = 0.45f;
constexpr float kMaximumRadius = 3.0f;
// 升目ごとの炎の層のエフェクト（ArenaFireTile）：入力の名前と、資産が作られている升目の一辺 (m)、火のかたまりに合わせて
// 根を伸ばす時の長さ・幅の下限 (m)、量を面積で決める時の下限（升目の端の少ないセルでも炎が途切れない）。
constexpr const char* kTileAmountInput = "input.amount";
constexpr const char* kTileFadeInput = "input.fade";
constexpr float kTileAuthoredSize = 2.0f;
constexpr float kTileFlameMinimum = 0.9f;
constexpr float kTileAreaShareMinimum = 0.25f;

// 升目の炎の層を全部止める（升目を確保し直す前）。
void StopTileFlames(HIKARI::WorldView& world, BurnFieldComponent& field) {
    for (BurnTile& tile : field.tiles) {
        if (tile.flame.IsValid())
            world.GetVfx().Stop(tile.flame);
        tile.flame = {};
        tile.flameAmount = 0.0f;
    }
}

// 升目（タイル）の大きさを決め、持ち越しと作業場所を、その数に合わせる（大きさが変わった時だけ確保し直す）。
void LayoutTiles(HIKARI::WorldView& world, BurnFieldComponent& field) {
    const BurnMap& map = field.map;
    const int side = std::max(map.Width(), map.Height());
    const int wanted = std::max(1, static_cast<int>(std::lround(field.emitterTile / map.CellSize())));
    const int tileCells = std::max(wanted, (side + kBurnTilesPerSideMax - 1) / kBurnTilesPerSideMax);
    const int tilesPerSide = (side + tileCells - 1) / tileCells;
    if (tileCells == field.tileCells && tilesPerSide == field.tilesPerSide)
        return;
    StopTileFlames(world, field);
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

// 升目ごとの炎の層（c-3c908c）：燃えているセルのある升目に、炎の舌・熾火・煙・地面の光のエフェクトを 1 つずつ重ねる。
// 流体の火は格子が粗く（約 34 cm）、炎の舌の形が出ないので、形はここで見せる。置く所は升目の火のかたまりの重心、
// 局所 X を長い方へ向け、長さ・幅に合わせて根を伸ばす。量は、燃えている面積と消えかけ（fadeSeconds）で決め、燃え始めに
// 上げ、消えた後は下げてから止める（吸って消した所も、ふっと消える）。
void UpdateTileFlames(HIKARI::WorldView& world, BurnFieldComponent& field, bool anyBurning, float seconds) {
    if (field.tileEffect.IsEmpty()) {
        StopTileFlames(world, field);
        return;
    }
    const BurnMap& map = field.map;
    const HIKARI::VfxView vfx = world.GetVfx();
    const float tileArea = kTileAuthoredSize * kTileAuthoredSize;
    for (size_t index = 0; index < field.tiles.size(); ++index) {
        BurnTile& tile = field.tiles[index];
        const bool burning = anyBurning && field.tileSums[index].cells > 0;
        if (tile.flame.IsValid() && !vfx.IsActive(tile.flame)) {
            tile.flame = {};
            tile.flameAmount = 0.0f;
        }
        if (!burning && !tile.flame.IsValid())
            continue;
        float target = 0.0f;
        float areaShare = 1.0f;
        if (burning) {
            const BurnTileSum& sum = field.tileSums[index];
            const TileShape shape = ShapeOf(field, sum, tile, static_cast<int>(index) % field.tilesPerSide,
                                            static_cast<int>(index) / field.tilesPerSide);
            target = std::clamp(sum.remaining / std::max(0.01f, field.fadeSeconds), 0.0f, 1.0f);
            areaShare =
                std::clamp(static_cast<float>(sum.cells) * map.CellArea() / tileArea, kTileAreaShareMinimum, 1.0f);
            tile.flameTransform = {};
            tile.flameTransform.position = {shape.x, map.FloorY(), shape.z};
            tile.flameTransform.rotation = VectorMath::RotationFromXAxis({shape.axisX, 0.0f, shape.axisZ});
            tile.flameTransform.scale = {std::max(kTileFlameMinimum, shape.along) / kTileAuthoredSize, 1.0f,
                                         std::max(kTileFlameMinimum, shape.across) / kTileAuthoredSize};
        }
        const float ramp = target > tile.flameAmount ? field.tileEffectRise : field.tileEffectFade;
        const float step = ramp > 0.0f ? seconds / ramp : 1.0f;
        tile.flameAmount = target > tile.flameAmount ? std::min(target, tile.flameAmount + step)
                                                     : std::max(target, tile.flameAmount - step);
        if (!burning && !(tile.flameAmount > 0.0f)) {
            vfx.Stop(tile.flame);
            tile.flame = {};
            continue;
        }
        HIKARI::GameProjectVfxParameterOverrides parameters{};
        VfxParameters::SetFloat(parameters, kTileAmountInput,
                                tile.flameAmount * areaShare * std::max(0.0f, field.tileEffectAmount));
        VfxParameters::SetColor(parameters, kTileFadeInput, {tile.flameAmount, tile.flameAmount, tile.flameAmount});
        if (tile.flame.IsValid()) {
            HIKARI::GameProjectVfxUpdateRequest update{};
            update.updateTransform = 1u;
            update.updateParameters = 1u;
            update.transform = tile.flameTransform;
            update.parameters = parameters;
            field.vfxAccepted = vfx.Update(tile.flame, update) && field.vfxAccepted;
            continue;
        }
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = field.tileEffect;
        play.transform = tile.flameTransform;
        play.parameters = parameters;
        play.randomSeed = static_cast<uint32_t>(index) * 2654435761u + 1u;
        tile.flame = vfx.Play(play);
        field.vfxAccepted = tile.flame.IsValid() && field.vfxAccepted;
    }
}

// 何フレームに 1 回ずつ入れるか。足りなければすぐ広げ、狭めるのは、1 つ狭い間隔でも上限の 85 % に収まる時だけ
// （境目で間隔が行き来すると、升目の番が入れ替わり、入る間隔が乱れる）。
int HoldStride(BurnFieldComponent& field, int limit) {
    const int needed = std::max(1, (field.burningTiles + limit - 1) / limit);
    int stride = std::max(needed, field.emitterStride);
    while (stride > needed &&
           static_cast<float>(field.burningTiles) <= 0.85f * static_cast<float>((stride - 1) * limit))
        --stride;
    field.emitterStride = stride;
    return stride;
}

// 燃えている升目に番を割り振る。番を持っている升目はそのまま（1 つの番が上限を超えた分だけ外す）、燃え始めた升目と
// 番の外れた升目を、いちばん空いている番へ入れる。どの番も上限以下なので、1 フレームに入れる数は上限を超えない。
void AssignPhases(BurnFieldComponent& field, int stride, int limit) {
    std::vector<int>& counts = field.phaseCounts;
    counts.assign(static_cast<size_t>(stride), 0);
    for (size_t index = 0; index < field.tiles.size(); ++index) {
        BurnTile& tile = field.tiles[index];
        if (field.tileSums[index].cells == 0 || tile.phase >= stride ||
            (tile.phase >= 0 && counts[static_cast<size_t>(tile.phase)] >= limit))
            tile.phase = -1;
        if (tile.phase >= 0)
            ++counts[static_cast<size_t>(tile.phase)];
    }
    for (size_t index = 0; index < field.tiles.size(); ++index) {
        BurnTile& tile = field.tiles[index];
        if (field.tileSums[index].cells == 0 || tile.phase >= 0)
            continue;
        const auto least = std::min_element(counts.begin(), counts.end());
        tile.phase = static_cast<int>(least - counts.begin());
        ++*least;
    }
}

// 塊を、場の火の域の流体で見せる。当たった所や刀の点火は小さな火球、煙の刃が当たった所は霞の塊
// （ボスの演出が AddBlast で積む）。
void RenderBlasts(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field) {
    const uint32_t count = std::min<uint32_t>(field.blastCount, kBlastsPerFrame);
    if (count == 0)
        return;
    const HIKARI::FluidView fluid = world.GetFluid();
    field.blastFlip ^= 1u;
    for (uint32_t index = 0; index < count; ++index) {
        const BurnBlast& blast = field.blasts[index];
        const bool smoke = blast.kind == BurnBlastKind::Smoke;
        const float radius = std::max(0.3f, blast.radius);
        const HIKARI::GameProjectFloat3 center = blast.position;
        const uint64_t slot = static_cast<uint64_t>(index) * 2u + field.blastFlip;
        const float fuelStrength =
            smoke ? field.smokeBlastStrength * field.smokeBlastFuelShare : field.blastFuel * field.hitFuelScale;
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
        // 向きの無い衝撃：火の域では塊を外へ膨らませ、霞の塊は場の霧を押す（当たった所の霧はボスの側が押す）。
        const float pushSpeed = smoke ? field.smokeBlastPush : field.blastPush * 0.6f;
        if (pushSpeed > 0.0f) {
            HIKARI::GameProjectFluidInteraction push{};
            push.kind = Kind::Impulse;
            push.position = center;
            push.extents = {radius * 1.5f, 0.0f, 0.0f};
            push.strength = pushSpeed;
            push.channels = smoke ? FluidChannels::kFog : FluidChannels::kFire;
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kBlastPushBase + slot, push).Accepted() && field.fluidAccepted;
        }
    }
    // 見せた分を前から詰める。
    std::move(field.blasts.begin() + count, field.blasts.begin() + field.blastCount, field.blasts.begin());
    field.blastCount -= count;
}

// 火の刃が当たった所：攻撃の向きへ速さのある熱い燃料を、体の手前から向こうへ動かしながら数フレーム入れる
// （同じ番号で動かすので、エンジンが通った線に沿って燃料を広げ、炎が体を突き抜ける舌になる）。
void RenderJet(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field, float seconds) {
    BurnJet& jet = field.jet;
    if (!(jet.remaining > 0.0f))
        return;
    const HIKARI::FluidView fluid = world.GetFluid();
    // 終わりへ向けて細らせる（急に切ると、炎の舌の尻が角ばって残る）。
    const float fade = std::sqrt(std::clamp(jet.remaining / std::max(0.001f, field.jetSeconds), 0.0f, 1.0f));
    const float radius = std::max(0.1f, field.jetRadius * jet.scale);
    HIKARI::GameProjectFluidInteraction fuel{};
    fuel.kind = Kind::Fuel;
    fuel.position = jet.position;
    fuel.extents = {radius, std::max(0.0f, field.jetTemperature), 0.0f};
    fuel.direction = {jet.direction.x * field.jetSpeed, 0.0f, jet.direction.z * field.jetSpeed};
    fuel.strength = std::clamp(field.jetFuel * jet.scale * fade, 0.0f, kMaximumStrength);
    fuel.channels = FluidChannels::kFire;
    if (fuel.strength > 0.0f)
        field.fluidAccepted = fluid.Submit(object.GetHandle(), kJetFuelId, fuel).Accepted() && field.fluidAccepted;
    if (field.jetPush > 0.0f) {
        // 同じ向きへ、まわりの火と場の霧も押す（霧が、攻撃の通った向きへ割れる）。
        HIKARI::GameProjectFluidInteraction push{};
        push.kind = Kind::Impulse;
        push.position = jet.position;
        push.extents = {radius * 2.0f, 0.0f, 0.0f};
        push.direction = jet.direction;
        push.strength = field.jetPush * fade;
        push.channels = FluidChannels::kFire | FluidChannels::kFog;
        field.fluidAccepted = fluid.Submit(object.GetHandle(), kJetPushId, push).Accepted() && field.fluidAccepted;
    }
    const float step = std::min(field.jetTravel, 55.0f) * seconds;
    jet.position = {jet.position.x + jet.direction.x * step, jet.position.y, jet.position.z + jet.direction.z * step};
    jet.remaining -= seconds;
}

// 炎上の間：体の周りを回る 2 つの点（上と下、向かい合わせ）から、燃料を同じ番号で入れ続ける。体は火の域の障害物なので、
// 体の中へ入れた燃料は流れず、炎が見えない。体の外側を回して入れると、エンジンが点の通った線に沿って燃料を広げ、
// 炎が体を巻いて立ちのぼり、動けば尾を引く。ローリング・吸い込みで消えた瞬間は、残った炎を外へ散らす。
void RenderBody(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field, const BurnBody& body) {
    if (!body.found)
        return;
    const HIKARI::FluidView fluid = world.GetFluid();
    if (body.remaining > 0.0f && field.bodyFireFuel > 0.0f) {
        const float fade = field.bodyFireFadeSeconds > 0.0f
                               ? std::clamp(body.remaining / field.bodyFireFadeSeconds, 0.0f, 1.0f)
                               : 1.0f;
        const float angle = 6.2831853f * field.bodyFireTurns * field.visualTime;
        for (uint64_t i = 0; i < 2; ++i) {
            const float side = i == 0 ? 1.0f : -1.0f;
            HIKARI::GameProjectFluidInteraction fuel{};
            fuel.kind = Kind::Fuel;
            fuel.position = {body.position.x + side * std::cos(angle) * field.bodyFireOrbit,
                             body.position.y + side * field.bodyFireSpread,
                             body.position.z + side * std::sin(angle) * field.bodyFireOrbit};
            fuel.extents = {std::max(0.1f, field.bodyFireRadius), std::max(0.0f, field.bodyFireTemperature), 0.0f};
            fuel.direction = {0.0f, field.bodyFireRise, 0.0f};
            fuel.strength = std::min(kMaximumStrength, field.bodyFireFuel * fade);
            fuel.channels = FluidChannels::kFire;
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kBodyFuelBase + i, fuel).Accepted() && field.fluidAccepted;
        }
    }
    if (body.endings == field.bodyEndingsSeen)
        return;
    field.bodyEndingsSeen = body.endings;
    if (!body.scatter || !(field.bodyFireScatter > 0.0f))
        return;
    HIKARI::GameProjectFluidInteraction scatter{};
    scatter.kind = Kind::Impulse;
    scatter.position = body.position;
    scatter.extents = {field.bodyFireOrbit + 2.0f * std::max(0.1f, field.bodyFireRadius), 0.0f, 0.0f};
    scatter.strength = field.bodyFireScatter;
    scatter.channels = FluidChannels::kFire;
    field.fluidAccepted = fluid.Submit(object.GetHandle(), kBodyScatterId, scatter).Accepted() && field.fluidAccepted;
}

} // namespace

void BurnFieldVisuals::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field,
                              const BurnBody& body, float seconds) {
    const BurnMap& map = field.map;
    field.fluidAccepted = true;
    field.vfxAccepted = true;
    field.activeEmitters = 0;
    field.maxFeedGap = 0;
    if (seconds <= 0.0f)
        return;
    ++field.renderFrame;
    field.visualTime += seconds;
    // 爆ぜた塊は、火のセルが全部消えた後のフレームにも残っているので、先に見せる。
    RenderBlasts(world, object, field);
    RenderJet(world, object, field, seconds);
    RenderBody(world, object, field, body);
    field.burningTiles = 0;
    if (map.BurningCells() == 0) {
        field.emitterStride = 1;
        for (BurnTile& tile : field.tiles) {
            tile.phase = -1;
            tile.fedFrame = 0;
        }
        UpdateTileFlames(world, field, false, seconds);
        return;
    }
    // 燃えているセルを、場所の決まった升目ごとにまとめ、升目ごとに 1 か所へ燃料を入れる。升目の炎の位置と太さは、
    // その升目のセルだけで決まる（ほかの所で火が増えても減っても動かない）。燃えている升目が 1 フレームに出せる数より
    // 多い時は、stride フレームに 1 回ずつ、升目ごとに決まった番で入れ、その分だけ燃料を増やす（どの升目も同じ間隔で
    // 入る。番は燃えている間は変えない）。
    LayoutTiles(world, field);
    field.burningTiles = SumTiles(field);
    UpdateTileFlames(world, field, true, seconds);
    const int limit = std::clamp(field.emitterCount, 1, static_cast<int>(kBurnEmitterCapacity));
    const int stride = HoldStride(field, limit);
    AssignPhases(field, stride, limit);
    const int turn = static_cast<int>(field.renderFrame % static_cast<uint32_t>(stride));

    const float sweepSpeed = std::clamp(field.fuelSweep, 0.0f, 55.0f);
    // 順番待ちの升目へまとめて入れる燃料の倍率（待った回数の累乗。広い火ほど控えめにして、白く飛ばさない）。
    const float crowdGain = std::pow(static_cast<float>(stride), std::clamp(field.fuelCrowdPower, 0.0f, 1.0f));
    const HIKARI::FluidView fluid = world.GetFluid();
    for (size_t index = 0; index < field.tileSums.size(); ++index) {
        const BurnTileSum& sum = field.tileSums[index];
        BurnTile& tile = field.tiles[index];
        if (sum.cells == 0) {
            tile.fedFrame = 0;
            continue;
        }
        if (tile.phase != turn)
            continue;
        if (tile.fedFrame != 0)
            field.maxFeedGap = std::max(field.maxFeedGap, static_cast<int>(field.renderFrame - tile.fedFrame));
        tile.fedFrame = field.renderFrame;
        const TileShape shape = ShapeOf(field, sum, tile, static_cast<int>(index) % field.tilesPerSide,
                                        static_cast<int>(index) / field.tilesPerSide);
        tile.axisX = shape.axisX;
        tile.axisZ = shape.axisZ;
        if (field.activeEmitters++ == 0) {
            field.traceAlong = shape.along;
            field.traceAcross = shape.across;
        }
        const float rate = std::max(0.0f, field.fuelRate);
        const float rise = field.fuelRise;
        const float temperature = field.fuelTemperature;
        // 燃えているセルの少ない升目（火の端、吸われた残り）と、消えかけの升目は、燃料を絞る。
        const float fade = std::clamp(sum.remaining / std::max(0.01f, field.fadeSeconds), 0.0f, 1.0f);
        const float share = std::clamp(static_cast<float>(sum.cells) / 6.0f, 0.35f, 1.0f);
        // 炎の幅は、かたまりの幅に合わせる。球の中心が床より下にある分、床の高さでの切り口がその幅になる半径にする。
        const float halfWidth = 0.5f * shape.across * field.fuelRadiusScale;
        const float radius = std::clamp(std::sqrt(halfWidth * halfWidth + field.fuelHeight * field.fuelHeight),
                                        kMinimumRadius, kMaximumRadius);
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
        fuel.strength = std::min(kMaximumStrength, rate * share * fade * swell * stretch * crowdGain);
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
