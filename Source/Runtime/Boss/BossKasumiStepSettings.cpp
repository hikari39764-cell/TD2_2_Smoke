#include "BossKasumiStepSettings.h"

namespace Project_TD2_2 {

void BossKasumiStepSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("霞歩（横っ飛び）とかわし", false))
        return;
    fields.Float("kasumiStepDistance", "跳ぶ距離 (m。技の表の「跳ぶ距離 m」が空欄の行)", distance, 0.5f, 20.0f);
    fields.Float("kasumiStepHeight", "跳ぶ弧の高さ (m。表の「跳ぶ高さ m」が空欄の行)", height, 0.0f, 5.0f);
    fields.Float("kasumiStepDiagonalDegrees", "斜め前・斜め後ろの角度（真横から前・後ろへ、度）", diagonalDegrees, 5.0f,
                 85.0f);
    fields.Float(
        "kasumiStepOrbit",
        "真横へ跳ぶ時、プレイヤーを中心に回る向きへ曲げる割合（1 で同じ距離に降りる。0 でまっすぐ横＝遠ざかる）", orbit,
        0.0f, 1.0f);
    fields.Float("kasumiStepEasePower", "跳び出しの鋭さ（1 で一定の速さ。大きいほど速く跳び出す）", easePower, 1.0f,
                 4.0f);
    fields.Float("kasumiStepTurnRate", "跳ぶ間にプレイヤーへ向き直る速さ (度/秒)", turnRate, 0.0f, 3600.0f);
    fields.Float("kasumiStepEdgeMargin", "戦場の縁からこれだけ内側に降りる (m)", edgeMargin, 0.0f, 10.0f);
    fields.Float("kasumiStepPlayerGap", "プレイヤーの中心からこれより近くへは降りない (m)", playerGap, 0.0f, 20.0f);
    fields.Float("kasumiStepFireClearance", "行き先のこの半径が燃えていたら跳ばない (m)", fireClearance, 0.0f, 10.0f);
    fields.Int("kasumiStepGuardFrames", "弾をかわす霞歩：跳び始めから当たらない (F。0 は体が動いた分だけでよける)",
               guardFrames, 0, 30);
    fields.Float("kasumiStepLandPush", "着地で足元の霧を押す大きさ（爆発の押しに対する倍。0 で押さない）", landPush,
                 0.0f, 4.0f);
    fields.Int("kasumiStepDodgeMax", "続けてかわせる回数", dodgeMax, 1, 10);
    fields.Float("kasumiStepDodgeChainSeconds", "続けたとみなす間 (秒。前のかわしからこれより空けば数え直す)",
                 dodgeChainSeconds, 0.0f, 60.0f);
    fields.Float("kasumiStepDodgeLockSeconds", "使い切った後、かわさない間 (秒)", dodgeLockSeconds, 0.0f, 60.0f);
    fields.Bool("kasumiStepRestWhileSpent", "かわさない間は様子見の霞歩も出さない", restWhileSpent);
    fields.Float("kasumiStepSpentSmoke", "かわさない間の胴の煙の量（ふだんに対する倍）", spentSmoke, 0.0f, 1.0f);
    fields.Note("霞歩の向き・跳ぶ F・着地の止まり（後隙）・様子見の後に何をするか（コンボ先と重み）は技の表の行"
                "（型 kasumiStep）。射撃をかわすか（確率・間隔・反応の遅れ）と、かわした後の反撃は行動ツリーの枝"
                "「射撃をかわす」。地面や画面に印は出さず、かわさない間は胴の煙が薄くなるだけです。");
}

} // namespace Project_TD2_2
