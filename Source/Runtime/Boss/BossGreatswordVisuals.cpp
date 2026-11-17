#include "BossGreatswordVisuals.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/VfxView.h"
#include "BossMath.h"
#include "BossVfx.h"
#include "BossWaves.h"
#include "../Common/FluidChannels.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;
using namespace BossVfx;
using Kind = HIKARI::GameProjectFluidInteractionKind;

// 場の霧を押す所・塊を出す所を、地面からこの高さに置く。
constexpr float kGreatswordPushLift = 0.5f;
// 流体への提出 ID（持ち主はボスの物体。BossSmoke・BossVisuals の番号と重ならない所）。
// 叩きつけた瞬間に霧を割る押し（所ごとに左右 2 つ）と、噴き上がりの前線の吹き上げ・前線が霧を割る押し（左右 2 つ）。
constexpr uint64_t kSplitBase = 1010;
constexpr int kSplitCapacity = 6;
constexpr uint64_t kGeyserId = 1030;
constexpr uint64_t kGeyserSplitBase = 1031;
// 噴き上がりの前線が霧を割る押しを、前線からどれだけ後ろへ入れるか（押しの半径に対する倍）。
constexpr float kGeyserSplitBehind = 1.6f;

void Submit(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id,
            const HIKARI::GameProjectFluidInteraction& interaction) {
    boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, interaction).Accepted() && boss.fluidAccepted;
}

// 霞（場の霧へ）か、炎の大剣なら燃料（場の火へ）を、position の半径 radius へ入れる。velocity は入れた物を押す速さ。
void Pour(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id, Vec3 position,
          float radius, Vec3 velocity, float fog, float fuel, float temperature) {
    const bool fire = boss.greatsword.fire;
    HIKARI::GameProjectFluidInteraction pour{};
    pour.kind = fire ? Kind::Fuel : Kind::Emit;
    pour.position = position;
    pour.extents = {radius, fire ? std::max(0.0f, temperature) : 0.0f, 0.0f};
    pour.direction = velocity;
    pour.strength = fire ? fuel : fog;
    pour.channels = fire ? FluidChannels::kFire : FluidChannels::kFog;
    if (pour.strength > 0.0f && radius > 0.0f)
        Submit(world, object, boss, id, pour);
}

// 薙ぎ終わった瞬間：剣が通った扇に沿って、場の霧を外へ押す（爆発の枠の「押し」だけを使う。光は出さない。爆発ではないので
// 跡の煙も残さない）。
void PushSweptFog(BossComponent& boss) {
    if (boss.greatswordSweepPushes <= 0 || !(boss.greatswordSweepPushDistance > 0.0f))
        return;
    const float yawEnd = YawOf(boss.greatsword.direction);
    const float swept = boss.greatswordSweepDegrees * kRadiansPerDegree;
    for (int i = 0; i < boss.greatswordSweepPushes; ++i) {
        const float back = swept * (static_cast<float>(i) + 0.5f) / static_cast<float>(boss.greatswordSweepPushes);
        const Vec3 at = Add(boss.ground, Mul(Forward(yawEnd - back), boss.greatswordSweepPushDistance));
        BossWaves::AddBlast(
            boss,
            {.pushPending = true, .leavesFog = false, .position = {at.x, boss.ground.y + kGreatswordPushLift, at.z}});
    }
}

// at の所の場の霧を、line の向きの線から左右へ 1 回押す（id と id+1 を使う）。場の火は押さない（炎が地面に平たく潰れる）。
void Part(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id, Vec3 at, Vec3 line) {
    const Vec3 side{line.z, 0.0f, -line.x};
    for (int k = 0; k < 2; ++k) {
        HIKARI::GameProjectFluidInteraction push{};
        push.kind = Kind::Impulse;
        push.direction = Mul(side, k == 0 ? 1.0f : -1.0f);
        const Vec3 center = Add(at, Mul(push.direction, boss.greatswordSplitRadius * 0.5f));
        push.position = {center.x, boss.ground.y + kGreatswordPushLift, center.z};
        push.extents = {boss.greatswordSplitRadius, 0.0f, 0.0f};
        push.strength = boss.greatswordSplitSpeed;
        push.channels = FluidChannels::kFog;
        Submit(world, object, boss, id + static_cast<uint64_t>(k), push);
    }
}

// 叩きつけた瞬間：剣先に塊を出し、刃の線（ボスの足元〜剣先）に沿って場の霧を左右へ 1 回押して割る。
void Land(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss) {
    const BossGreatswordState& sword = boss.greatsword;
    const Vec3 tip = sword.eruptionFrom;
    if (boss.greatswordLandBlastScale > 0.0f)
        BossWaves::AddBlast(boss, {.effectPending = true,
                                   .fire = sword.fire,
                                   .scale = boss.greatswordLandBlastScale,
                                   .position = {tip.x, boss.ground.y + kGreatswordPushLift, tip.z}});
    const int splits = std::clamp(boss.greatswordSplitPushes, 0, kSplitCapacity);
    if (!(boss.greatswordSplitSpeed > 0.0f))
        return;
    for (int i = 0; i < splits; ++i)
        Part(world, object, boss, kSplitBase + static_cast<uint64_t>(i * 2),
             Lerp(boss.ground, tip, (static_cast<float>(i) + 0.5f) / static_cast<float>(splits)),
             Forward(sword.cleaveYaw));
}

// 噴き上がりの前線と一緒に地を走る縦の刃（斬撃のエフェクトを 1 つ、縦に立てて持って行く）。走り切ったら（打ち切られても）
// 刃を出すのをやめて砕けさせ、勢いを落としてその場で散らす。縦割りがプレイヤーに当たった時は出さない・その場で砕けさせる
// （そのままの刃がプレイヤーの後ろのカメラを通り抜けて、画面を覆わないように。飛ぶ斬撃と同じ）。
void UpdateGeyserBlade(HIKARI::WorldView& world, BossComponent& boss, float seconds) {
    const BossGreatswordState& sword = boss.greatsword;
    BossSlashEffect& blade = boss.greatswordVisual.geyserBlade;
    if (blade.handle.IsValid() && !world.GetVfx().IsActive(blade.handle))
        blade = {};
    if (sword.erupting && !sword.cleaveStruck && boss.greatswordGeyserBladeHeight > 0.0f &&
        !boss.slashEffect.IsEmpty()) {
        const Vec3 forward = Forward(sword.cleaveYaw);
        const Vec3 at{sword.eruptionFront.x, boss.ground.y + boss.greatswordGeyserBladeLift, sword.eruptionFront.z};
        if (!blade.handle.IsValid()) {
            blade = {};
            blade.flying = true;
            blade.position = at;
            blade.velocity = Mul(forward, boss.greatswordEruptionSpeed);
            blade.rotation = BasisRotation(forward, {0.0f, 1.0f, 0.0f});
            // 刃は中心から上下へ広がるので、地面から出ている高さの 2 倍が刃の幅。
            blade.scale = boss.greatswordGeyserBladeHeight * 2.0f / std::max(0.01f, boss.slashEffectWidth);
            StartSlash(world, boss, blade, SlashAsset(boss, sword.fire), 0u);
        } else {
            blade.position = at;
        }
    } else if (blade.handle.IsValid() && blade.flying) {
        Shatter(boss, blade, boss.slashFadeSeconds);
    }
    if (blade.handle.IsValid() && !blade.flying)
        StepShattered(world, boss, blade, seconds);
    if (blade.handle.IsValid())
        SendSlash(world, boss, blade);
}

// 地を走る噴き上がり：前線が足元の場の霧を左右へ割りながら進み、前線から霞（炎の大剣は燃料）を吹き上げる
// （同じ番号で出し続けるので、走った線に沿って壁になる）。走った距離が間隔を越えるたびに塊を出し、走り切った所にも出す
// （打ち切られた時は出さない）。
void UpdateGeyser(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss) {
    const BossGreatswordState& sword = boss.greatsword;
    BossGreatswordVisualState& look = boss.greatswordVisual;
    const bool was = look.seenErupting;
    look.seenErupting = sword.erupting;
    const float lift = boss.ground.y + kGreatswordPushLift;
    if (!sword.erupting) {
        // 走り切った（剣がまだある）：最後の所に塊を出す。
        if (was && sword.phase != BossGreatswordPhase::None && boss.greatswordGeyserEndScale > 0.0f)
            BossWaves::AddBlast(boss, {.pushPending = true,
                                       .effectPending = true,
                                       .fire = sword.fire,
                                       .scale = boss.greatswordGeyserEndScale,
                                       .position = {look.geyserFront.x, lift, look.geyserFront.z}});
        return;
    }
    if (!was)
        look.geyserBurstAt = 0.0f;
    look.geyserFront = sword.eruptionFront;
    const Vec3 forward = Forward(sword.cleaveYaw);
    const float run = FlatLength(Sub(sword.eruptionFront, sword.eruptionFrom));
    // 霧を割る押しは、前線の少し後ろへ入れる（走る刃の域の中の霞まで押すと、刃の足元が箱の形に広がる）。
    const float behind = boss.greatswordSplitRadius * kGeyserSplitBehind;
    if (boss.greatswordSplitSpeed > 0.0f && run >= behind)
        Part(world, object, boss, kGeyserSplitBase, Sub(sword.eruptionFront, Mul(forward, behind)), forward);
    Pour(world, object, boss, kGeyserId,
         {sword.eruptionFront.x, boss.ground.y + boss.greatswordGeyserRadius * 0.5f, sword.eruptionFront.z},
         boss.greatswordGeyserRadius, {0.0f, boss.greatswordGeyserRise, 0.0f}, boss.greatswordGeyserFog,
         boss.greatswordGeyserFuel, boss.greatswordGeyserTemperature);
    if (!(boss.greatswordGeyserBurstSpacing > 0.0f) || !(boss.greatswordGeyserBurstScale > 0.0f))
        return;
    while (run - look.geyserBurstAt >= boss.greatswordGeyserBurstSpacing) {
        look.geyserBurstAt += boss.greatswordGeyserBurstSpacing;
        const Vec3 at = Add(sword.eruptionFrom, Mul(forward, look.geyserBurstAt));
        BossWaves::AddBlast(boss, {.effectPending = true,
                                   .fire = sword.fire,
                                   .scale = boss.greatswordGeyserBurstScale,
                                   .position = {at.x, lift, at.z}});
    }
}

} // namespace

// 満ちる割合（柄から剣先へ）は規則が書く boss.greatsword.fill をそのまま送る。
void BossGreatswordVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                                   float seconds) {
    const BossGreatswordState& sword = boss.greatsword;
    BossGreatswordVisualState& look = boss.greatswordVisual;
    const BossGreatswordPhase seen = look.seenPhase;
    look.seenPhase = sword.phase;
    // 叩きつけた瞬間（着地の tick が変わったフレーム）と、噴き上がりは、剣のエフェクトが出ているかに関わらず進める。
    if (sword.landTick != 0 && sword.landTick != look.seenLandTick)
        Land(world, object, boss);
    look.seenLandTick = sword.landTick;
    UpdateGeyser(world, object, boss);
    UpdateGeyserBlade(world, boss, seconds);
    if (look.vfx.IsValid() && !world.GetVfx().IsActive(look.vfx))
        look.vfx = {};
    const bool held = sword.phase != BossGreatswordPhase::None;
    if (held && seen == BossGreatswordPhase::None && look.vfx.IsValid()) {
        // 前の大剣がまだ消えかけている：止めて、次のフレームに出す（止めた域もそのフレームの間は流体域の数に入る）。
        StopHandle(world, look.vfx);
        look.seenPhase = BossGreatswordPhase::None;
        return;
    }
    if (held) {
        // 剣の平らな面：作る間はボスの正面へ向け（プレイヤーから剣の形が見える）、薙ぐ時は上へ向ける（刃が進む向きに立つ）。
        // 薙ぐ時の面は真上から少しボスの正面へ傾け、前にいるプレイヤーからも面が見えるようにする。
        const Vec3 front = Forward(boss.yaw);
        const Vec3 swing =
            Normalize(Add(Vec3{0.0f, 1.0f, 0.0f}, Mul(front, boss.greatswordSweepFaceTilt)), {0.0f, 1.0f, 0.0f});
        // 縦割り（振りかぶる・叩きつける・刺さっている・その後ほどける）の面は横へ向ける（刃が縦に立つ）。真横だと正面の
        // プレイヤーから線にしか見えないので、正面へ greatswordCleaveFaceTurn 度だけ回す。振りかぶる間は作った時・薙いだ時の
        // 面から、構え直す間は横から薙ぐ時の面へ回す。
        const float turn = boss.greatswordCleaveFaceTurn * kRadiansPerDegree;
        const Vec3 side = Add(Mul(Vec3{front.z, 0.0f, -front.x}, std::cos(turn)), Mul(front, std::sin(turn)));
        const Vec3 before = sword.chained ? swing : front;
        const Vec3 normal = sword.phase == BossGreatswordPhase::Raise ? Normalize(Lerp(before, side, sword.raise), side)
                            : sword.phase == BossGreatswordPhase::Ready
                                ? Normalize(Lerp(side, swing, sword.wind), swing)
                            : sword.cleave && sword.phase != BossGreatswordPhase::Conjure
                                ? side
                                : Normalize(Lerp(front, swing, sword.wind), {0.0f, 1.0f, 0.0f});
        // 資産の剣は greatswordEffectLength m。当たりの長さ（sword.length）に合わせた大きさで置く。
        look.transform = Placed(sword.hilt, sword.length / std::max(0.01f, boss.greatswordEffectLength),
                                BasisRotation(Cross(sword.direction, normal), sword.direction));
        look.fade = boss.greatswordFadeSeconds;
    } else if (look.vfx.IsValid()) {
        // 技が終わった・崩された：出すのをやめ、残った霞が消えるまで待ってから止める（域ごと急に消さない）。
        look.fade -= seconds;
        if (look.fade <= 0.0f) {
            StopHandle(world, look.vfx);
            return;
        }
    } else {
        return;
    }
    const auto& asset = sword.fire ? boss.greatswordFireEffect : boss.greatswordMistEffect;
    if (!look.vfx.IsValid() && asset.IsEmpty())
        return;
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kFillInput, held ? sword.fill : 1.0f);
    // 薙ぐ・叩きつける間は多めに流し込む。ほどける間と終わった後は、もう出さない（残った霞が自分で消える）。
    // 振りかぶる間は、振りかぶり終わりへ向けて増やす（刃がふくらんで明るくなる。叩きつける前の合図は剣そのものだけ）。
    // 連撃の構え直し・刺さっている間は、作る間と同じだけ流し込む（剣の形を保つ）。
    const bool swinging = sword.phase == BossGreatswordPhase::Sweep || sword.phase == BossGreatswordPhase::Slam;
    float amount = 1.0f;
    if (swinging)
        amount = boss.greatswordSweepAmount;
    else if (sword.phase == BossGreatswordPhase::Raise)
        amount = Lerp(1.0f, boss.greatswordRaiseAmount, sword.raise);
    else if (sword.phase == BossGreatswordPhase::Dissolve || !held)
        amount = 0.0f;
    VfxParameters::SetFloat(parameters, kAmountInput, amount);
    StartOrSend(world, boss, look.vfx, asset, look.transform, parameters);
    if (sword.phase == BossGreatswordPhase::Dissolve && seen == BossGreatswordPhase::Sweep)
        PushSweptFog(boss);
}

void BossGreatswordVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    StopHandle(world, boss.greatswordVisual.vfx);
    StopHandle(world, boss.greatswordVisual.geyserBlade.handle);
}

} // namespace Project_TD2_2
