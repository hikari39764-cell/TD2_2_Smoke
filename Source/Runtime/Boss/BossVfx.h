#pragma once

#include <algorithm>

#include "HIKARI/Project/VfxView.h"
#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "../Common/VfxParameters.h"
#include "../Common/FieldValues.h"

// ボスの見た目（BossVisuals・体の BossBody*）が共通に使う、エフェクトを出す・送る・止める
// 小さな関数と、エフェクト資産の入力の名前。出す・送る関数は、受け付けられたかを boss.vfxAccepted へ合わせる。
namespace Project_TD2_2::BossVfx {

// 資産の入力（エフェクト側の契約）。
inline constexpr const char* kAmountInput = "input.amount";
inline constexpr const char* kColorInput = "input.color";
// 剣先の火の粉の光の大きさ（0 で出さない。ナイフの尾は出さない）。
inline constexpr const char* kEmberInput = "input.ember";
// 同じ光を青白で出す入力（霞の技の構え。火を残す技の間は ember の橙で出す）。
inline constexpr const char* kMistInput = "input.mist";
// 斬撃のエフェクトの入力：fly＝刃を出す量、impact＝当たって砕ける量。
inline constexpr const char* kFlyInput = "input.fly";
inline constexpr const char* kImpactInput = "input.impact";
// 光の層（刃口・炎・光・屈折）の大きさ。霧の源（fly）とは別に、数フレームかけて縮める。
inline constexpr const char* kGlowInput = "input.glow";
// 炎の輪のシェーダーの層の明るさ（Color。輪が消える時に 0 へ下げる）。
inline constexpr const char* kFadeInput = "input.fade";

// 位置・一様な大きさ・向き（既定は回さない）の変換。
inline HIKARI::GameProjectTransform Placed(HIKARI::GameProjectFloat3 position, float scale,
                                           HIKARI::GameProjectQuaternion rotation = {}) {
    HIKARI::GameProjectTransform transform{};
    transform.position = position;
    transform.rotation = rotation;
    transform.scale = {scale, scale, scale};
    return transform;
}

// 出す。資産が空なら出さずに無効な再生を返す（受け付けの印は変えない）。
inline HIKARI::GameProjectVfxPlaybackHandle Start(HIKARI::WorldView& world, Boss& boss,
                                                  const HIKARI::GameProjectVfxPlayRequest& play) {
    if (play.asset.IsEmpty())
        return {};
    const auto handle = world.GetVfx().Play(play);
    boss.vfxAccepted = handle.IsValid() && boss.vfxAccepted;
    return handle;
}

// 出して、エフェクト側の速度を与える（{} なら速度では動かさず、位置を毎フレーム送る）。
inline HIKARI::GameProjectVfxPlaybackHandle StartMoving(HIKARI::WorldView& world, Boss& boss,
                                                        const HIKARI::GameProjectVfxPlayRequest& play,
                                                        HIKARI::GameProjectFloat3 velocity) {
    const auto handle = Start(world, boss, play);
    if (handle.IsValid())
        boss.vfxAccepted = world.GetVfx().SetVelocity(handle, velocity) && boss.vfxAccepted;
    return handle;
}

// 出ているエフェクトへ、変換と入力を送る。
inline void Send(HIKARI::WorldView& world, Boss& boss, HIKARI::GameProjectVfxPlaybackHandle handle,
                 const HIKARI::GameProjectTransform& transform,
                 const HIKARI::GameProjectVfxParameterOverrides& parameters) {
    HIKARI::GameProjectVfxUpdateRequest update{};
    update.updateTransform = 1u;
    update.updateParameters = 1u;
    update.transform = transform;
    update.parameters = parameters;
    boss.vfxAccepted = world.GetVfx().Update(handle, update) && boss.vfxAccepted;
}

// 出ていなければ出し、出ていれば送る（出し続けるエフェクト）。
inline void StartOrSend(HIKARI::WorldView& world, Boss& boss, HIKARI::GameProjectVfxPlaybackHandle& handle,
                        const HIKARI::GameProjectVfxAssetReference& asset,
                        const HIKARI::GameProjectTransform& transform,
                        const HIKARI::GameProjectVfxParameterOverrides& parameters) {
    if (handle.IsValid()) {
        Send(world, boss, handle, transform, parameters);
        return;
    }
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = asset;
    play.transform = transform;
    play.parameters = parameters;
    handle = Start(world, boss, play);
}

inline void StopHandle(HIKARI::WorldView& world, HIKARI::GameProjectVfxPlaybackHandle& handle) {
    if (handle.IsValid())
        world.GetVfx().Stop(handle);
    handle = {};
}

// 斬撃のエフェクトの資産：火を残す一撃は炎の刃、残さない一撃は煙の刃（出た刃を見れば、地面が燃えるかどうかが分かる）。
inline HIKARI::GameProjectVfxAssetReference SlashAsset(const Boss& boss, bool fire) {
    return fire || !boss.slashSmokeEffect.IsSet() ? VfxPlayback::Reference(boss.slashEffect) : VfxPlayback::Reference(boss.slashSmokeEffect);
}

inline HIKARI::GameProjectVfxParameterOverrides SlashParameters(const BossSlashEffect& effect) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kFlyInput, effect.flying ? effect.fly : 0.0f);
    VfxParameters::SetFloat(parameters, kImpactInput, effect.impactSeconds > 0.0f ? 1.0f : 0.0f);
    VfxParameters::SetFloat(parameters, kGlowInput, effect.glow * (1.0f + effect.bite));
    return parameters;
}

// 斬撃のエフェクトを effect の位置・向き・大きさで出す。位置は毎フレーム送るので、エフェクト側の速度では動かさない。
inline void StartSlash(HIKARI::WorldView& world, Boss& boss, BossSlashEffect& effect,
                       const HIKARI::GameProjectVfxAssetReference& asset, uint32_t randomSeed) {
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = asset;
    play.transform = Placed(effect.position, effect.scale, effect.rotation);
    play.randomSeed = randomSeed;
    play.parameters = SlashParameters(effect);
    effect.handle = StartMoving(world, boss, play, {});
}

// 資産の向き：+X が飛ぶ向き、+Y が幅、+Z が厚み。pinch は、幅のほかの向きだけを細くする（引き込まれる刃が細い筋になる）。
inline void SendSlash(HIKARI::WorldView& world, Boss& boss, const BossSlashEffect& effect) {
    HIKARI::GameProjectTransform transform = Placed(effect.position, effect.scale, effect.rotation);
    transform.scale = {effect.scale * effect.pinch, effect.scale, effect.scale * effect.pinch};
    Send(world, boss, effect.handle, transform, SlashParameters(effect));
}

// 斬撃の刃を砕けさせる：刃を出すのをやめ、衝撃の入力を slashImpactSeconds 出し、fadeSeconds で消し、勢いを
// slashHitSpeedScale 倍に落とす（その場で散る）。この後は StepShattered で進める。
inline void Shatter(const Boss& boss, BossSlashEffect& effect, float fadeSeconds) {
    effect.flying = false;
    effect.impactSeconds = boss.slashImpactSeconds;
    effect.fadeSeconds = fadeSeconds;
    const float scale = boss.slashHitSpeedScale;
    effect.velocity = {effect.velocity.x * scale, effect.velocity.y * scale, effect.velocity.z * scale};
}

// 砕けた刃を 1 描画フレーム進める：光を下げ、残りの勢いで流し、消える時が来たら止めて空にする。
inline void StepShattered(HIKARI::WorldView& world, const Boss& boss, BossSlashEffect& effect, float seconds) {
    effect.fadeSeconds -= seconds;
    effect.glow = std::max(0.0f, effect.glow - seconds / std::max(0.001f, boss.slashGlowFadeSeconds));
    effect.impactSeconds = std::max(0.0f, effect.impactSeconds - seconds);
    effect.position = {effect.position.x + effect.velocity.x * seconds, effect.position.y + effect.velocity.y * seconds,
                       effect.position.z + effect.velocity.z * seconds};
    if (effect.fadeSeconds <= 0.0f) {
        StopHandle(world, effect.handle);
        effect = {};
    }
}

// 炎の輪のエフェクトの入力：広がっている間は燃料を出し（flying）、シェーダーの層を glow の明るさにする。
inline HIKARI::GameProjectVfxParameterOverrides RingParameters(bool flying, float glow) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kFlyInput, flying ? 1.0f : 0.0f);
    VfxParameters::SetColor(parameters, kFadeInput, {glow, glow, glow});
    return parameters;
}

} // namespace Project_TD2_2::BossVfx
