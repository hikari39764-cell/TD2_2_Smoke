#pragma once

#include <cstdint>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/ValueTypes.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kFlowType = "Project_TD2_2.Flow";
inline constexpr std::string_view kFlowSystem = "Project_TD2_2.FlowSystem";
// 世界変数の ID。ProjectSettings/runtime_bindings.json の宣言と同じ名前・型（Int）にする。値は FlowPhase の番号。
inline constexpr std::string_view kFightStateVariable = "Fight.State";

// 1 戦の流れの段階。値は Fight.State の値と同じ。
enum class FlowPhase { Waiting, Fighting, Lost, Won };

// 戦いの流れ（開始・負け・勝ち・やり直し）と、文字の無い試験用 HUD の設定。1 シーンに 1 つ置く。
class FlowComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    // 流れ
    float engageDistance = 18.0f;
    float fallDepth = 20.0f;
    float loseFadeSeconds = 2.0f;
    float loseRestartSeconds = 3.0f;
    float winFadeSeconds = 2.0f;
    float winRestartSeconds = 3.0f;
    // 負け・勝ちの画面の色（暗い赤・白）と、色が最も濃くなった時の不透明度。
    HIKARI::GameProjectFloat3 loseColor{0.3f, 0.0f, 0.0f};
    float loseOpacity = 0.9f;
    HIKARI::GameProjectFloat3 winColor{1.0f, 1.0f, 1.0f};
    float winOpacity = 0.9f;
    // HUD の置き場所と大きさ。参照の画面（1920 × 1080）の点。プレイヤーの 4 つは左下から上へ並ぶ。
    float playerBarX = 60.0f;
    float playerBarBottom = 1020.0f;
    float playerBarWidth = 420.0f;
    float healthBarHeight = 24.0f;
    float staminaBarHeight = 16.0f;
    float smokeBarHeight = 16.0f;
    float healSquareSize = 20.0f;
    float barGap = 10.0f;
    // 火弾の残り時間の細い条（煙の条のすぐ下。火弾の間だけ出す）の太さと、煙の条との間。
    float fireShotBarHeight = 5.0f;
    float fireShotBarGap = 3.0f;
    // ボスの HP の帯は画面の上の中央。
    float bossBarWidth = 960.0f;
    float bossBarHeight = 22.0f;
    float bossBarTop = 56.0f;
    // 条の色。底は半透明の深い灰色。
    HIKARI::GameProjectFloat3 backColor{0.08f, 0.08f, 0.09f};
    float backOpacity = 0.6f;
    HIKARI::GameProjectFloat3 healthColor{0.82f, 0.12f, 0.12f};
    // プレイヤーが燃えている間（Player.Burn が 0 より大きい間）の HP の色。消えたら healthColor へ戻る。
    HIKARI::GameProjectFloat3 burningHealthColor{1.0f, 0.5f, 0.1f};
    HIKARI::GameProjectFloat3 staminaColor{0.25f, 0.75f, 0.28f};
    HIKARI::GameProjectFloat3 healColor{0.85f, 0.85f, 0.55f};
    HIKARI::GameProjectFloat3 smokeColor{0.45f, 0.65f, 0.68f};
    // 火弾の間（Gourd.FireShot が 0 より大きい間）の煙の条と、残り時間の条の色。切れたら smokeColor へ戻る。
    HIKARI::GameProjectFloat3 fireSmokeColor{1.0f, 0.45f, 0.1f};
    // 満杯で吸おうとした時（Gourd.Blocked が 0 より大きくなった時）に煙の条が光る色と、元の色へ戻るまでの秒。
    HIKARI::GameProjectFloat3 smokeBlockedColor{1.0f, 1.0f, 1.0f};
    float smokeBlockedFlashSeconds = 0.3f;
    // ボスの HP の条はフェーズごとに 1 本（c-3e0ffa）。フェーズ 1 は灰白。1 本目が空になると、同じ条が火の橙で
    // 左から bossRefillSeconds 秒かけて満ちて戻る（Boss.PhaseSeconds で測る。変わり目の演出 c-8dd296 では、構えの拍
    // （1.8 秒）の始めから満ち始める）。
    HIKARI::GameProjectFloat3 bossPhaseOneColor{0.78f, 0.77f, 0.74f};
    HIKARI::GameProjectFloat3 bossPhaseTwoColor{1.0f, 0.42f, 0.08f};
    float bossRefillSeconds = 1.5f;

    // 実行時の状態は保存しない。
    FlowPhase phase = FlowPhase::Waiting;
    // いまの段階に入ってからの固定 tick 数。
    uint64_t phaseTicks = 0;
    uint64_t tick = 0;
    // 戦いの始まりのプレイヤーの高さ（落下の判定の基準）。
    bool startHeightKnown = false;
    float startHeight = 0.0f;
    bool restartRequested = false;
    bool restartRejectedLogged = false;
    // HUD の矩形を出せなかった警告を出したか（1 回だけ出す）。
    bool hudRejectedLogged = false;
    // 世界変数の書き込みが断られた警告を出したか。
    bool variableWriteWarned = false;
};

} // namespace Project_TD2_2
