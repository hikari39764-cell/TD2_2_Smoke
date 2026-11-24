#include "BossIntroSettings.h"

namespace Project_TD2_2 {

void BossIntroSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section(
            "登場（崩れた鎧が煙に起こされ、手甲と兜が付き、刀を抜く。動きを捕まえた演技を骨格の動画で流す）", false))
        return;
    fields.Note(
        "戦いの前、鎧は地面に崩れ、兜と手甲は離れて落ちています。戦いが始まると、役者（骨格と動画を持つ模型）が演技を"
        "演じ、済むと戦いの体に替わります。時計・カメラ・片段を流す時刻（事件段 Anim.<片段>。段の長さ＝流す長さ）・部品が"
        "飛ぶ時刻（Part.<部品>.Fly）・効果の拍は、演出の資産（Sequence）が受け持ちます。兜と手甲が落ちている所は、その"
        "物体の位置（シーンか、演出の資産の物体の轨）で決めます。登場の間、ボスは当たらず、動きません。");
    fields.Bool("introEnabled", "登場をする（切ると、初めから組み上がって立っている）", enabled);
    fields.Bool("introOncePerPlay", "登場は Play の間に 1 度だけ（やり直しの後は流さない）", oncePerPlay);
    fields.SceneObject("introActorObject", "役者の物体（骨格の動画で演じる体。無ければ登場をしない）", actorObject);
    fields.SceneObject("introHelmObject", "付く前の兜の物体", helmObject);
    fields.SceneObject("introGauntletRightObject", "付く前の右の手甲の物体", gauntletRightObject);
    fields.SceneObject("introGauntletLeftObject", "付く前の左の手甲の物体", gauntletLeftObject);
    fields.SceneObject("introKeyLightObject", "演出の間だけ点ける灯の物体（置き場所と明るさは演出の資産の物体の轨）",
                       keyLightObject);
    fields.Asset("introSequence", "演出の資産（時計・カメラ・片段・部品・効果の拍）",
                 HIKARI::GameProjectAssetKind::Sequence, sequence);
    fields.Float("introPlaybackRate", "演技を流す速さの倍率（1 で資産のまま）", playbackRate, 0.1f, 4.0f);
    fields.Float("introSettleSeconds", "演技が済んだ後、入力を止めたまま、担いだ刀を構えへ下ろす長さ (秒)",
                 settleSeconds, 0.0f, 10.0f);
    fields.Float("introSettleSwapSeconds",
                 "演技が済んでから、役者を戦いの体に替えるまでの秒（カメラが引いてから替える。0 ですぐ替える）",
                 settleSwapSeconds, 0.0f, 10.0f);
    fields.Float("introSettleLightFadeSeconds",
                 "演技が済んでから、演出の灯を消し切るまでの秒（役者を替える時には必ず 0。0 で替える時にすぐ消す）",
                 settleLightFadeSeconds, 0.0f, 10.0f);
    fields.Float("introMotionBlurShutter",
                 "演出の動きのぼかし：シャッター角（度。180 で映画の標準、大きいほど長く流れる。0 で掛けない）",
                 motionBlurShutterDegrees, 0.0f, 360.0f);
    fields.Float("introMotionBlurMaxPixels", "演出の動きのぼかし：ぼかしの長さの上限（1080p の画素）",
                 motionBlurMaxPixels, 0.0f, 64.0f);
    fields.Float("introGauntletFlyPower", "手甲が吸われる道：最後の速さ（大きいほど、初めゆっくりで最後に速い）",
                 gauntletFlyPower, 1.0f, 6.0f);
    fields.Float("introGauntletFlyLift", "手甲が吸われる道：途中で浮く高さ (m)", gauntletFlyLift, 0.0f, 5.0f);
    fields.Float("introHelmFlyBulge", "兜が持ち上がる道：途中で外へふくらむ幅 (m)", helmFlyBulge, 0.0f, 3.0f);
    fields.Bool("introCameraEnabled", "演出のカメラの表：出す（切るとゲームのカメラのまま）", cameraEnabled);
    fields.Text("introCameraPath", "演出のカメラの表：カットの表（Data/ からの場所）", cameraPath);
    fields.Float("introCameraFocusHeight", "演出のカメラの表：演出の間、ゲームのカメラが向くボスの高さ (m)",
                 cameraFocusHeight, 0.0f, 20.0f);
    fields.Text("introSkipAction", "飛ばす：登場の間にこの入力（行動 id）を押すと、構えへ進む（空なら飛ばせない）",
                skipAction);
    fields.Int("introTestStartFrames", "確かめ用：この tick に登場を始める（-1 で使わない。戦いの流れに従う）",
               testStartFrames, -1, 100000);

    fields.Float("introDrawSlowScale", "効果：スローの間の時間の倍率（変わり目の閃光も使う）", drawSlowScale, 0.05f,
                 1.0f);
    fields.Float("introEyesFlare", "効果：目の閃きの強さ（0 で出さない）", eyesFlare, 0.0f, 8.0f);
    fields.Vfx("introSlashFlashEffect", "効果：刀身の上で弾ける閃光のエフェクト（空なら出さない）", slashFlashEffect);
    fields.Float("introSlashFlashScale", "効果：閃光の大きさ（0 で出さない）", slashFlashScale, 0.0f, 10.0f);
    fields.Asset("introSlashMonoProfile", "効果：白黒の閃きの効果のプロファイル（BossImpactMono。空なら出さない）",
                 HIKARI::GameProjectAssetKind::PostProfile, slashMonoProfile);
    fields.Float("introSlashMonoInvertSeconds", "白黒の閃き：反転した白黒を出す長さ（実時間の秒）",
                 slashMonoInvertSeconds, 0.0f, 1.0f);
    fields.Float("introSlashMonoHoldSeconds", "白黒の閃き：反転しない白黒を出す長さ（実時間の秒）",
                 slashMonoHoldSeconds, 0.0f, 1.0f);
    fields.Float("introSlashMonoFadeSeconds", "白黒の閃き：色へ戻る長さ（実時間の秒）", slashMonoFadeSeconds, 0.0f,
                 2.0f);
    fields.Float("introSlashMonoStrength", "白黒の閃き：強さ（0 で出さない）", slashMonoStrength, 0.0f, 1.0f);
    fields.Float("introAccentShake", "手応え：強い拍のカメラの揺れ幅 (m。0 で揺らさない)", accentShake, 0.0f, 1.0f);
    fields.Int("introAccentShakeFrames", "手応え：揺れの長さ (F)", accentShakeFrames, 0, 120);
    fields.Float("introAccentShakeFrequency", "手応え：揺れの速さ", accentShakeFrequency, 1.0f, 60.0f);
    fields.Float("introAccentFovDegrees", "手応え：強い拍で画角を一瞬広げる量（度。0 で動かさない）", accentFovDegrees,
                 0.0f, 20.0f);
    fields.Float("introHitStopScale", "手応え：時間を止めている間の時間の速さ", hitStopScale, 0.0f, 1.0f);
    fields.Float("introAccentRumble", "手応え：振動の強さ（0 で出さない）", accentRumble, 0.0f, 1.0f);
    fields.Int("introAccentRumbleFrames", "手応え：振動の長さ (F)", accentRumbleFrames, 0, 120);

    fields.Vfx("introSmokeEffect", "煙：黒い煙の流体の域（BossIntroSmoke。空なら煙を出さない）", smokeEffect);
    fields.Vfx("introStreakEffect", "煙：地を走って鎧を起こす煙のエフェクト（BossIntroStreak。空なら出さない）",
               streakEffect);
    fields.Float("introStreakAmount", "煙：地を走る煙の量の倍率", streakAmount, 0.0f, 10.0f);
    fields.Float("introStreakLead", "煙：地を走る煙が、刀の向こう何 m から来るか", streakLead, 0.0f, 60.0f);
    fields.Float("introStreakSwerve", "煙：地を走る煙が左右へうねる幅 (m)", streakSwerve, -5.0f, 5.0f);
    fields.Float("introWrapAmount", "煙：鎧に着いた後、鎧を包む煙の量", wrapAmount, 0.0f, 5000.0f);
    fields.Float("introRiseAmount", "煙：鎧を引き起こす煙の柱の量", riseAmount, 0.0f, 5000.0f);
    fields.Float("introTendrilAmount", "煙：手甲が付く前、肩から伸びる煙の腕の量", tendrilAmount, 0.0f, 5000.0f);
    fields.Float("introArmAmount", "煙：手甲が付いた後、鎧と手甲をつなぐ細い煙の量", armAmount, 0.0f, 5000.0f);
    fields.Float("introPuffAmount", "煙：手甲が付いた瞬間に弾ける煙の量", puffAmount, 0.0f, 5000.0f);
    fields.Float("introAttachShake", "手甲が付いた瞬間のカメラの揺れの強さ（0 で揺らさない）", attachShake, 0.0f, 3.0f);
    fields.Float("introLiftAmount", "煙：兜を持ち上げる煙の柱の量", liftAmount, 0.0f, 5000.0f);
    fields.Float("introSeatBurstAmount", "兜が収まった瞬間：襟から噴く煙の量", seatBurstAmount, 0.0f, 10000.0f);
    fields.Float("introSeatShock", "兜が収まった瞬間：衝撃の輪の大きさ（0 で出さない）", seatShock, 0.0f, 20.0f);
    fields.Float("introSeatShake", "兜が収まった瞬間：カメラの揺れの強さ（1 以上で画角も広げる）", seatShake, 0.0f,
                 3.0f);
    fields.Int("introSeatHitStopFrames", "兜が収まった瞬間：時間を止める長さ (F)", seatHitStopFrames, 0, 60);
    fields.Float("introGrabRumble", "刀を握っている間：高まって行く画面の揺れの強さ（握り終わり。0 で揺らさない）",
                 grabRumble, 0.0f, 3.0f);
    fields.Float("introGrabTension", "刀を握っている間：画の縁が締まって色がずれる強さ（握り終わり。0 で出さない）",
                 grabTension, 0.0f, 1.0f);
    fields.Float("introDrawShockPost", "刀が抜けた瞬間：根元から画の上を走る衝撃の輪の強さ（0 で出さない）",
                 drawShockPost, 0.0f, 1.0f);
    fields.Float("introDrawShockPostSeconds", "刀が抜けた瞬間：衝撃の輪が画面を走り切る長さ (秒。実時間)",
                 drawShockPostSeconds, 0.05f, 3.0f);
    fields.Float("introDisperseBlow", "見得の終わり：黒い霧を主役から外へ流す加速 (m/秒²)", disperseBlow, 0.0f, 200.0f);
    fields.Float("introDisperseFogPush", "見得の終わり：場の霧を外へ押す速さ (m/秒)", disperseFogPush, 0.0f, 60.0f);
    fields.Float("introAmbientAmount", "序幕：部品の周りを巡る黒い煙の筋の量（1 秒に入れる総量）", ambientAmount, 0.0f,
                 5000.0f);
    fields.Float("introAmbientTurn", "序幕：煙の筋が走る速さ (m/秒。遅いと、その場に溜まった塊に見える)", ambientTurn,
                 0.0f, 12.0f);
    fields.Float("introAmbientRing", "序幕：煙の筋が巡る輪の大きさ（部品ごとの半径に掛ける倍率）", ambientRing, 0.1f,
                 3.0f);
    fields.Float("introAmbientHeat", "序幕：煙の温度（上向きの加速 m/秒²。高いほど立ちのぼる。0 で地面を這う）",
                 ambientHeat, 0.0f, 10.0f);
    fields.Float("introAmbientStir", "序幕：煙の輪を同じ向きへ回し続ける加速 (m/秒²)", ambientStir, 0.0f, 50.0f);
    fields.Float("introAmbientFogPush", "序幕：部品の周りの場の霧を押しのける速さ (m/秒。0 で押さない)", ambientFogPush,
                 0.0f, 20.0f);
    fields.Float("introAmbientFade", "序幕：煙の筋の尾を薄める速さ (1/秒。大きいほど尾が短い)", ambientFade, 0.0f,
                 20.0f);
    fields.Float("introOmenPull", "前触れ：鎧の周りへ寄った煙の渦を回す加速 (m/秒²。拍の終わり)", omenPull, 0.0f,
                 50.0f);
    fields.Float("introOmenDim", "前触れ：画が暗くなる強さ（0 で出さない）", omenDim, 0.0f, 1.0f);
    fields.Float("introOmenTension", "前触れ：画の縁が締まる強さ（0 で出さない）", omenTension, 0.0f, 1.0f);
    fields.Float("introOmenRumble", "前触れ：低く続く画面の揺れの強さ（拍の終わり。0 で揺らさない）", omenRumble, 0.0f,
                 3.0f);
    fields.Float("introRumbleFrequency", "続けて出す画面の揺れ（前触れ・刀を握る間）の速さ（1 秒の回数）",
                 rumbleFrequency, 0.5f, 30.0f);
    fields.Float("introHelmGlint", "序幕：落ちている兜の隙間の瞬きの明るさ（画面基準。0 で出さない）", helmGlint, 0.0f,
                 64.0f);
    fields.Float("introGatherAmount", "刀を握っている間：根元へ集まって来る黒い霧の量", gatherAmount, 0.0f, 5000.0f);
    fields.Float("introBackdropAmount", "見得：主役の後ろに立つ黒い霧と、前へ這い出す霧の量", backdropAmount, 0.0f,
                 5000.0f);
    fields.Float("introFinaleTension", "見得：画の縁が締まる強さ（0 で出さない）", finaleTension, 0.0f, 1.0f);
    fields.Float("introBladeEdgeGlow", "刀身の光（登場だけ）：刃口の細い線の明るさ（画面基準）", bladeEdgeGlow, 0.0f,
                 64.0f);
    fields.Float("introBladeStar", "刀身の光（登場だけ）：振り上げの頂点で剣先に出る星の明るさ（0 で出さない）",
                 bladeStar, 0.0f, 8.0f);
    fields.Vfx("introEyeFireEffect", "目の炎：兜の前に付く細かい流体の域（BossEyeFire。空なら出さない）",
               eyeFireEffect);
    fields.Float("introEyeFireAmount", "目の炎：面頬の隙間から立つ青白い炎の量（0 で出さない）", eyeFireAmount, 0.0f,
                 100.0f);
    fields.Float("introSeepAmount", "煙：刀を握っている間、根元から滲む煙の量", seepAmount, 0.0f, 5000.0f);
    fields.Float("introDrawBurstAmount", "刀が抜けた瞬間：噴き上げる黒い煙の量", drawBurstAmount, 0.0f, 10000.0f);
    fields.Float("introDrawBurstSeconds", "刀が抜けた瞬間：煙を噴く長さ (秒)", drawBurstSeconds, 0.0f, 3.0f);
    fields.Float("introDrawJetSpeed", "刀が抜けた瞬間：煙の柱が上がる速さ (m/秒)", drawJetSpeed, 0.0f, 60.0f);
    fields.Float("introDrawPush", "刀が抜けた瞬間：場の霧を外へ押す速さ (m/秒)", drawPush, 0.0f, 60.0f);
    fields.Float("introDrawPushRadius", "刀が抜けた瞬間：外へ押す力が届く半径 (m)", drawPushRadius, 0.5f, 40.0f);
    fields.Float("introDrawTrailAmount", "刀が抜けた瞬間：刀身が引く煙の量", drawTrailAmount, 0.0f, 5000.0f);
    fields.Float("introDrawShock", "刀が抜けた瞬間：衝撃の輪の大きさ（0 で出さない）", drawShock, 0.0f, 20.0f);
    fields.Float("introDrawShake", "刀が抜けた瞬間：カメラの揺れの強さ（1 以上で画角も広げる）", drawShake, 0.0f, 3.0f);
    fields.Int("introDrawHitStopFrames", "刀が抜けた瞬間：時間を止める長さ (F)", drawHitStopFrames, 0, 60);
    fields.Float("introStepShake", "刀へ歩く一歩：カメラの揺れの強さ（0 で揺らさない）", stepShake, 0.0f, 3.0f);
    fields.Float("introStepAmount", "刀へ歩く一歩：足元に立つ煙の量", stepAmount, 0.0f, 5000.0f);
    fields.Int("introDrawSlowFrames", "刀が抜けた瞬間：停止の後のスローの長さ (F。0 でスローにしない)", drawSlowFrames,
               0, 240);
    fields.Float("introDrawFlashScale", "刀が抜けた瞬間：根元で弾ける閃光の大きさ（0 で出さない）", drawFlashScale,
                 0.0f, 20.0f);
    fields.Float("introClearAbsorption", "煙：消す速さ (1/秒。拍 Smoke.Clear と演技の後)", clearAbsorption, 0.0f,
                 100.0f);
    fields.Float("introSmokeLingerSeconds", "煙：演技の後、消し続けてから域を止めるまでの長さ (秒)", smokeLingerSeconds,
                 0.0f, 10.0f);

    fields.Float3("introDrawRipGrip", "変わり目の抜刀：体の右の外へ振り出す所の握りの位置（1 倍の体での値）",
                  drawRipGrip);
    fields.Float3("introDrawRipTip", "変わり目の抜刀：体の右の外へ振り出す所の剣先の向き（局所）", drawRipTip);
    fields.Float3("introHighGrip", "変わり目の八相：握りの位置（1 倍の体での値）", highGrip);
    fields.Float3("introHighTip", "変わり目の八相：剣先の向き（局所）", highTip);
    fields.Float("introHighTwistDegrees", "変わり目の八相：体の捻り（度。正が左、負が右）", highTwistDegrees, -90.0f,
                 90.0f);
    fields.Float("introHighLeanDegrees", "変わり目の八相：体の前への傾き（度）", highLeanDegrees, -45.0f, 45.0f);
}

} // namespace Project_TD2_2
