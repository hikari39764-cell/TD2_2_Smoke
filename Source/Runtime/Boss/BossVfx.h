#pragma once

#include "HIKARI/Project/VfxView.h"
#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "../Common/VfxParameters.h"

// ボスの見た目（BossVisuals・BossSkyfallVisuals・BossGreatswordVisuals）が共通に使う、エフェクトを出す・送る・止める
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
// 霞の大剣のエフェクトの入力：柄から剣先へ満たす割合（0〜1）。出す量は amount。
inline constexpr const char* kFillInput = "input.fill";

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
inline HIKARI::GameProjectVfxPlaybackHandle Start(HIKARI::WorldView& world, BossComponent& boss,
                                                  const HIKARI::GameProjectVfxPlayRequest& play) {
    if (play.asset.IsEmpty())
        return {};
    const auto handle = world.GetVfx().Play(play);
    boss.vfxAccepted = handle.IsValid() && boss.vfxAccepted;
    return handle;
}

// 出して、エフェクト側の速度を与える（{} なら速度では動かさず、位置を毎フレーム送る）。
inline HIKARI::GameProjectVfxPlaybackHandle StartMoving(HIKARI::WorldView& world, BossComponent& boss,
                                                        const HIKARI::GameProjectVfxPlayRequest& play,
                                                        HIKARI::GameProjectFloat3 velocity) {
    const auto handle = Start(world, boss, play);
    if (handle.IsValid())
        boss.vfxAccepted = world.GetVfx().SetVelocity(handle, velocity) && boss.vfxAccepted;
    return handle;
}

// 出ているエフェクトへ、変換と入力を送る。
inline void Send(HIKARI::WorldView& world, BossComponent& boss, HIKARI::GameProjectVfxPlaybackHandle handle,
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
inline void StartOrSend(HIKARI::WorldView& world, BossComponent& boss, HIKARI::GameProjectVfxPlaybackHandle& handle,
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
inline const HIKARI::GameProjectVfxAssetReference& SlashAsset(const BossComponent& boss, bool fire) {
    return fire || boss.slashSmokeEffect.IsEmpty() ? boss.slashEffect : boss.slashSmokeEffect;
}

inline HIKARI::GameProjectVfxParameterOverrides SlashParameters(const BossSlashEffect& effect) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kFlyInput, effect.flying ? 1.0f : 0.0f);
    VfxParameters::SetFloat(parameters, kImpactInput, effect.impactSeconds > 0.0f ? 1.0f : 0.0f);
    VfxParameters::SetFloat(parameters, kGlowInput, effect.glow);
    return parameters;
}

// 斬撃のエフェクトを effect の位置・向き・大きさで出す。位置は毎フレーム送るので、エフェクト側の速度では動かさない。
inline void StartSlash(HIKARI::WorldView& world, BossComponent& boss, BossSlashEffect& effect,
                       const HIKARI::GameProjectVfxAssetReference& asset, uint32_t randomSeed) {
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = asset;
    play.transform = Placed(effect.position, effect.scale, effect.rotation);
    play.randomSeed = randomSeed;
    play.parameters = SlashParameters(effect);
    effect.handle = StartMoving(world, boss, play, {});
}

inline void SendSlash(HIKARI::WorldView& world, BossComponent& boss, const BossSlashEffect& effect) {
    Send(world, boss, effect.handle, Placed(effect.position, effect.scale, effect.rotation), SlashParameters(effect));
}

// 炎の輪のエフェクトの入力：広がっている間は燃料を出し（flying）、シェーダーの層を glow の明るさにする。
inline HIKARI::GameProjectVfxParameterOverrides RingParameters(bool flying, float glow) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kFlyInput, flying ? 1.0f : 0.0f);
    VfxParameters::SetColor(parameters, kFadeInput, {glow, glow, glow});
    return parameters;
}

} // namespace Project_TD2_2::BossVfx
