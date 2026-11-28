#pragma once

#include <cstdint>

#include "HIKARI/Game.h"
#include "HIKARI/Project/ValueTypes.h"

namespace Project_TD2_2 {

// ゲーム全体の値（Params）の ID。ProjectSettings/runtime_bindings.json の宣言と同じ名前・型（Int）にする。値は FlowPhase の番号。
inline constexpr std::string_view kFightStateVariable = "Fight.State";

// 1 戦の流れの段階。値は Fight.State の値と同じ。
enum class FlowPhase { Waiting, Fighting, Lost, Won };

// 戦いの流れ（開始・負け・勝ち・やり直し）と、文字の無い試験用 HUD の設定。1 シーンに 1 つ置く。
struct Flow : HIKARI::Component {
    HIKARI_COMPONENT(Flow, "戦いの流れと HUD")
    // 流れ
    FIELD(float, engageDistance, 18.0f, "戦いが始まる、プレイヤーとボスの水平の距離 (m)", HIKARI::Range(0.0f, 500.0f), HIKARI::Group("戦いの流れ"))
    FIELD(float, fallDepth, 20.0f, "戦いが始まる前のプレイヤーの高さから、これだけ落ちたら負け (m)", HIKARI::Range(1.0f, 10000.0f))
    FIELD(float, loseFadeSeconds, 2.0f, "負け：画面が暗くなりきるまで (秒)", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, loseRestartSeconds, 3.0f, "負け：やり直すまで (秒。負けてから)", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, winFadeSeconds, 2.0f, "勝ち：画面が明るくなりきるまで (秒)", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, winRestartSeconds, 3.0f, "勝ち：やり直すまで (秒。勝ってから)", HIKARI::Range(0.0f, 60.0f))
    // 負け・勝ちの画面の色（暗い赤・白）と、色が最も濃くなった時の不透明度。
    FIELD(HIKARI::Vec3, loseColor, (HIKARI::Vec3{0.3f, 0.0f, 0.0f}), "負けの画面の色")
    FIELD(float, loseOpacity, 0.9f, "負けの色が最も濃くなった時の不透明度", HIKARI::Range(0.0f, 1.0f))
    FIELD(HIKARI::Vec3, winColor, (HIKARI::Vec3{1.0f, 1.0f, 1.0f}), "勝ちの画面の色")
    FIELD(float, winOpacity, 0.9f, "勝ちの色が最も濃くなった時の不透明度", HIKARI::Range(0.0f, 1.0f))
    NOTE("やり直しは、いまのシーンを読み直します。戦いは、プレイヤーがボスへ近づく（または最初にボスへ当てる）"
            "まで始まらず、それまでボスは動きません。")
    // HUD の置き場所と大きさ。参照の画面（1920 × 1080）の点。プレイヤーの 4 つは左下から上へ並ぶ。
    FIELD(float, playerBarX, 60.0f, "プレイヤーの条の左端 X", HIKARI::Range(0.0f, 1920.0f), HIKARI::Group("試験用 HUD（文字なし）"))
    FIELD(float, playerBarBottom, 1020.0f, "プレイヤーの条の一番下の Y", HIKARI::Range(0.0f, 1080.0f))
    FIELD(float, playerBarWidth, 420.0f, "プレイヤーの条の長さ", HIKARI::Range(10.0f, 1920.0f))
    FIELD(float, healthBarHeight, 24.0f, "HP の条の太さ", HIKARI::Range(1.0f, 200.0f))
    FIELD(float, staminaBarHeight, 16.0f, "スタミナの条の太さ", HIKARI::Range(1.0f, 200.0f))
    FIELD(float, smokeBarHeight, 16.0f, "煙の量の条の太さ", HIKARI::Range(1.0f, 200.0f))
    FIELD(float, healSquareSize, 20.0f, "回復の残り回数の印の一辺", HIKARI::Range(1.0f, 200.0f))
    FIELD(float, barGap, 10.0f, "条と条の間", HIKARI::Range(0.0f, 100.0f))
    // 火弾の残り時間の細い条（煙の条のすぐ下。火弾の間だけ出す）の太さと、煙の条との間。
    FIELD(float, fireShotBarHeight, 5.0f, "火弾の残り時間の条の太さ（煙の条のすぐ下）", HIKARI::Range(1.0f, 100.0f))
    FIELD(float, fireShotBarGap, 3.0f, "…煙の条との間", HIKARI::Range(0.0f, 100.0f))
    // ボスの HP の帯は画面の上の中央。
    FIELD(float, bossBarWidth, 960.0f, "ボスの HP の条の長さ（画面の上の中央）", HIKARI::Range(10.0f, 1920.0f))
    FIELD(float, bossBarHeight, 22.0f, "ボスの HP の条の太さ", HIKARI::Range(1.0f, 200.0f))
    FIELD(float, bossBarTop, 56.0f, "ボスの HP の条の上端 Y", HIKARI::Range(0.0f, 1080.0f))
    // 条の色。底は半透明の深い灰色。
    FIELD(HIKARI::Vec3, backColor, (HIKARI::Vec3{0.08f, 0.08f, 0.09f}), "条の底の色")
    FIELD(float, backOpacity, 0.6f, "条の底の不透明度", HIKARI::Range(0.0f, 1.0f))
    FIELD(HIKARI::Vec3, healthColor, (HIKARI::Vec3{0.82f, 0.12f, 0.12f}), "HP の色")
    // プレイヤーが燃えている間（Player.Burn が 0 より大きい間）の HP の色。消えたら healthColor へ戻る。
    FIELD(HIKARI::Vec3, burningHealthColor, (HIKARI::Vec3{1.0f, 0.5f, 0.1f}), "燃えている間（炎上）の HP の色")
    FIELD(HIKARI::Vec3, staminaColor, (HIKARI::Vec3{0.25f, 0.75f, 0.28f}), "スタミナの色")
    FIELD(HIKARI::Vec3, healColor, (HIKARI::Vec3{0.85f, 0.85f, 0.55f}), "回復の残り回数の印の色")
    FIELD(HIKARI::Vec3, smokeColor, (HIKARI::Vec3{0.45f, 0.65f, 0.68f}), "煙の量の色")
    // 火弾の間（Gourd.FireShot が 0 より大きい間）の煙の条と、残り時間の条の色。切れたら smokeColor へ戻る。
    FIELD(HIKARI::Vec3, fireSmokeColor, (HIKARI::Vec3{1.0f, 0.45f, 0.1f}), "火弾の間の煙の量の色（残り時間の条も同じ色）")
    // 満杯で吸おうとした時（Gourd.Blocked が 0 より大きくなった時）に煙の条が光る色と、元の色へ戻るまでの秒。
    FIELD(HIKARI::Vec3, smokeBlockedColor, (HIKARI::Vec3{1.0f, 1.0f, 1.0f}), "満杯で吸おうとした時に煙の量の条が光る色")
    FIELD(float, smokeBlockedFlashSeconds, 0.3f, "…光ってから元の色へ戻るまで (秒、0 は光らない)", HIKARI::Range(0.0f, 5.0f))
    // ボスの HP の条はフェーズごとに 1 本（c-3e0ffa）。フェーズ 1 は灰白。1 本目が空になると、同じ条が火の橙で
    // 左から bossRefillSeconds 秒かけて満ちて戻る（Boss.PhaseSeconds で測る。変わり目の演出 c-8dd296 では、構えの拍
    // （1.8 秒）の始めから満ち始める）。
    FIELD(HIKARI::Vec3, bossPhaseOneColor, (HIKARI::Vec3{0.78f, 0.77f, 0.74f}), "ボスの HP の色（フェーズ 1 の条）")
    FIELD(HIKARI::Vec3, bossPhaseTwoColor, (HIKARI::Vec3{1.0f, 0.42f, 0.08f}), "ボスの HP の色（フェーズ 2 の条。満ちて戻る時も同じ色）")
    FIELD(float, bossRefillSeconds, 1.5f, "ボスの条：1 本目が空になった後、2 本目が左から満ちきるまで (秒。0 で一度に)", HIKARI::Range(0.0f, 10.0f))
    NOTE("値はゲーム全体の値（Params。Player.*・Gourd.*・Boss.*）から読みます。文字・数字・絵は出しません。"
            "回復の印は、残っている回数だけ並びます。"
            "火弾の間（Gourd.FireShot が 0 より大きい間）は、煙の条の色が変わり、すぐ下に残り時間の条が"
            "出て減っていきます（残り ÷ ヒョウタンの火弾の長さ）。"
            "満杯で吸おうとした時（Gourd.Blocked が 0 より大きくなった時）は、煙の条が光る色から"
            "元の色へ戻ります（吸い込みを始め直すたびに 1 回）。"
            "ボスの HP の条はフェーズごとに 1 本で、同じ場所に出ます（Boss.Phase と Boss.PhaseSeconds を読む）。"
            "ボスの変わり目の長さはボスの phaseShiftFrames です（条が満ちきる長さはそれより短くしておく）。")

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
};

} // namespace Project_TD2_2
