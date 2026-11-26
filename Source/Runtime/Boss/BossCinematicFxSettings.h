#pragma once

#include <array>
#include <string>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "../Player/Feedback.h"

namespace Project_TD2_2 {

// 演出の効果の部品の既定（c-eb3b08）：登場（BossIntroEffects）とフェーズの変わり目の演出（BossPhaseShiftVisuals）が
// 共通で使う、閃光・白黒の閃き・揺れ・時間の停止・スロー・振動・目の閃き・刃口の光の値。項目の名前は、登場の部品に
// あった時のまま（場面に保存した値をそのまま読む）。
struct BossCinematicFxSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // スロー（Slow）の間の時間の倍率。
    float drawSlowScale = 0.35f;
    // 目の閃き（EyeFlare）の強さ。
    float eyesFlare = 2.0f;
    // 閃光のエフェクト（MistBurst）とその大きさ（0 で出さない）。
    HIKARI::GameProjectVfxAssetReference slashFlashEffect{"c7a41d6be2f0483a9d15b0e36f7a2c18",
                                                          "Assets/Vfx/BossMistBurst.hvfx"};
    float slashFlashScale = 2.0f;
    // 白黒の閃き（MonoFlash。全画面の効果。Assets/Post/BossImpactMono）：プロファイル（空なら出さない）、反転した白黒の
    // 長さ・反転しない白黒の長さ・色へ戻る長さ（実時間の秒。時間の停止の間も進む）、強さ（0〜1）。白と黒の境・硬さ・
    // 明るさは、効果の資産の項目で変える。
    std::string slashMonoProfile{"9e45ffde0ff4c6ee90345798c55b262f"};
    float slashMonoInvertSeconds = 0.035f;
    float slashMonoHoldSeconds = 0.05f;
    float slashMonoFadeSeconds = 0.12f;
    float slashMonoStrength = 1.0f;
    // 手応え（Shake）：強さ 1 の時のカメラの揺れ（揺れ幅 m・長さ F・速さ）、強さ 1 で画角を一瞬広げる量（度）、振動
    // （コントローラー）の強さと長さ (F)。時間を止める間（HitStop）の時間の速さ。
    float accentShake = 0.14f;
    int accentShakeFrames = 16;
    float accentShakeFrequency = 20.0f;
    float accentFovDegrees = 2.5f;
    float hitStopScale = 0.1f;
    float accentRumble = 0.6f;
    int accentRumbleFrames = 12;
    // 続けて出す画面の揺れ（KeepRumble。前触れ・刀を握る間）の速さ（1 秒の回数）。
    float rumbleFrequency = 6.0f;
    // 刀身の光（BladeEdge。登場と変わり目の演出の間だけ。戦いの間の刀の光は BossBlade のまま）：刃口の細い線の明るさ
    // （画面基準。握る間に高まり、抜けた後は弱く残る）。
    float bladeEdgeGlow = 6.0f;
};

// 演出の効果への頼みの口（登場と変わり目の演出が書き、BossCinematicFx::Feedback（全画面・揺れ・停止・閃光）と
// BossBodyAccents（気配・衝撃の輪・刀身の光・目）が出す）。ボスの部品が持つ（登場の部品が無くても変わり目の演出が使う）。
struct BossCinematicFxState {
    // 効果への頼み：体を包む気配の強さ、衝撃の輪（置く所・大きさ・出すまでの秒。大きさ 0 は頼んで
    // いない）、刀身を走る光の明るさ（0 は頼んでいない）、閃光（出すか・所・大きさ）。
    float aura = 0.0f;
    struct Shock {
        HIKARI::GameProjectFloat3 at{};
        float size = 0.0f;
        float delay = 0.0f;
    };
    std::array<Shock, 2> shocks{};
    float glint = 0.0f;
    bool flashPending = false;
    HIKARI::GameProjectFloat3 flashAt{};
    float flashScale = 0.0f;
    // 刀に付くエフェクトへの頼み（BossBodyAccents が読む。毎フレーム 0 に戻す）：刃口の光の強さ・刀光とゆがみを出すか。
    float bladeEdge = 0.0f;
    bool bladeSwing = false;
    // 目の閃きの頼み（強さ。BossBodyAccents が次に目の閃きを進める時に取り込んで 0 に戻す。BeginFrame では戻さない）。
    float eyeFlareRequest = 0.0f;
    // 白黒の閃き：次の描画のフレームで始める（強さ・長さ。0 は既定）、出している強さ・長さ、始めてからの実時間の秒（負は
    // 出していない）。
    bool monoPending = false;
    float monoStrength = 0.0f;
    float monoLength = 0.0f;
    float monoActiveStrength = 0.0f;
    float monoActiveLength = 0.0f;
    float monoSeconds = -1.0f;
    // 張りつめの頼み（このフレームだけ。拍が毎フレーム入れる）。
    float tensionPending = 0.0f;
    // 全画面の衝撃の輪と剣先の星の、このフレームの強さ（登場の効果が毎フレーム書く。0 なら出さない）：衝撃の輪の強さ・
    // 半径・中心（世界）、星の強さ・剣先の所（世界。読めなければ星は画面の上の真ん中）。
    float postShock = 0.0f;
    float postShockRadius = 0.0f;
    HIKARI::GameProjectFloat3 postShockAt{};
    float postStar = 0.0f;
    bool postStarPointValid = false;
    HIKARI::GameProjectFloat3 postStarPoint{};
    // 画を暗くする頼み（このフレームだけ。拍が毎フレーム入れる）と、いま掛けている強さ・張りつめの強さ（急に切り替えず、
    // 実時間で寄せる）。
    float dimPending = 0.0f;
    float dimNow = 0.0f;
    float tensionNow = 0.0f;
    // 続けて出す画面の揺れ：頼まれた強さ（このフレームだけ）と、揺れの位相（自分で進める）。
    float rumblePending = 0.0f;
    float rumblePhase = 0.0f;
    // 目の光へ足す明るさの倍率（このフレームだけ。BossIntroEffects が書き、BossBodyAccents が読む。0 は普段の明るさ）。
    float eyeGlow = 0.0f;
    // スローを次の描画のフレームで頼む F。手応え：頼まれた強さ（0〜1。描画のフレームで使ったら 0 に戻す）、時間を止める F、
    // カメラの揺れ。
    int slowPending = 0;
    float accentPending = 0.0f;
    int hitStopPending = 0;
    Feedback::CameraShake shake{};
};

} // namespace Project_TD2_2
