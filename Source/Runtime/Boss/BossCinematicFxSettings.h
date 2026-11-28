#pragma once

#include <array>
#include <string>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "../Player/Feedback.h"

namespace Project_TD2_2 {


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
