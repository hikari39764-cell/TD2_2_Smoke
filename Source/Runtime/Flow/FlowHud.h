#pragma once

#include "HIKARI/Project/UiView.h"

#include "FlowComponent.h"

namespace Project_TD2_2 {

// HUD に出す値。ゲーム全体の値（Params）から集めたものを渡す。
struct FlowHudValues {
    float playerHealth = 0.0f;
    float playerMaxHealth = 0.0f;
    float playerStamina = 0.0f;
    float playerMaxStamina = 0.0f;
    int healCharges = 0;
    // 炎上の残り秒数（燃えていなければ 0）。
    float playerBurn = 0.0f;
    float gourdSmoke = 0.0f;
    float gourdCapacity = 0.0f;
    // 火弾の残り秒数（火弾でなければ 0）と、火弾の長さ（残りの条の分母。ヒョウタンの fireShotDuration）。
    float fireShotSeconds = 0.0f;
    float fireShotDuration = 0.0f;
    // 満杯で吸えないまま吸い込みを続けている秒数（Gourd.Blocked。吸えている・吸っていない間は 0）。
    float gourdBlocked = 0.0f;
    float bossHealth = 0.0f;
    float bossMaxHealth = 0.0f;
    // ボスのいまのフェーズ（いまの条が何本目か）と、その条に入ってからの秒（Boss.Phase・Boss.PhaseSeconds）。
    int bossPhase = 1;
    float bossPhaseSeconds = 0.0f;
};

// 文字の無い試験用 HUD。矩形（単色の画像）だけを毎フレーム出す。
class FlowHud final {
public:
    // プレイヤーの 4 つの条（HP・スタミナ・回復の残り・煙）と、火弾の間だけ煙の下の残り時間の条。出せなかった矩形があれば false。
    static bool DrawPlayer(const HIKARI::UiView& ui, const Flow& flow, const FlowHudValues& values);
    // ボスの HP の条（画面の上の中央）。フェーズ 1 は灰白、フェーズ 2 は火の橙で、2 本目は左から満ちて戻る。
    static bool DrawBoss(const HIKARI::UiView& ui, const Flow& flow, const FlowHudValues& values);
    // 画面全体の色（負け・勝ちの段階の fade が 0 を超えている間）。
    static bool DrawFade(const HIKARI::UiView& ui, const Flow& flow, float fade);
};

} // namespace Project_TD2_2
