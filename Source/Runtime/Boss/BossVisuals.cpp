#include "BossVisuals.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/VfxView.h"
#include "BossMath.h"
#include "BossPose.h"
#include "../Burn/BurnFieldRules.h"
#include "../Common/FluidChannels.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 資産の入力（エフェクト側の契約）。
constexpr const char* kAmountInput = "input.amount";
constexpr const char* kColorInput = "input.color";
// 剣先の火の粉の光の大きさ（0 で出さない。ナイフの尾は出さない）。
constexpr const char* kEmberInput = "input.ember";
// 斬撃のエフェクトの入力：fly＝刃を出す量、impact＝当たって砕ける量。
constexpr const char* kFlyInput = "input.fly";
constexpr const char* kImpactInput = "input.impact";
// 光の層（刃口・炎・光・屈折）の大きさ。霧の源（fly）とは別に、数フレームかけて縮める。
constexpr const char* kGlowInput = "input.glow";
// 炎の輪のシェーダーの層の明るさ（Color。輪が消える時に 0 へ下げる）。
constexpr const char* kFadeInput = "input.fade";
// 剣に沿って場の火の域へ入れる燃料の番号（BossSmoke の注入・押しの番号と重ならない所）。
constexpr uint64_t kBladeFuelBase = 900;

// lit は剣が燃えている間（霞を炎の色にする）。
HIKARI::GameProjectVfxParameterOverrides Parameters(const BossComponent& boss, float amount, float ember,
                                                    bool lit = false) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kAmountInput, amount);
    VfxParameters::SetFloat(parameters, kEmberInput, ember);
    VfxParameters::SetColor(parameters, kColorInput, lit ? boss.bladeLitColor : boss.hazeColor);
    return parameters;
}

HIKARI::GameProjectVfxPlaybackHandle Play(HIKARI::WorldView& world, BossComponent& boss,
                                          const HIKARI::GameProjectVfxAssetReference& asset,
                                          const HIKARI::GameProjectTransform& transform, float amount, float ember,
                                          bool lit = false) {
    if (asset.IsEmpty())
        return {};
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = asset;
    play.transform = transform;
    play.parameters = Parameters(boss, amount, ember, lit);
    const auto handle = world.GetVfx().Play(play);
    boss.vfxAccepted = handle.IsValid() && boss.vfxAccepted;
    return handle;
}

void StopHandle(HIKARI::WorldView& world, HIKARI::GameProjectVfxPlaybackHandle& handle) {
    if (handle.IsValid())
        world.GetVfx().Stop(handle);
    handle = {};
}

const BossWave* FindWave(const BossComponent& boss, uint64_t id) {
    for (const auto& wave : boss.waves) {
        if (wave.active && !wave.knife && !wave.ring && wave.id == id)
            return &wave;
    }
    return nullptr;
}

void SendSlash(HIKARI::WorldView& world, BossComponent& boss, const BossSlashEffect& effect) {
    HIKARI::GameProjectVfxUpdateRequest update{};
    update.updateTransform = 1u;
    update.updateParameters = 1u;
    update.transform.position = effect.position;
    update.transform.rotation = effect.rotation;
    update.transform.scale = {effect.scale, effect.scale, effect.scale};
    VfxParameters::SetFloat(update.parameters, kFlyInput, effect.flying ? 1.0f : 0.0f);
    VfxParameters::SetFloat(update.parameters, kImpactInput, effect.impactSeconds > 0.0f ? 1.0f : 0.0f);
    VfxParameters::SetFloat(update.parameters, kGlowInput, effect.glow);
    boss.vfxAccepted = world.GetVfx().Update(effect.handle, update) && boss.vfxAccepted;
}

// 飛ぶ斬撃の本体：斬撃 1 つにつき、自前の流体域を持つ三日月の霧のエフェクトを 1 つ飛ばす。
void UpdateSlashes(HIKARI::WorldView& world, BossComponent& boss, float seconds) {
    const auto vfx = world.GetVfx();
    for (auto& effect : boss.slashEffects) {
        if (!effect.handle.IsValid())
            continue;
        if (!vfx.IsActive(effect.handle)) {
            effect = {};
            continue;
        }
        const BossWave* wave = effect.flying ? FindWave(boss, effect.waveId) : nullptr;
        if (wave != nullptr) {
            effect.position = wave->position;
            effect.scale = wave->width / std::max(0.01f, boss.slashEffectWidth);
            effect.travelled = wave->travelled;
            if (wave->struck && !effect.struck) {
                // プレイヤーに当たった：刃を出すのをやめて砕けさせ、勢いを落としてその場で散らす
                // （そのままの刃がプレイヤーの後ろのカメラを通り抜けて、画面を白く覆わないように）。
                effect.struck = true;
                effect.flying = false;
                effect.impactSeconds = boss.slashImpactSeconds;
                effect.fadeSeconds = boss.slashHitFadeSeconds;
                effect.velocity = Mul(effect.velocity, boss.slashHitSpeedScale);
            }
        } else {
            // 斬撃が消えた：刃を出すのをやめ、残った霧が消えるまで同じ速さで飛ばしてから止める（域ごと急に消さない）。
            if (effect.flying) {
                effect.flying = false;
                effect.fadeSeconds = boss.slashFadeSeconds;
            }
            effect.fadeSeconds -= seconds;
            if (effect.fadeSeconds <= 0.0f) {
                StopHandle(world, effect.handle);
                effect = {};
                continue;
            }
            effect.position = Add(effect.position, Mul(effect.velocity, seconds));
        }
        if (!effect.flying)
            effect.glow = std::max(0.0f, effect.glow - seconds / std::max(0.001f, boss.slashGlowFadeSeconds));
        effect.impactSeconds = std::max(0.0f, effect.impactSeconds - seconds);
        SendSlash(world, boss, effect);
    }
    if (boss.slashEffect.IsEmpty())
        return;
    for (size_t i = 0; i < boss.waves.size(); ++i) {
        const BossWave& wave = boss.waves[i];
        // waveVfxId は「この斬撃にはもうエフェクトを出した」の印（1 つの斬撃に出すのは 1 回だけ）。
        // 霞の輪（回し蹴り）は斬撃のエフェクトを使わない（UpdateRing が見せる）。
        if (!wave.active || wave.knife || wave.ring || boss.waveVfxId[i] == wave.id)
            continue;
        BossSlashEffect* slot = nullptr;
        BossSlashEffect* oldest = nullptr;
        for (auto& effect : boss.slashEffects) {
            if (!effect.handle.IsValid()) {
                slot = &effect;
                break;
            }
            if (oldest == nullptr || effect.flying < oldest->flying ||
                (effect.flying == oldest->flying && effect.travelled > oldest->travelled))
                oldest = &effect;
        }
        if (slot == nullptr) {
            // 枠が無い：消えかけ（無ければ一番遠くまで飛んだもの）を止め、次のフレームで出す。
            // 止めた域もそのフレームの間は流体域の数に入るので、同じフレームには出さない。
            StopHandle(world, oldest->handle);
            *oldest = {};
            break;
        }
        boss.waveVfxId[i] = wave.id;
        *slot = {};
        slot->waveId = wave.id;
        slot->flying = true;
        slot->position = wave.position;
        slot->velocity = Mul(wave.direction, wave.speed);
        slot->rotation = BasisRotation(wave.direction, wave.across);
        slot->scale = wave.width / std::max(0.01f, boss.slashEffectWidth);
        HIKARI::GameProjectVfxPlayRequest play{};
        // 火を残す一撃は炎の刃、残さない一撃は煙の刃（出た刃を見れば、地面が燃えるかどうかが分かる）。
        play.asset = wave.fire || boss.slashSmokeEffect.IsEmpty() ? boss.slashEffect : boss.slashSmokeEffect;
        play.transform.position = slot->position;
        play.transform.rotation = slot->rotation;
        play.transform.scale = {slot->scale, slot->scale, slot->scale};
        play.randomSeed = static_cast<uint32_t>(wave.id);
        VfxParameters::SetFloat(play.parameters, kFlyInput, 1.0f);
        VfxParameters::SetFloat(play.parameters, kImpactInput, 0.0f);
        VfxParameters::SetFloat(play.parameters, kGlowInput, 1.0f);
        slot->handle = vfx.Play(play);
        boss.vfxAccepted = slot->handle.IsValid() && boss.vfxAccepted;
        // 位置は毎フレーム送る。エフェクト側の速度で二重に動かさない。
        if (slot->handle.IsValid())
            boss.vfxAccepted = vfx.SetVelocity(slot->handle, {}) && boss.vfxAccepted;
    }
}

// 回し蹴りの霞の輪：広がる炎の輪のエフェクトを 1 つ出し、輪の半径に合わせて根の大きさを変える。
// 輪が消えたら燃料を止め、炎が消えるまで待ってから止める（域ごと急に消さない）。
void UpdateRing(HIKARI::WorldView& world, BossComponent& boss, float seconds) {
    const auto vfx = world.GetVfx();
    if (boss.ringVfx.IsValid() && !vfx.IsActive(boss.ringVfx))
        boss.ringVfx = {};
    const BossWave* ring = nullptr;
    for (const auto& wave : boss.waves) {
        if (wave.active && wave.ring)
            ring = &wave;
    }
    if (ring != nullptr && ring->id != boss.ringVfxId && !boss.ringEffect.IsEmpty()) {
        // 前の輪が残っていたら止め、次のフレームで出す（止めた域もそのフレームの間は流体域の数に入る）。
        if (boss.ringVfx.IsValid()) {
            StopHandle(world, boss.ringVfx);
            return;
        }
        boss.ringVfxId = ring->id;
        boss.ringFade = -1.0f;
        boss.ringScale = ring->radius / std::max(0.01f, boss.ringEffectRadius);
        boss.ringCenter = {ring->position.x, boss.ground.y + boss.ringHeight, ring->position.z};
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = boss.ringEffect;
        play.transform.position = boss.ringCenter;
        play.transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        play.transform.scale = {boss.ringScale, boss.ringScale, boss.ringScale};
        play.randomSeed = static_cast<uint32_t>(ring->id);
        VfxParameters::SetFloat(play.parameters, kFlyInput, 1.0f);
        VfxParameters::SetColor(play.parameters, kFadeInput, {1.0f, 1.0f, 1.0f});
        boss.ringVfx = vfx.Play(play);
        boss.vfxAccepted = boss.ringVfx.IsValid() && boss.vfxAccepted;
        return;
    }
    if (!boss.ringVfx.IsValid())
        return;
    const bool flying = ring != nullptr && ring->id == boss.ringVfxId;
    if (flying) {
        boss.ringScale = ring->radius / std::max(0.01f, boss.ringEffectRadius);
    } else {
        if (boss.ringFade < 0.0f)
            boss.ringFade = boss.ringFadeSeconds;
        boss.ringFade -= seconds;
        if (boss.ringFade <= 0.0f) {
            StopHandle(world, boss.ringVfx);
            return;
        }
    }
    // シェーダーの輪は、輪が消えたら明るさを 0 へ下げる（流体の炎は自分で燃え尽きる）。
    const float glow = flying ? 1.0f : std::clamp(boss.ringFade / std::max(0.001f, boss.ringFadeSeconds), 0.0f, 1.0f);
    HIKARI::GameProjectVfxUpdateRequest update{};
    update.updateTransform = 1u;
    update.updateParameters = 1u;
    update.transform.position = boss.ringCenter;
    update.transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    update.transform.scale = {boss.ringScale, boss.ringScale, boss.ringScale};
    VfxParameters::SetFloat(update.parameters, kFlyInput, flying ? 1.0f : 0.0f);
    VfxParameters::SetColor(update.parameters, kFadeInput, {glow, glow, glow});
    boss.vfxAccepted = vfx.Update(boss.ringVfx, update) && boss.vfxAccepted;
}

} // namespace

// 火を残す技の間、剣を燃やす：剣に沿って場の火の域へ燃料を入れ（振ると炎が弧を引く）、火が付いた瞬間に小さな火球を出す。
void UpdateBladeFire(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, bool lit) {
    const BossSwordPoints sword = boss.swordSmoke ? BossSwordPoints{boss.swordInner, boss.swordInner, boss.swordOuter}
                                                  : BossPose::SwordPoints(boss, BossPose::Target(boss));
    if (lit && !boss.bladeWasLit && boss.igniteFlashRadius > 0.0f)
        BurnFieldRules::AddBlast(world, sword.outer, boss.igniteFlashRadius, BurnBlastKind::Hit);
    boss.bladeWasLit = lit;
    if (!lit || !(boss.bladeFuel > 0.0f))
        return;
    const Vec3 points[2] = {sword.outer, Mul(Add(sword.inner, sword.outer), 0.5f)};
    for (uint64_t i = 0; i < 2; ++i) {
        // 同じ番号で出し続けるので、剣が動くとエンジンが通った線に沿って燃料を広げる（炎が剣の跡を引く）。
        HIKARI::GameProjectFluidInteraction fuel{};
        fuel.kind = HIKARI::GameProjectFluidInteractionKind::Fuel;
        fuel.position = points[i];
        fuel.extents = {boss.bladeFuelRadius, std::max(0.0f, boss.bladeFuelTemperature), 0.0f};
        fuel.strength = boss.bladeFuel;
        fuel.channels = FluidChannels::kFire;
        boss.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), kBladeFuelBase + i, fuel).Accepted() && boss.fluidAccepted;
    }
}

void BossVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds) {
    UpdateSlashes(world, boss, seconds);
    UpdateRing(world, boss, seconds);
    // 当たった所の爆発の光（一度きりのエフェクト。入力は無い）。
    for (auto& blast : boss.blasts) {
        if (!blast.effectPending)
            continue;
        blast.effectPending = false;
        // 塊そのものは、場の火の域の流体で出す：剣が燃えている技で当てた所は火球、ほかは暗い煙の塊。
        BurnFieldRules::AddBlast(world, blast.position,
                                 (blast.puff ? boss.puffRadius : boss.hitBlastRadius) * blast.scale,
                                 blast.fire && !blast.puff ? BurnBlastKind::Hit : BurnBlastKind::Smoke);
        // その上に重ねるエフェクト（当たった所は光・衝撃の輪・火花）。
        const auto& asset = blast.puff ? boss.puffEffect : boss.blastEffect;
        if (asset.IsEmpty())
            continue;
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = asset;
        play.transform.position = blast.position;
        play.transform.scale = {blast.scale, blast.scale, blast.scale};
        play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
        boss.vfxAccepted = world.GetVfx().Play(play).IsValid() && boss.vfxAccepted;
    }
    // ナイフ：刃と光点のエフェクトを飛ばし、尾は場の霧へ霞を入れるエフェクトで出す。どちらもエフェクト側の速度で進める。
    for (size_t i = 0; i < boss.waves.size(); ++i) {
        const BossWave& wave = boss.waves[i];
        auto& handle = boss.waveVfx[i];
        auto& knife = boss.knifeVfx[i];
        if (!wave.active || boss.waveVfxId[i] != wave.id) {
            StopHandle(world, handle);
            StopHandle(world, knife);
        }
        if (!wave.active || !wave.knife || boss.waveVfxId[i] == wave.id)
            continue;
        boss.waveVfxId[i] = wave.id;
        const Vec3 velocity = Mul(wave.direction, wave.speed);
        HIKARI::GameProjectTransform transform{};
        transform.position = wave.position;
        transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        handle = Play(world, boss, boss.swordEffect, transform, boss.knifeEffectAmount, 0.0f);
        if (handle.IsValid())
            boss.vfxAccepted = world.GetVfx().SetVelocity(handle, velocity) && boss.vfxAccepted;
        if (boss.knifeEffect.IsEmpty())
            continue;
        // 刃のエフェクトには入力が無い（霞の尾の入力を渡すと、知らない入力として再生を断られる）。
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = boss.knifeEffect;
        play.transform.position = wave.position;
        play.transform.rotation = BasisRotation(wave.direction, wave.across);
        knife = world.GetVfx().Play(play);
        boss.vfxAccepted = knife.IsValid() && boss.vfxAccepted;
        if (knife.IsValid())
            boss.vfxAccepted = world.GetVfx().SetVelocity(knife, velocity) && boss.vfxAccepted;
    }
    // 剣先の霞：構えでは剣先に集まり、振っている間は剣先が弧を描く（動く注入は通った線に沿って広がる）。
    const bool swinging = boss.swordSmoke;
    const bool kamae = boss.glintFrames > 0 && boss.state == BossState::Act;
    // 火を残す技の間は、前隙の最初から剣が燃えている（技を出す前の 1 枚の画面で見分けられる）。
    // フェーズ 2 では、待機中も技の間も、剣がいつも燃えている（剣の斬撃がどれも火を残すようになる）。気絶の間は消える。
    const bool acting = boss.state == BossState::Act;
    const bool lit = (boss.bladeLit && acting) ||
                     (boss.lastPhaseNumber >= 2 && (acting || boss.state == BossState::Idle) && !boss.hidden);
    UpdateBladeFire(world, object, boss, lit);
    if (!swinging && !kamae && !lit) {
        StopHandle(world, boss.swordVfx);
        return;
    }
    HIKARI::GameProjectTransform transform{};
    transform.position = swinging ? boss.swordOuter : BossPose::SwordPoints(boss, BossPose::Target(boss)).outer;
    transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    const float amount = swinging ? boss.swordEffectAmount : boss.kamaeEffectAmount;
    const float ember = lit        ? std::max(boss.bladeLitEmber, boss.swordEmberScale)
                        : swinging ? boss.swordEmberScale
                                   : boss.kamaeEmberScale;
    if (!boss.swordVfx.IsValid()) {
        boss.swordVfx = Play(world, boss, boss.swordEffect, transform, amount, ember, lit);
        return;
    }
    HIKARI::GameProjectVfxUpdateRequest update{};
    update.updateTransform = 1u;
    update.updateParameters = 1u;
    update.transform = transform;
    update.parameters = Parameters(boss, amount, ember, lit);
    boss.vfxAccepted = world.GetVfx().Update(boss.swordVfx, update) && boss.vfxAccepted;
}

void BossVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    for (auto& handle : boss.waveVfx)
        StopHandle(world, handle);
    for (auto& handle : boss.knifeVfx)
        StopHandle(world, handle);
    StopHandle(world, boss.ringVfx);
    boss.ringVfxId = 0;
    for (auto& effect : boss.slashEffects) {
        StopHandle(world, effect.handle);
        effect = {};
    }
    StopHandle(world, boss.swordVfx);
}

} // namespace Project_TD2_2
