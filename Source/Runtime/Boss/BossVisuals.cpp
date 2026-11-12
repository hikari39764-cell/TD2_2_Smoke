#include "BossVisuals.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/VfxView.h"
#include "BossMath.h"
#include "BossPose.h"
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

HIKARI::GameProjectVfxParameterOverrides Parameters(const BossComponent& boss, float amount, float ember) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kAmountInput, amount);
    VfxParameters::SetFloat(parameters, kEmberInput, ember);
    VfxParameters::SetColor(parameters, kColorInput, boss.hazeColor);
    return parameters;
}

HIKARI::GameProjectVfxPlaybackHandle Play(HIKARI::WorldView& world, BossComponent& boss,
                                          const HIKARI::GameProjectVfxAssetReference& asset,
                                          const HIKARI::GameProjectTransform& transform, float amount, float ember) {
    if (asset.IsEmpty())
        return {};
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = asset;
    play.transform = transform;
    play.parameters = Parameters(boss, amount, ember);
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
        play.asset = boss.slashEffect;
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

constexpr float kTau = 6.28318530718f;

// 回し蹴りの霞の輪：輪の上に炎の筋を等間隔に並べ、輪と同じ速さで外へ飛ばす（流体域は使わない）。輪が消えたら止める。
void UpdateRing(HIKARI::WorldView& world, BossComponent& boss) {
    const BossWave* ring = nullptr;
    for (const auto& wave : boss.waves) {
        if (wave.active && wave.ring)
            ring = &wave;
    }
    if (ring == nullptr || ring->id != boss.ringVfxId) {
        for (auto& handle : boss.ringVfx)
            StopHandle(world, handle);
        boss.ringVfxId = 0;
    }
    if (ring == nullptr || ring->id == boss.ringVfxId || boss.ringEffect.IsEmpty())
        return;
    boss.ringVfxId = ring->id;
    for (size_t k = 0; k < boss.ringVfx.size(); ++k) {
        const float angle = kTau * static_cast<float>(k) / static_cast<float>(boss.ringVfx.size());
        const Vec3 out{std::sin(angle), 0.0f, std::cos(angle)};
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = boss.ringEffect;
        play.transform.position =
            Add({ring->position.x, boss.ground.y + boss.ringHeight, ring->position.z}, Mul(out, ring->radius));
        // 局所 +X が外向き、刃の面は地面に沿う（輪を上から見ても横から見ても筋が見える）。
        play.transform.rotation = BasisRotation(out, {out.z, 0.0f, -out.x});
        play.transform.scale = {boss.ringStreakScale, boss.ringStreakScale, boss.ringStreakScale};
        play.randomSeed = static_cast<uint32_t>(ring->id * 31u + k);
        auto& handle = boss.ringVfx[k];
        handle = world.GetVfx().Play(play);
        boss.vfxAccepted = handle.IsValid() && boss.vfxAccepted;
        if (handle.IsValid())
            boss.vfxAccepted = world.GetVfx().SetVelocity(handle, Mul(out, ring->speed)) && boss.vfxAccepted;
    }
}

} // namespace

void BossVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView, BossComponent& boss, float seconds) {
    UpdateSlashes(world, boss, seconds);
    UpdateRing(world, boss);
    // 当たった所の爆発の光（一度きりのエフェクト。入力は無い）。
    for (auto& blast : boss.blasts) {
        if (!blast.effectPending)
            continue;
        blast.effectPending = false;
        if (boss.blastEffect.IsEmpty())
            continue;
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = boss.blastEffect;
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
    if (!swinging && !kamae) {
        StopHandle(world, boss.swordVfx);
        return;
    }
    HIKARI::GameProjectTransform transform{};
    transform.position = swinging ? boss.swordOuter : BossPose::SwordPoints(boss, BossPose::Target(boss)).outer;
    transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    const float amount = swinging ? boss.swordEffectAmount : boss.kamaeEffectAmount;
    const float ember = swinging ? boss.swordEmberScale : boss.kamaeEmberScale;
    if (!boss.swordVfx.IsValid()) {
        boss.swordVfx = Play(world, boss, boss.swordEffect, transform, amount, ember);
        return;
    }
    HIKARI::GameProjectVfxUpdateRequest update{};
    update.updateTransform = 1u;
    update.updateParameters = 1u;
    update.transform = transform;
    update.parameters = Parameters(boss, amount, ember);
    boss.vfxAccepted = world.GetVfx().Update(boss.swordVfx, update) && boss.vfxAccepted;
}

void BossVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    for (auto& handle : boss.waveVfx)
        StopHandle(world, handle);
    for (auto& handle : boss.knifeVfx)
        StopHandle(world, handle);
    for (auto& handle : boss.ringVfx)
        StopHandle(world, handle);
    boss.ringVfxId = 0;
    for (auto& effect : boss.slashEffects) {
        StopHandle(world, effect.handle);
        effect = {};
    }
    StopHandle(world, boss.swordVfx);
}

} // namespace Project_TD2_2
