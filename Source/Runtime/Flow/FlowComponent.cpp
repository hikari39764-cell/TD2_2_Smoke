#include "FlowComponent.h"

namespace Project_TD2_2 {

void FlowComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("戦いの流れ", true)) {
        fields.Float("engageDistance", "戦いが始まる、プレイヤーとボスの水平の距離 (m)", engageDistance, 0.0f, 500.0f);
        fields.Float("fallDepth", "戦いが始まる前のプレイヤーの高さから、これだけ落ちたら負け (m)", fallDepth, 1.0f,
                     10000.0f);
        fields.Float("loseFadeSeconds", "負け：画面が暗くなりきるまで (秒)", loseFadeSeconds, 0.0f, 60.0f);
        fields.Float("loseRestartSeconds", "負け：やり直すまで (秒。負けてから)", loseRestartSeconds, 0.0f, 60.0f);
        fields.Float("winFadeSeconds", "勝ち：画面が明るくなりきるまで (秒)", winFadeSeconds, 0.0f, 60.0f);
        fields.Float("winRestartSeconds", "勝ち：やり直すまで (秒。勝ってから)", winRestartSeconds, 0.0f, 60.0f);
        fields.Color3("loseColor", "負けの画面の色", loseColor);
        fields.Float("loseOpacity", "負けの色が最も濃くなった時の不透明度", loseOpacity, 0.0f, 1.0f, 0.01f);
        fields.Color3("winColor", "勝ちの画面の色", winColor);
        fields.Float("winOpacity", "勝ちの色が最も濃くなった時の不透明度", winOpacity, 0.0f, 1.0f, 0.01f);
        fields.Note(
            "やり直しは、いまのシーンを読み直します。戦いは、プレイヤーがボスへ近づく（または最初にボスへ当てる）"
            "まで始まらず、それまでボスは動きません。");
    }
    if (fields.Section("試験用 HUD（文字なし）", true)) {
        fields.Float("playerBarX", "プレイヤーの条の左端 X", playerBarX, 0.0f, 1920.0f, 1.0f);
        fields.Float("playerBarBottom", "プレイヤーの条の一番下の Y", playerBarBottom, 0.0f, 1080.0f, 1.0f);
        fields.Float("playerBarWidth", "プレイヤーの条の長さ", playerBarWidth, 10.0f, 1920.0f, 1.0f);
        fields.Float("healthBarHeight", "HP の条の太さ", healthBarHeight, 1.0f, 200.0f, 1.0f);
        fields.Float("staminaBarHeight", "スタミナの条の太さ", staminaBarHeight, 1.0f, 200.0f, 1.0f);
        fields.Float("smokeBarHeight", "煙の量の条の太さ", smokeBarHeight, 1.0f, 200.0f, 1.0f);
        fields.Float("healSquareSize", "回復の残り回数の印の一辺", healSquareSize, 1.0f, 200.0f, 1.0f);
        fields.Float("barGap", "条と条の間", barGap, 0.0f, 100.0f, 1.0f);
        fields.Float("fireShotBarHeight", "火弾の残り時間の条の太さ（煙の条のすぐ下）", fireShotBarHeight, 1.0f, 100.0f,
                     1.0f);
        fields.Float("fireShotBarGap", "…煙の条との間", fireShotBarGap, 0.0f, 100.0f, 1.0f);
        fields.Float("bossBarWidth", "ボスの HP の条の長さ（画面の上の中央）", bossBarWidth, 10.0f, 1920.0f, 1.0f);
        fields.Float("bossBarHeight", "ボスの HP の条の太さ", bossBarHeight, 1.0f, 200.0f, 1.0f);
        fields.Float("bossBarTop", "ボスの HP の条の上端 Y", bossBarTop, 0.0f, 1080.0f, 1.0f);
        fields.Color3("backColor", "条の底の色", backColor);
        fields.Float("backOpacity", "条の底の不透明度", backOpacity, 0.0f, 1.0f, 0.01f);
        fields.Color3("healthColor", "HP の色", healthColor);
        fields.Color3("burningHealthColor", "燃えている間（炎上）の HP の色", burningHealthColor);
        fields.Color3("staminaColor", "スタミナの色", staminaColor);
        fields.Color3("healColor", "回復の残り回数の印の色", healColor);
        fields.Color3("smokeColor", "煙の量の色", smokeColor);
        fields.Color3("fireSmokeColor", "火弾の間の煙の量の色（残り時間の条も同じ色）", fireSmokeColor);
        fields.Color3("smokeBlockedColor", "満杯で吸おうとした時に煙の量の条が光る色", smokeBlockedColor);
        fields.Float("smokeBlockedFlashSeconds", "…光ってから元の色へ戻るまで (秒、0 は光らない)",
                     smokeBlockedFlashSeconds, 0.0f, 5.0f, 0.01f);
        fields.Color3("bossPhaseOneColor", "ボスの HP の色（フェーズ 1 の条）", bossPhaseOneColor);
        fields.Color3("bossPhaseTwoColor", "ボスの HP の色（フェーズ 2 の条。満ちて戻る時も同じ色）",
                      bossPhaseTwoColor);
        fields.Float("bossRefillSeconds", "ボスの条：1 本目が空になった後、2 本目が左から満ちきるまで (秒。0 で一度に)",
                     bossRefillSeconds, 0.0f, 10.0f, 0.05f);
        fields.Note(
            "値は物体変数（Player.*・Gourd.*・Boss.*）から読みます。文字・数字・絵は出しません。"
            "回復の印は、残っている回数だけ並びます。"
            "火弾の間（Gourd.FireShot が 0 より大きい間）は、煙の条の色が変わり、すぐ下に残り時間の条が"
            "出て減っていきます（残り ÷ ヒョウタンの火弾の長さ）。"
            "満杯で吸おうとした時（Gourd.Blocked が 0 より大きくなった時）は、煙の条が光る色から"
            "元の色へ戻ります（吸い込みを始め直すたびに 1 回）。"
            "ボスの HP の条はフェーズごとに 1 本で、同じ場所に出ます（Boss.Phase と Boss.PhaseSeconds を読む）。"
            "ボスの変わり目の長さはボスの phaseShiftFrames です（条が満ちきる長さはそれより短くしておく）。");
    }
}

} // namespace Project_TD2_2
