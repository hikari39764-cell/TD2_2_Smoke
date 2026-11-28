#include "BossPhaseShiftSettings.h"

namespace Project_TD2_2 {

void BossPhaseShiftSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("フェーズの変わり目の演出（打ち退け → 太陽を掴む → 燃え上がる → 刀に火 → 一閃）", false))
        return;
    fields.Note(
        "1 本目の条が空になると、ボスが打ち退けられて座り込み、左手を太陽へ伸ばして掴みます。拳から火が回って胴の煙が"
        "炎に、目が橙赤に、場の色が変わり、立ち上がって刀を拭って火を移し、一閃して待機の構えへ戻ります。動きは骨格の体の"
        "クリップ、カメラと効果の時刻は演出の資産（事件段 Shift.<拍>）が受け持ちます。舞台の向きは太陽で決め、"
        "プレイヤーは演出の始めにボスの正面へ置き直します。演出の間、ボスは当たらず、プレイヤーは動けず当たりません。"
        "HP の条は一閃の所から満ちます。");
    fields.Bool("shiftEnabled", "演出をする（切ると、変わり目は phaseShiftFrames の間、待機のまま）", enabled);
    fields.Asset("shiftSequence", "演出の資産（時計・カメラ・事件段）", HIKARI::GameProjectAssetKind::Sequence,
                 sequence);
    fields.Text("shiftClip", "骨格の体に流すクリップ（模型の中の名前）", clip);
    fields.Float("shiftClipSeconds", "クリップの長さ (秒。資産に Anim.<クリップ> の段が無い時に使う)", clipSeconds,
                 0.1f, 120.0f);
    fields.Float("shiftPlaybackRate", "資産を流す速さ（1 で等速）", playbackRate, 0.1f, 4.0f);
    fields.Text("shiftStageSlot", "カメラが付く舞台の印（資産のバインディングの名前。骨格の体の物体を結ぶ）",
                stageSlot);
    fields.Bool("shiftFaceSun", "舞台の向きを太陽で決める（切るとプレイヤーのいる側を前にする）", faceSun);
    fields.Float("shiftSunStageYawDegrees",
                 "太陽が来る方位（ボスの正面からの度。正は左。演出の資産を作った値と同じに）", sunStageYawDegrees,
                 -180.0f, 180.0f);
    fields.Float("shiftStageEdgeMargin", "舞台の真ん中を場の縁からこれだけ内側へ寄せる (m)", stageEdgeMargin, 0.0f,
                 60.0f);
    fields.Bool("shiftPlacePlayer", "プレイヤーをボスの正面へ置き直す", placePlayer);
    fields.Float("shiftPlayerDistance", "…ボスの正面のこの距離 (m)", playerDistance, 2.0f, 60.0f);
    fields.Float("shiftPlayerArenaMargin", "…場の縁からの内側の余白 (m)", playerArenaMargin, 0.0f, 20.0f);
    fields.Float("shiftPlayerLift", "…置く高さ（置く所の地面から足元までの m。ここから落ちて地面に立つ。置く中心の高さは、これにプレイヤーの碰撞体の半分の高さを足した所）", playerLift, 0.0f, 5.0f);
    fields.Float("shiftSettleSeconds", "渡し：最後のカメラからゲームのカメラへ混ぜる長さ (秒)", settleSeconds, 0.0f,
                 5.0f);
    fields.Float("shiftCameraFocusHeight", "渡し：ゲームのカメラが見ておく、ボスの足元からの高さ (m)",
                 cameraFocusHeight, 0.0f, 12.0f);
    fields.Float("shiftClipBlendOutSeconds", "渡し：クリップから待機の姿へ混ぜる長さ (秒)", clipBlendOutSeconds, 0.0f,
                 2.0f);
    fields.Int("shiftCalmFrames", "演出の後、技を選ばない間 (F)", calmFrames, 0, 600);
    fields.Bool("shiftSkippable", "飛ばせる", skippable);
    fields.Text("shiftSkipAction", "飛ばす入力（行動 id）", skipAction);

    fields.Int("shiftHitStopFrames", "打ち退け：時間を止める長さ (F)", hitStopFrames, 0, 60);
    fields.Float("shiftHitStopScale", "打ち退け：止める間の時間の速さ", hitStopScale, 0.0f, 1.0f);
    fields.Float("shiftHitShake", "打ち退け：揺れ（0〜1）", hitShake, 0.0f, 2.0f);
    fields.Float("shiftHitBurst", "打ち退け：胴の煙が体から弾け出る速さ (m/秒)", hitBurst, 0.0f, 60.0f);
    fields.Float("shiftHitBurstRadius", "打ち退け：煙が弾ける半径 (m)", hitBurstRadius, 0.1f, 6.0f);
    fields.Float("shiftFallShake", "崩れ落ち：揺れ（0〜1）", fallShake, 0.0f, 2.0f);
    fields.Float("shiftFallBurst", "崩れ落ち：胴の煙が弾ける速さ (m/秒)", fallBurst, 0.0f, 60.0f);
    fields.Bool("shiftMotionBlur", "演出の間、カメラの動きのぼけを強める", motionBlur);
    fields.Float("shiftMotionBlurShutterDegrees", "動きのぼけ：シャッター角（度。180 が映画の標準）",
                 motionBlurShutterDegrees, 0.0f, 360.0f);
    fields.Float("shiftMotionBlurMaxPixels", "動きのぼけ：長さの上限（1080p の画素）", motionBlurMaxPixels, 0.0f,
                 64.0f);
    fields.Float("shiftLeakDensity", "座り込み：継ぎ目から漏れる黒い煙の濃さ（毎秒の密度。0 で出さない）", leakDensity,
                 0.0f, 5000.0f);
    fields.Float("shiftLeakRadius", "座り込み：黒い煙の半径（1 倍の体での m）", leakRadius, 0.02f, 2.0f);
    fields.Float("shiftLeakSpeed", "座り込み：黒い煙が外へ吹く速さ (m/秒)", leakSpeed, 0.0f, 20.0f);
    fields.Float("shiftSlumpSmoke", "座り込み：火が回るまで、胴の煙の量をこの割合まで薄くする（0〜1）", slumpSmoke,
                 0.0f, 1.0f);
    fields.Float("shiftReachStar", "太陽へ伸ばす：太陽の所の画面の星の明るさ", reachStar, 0.0f, 4.0f);
    fields.Float("shiftGrabShake", "掴む：揺れ（0〜1。小さく）", grabShake, 0.0f, 2.0f);
    fields.Float("shiftGrabShock", "掴む：光を拳へ吸い込む輪の強さ", grabShock, 0.0f, 4.0f);
    fields.Float("shiftGrabShockRadius", "掴む：吸い込む輪の出始めの大きさ（画面の高さを 1）", grabShockRadius, 0.05f,
                 3.0f);
    fields.Float("shiftGrabShockSeconds", "掴む：吸い込む長さ (秒)", grabShockSeconds, 0.05f, 2.0f);
    fields.Float("shiftGrabDim", "掴む：画が暗くなる強さ（0〜1）", grabDim, 0.0f, 1.0f);
    fields.Float("shiftGrabDimSeconds", "掴む：暗さが戻る長さ (秒)", grabDimSeconds, 0.05f, 4.0f);
    fields.Float("shiftFistFuel", "拳の火：燃料の強さ", fistFuel, 0.0f, 2000.0f);
    fields.Float("shiftFistFuelRadius", "拳の火：半径（1 倍の体での m）", fistFuelRadius, 0.02f, 2.0f);
    fields.Float("shiftFistFuelHeat", "拳の火：温度", fistFuelHeat, 0.0f, 20.0f);
    fields.Float("shiftSpreadFuel", "火が回る：腕を伝う火の燃料の強さ", spreadFuel, 0.0f, 2000.0f);
    fields.Float("shiftSpreadFire", "火が回る：胴の煙の火の強さを、Spread の終わりにここまで（0〜1）", spreadFire, 0.0f,
                 1.0f);
    fields.Float("shiftColumnFuel", "火が回る：胴の煙の柱を降りていく火の燃料の強さ（0 で出さない）", columnFuel, 0.0f,
                 2000.0f);
    fields.Float("shiftColumnFuelRadius", "火が回る：柱を降りる火の半径（1 倍の体での m）", columnFuelRadius, 0.02f,
                 2.0f);
    fields.Float("shiftColumnSustain",
                 "火が回る：回り切った後も、演出の終わりまで柱を燃やし続ける割合（0 で Spread と一緒に止む）",
                 columnSustain, 0.0f, 1.0f);
    fields.Float("shiftFlareFuel", "燃え上がり：火が胸に届いた瞬間に、胴へ一気に入れる燃料の強さ（0 で出さない）",
                 flareFuel, 0.0f, 5000.0f);
    fields.Float("shiftFlareRadius", "燃え上がり：燃料を入れる半径（1 倍の体での m）", flareRadius, 0.02f, 3.0f);
    fields.Float("shiftFlareSeconds", "燃え上がり：燃料を入れ続ける長さ (秒)", flareSeconds, 0.0f, 3.0f);
    fields.Float("shiftFlarePush", "燃え上がり：炎を外へ膨らませる速さ (m/秒。0 で押さない)", flarePush, 0.0f, 10.0f);
    fields.Float("shiftFlareShake", "燃え上がり：揺れの強さ", flareShake, 0.0f, 4.0f);
    fields.Float("shiftFlareEmbers", "燃え上がり：火の粉を何倍にするか", flareEmbers, 1.0f, 10.0f);
    fields.Float("shiftFlareEmbersSeconds", "燃え上がり：火の粉が元の量へ戻るまでの秒", flareEmbersSeconds, 0.0f, 6.0f);
    fields.Float("shiftFieldWarm", "場の色：Field の終わりにフェーズ 2 の色のこの割合まで（残りは一閃で）", fieldWarm,
                 0.0f, 1.0f);
    fields.Float("shiftFuseLead", "刀に火を移す：火の先が左手より先へ進む割合（1 で手の所）", fuseLead, 0.5f, 5.0f);
    fields.Float("shiftBladeFuel", "刀に火を移す：刀身の燃料の強さ", bladeFuel, 0.0f, 2000.0f);
    fields.Float("shiftBladeFuelRadius", "刀に火を移す：刀身の火の半径 (m)", bladeFuelRadius, 0.02f, 2.0f);
    fields.Float("shiftBladeFuelHeat", "刀に火を移す：刀身の火の温度", bladeFuelHeat, 0.0f, 20.0f);
    fields.Float("shiftWallDistance", "火の壁：舞台の足元から正面へこの距離 (m)", wallDistance, 1.0f, 30.0f);
    fields.Float("shiftWallHalfWidth", "火の壁：半分の幅 (m)", wallHalfWidth, 0.5f, 30.0f);
    fields.Float("shiftWallPaintWidth", "火の壁：燃える地面として塗る幅 (m。0 で塗らない)", wallPaintWidth, 0.0f, 6.0f);
    fields.Float("shiftWallRiseSeconds", "火の壁：真ん中から端へ広がる長さ (秒)", wallRiseSeconds, 0.05f, 4.0f);
    fields.Int("shiftWallPoints", "火の壁：炎を持ち上げる燃料の点の数（0 で出さない）", wallPoints, 0, 16);
    fields.Float("shiftWallFuel", "火の壁：燃料の強さ", wallFuel, 0.0f, 2000.0f);
    fields.Float("shiftWallFuelRadius", "火の壁：燃料の半径 (m)", wallFuelRadius, 0.1f, 4.0f);
    fields.Float("shiftWallFuelHeat", "火の壁：燃料の温度", wallFuelHeat, 0.0f, 20.0f);
    fields.Float("shiftWallFuelRise", "火の壁：上へ吹く速さ (m/秒)", wallFuelRise, 0.0f, 30.0f);
    fields.Float("shiftWallBurstRadius", "一閃：火の壁に沿って走る炎の塊の半径 (m。0 で出さない)", wallBurstRadius,
                 0.0f, 10.0f);
    fields.Float("shiftWallBurstSpeed", "一閃：炎の塊が壁に沿って走る速さ (m/秒)", wallBurstSpeed, 1.0f, 200.0f);
    fields.Float("shiftChargeEdge", "振りかぶり：刃口の光の強さ", chargeEdge, 0.0f, 10.0f);
    fields.Float("shiftChargeAura", "振りかぶり：体を包む気配（ゆがみ）の強さ", chargeAura, 0.0f, 10.0f);
    fields.Float("shiftChargeTension", "振りかぶり：画の張りつめ（0〜1）", chargeTension, 0.0f, 1.0f);
    fields.Bool("shiftSlashMono", "一閃：白黒の閃き", slashMono);
    fields.Float("shiftSlashShock", "一閃：衝撃の輪の強さ（1 つ目。0 で出さない）", slashShock, 0.0f, 10.0f);
    fields.Float("shiftSlashShockSecond", "一閃：遅れて出る衝撃の輪の強さ", slashShockSecond, 0.0f, 10.0f);
    fields.Float("shiftSlashGlint", "一閃：刀身を走る光の明るさ", slashGlint, 0.0f, 10.0f);
    fields.Int("shiftSlashHitStopFrames", "一閃：時間を止める長さ (F)", slashHitStopFrames, 0, 60);
    fields.Float("shiftSlashHitStopScale", "一閃：止める間の時間の速さ", slashHitStopScale, 0.0f, 1.0f);
    fields.Float("shiftSlashShake", "一閃：揺れ（0〜1）", slashShake, 0.0f, 2.0f);
    fields.Float("shiftSlashBlastRadius", "一閃：剣先の落ちた所の炎の塊の半径 (m。0 で出さない)", slashBlastRadius,
                 0.0f, 10.0f);
    fields.Int("shiftSlashBlastAlong", "一閃：刀身に沿って地面に並べる炎の塊の数", slashBlastAlong, 0, 8);
    fields.Int("shiftSlashBlastAhead", "一閃：剣先の先へ続ける炎の塊の数", slashBlastAhead, 0, 8);
    fields.Int("shiftSlashBlastWaves", "一閃：炎の塊を続けて出す回数", slashBlastWaves, 1, 8);
    fields.Float("shiftSlashBlastInterval", "一閃：続けて出す間 (秒)", slashBlastInterval, 0.01f, 0.5f);
    fields.Float("shiftRingRadius", "一閃：火の輪が広がり切る半径 (m。0 で出さない)", ringRadius, 0.0f, 40.0f);
    fields.Float("shiftRingSeconds", "一閃：火の輪が広がる長さ (秒)", ringSeconds, 0.05f, 3.0f);
    fields.Float("shiftRingFadeSeconds", "一閃：火の輪の光を下げて消す長さ (秒)", ringFadeSeconds, 0.05f, 3.0f);
    fields.Float("shiftRingThinGlow", "一閃：資産の細い輪の明るさ（0 で出さない。細い輪は地面の予告に見える）",
                 ringThinGlow, 0.0f, 1.0f);
    fields.Float("shiftFogPush", "一閃：地面の霧を外へ押す強さ (m/秒)", fogPush, 0.0f, 100.0f);
    fields.Float("shiftFogPushRadius", "一閃：霧を押す半径 (m)", fogPushRadius, 0.0f, 40.0f);
    fields.Float("shiftSkipQuenchSeconds", "飛ばした時：演出で入れた火を吸い消す長さ (秒。0 で消さない)",
                 skipQuenchSeconds, 0.0f, 2.0f);
    fields.Float("shiftSkipQuenchRadius", "飛ばした時：吸い消す球の半径（1 倍の体での m）", skipQuenchRadius, 0.05f,
                 2.0f);
    fields.Float("shiftSkipQuenchRate", "飛ばした時：消す速さ (1/秒)", skipQuenchRate, 0.0f, 1000.0f);
}

} // namespace Project_TD2_2
