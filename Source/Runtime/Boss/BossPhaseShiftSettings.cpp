#include "BossPhaseShiftSettings.h"

namespace Project_TD2_2 {

void BossPhaseShiftSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("フェーズの変わり目の演出（崩れる → 内から燃える → 火の輪が場を払う → 新しい構え）", false))
        return;
    fields.Note(
        "1 本目の条が空になると、時間が一瞬止まります。鎧の継ぎ目から黒い煙が漏れ、胸の煙の中に火の粉が灯って内から"
        "火が回り、刀身に根元から火が灯ります。閃光とともに火の輪が場を払い（霧を押しのけ、プレイヤーを中距離まで"
        "押し出す。ダメージなし）、その閃光の中で鎧が灼けて場の色が変わります。最後に HP の条が満ちます。体は骨格の体が"
        "待機の姿で見せ、ここの値は効果の拍を決めます。演出の間、ボスは当たらず、プレイヤーは"
        "動けず当たりません。位置は、変わり目に入った時の足元から、プレイヤーのいる側を前にした局所（+X が左、"
        "+Z が前）。");
    fields.Bool("shiftEnabled", "演出をする（切ると、変わり目は phaseShiftFrames の間、待機のまま）", enabled);
    fields.Int("shiftCollapseFrames", "拍の長さ：崩れる (F)", collapseFrames, 1, 1200);
    fields.Int("shiftIgniteFrames", "拍の長さ：内から燃える (F)", igniteFrames, 1, 1200);
    fields.Int("shiftBurstFrames", "拍の長さ：閃光と火の輪 (F)", burstFrames, 1, 1200);
    fields.Int("shiftSettleFrames", "拍の長さ：収まる (F。HP の条はこの間に満ちる)", settleFrames, 1, 1200);
    fields.Int("shiftCalmFrames", "演出の後、技を選ばない間 (F)", calmFrames, 0, 600);
    fields.Bool("shiftSkippable", "飛ばせる", skippable);
    fields.Text("shiftSkipAction", "飛ばす入力（行動 id）", skipAction);

    fields.Int("shiftHitStopFrames", "崩：最後の当たりで時間を止める長さ (F)", hitStopFrames, 0, 60);
    fields.Float("shiftHitStopScale", "崩：止める間の時間の速さ", hitStopScale, 0.0f, 1.0f);
    fields.Float("shiftPlantShare", "崩：2 つ目の揺れを出す所（崩の中の割合）", plantShare, 0.1f, 1.0f);
    fields.Float("shiftPlantBuried", "刀身の火：届かない、剣先の側の長さ (m)", plantBuried, 0.0f, 4.0f);
    fields.Float("shiftLeakDensity", "継ぎ目から漏れる黒い煙：濃さ（中心の毎秒の密度。0 で出さない）", leakDensity,
                 0.0f, 5000.0f);
    fields.Float("shiftLeakRadius", "継ぎ目から漏れる黒い煙：半径（1 倍の体での m）", leakRadius, 0.02f, 2.0f);
    fields.Float("shiftLeakSpeed", "継ぎ目から漏れる黒い煙：外へ吹く速さ (m/秒)", leakSpeed, 0.0f, 20.0f);
    fields.Float("shiftChestFuel", "燃：胸の火の粉（燃料）の強さ", chestFuel, 0.0f, 1000.0f);
    fields.Float("shiftChestRadius", "燃：胸の火の粉の半径（1 倍の体での m）", chestRadius, 0.02f, 2.0f);
    fields.Float("shiftIgniteFire", "燃：胴の煙の火の強さを、燃の終わりにここまで上げる（0〜1）", igniteFire, 0.0f,
                 1.0f);
    fields.Float("shiftWipeStartShare", "燃：刀身に火が灯り始める所（燃の中の割合）", wipeStartShare, 0.0f, 1.0f);
    fields.Float("shiftWipeEndShare", "燃：刀身の火が剣先の側まで届く所（燃の中の割合）", wipeEndShare, 0.0f, 1.0f);
    fields.Float("shiftBladeFuel", "燃：刀身に灯る火（燃料）の強さ", bladeFuel, 0.0f, 1000.0f);
    fields.Float("shiftBladeFuelRadius", "燃：刀身の火の半径 (m)", bladeFuelRadius, 0.02f, 2.0f);
    fields.Float("shiftBladeFuelHeat", "燃：刀身の火の温度", bladeFuelHeat, 0.0f, 20.0f);
    fields.Float("shiftDrawShare", "爆：火の輪と閃光を出す所（爆の中の割合）", drawShare, 0.0f, 1.0f);
    fields.Float("shiftRipShare", "爆：閃光の後、刀光を出し終える所（爆の中の割合）", ripShare, 0.0f, 1.0f);
    fields.Float("shiftRingRadius", "火の輪：広がり切る半径 (m。0 で出さない)", ringRadius, 0.0f, 40.0f);
    fields.Float("shiftRingSeconds", "火の輪：広がる長さ (秒)", ringSeconds, 0.05f, 3.0f);
    fields.Float("shiftRingFadeSeconds", "火の輪：光を下げて消す長さ (秒)", ringFadeSeconds, 0.05f, 3.0f);
    fields.Float("shiftRingThinGlow", "火の輪：資産の細い輪（シェーダーの層）の明るさ（0 で出さない。太い炎の帯だけ）",
                 ringThinGlow, 0.0f, 1.0f);
    fields.Float("shiftSkipQuenchSeconds", "飛ばした時：演出で入れた火を吸い消す長さ (秒。0 で消さない)",
                 skipQuenchSeconds, 0.0f, 2.0f);
    fields.Float("shiftSkipQuenchRadius", "飛ばした時：吸い消す球の半径（1 倍の体での m）", skipQuenchRadius, 0.05f,
                 2.0f);
    fields.Float("shiftSkipQuenchRate", "飛ばした時：消す速さ (1/秒)", skipQuenchRate, 0.0f, 1000.0f);
    fields.Float("shiftFlashShock", "閃光：衝撃の輪の大きさ（1 つ目。0 で出さない）", flashShock, 0.0f, 10.0f);
    fields.Float("shiftFlashShockSecond", "閃光：遅れて出る衝撃の輪の大きさ", flashShockSecond, 0.0f, 10.0f);
    fields.Float("shiftFlashGlint", "閃光：刀身を走る光の明るさ", flashGlint, 0.0f, 10.0f);
    fields.Int("shiftFlashSlowFrames", "閃光：スローの長さ（実時間の F。倍率は introDrawSlowScale）", flashSlowFrames,
               0, 240);
    fields.Float("shiftFlashWarm", "閃光：場の色を一気に進める割合（残りは bodyFieldWarmSeconds で）", flashWarm, 0.0f,
                 1.0f);
    fields.Float("shiftFogPush", "火の輪：地面の霧を外へ押す強さ (m/秒)", fogPush, 0.0f, 100.0f);
    fields.Float("shiftFogPushRadius", "火の輪：霧を押す半径 (m)", fogPushRadius, 0.0f, 40.0f);
    fields.Float("shiftPushToDistance", "押し出し：ボスからこの距離まで押す (m。もう遠ければ押さない)", pushToDistance,
                 0.0f, 40.0f);
    fields.Float("shiftPushMaxDistance", "押し出し：押す距離の上限 (m)", pushMaxDistance, 0.0f, 40.0f);
    fields.Int("shiftPushFrames", "押し出し：押す長さ (F)", pushFrames, 1, 240);
    fields.Float("shiftPushEasePower", "押し出し：出だしの速さ（1 は等速、大きいほど初め速い）", pushEasePower, 1.0f,
                 6.0f);
    fields.Float("shiftPushArenaMargin", "押し出し：場の縁（Boss.arenaRadius）からこれだけ内側で止める (m)",
                 pushArenaMargin, 0.0f, 20.0f);
}

} // namespace Project_TD2_2
