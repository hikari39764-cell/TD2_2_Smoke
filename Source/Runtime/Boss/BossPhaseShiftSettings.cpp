#include "BossPhaseShiftSettings.h"

namespace Project_TD2_2 {

void BossPhaseShiftSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("フェーズの変わり目の演出（崩れる → 内から燃える → 火の輪が場を払う → 新しい構え）", false))
        return;
    fields.Note(
        "1 本目の条が空になると、時間が一瞬止まり、よろめいて膝をつき、刀を地面に突き立てます。鎧の継ぎ目から黒い煙が"
        "漏れ、胸の煙の中に火の粉が灯って内から火が回り、左手が刀身を拭って刀が灯ります。立ち上がって刀を抜くと火の輪が"
        "場を払い（霧を押しのけ、プレイヤーを中距離まで押し出す。ダメージなし）、その閃光の中で鎧が灼けて場の色が"
        "変わります。最後にフェーズ 2 の構えに収まり、HP の条が満ちます。演出の間、ボスは当たらず、プレイヤーは"
        "動けず当たりません。位置は、変わり目に入った時の足元から、プレイヤーのいる側を前にした局所（+X が左、"
        "+Z が前）。");
    fields.Bool("shiftEnabled", "演出をする（切ると、変わり目は phaseShiftFrames の間、待機のまま）", enabled);
    fields.Int("shiftCollapseFrames", "拍の長さ：崩れる (F)", collapseFrames, 1, 1200);
    fields.Int("shiftIgniteFrames", "拍の長さ：内から燃える (F)", igniteFrames, 1, 1200);
    fields.Int("shiftBurstFrames", "拍の長さ：刀を抜いて火の輪 (F)", burstFrames, 1, 1200);
    fields.Int("shiftSettleFrames", "拍の長さ：新しい構えに収まる (F。HP の条はこの間に満ちる)", settleFrames, 1, 1200);
    fields.Int("shiftCalmFrames", "演出の後、技を選ばない間 (F)", calmFrames, 0, 600);
    fields.Bool("shiftSkippable", "飛ばせる", skippable);
    fields.Text("shiftSkipAction", "飛ばす入力（行動 id）", skipAction);
    fields.Bool("shiftCameraEnabled", "演出のカメラ：出す（切るとゲームのカメラのまま）", cameraEnabled);
    fields.Text("shiftCameraPath", "演出のカメラ：カットの表（Data/ からの場所）", cameraPath);
    fields.Float("shiftCameraFocusHeight", "演出のカメラ：演出の間、ゲームのカメラが向くボスの高さ (m)",
                 cameraFocusHeight, 0.0f, 20.0f);

    fields.Int("shiftHitStopFrames", "崩：最後の当たりで時間を止める長さ (F)", hitStopFrames, 0, 60);
    fields.Float("shiftHitStopScale", "崩：止める間の時間の速さ", hitStopScale, 0.0f, 1.0f);
    fields.Float("shiftStaggerDegrees", "崩：よろめいて反る角度（度。負が後ろ）", staggerDegrees, -45.0f, 45.0f);
    fields.Float("shiftStaggerShare", "崩：よろめき終える所（崩の中の割合）", staggerShare, 0.0f, 1.0f);
    fields.Float("shiftKneelSink", "崩：膝をついて腰を落とす量（立った高さに対する割合）", kneelSink, 0.0f, 0.6f);
    fields.Float("shiftKneelLeanDegrees", "崩：膝をついて前へ傾く角度（度）", kneelLeanDegrees, -30.0f, 60.0f);
    fields.Float("shiftKneelTwistDegrees", "崩：膝をついて刀の側へ捻る角度（度。負が右）", kneelTwistDegrees, -60.0f,
                 60.0f);
    fields.Float("shiftPlantShare", "崩：刀が地面に刺さる所（崩の中の割合）", plantShare, 0.1f, 1.0f);
    fields.Float("shiftFallShare", "崩：空中で空になった時、地面へ落ちる長さ（崩の中の割合）", fallShare, 0.05f, 1.0f);
    fields.Float3("shiftPlantGround", "突き立てた刀：剣先が刺さる地面の点（1 倍の体での局所 m）", plantGround);
    fields.Float("shiftPlantLeanDegrees", "突き立てた刀：柄が体の側へ傾く角度（度）", plantLeanDegrees, -45.0f, 45.0f);
    fields.Float("shiftPlantBuried", "突き立てた刀：地面に埋まる長さ (m)", plantBuried, 0.0f, 4.0f);
    fields.Float("shiftPlantEdgeYawDegrees", "突き立てた刀：刃が向く向き（局所の yaw。度。0 で前）",
                 plantEdgeYawDegrees, -180.0f, 180.0f);
    fields.Float("shiftLeakDensity", "継ぎ目から漏れる黒い煙：濃さ（中心の毎秒の密度。0 で出さない）", leakDensity,
                 0.0f, 5000.0f);
    fields.Float("shiftLeakRadius", "継ぎ目から漏れる黒い煙：半径（1 倍の体での m）", leakRadius, 0.02f, 2.0f);
    fields.Float("shiftLeakSpeed", "継ぎ目から漏れる黒い煙：外へ吹く速さ (m/秒)", leakSpeed, 0.0f, 20.0f);
    fields.Float("shiftChestFuel", "燃：胸の火の粉（燃料）の強さ", chestFuel, 0.0f, 1000.0f);
    fields.Float("shiftChestRadius", "燃：胸の火の粉の半径（1 倍の体での m）", chestRadius, 0.02f, 2.0f);
    fields.Float("shiftIgniteFire", "燃：胴の煙の火の強さを、燃の終わりにここまで上げる（0〜1）", igniteFire, 0.0f,
                 1.0f);
    fields.Float("shiftWipeReachShare", "燃：左手が刀身の根元に届く所（燃の中の割合）", wipeReachShare, 0.0f, 1.0f);
    fields.Float("shiftWipeStartShare", "燃：拭い始める所（燃の中の割合）", wipeStartShare, 0.0f, 1.0f);
    fields.Float("shiftWipeEndShare", "燃：先へ拭い終わる所（燃の中の割合）", wipeEndShare, 0.0f, 1.0f);
    fields.Float("shiftBladeFuel", "燃：拭った刀身に灯る火（燃料）の強さ", bladeFuel, 0.0f, 1000.0f);
    fields.Float("shiftBladeFuelRadius", "燃：刀身の火の半径 (m)", bladeFuelRadius, 0.02f, 2.0f);
    fields.Float("shiftBladeFuelHeat", "燃：刀身の火の温度", bladeFuelHeat, 0.0f, 20.0f);
    fields.Float("shiftDrawShare", "爆：刀が地面を離れる所（火の輪と閃光。爆の中の割合）", drawShare, 0.0f, 1.0f);
    fields.Float("shiftRipShare", "爆：八相に構え切る所（爆の中の割合）", ripShare, 0.0f, 1.0f);
    fields.Float("shiftRingRadius", "火の輪：広がり切る半径 (m。0 で出さない)", ringRadius, 0.0f, 40.0f);
    fields.Float("shiftRingSeconds", "火の輪：広がる長さ (秒)", ringSeconds, 0.05f, 3.0f);
    fields.Float("shiftRingFadeSeconds", "火の輪：光を下げて消す長さ (秒)", ringFadeSeconds, 0.05f, 3.0f);
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
