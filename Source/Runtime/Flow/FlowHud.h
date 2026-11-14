#pragma once

#include "HIKARI/Project/UiView.h"

#include "FlowComponent.h"

namespace Project_TD2_2 {

// HUD に出す値。物体変数から集めたものを渡す。
struct FlowHudValues {
    float playerHealth = 0.0f;
    float playerMaxHealth = 0.0f;
    float playerStamina = 0.0f;
    float playerMaxStamina = 0.0f;
    int healCharges = 0;
    float gourdSmoke = 0.0f;
    float gourdCapacity = 0.0f;
    float bossHealth = 0.0f;
    float bossMaxHealth = 0.0f;
};

// 文字の無い試験用 HUD。矩形（単色の画像）だけを毎フレーム出す。
class FlowHud final {
public:
    // プレイヤーの 4 つの条（HP・スタミナ・回復の残り・煙）。出せなかった矩形があれば false。
    static bool DrawPlayer(const HIKARI::UiView& ui, const FlowComponent& flow, const FlowHudValues& values);
    // ボスの HP の条（画面の上の中央）。
    static bool DrawBoss(const HIKARI::UiView& ui, const FlowComponent& flow, const FlowHudValues& values);
    // 画面全体の色（負け・勝ちの段階の fade が 0 を超えている間）。
    static bool DrawFade(const HIKARI::UiView& ui, const FlowComponent& flow, float fade);
};

} // namespace Project_TD2_2
