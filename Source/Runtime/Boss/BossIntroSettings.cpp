#include "BossIntroSettings.h"

namespace Project_TD2_2 {

void BossIntroSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("登場（立ち込めた黒い煙が、散らばった鎧に憑いて武者の姿になり、刀を抜く）", false))
        return;
    fields.Note(
        "戦いの前、鎧は地面に散らばり、刀は地面に刺さり、黒い煙が広く立ち込めて中央でとぐろを巻いています。戦いが"
        "始まると、周りの煙が中央へ流れ、煙の筋が鎧に憑いて引きずって集め、煙の中でゆるく形になり、しだいに"
        "締まって武者の姿になり、右手で刀を抜いて構えます。位置は、ボスの始めの足元から、プレイヤーのいる側を"
        "前にした局所 m（+X が左、+Z が前）。登場の間、ボスは当たらず、動きません。");
    fields.Bool("introEnabled", "登場をする（切ると、初めから組み上がって立っている）", enabled);
    fields.Bool("introOncePerPlay", "登場は Play の間に 1 度だけ（やり直しの後は流さない）", oncePerPlay);
    fields.Int("introGatherFrames", "煙が憑いて引き寄せる長さ (F)", gatherFrames, 1, 2400);
    fields.Int("introAssembleFrames", "煙の中で形になる長さ (F)", assembleFrames, 1, 2400);
    fields.Int("introDrawFrames", "刀を抜く長さ (F)", drawFrames, 1, 1200);
    fields.Int("introFlourishFrames", "一振りして構える長さ (F)", flourishFrames, 1, 1200);
    fields.Int("introSettleFrames", "構えてから動き出すまで (F)", settleFrames, 0, 1200);
    fields.Bool("introCameraEnabled", "演出のカメラ：出す（切るとゲームのカメラのまま）", cameraEnabled);
    fields.Text("introCameraPath", "演出のカメラ：カットの表（Data/ からの場所）", cameraPath);
    fields.Float("introCameraFocusHeight", "演出のカメラ：演出の間、ゲームのカメラが向くボスの高さ (m)",
                 cameraFocusHeight, 0.0f, 20.0f);
    fields.Text("introSkipAction", "飛ばす：登場の間にこの入力（行動 id）を押すと、構えへ進む（空なら飛ばせない）",
                skipAction);
    fields.Int("introDrawSlowFrames", "刀が地面を離れる瞬間のスロー：長さ（実時間の F。0 で短い停止）", drawSlowFrames,
               0, 240);
    fields.Float("introDrawSlowScale", "刀が地面を離れる瞬間のスロー：時間の倍率", drawSlowScale, 0.05f, 1.0f);
    fields.Float("introBarShare", "ボスの HP の条を出す所（一振りの段の中の割合。それまでは出さない）", barShare, 0.0f,
                 1.0f);
    fields.Int("introTestStartFrames", "確かめ用：この tick に登場を始める（-1 で使わない。戦いの流れに従う）",
               testStartFrames, -1, 100000);

    fields.Vfx("introHazeEffect", "立ち込める黒い煙のエフェクト（広い・低い流体域。空なら出さない）", hazeEffect);
    fields.Float("introHazeDrain", "立ち込める煙：中央へ流す速さ (m/秒。0 で流さない)", hazeDrain, 0.0f, 20.0f);
    fields.Float("introHazeDrainRadius", "立ち込める煙：中央へ流す力が届く半径 (m)", hazeDrainRadius, 1.0f, 60.0f);
    fields.Float("introHazeClearRate", "立ち上がった後、立ち込める煙を消す速さ（毎秒の割合。大きいほど早い）",
                 hazeClearRate, 0.0f, 50.0f);
    fields.Float("introCoilClearRate", "立ち上がった後、とぐろの輪を消す速さ（毎秒の割合）", coilClearRate, 0.0f,
                 50.0f);
    fields.Float("introHazeFadeSeconds", "立ち込める煙：登場が済んでから域を止めるまで (秒)", hazeFadeSeconds, 0.0f,
                 60.0f);

    fields.Vfx("introCoilEffect", "とぐろの黒い煙のエフェクト（世界に固定の流体域。空なら出さない）", coilEffect);
    fields.Int("introCoilStreams", "とぐろ：巡る頭の数", coilStreams, 0, kBossIntroCoilCapacity);
    fields.Float("introCoilRadius", "とぐろ：いちばん下の輪の半径 (m)", coilRadius, 0.2f, 10.0f);
    fields.Float("introCoilRadiusStep", "とぐろ：上の輪ほど半径を変える量 (m。負で、上ほどすぼまる)", coilRadiusStep,
                 -5.0f, 5.0f);
    fields.Float("introCoilRadiusWave", "とぐろ：半径のうねり (m)", coilRadiusWave, 0.0f, 3.0f);
    fields.Float("introCoilHeight", "とぐろ：いちばん下の輪の高さ (m)", coilHeight, 0.0f, 6.0f);
    fields.Float("introCoilHeightStep", "とぐろ：上の輪ほど高くする量 (m)", coilHeightStep, 0.0f, 4.0f);
    fields.Float("introCoilHeightWave", "とぐろ：上下のうねり (m)", coilHeightWave, 0.0f, 3.0f);
    fields.Float("introCoilTiltDegrees", "とぐろ：輪の傾き（度。帯どうしが斜めに交わる）", coilTiltDegrees, 0.0f,
                 45.0f);
    fields.Float("introCoilTiltTurnDegrees", "とぐろ：傾く向きがゆっくり回る速さ (度/秒)", coilTiltTurnDegrees, -90.0f,
                 90.0f);
    fields.Float("introCoilSpeed", "とぐろ：頭が巡る速さ (m/秒)", coilSpeed, 0.0f, 20.0f);
    fields.Float("introCoilEmitRadius", "とぐろ：頭が煙を入れる半径 (m)", coilEmitRadius, 0.1f, 3.0f);
    fields.Float("introCoilDensity", "とぐろ：頭が入れる煙の濃さ（中心の毎秒の密度）", coilDensity, 0.0f, 10000.0f);
    fields.Float("introCoilHeat", "とぐろ：頭の温度（内側の暗い赤の光。0 で光らない）", coilHeat, 0.0f, 20.0f);
    fields.Float("introCoilCarry", "とぐろ：頭の速さを新しい煙に持たせる割合", coilCarry, 0.0f, 2.0f);
    fields.Float("introCoilFlicker", "とぐろ：頭が入れる煙の濃さのむら（0 で一様）", coilFlicker, 0.0f, 1.0f);
    fields.Float("introCoilRearSeconds", "鎌首：持ち上がって沈むまで (秒)", coilRearSeconds, 0.1f, 30.0f);
    fields.Float("introCoilRearEverySeconds", "鎌首：次に持ち上がるまでの間隔 (秒。頭ごとにずれる)",
                 coilRearEverySeconds, 0.5f, 120.0f);
    fields.Float("introCoilRearHeight", "鎌首：持ち上がる高さ (m)", coilRearHeight, 0.0f, 8.0f);
    fields.Float("introGatherRadius", "引き寄せる：頭が寄る先の半径 (m)", gatherRadius, 0.1f, 10.0f);
    fields.Float("introGatherHeight", "引き寄せる：頭が昇る高さ (m)", gatherHeight, 0.0f, 7.0f);
    fields.Float("introGatherSpeedScale", "引き寄せる：巡りが速まる倍率", gatherSpeedScale, 0.1f, 10.0f);
    fields.Float("introCoilFrontShare",
                 "とぐろ：頭がプレイヤーの側を通る間の煙の量（後ろに対する割合。小さいほど前が開く）", coilFrontShare,
                 0.0f, 1.0f);
    fields.Float("introMantleBack", "引き寄せる：頭が寄る先を、ボスの後ろへずらす量 (m)", mantleBack, 0.0f, 8.0f);
    fields.Float("introGatherCoilShare", "引き寄せる：引き寄せ切った時の、とぐろの煙の量（待つ間に対する割合）",
                 gatherCoilShare, 0.0f, 1.0f);
    fields.Float("introGatherPull", "引き寄せる：とぐろの煙を中央へ吸い寄せる速さ (m/秒。0 で吸わない)", gatherPull,
                 0.0f, 30.0f);
    fields.Float("introGatherPullRadius", "引き寄せる：吸い寄せが届く半径 (m)", gatherPullRadius, 0.5f, 30.0f);
    fields.Float("introGatherPullHeight", "引き寄せる：吸い寄せる点の高さ (m)", gatherPullHeight, 0.0f, 8.0f);
    fields.Float("introCoilHandoffShare", "形になる段のこの割合の間に、とぐろの煙を出し終える（胴の煙へ引き継ぐ）",
                 coilHandoffShare, 0.01f, 1.0f);
    fields.Float("introCoilFadeSeconds", "とぐろの煙を出し終えてから、域を止めるまで (秒)", coilFadeSeconds, 0.0f,
                 60.0f);

    fields.Float3("introSwordGround", "刀：剣先が刺さる地面の点", swordGround);
    fields.Float("introSwordLeanYawDegrees", "刀：柄が傾く向き（局所の yaw。度）", swordLeanYawDegrees, -360.0f,
                 360.0f);
    fields.Float("introSwordLeanDegrees", "刀：傾き（度。0 でまっすぐ立つ）", swordLeanDegrees, 0.0f, 80.0f);
    fields.Float("introSwordBuried", "刀：地面に埋まる長さ (m)", swordBuried, 0.0f, 4.0f);
    fields.Float("introSwordEdgeYawDegrees", "刀：刃が向く向き（局所の yaw。度）", swordEdgeYawDegrees, -360.0f,
                 360.0f);
    fields.Float3("introTorsoRest", "胴：地面に置いてある位置", torsoRest);
    fields.Float3("introTorsoRestTurn", "胴：置いてある向き（X・Y・Z まわりの度）", torsoRestTurn);
    fields.Float3("introHeadRest", "兜：地面に置いてある位置", headRest);
    fields.Float3("introHeadRestTurn", "兜：置いてある向き（X・Y・Z まわりの度）", headRestTurn);
    fields.Float3("introSodeRestRight", "右の大袖：地面に置いてある位置", sodeRestRight);
    fields.Float3("introSodeRestRightTurn", "右の大袖：置いてある向き（X・Y・Z まわりの度）", sodeRestRightTurn);
    fields.Float3("introSodeRestLeft", "左の大袖：地面に置いてある位置", sodeRestLeft);
    fields.Float3("introSodeRestLeftTurn", "左の大袖：置いてある向き（X・Y・Z まわりの度）", sodeRestLeftTurn);
    fields.Float3("introCuffRestRight", "右の籠手：地面に置いてある位置", cuffRestRight);
    fields.Float3("introCuffRestRightTurn", "右の籠手：置いてある向き（X・Y・Z まわりの度）", cuffRestRightTurn);
    fields.Float3("introCuffRestLeft", "左の籠手：地面に置いてある位置", cuffRestLeft);
    fields.Float3("introCuffRestLeftTurn", "左の籠手：置いてある向き（X・Y・Z まわりの度）", cuffRestLeftTurn);
    fields.Float("introTassetRestRadiusMin", "草摺：散らす半径の内側 (m)", tassetRestRadiusMin, 0.0f, 15.0f);
    fields.Float("introTassetRestRadiusMax", "草摺：散らす半径の外側 (m)", tassetRestRadiusMax, 0.0f, 15.0f);
    fields.Float("introTassetRestHeight", "草摺：地面からの高さ (m)", tassetRestHeight, 0.0f, 2.0f);
    fields.Int("introTassetSeed", "草摺：散らし方の種（変えると並びが変わる）", tassetSeed, 0, 9999);

    fields.Float("introPossessFirstShare", "憑く：最初の部品（胴）に煙の筋が届く割合（引き寄せる段の中で）",
                 possessFirstShare, 0.0f, 1.0f);
    fields.Float("introPossessLastShare", "憑く：最後の部品（兜）に煙の筋が届く割合", possessLastShare, 0.0f, 1.0f);
    fields.Float("introTendrilShare", "憑く：煙の筋が中央から部品へ這う長さ（割合）", tendrilShare, 0.01f, 1.0f);
    fields.Float("introPossessLift", "憑く：憑かれた部品が浮く高さ (m)", possessLift, 0.0f, 2.0f);
    fields.Float("introDragWobble", "引きずる：横へふらつく幅 (m)", dragWobble, 0.0f, 3.0f);
    fields.Float("introDragCarryHeight", "引きずる：運ばれる高さ (m)", dragCarryHeight, 0.0f, 3.0f);
    fields.Float("introPileRadiusMin", "引きずる：中央で溜まる所の半径の内側 (m)", pileRadiusMin, 0.0f, 8.0f);
    fields.Float("introPileRadiusMax", "引きずる：中央で溜まる所の半径の外側 (m)", pileRadiusMax, 0.0f, 8.0f);
    fields.Float("introPieceSmokeRadius", "憑いた部品が出す煙：半径 (m)", pieceSmokeRadius, 0.05f, 3.0f);
    fields.Float("introPieceSmokeDensity", "憑いた部品が出す煙：濃さ（0 で出さない）", pieceSmokeDensity, 0.0f,
                 10000.0f);
    fields.Float("introPieceSmokeHeat", "憑いた部品が出す煙：温度（隙間の暗い赤の光。0 で光らない）", pieceSmokeHeat,
                 0.0f, 20.0f);
    fields.Float("introLooseOffset", "形になる：浮き上がった部品の、付く所からのずれ (m)", looseOffset, 0.0f, 3.0f);
    fields.Float("introLooseTiltDegrees", "形になる：浮き上がった部品の傾き（度）", looseTiltDegrees, 0.0f, 90.0f);
    fields.Float("introLooseAfterBurst", "形になる：立ち上がった直後に残るゆるさ（0〜1）", looseAfterBurst, 0.0f, 1.0f);
    fields.Float("introLooseSettleEndShare", "形になる：締まり切る割合", looseSettleEndShare, 0.0f, 1.0f);
    fields.Float("introSlumpHeight", "立ち上がる：うずくまった始めの高さ（立った高さに対する割合）", slumpHeight, 0.1f,
                 1.0f);
    fields.Float("introSlumpLean", "立ち上がる：うずくまっている間、前へのめる量 (m)", slumpLean, 0.0f, 4.0f);
    fields.Float("introSlumpPitchDegrees", "立ち上がる：うずくまっている間、前へ傾く角度（度）", slumpPitchDegrees,
                 0.0f, 80.0f);
    fields.Float("introRiseStartShare", "立ち上がる：立ち上がり始める割合（形になる段の中で）", riseStartShare, 0.0f,
                 1.0f);
    fields.Float("introRiseEndShare", "立ち上がる：立ち上がり切る割合", riseEndShare, 0.0f, 1.0f);
    fields.Float("introBurstOvershoot", "立ち上がる：勢いで立った高さを行き過ぎる量（割合。すぐ戻る）", burstOvershoot,
                 0.0f, 0.5f);
    fields.Float("introBurstFollowScale", "立ち上がる：その間に、ばねを硬くする倍率（一気に引き締まる）",
                 burstFollowScale, 1.0f, 10.0f);
    fields.Float("introEyesFlare", "演出：目が灯る瞬間の閃きの強さ（0 で出さない）", eyesFlare, 0.0f, 8.0f);
    fields.Float("introDrawShock", "演出：刀が地面を離れる瞬間の衝撃の輪の大きさ（0 で出さない）", drawShock, 0.0f,
                 10.0f);
    fields.Float("introDrawGlint", "演出：刀が地面を離れる瞬間に刀身を走る光の明るさ（0 で出さない）", drawGlint, 0.0f,
                 8.0f);
    fields.Float("introFollowFrequency", "部品が煙に付いて行くばね：1 秒あたりの揺れの回数（小さいほど遅れる）",
                 followFrequency, 0.2f, 10.0f);
    fields.Float("introFollowDamping", "部品が煙に付いて行くばね：収まりの強さ（1 で行き過ぎない）", followDamping,
                 0.1f, 2.0f);
    fields.Float("introTurnSeconds", "部品の向きが追う時間 (秒)", turnSeconds, 0.01f, 3.0f);
    fields.Float("introHeadLookDegrees", "兜：持ち上がる間に左右を見回す角度（度）", headLookDegrees, 0.0f, 90.0f);
    fields.Float("introSmokeColumnScale", "胴の煙：形になる段の始めに濃く立つ倍率", smokeColumnScale, 0.0f, 8.0f);
    fields.Float("introSmokeCrouchScale", "胴の煙：立ち上がる前（うずくまっている間）の量（倍率）", smokeCrouchScale,
                 0.0f, 2.0f);
    fields.Float("introSmokeBurstScale", "胴の煙：立ち上がった瞬間に継ぎ目から噴き出す量（普段に足す倍率）",
                 smokeBurstScale, 0.0f, 8.0f);
    fields.Float("introSmokeSettleShare", "胴の煙：この割合で普段の量に落ち着く", smokeSettleShare, 0.05f, 1.0f);

    fields.Float3("introRightRestWrist", "握る前の右手の置き場所（1 倍の体での値）", rightRestWrist);
    fields.Float("introDrawTendrilShare", "刀を抜く：袖口の煙が柄へ流れ始める割合（刀を抜く段の中で）",
                 drawTendrilShare, 0.0f, 1.0f);
    fields.Float("introDrawReachShare", "刀を抜く：手が柄に届く割合", drawReachShare, 0.0f, 1.0f);
    fields.Float("introDrawHoldShare", "刀を抜く：握って力を込め終わる割合（ここから斬り上げる）", drawHoldShare, 0.0f,
                 1.0f);
    fields.Float("introDrawRipShare", "刀を抜く：斬り上げて八相に構え切る割合（残りで構えが決まる）", drawRipShare,
                 0.0f, 1.0f);
    fields.Float3("introDrawRipGrip", "斬り上げ：体の右の外へ振り出す所の握りの位置（1 倍の体での値）", drawRipGrip);
    fields.Float3("introDrawRipTip", "斬り上げ：体の右の外へ振り出す所の剣先の向き（局所）", drawRipTip);
    fields.Float3("introHighGrip", "八相：握りの位置（1 倍の体での値）", highGrip);
    fields.Float3("introHighTip", "八相：剣先の向き（局所）", highTip);
    fields.Float("introDrawRipOvershoot", "斬り上げ：八相を行き過ぎる量（道のりに対する割合）", drawRipOvershoot, 0.0f,
                 0.5f);
    fields.Float("introDrawTrailScale", "斬り上げ：刀の跡に残る黒い煙の濃さ（倍率。0 で出さない）", drawTrailScale,
                 0.0f, 40.0f);
    fields.Float("introStrainTwistDegrees", "体：力を込める間の捻り（度。正が左、負が右）", strainTwistDegrees, -90.0f,
                 90.0f);
    fields.Float("introStrainLeanDegrees", "体：力を込める間の前への傾き（度）", strainLeanDegrees, -45.0f, 45.0f);
    fields.Float("introStrainSink", "体：力を込める間に腰を落とす量（立った高さに対する割合）", strainSink, 0.0f, 0.4f);
    fields.Float("introHighTwistDegrees", "体：八相の構えの捻り（度。正が左、負が右）", highTwistDegrees, -90.0f,
                 90.0f);
    fields.Float("introHighLeanDegrees", "体：八相の構えの前への傾き（度）", highLeanDegrees, -45.0f, 45.0f);
    fields.Int("introChargeStreams", "溜め：外から巻き込む煙の筋の数（0 で出さない）", chargeStreams, 0, 8);
    fields.Float("introChargeOuterRadius", "溜め：煙の筋が出る半径 (m)", chargeOuterRadius, 0.5f, 9.0f);
    fields.Float("introChargeInnerRadius", "溜め：煙の筋が巻き込み終わる半径 (m)", chargeInnerRadius, 0.0f, 9.0f);
    fields.Float("introChargeSeconds", "溜め：煙の筋 1 本が巻き込むのにかかる秒", chargeSeconds, 0.1f, 5.0f);
    fields.Float("introChargeDensity", "溜め：煙の筋の濃さ", chargeDensity, 0.0f, 1000.0f);
    fields.Float("introChargeRadius", "溜め：煙の筋の塊の半径 (m)", chargeRadius, 0.05f, 3.0f);
    fields.Float("introChargeOuterHeight", "溜め：煙の筋の、外での高さ (m)", chargeOuterHeight, 0.0f, 9.0f);
    fields.Float("introChargeInnerHeight", "溜め：煙の筋の、巻き込み終わる所の高さ (m)", chargeInnerHeight, 0.0f, 9.0f);
    fields.Float("introChargeSpinDegrees", "溜め：煙の筋が 1 回の巻き込みで回る角度（度）", chargeSpinDegrees, -720.0f,
                 720.0f);
    fields.Float("introChargePull", "溜め：とぐろの煙をボスの足元へ引き込む速さ (m/秒)", chargePull, 0.0f, 20.0f);
    fields.Float("introChargeHazePull", "溜め：立ち込める煙を中央へ流す速さ (m/秒。普段の量に足す)", chargeHazePull,
                 0.0f, 20.0f);
    fields.Float("introDrawBurstRadius", "刀が抜ける瞬間の煙：半径 (m)", drawBurstRadius, 0.05f, 3.0f);
    fields.Float("introDrawBurstDensity", "刀が抜ける瞬間の煙：濃さ（0 で出さない）", drawBurstDensity, 0.0f, 10000.0f);
    fields.Float("introDrawBurstSeconds", "刀が抜ける瞬間の煙：噴く長さ (秒)", drawBurstSeconds, 0.0f, 3.0f);
    fields.Float("introDrawBurstSpeed", "刀が抜ける瞬間の煙：噴き上がる速さ (m/秒)", drawBurstSpeed, 0.0f, 30.0f);
    fields.Float("introBladeTrailRadius", "刀身が引く煙：半径 (m)", bladeTrailRadius, 0.05f, 3.0f);
    fields.Float("introBladeTrailDensity", "刀身が引く煙：濃さ（0 で出さない）", bladeTrailDensity, 0.0f, 10000.0f);
    fields.Float("introBladeTrailHeat", "刀身が引く煙：温度（暗い赤の光。0 で光らない）", bladeTrailHeat, 0.0f, 20.0f);
    fields.Float("introSlashHoldShare", "一振り：振り始める割合（それまでは八相で溜め、左手を柄へ添える）",
                 slashHoldShare, 0.0f, 1.0f);
    fields.Float("introSlashStrikeShare", "一振り：振り抜く割合（振り始めとの間が短いほど速い）", slashStrikeShare,
                 0.0f, 1.0f);
    fields.Float("introSlashRestShare", "一振り：残心が終わる割合（残りで待機の構えへ戻る）", slashRestShare, 0.0f,
                 1.0f);
    fields.Float3("introSlashMidGrip", "一振り：体の前を通る所の握りの位置（1 倍の体での値）", slashMidGrip);
    fields.Float3("introSlashMidTip", "一振り：体の前を通る所の剣先の向き（局所）", slashMidTip);
    fields.Float3("introSlashEndGrip", "一振り：振り抜いた所の握りの位置", slashEndGrip);
    fields.Float3("introSlashEndTip", "一振り：振り抜いた所の剣先の向き", slashEndTip);
    fields.Float("introSlashTwistDegrees", "体：振り抜いた時の捻り（度。正が左）", slashTwistDegrees, -90.0f, 90.0f);
    fields.Float("introSlashLeanDegrees", "体：振り抜いた時の前への傾き（度）", slashLeanDegrees, -45.0f, 45.0f);
    fields.Float("introSlashSink", "体：振り抜いた時に腰を落とす量（立った高さに対する割合）", slashSink, 0.0f, 0.4f);
    fields.Float("introPoseShare", "見得（血振り）：振り払い終える割合（一振りの段の中で）", poseShare, 0.0f, 1.0f);
    fields.Float("introPoseHoldShare", "見得：解く割合（残りで待機の構えへ戻る）", poseHoldShare, 0.0f, 1.0f);
    fields.Float3("introPoseGrip", "見得：握りの位置（1 倍の体での値）", poseGrip);
    fields.Float3("introPoseTip", "見得：剣先の向き（局所。右が -X、前が +Z）", poseTip);
    fields.Float("introPoseTwistDegrees", "見得：体の捻り（度。正が左）", poseTwistDegrees, -90.0f, 90.0f);
    fields.Float("introPoseLeanDegrees", "見得：体の前への傾き（度。負で胸を張る）", poseLeanDegrees, -45.0f, 45.0f);
    fields.Float("introSlashEdge", "一振り：溜めの間に刃口が明るくなる強さ", slashEdge, 0.0f, 8.0f);
    fields.Float("introSlashAura", "一振り：溜めの間の、体を包む気配（ゆがみ）の強さ", slashAura, 0.0f, 4.0f);
    fields.Float("introSlashShock", "一振り：振り抜いた瞬間の衝撃の輪の大きさ（倍率。0 で出さない）", slashShock, 0.0f,
                 10.0f);
    fields.Float("introSlashShockSecond", "一振り：少し遅れて出る 2 つ目の衝撃の輪の大きさ", slashShockSecond, 0.0f,
                 10.0f);
    fields.Float("introSlashGlint", "一振り：刀身を走る光の明るさ（0 で出さない）", slashGlint, 0.0f, 8.0f);
    fields.Vfx("introSlashFlashEffect", "一振り：振り抜いた瞬間に刀身の上で弾ける閃光のエフェクト（空なら出さない）",
               slashFlashEffect);
    fields.Float("introSlashFlashScale", "一振り：閃光の大きさ（0 で出さない）", slashFlashScale, 0.0f, 10.0f);
    fields.Float("introSlashBlast", "一振り：周りの煙を吹き払う強さ (m/秒。0 で払わない)", slashBlast, 0.0f, 40.0f);
    fields.Float("introSlashBlastRadius", "一振り：煙を吹き払う半径 (m)", slashBlastRadius, 0.5f, 40.0f);
    fields.Float("introSlashTrailScale",
                 "一振り：刀の跡に残る黒い煙の弧の濃さ（刀身が引く煙に対する倍率。0 で出さない）", slashTrailScale,
                 0.0f, 40.0f);
    fields.Float("introSlashTrailRadius", "一振り：刀の跡に残る煙の弧の、塊の半径 (m)", slashTrailRadius, 0.05f, 3.0f);
    fields.Float("introSlashTrailSpeed", "一振り：刀の跡に残る煙の弧が、外へ広がる速さ (m/秒)", slashTrailSpeed, 0.0f,
                 30.0f);
    fields.Int("introSlashHitStopFrames", "一振り：振り抜いた瞬間に時間を止める長さ (F。0 で止めない)",
               slashHitStopFrames, 0, 60);
    fields.Float("introSlashCoilBlast", "一振り：まわりに集まった煙（とぐろの域）を外へ吹き飛ばす強さ (m/秒)",
                 slashCoilBlast, 0.0f, 40.0f);
    fields.Asset("introSlashMonoProfile", "一振り：白黒の閃きの効果のプロファイル（BossImpactMono。空なら出さない）",
                 HIKARI::GameProjectAssetKind::PostProfile, slashMonoProfile);
    fields.Float("introSlashMonoInvertSeconds", "白黒の閃き：反転した白黒を出す長さ（実時間の秒）",
                 slashMonoInvertSeconds, 0.0f, 1.0f);
    fields.Float("introSlashMonoHoldSeconds", "白黒の閃き：反転しない白黒を出す長さ（実時間の秒）",
                 slashMonoHoldSeconds, 0.0f, 1.0f);
    fields.Float("introSlashMonoFadeSeconds", "白黒の閃き：色へ戻る長さ（実時間の秒）", slashMonoFadeSeconds, 0.0f,
                 2.0f);
    fields.Float("introSlashMonoStrength", "白黒の閃き：強さ（0 で出さない）", slashMonoStrength, 0.0f, 1.0f);
    fields.Int("introFlourishRingCount", "一振りの煙の輪：塊の数（0 で出さない）", flourishRingCount, 0,
               kBossIntroPuffCapacity);
    fields.Float("introFlourishRingStart", "一振りの煙の輪：出る半径 (m)", flourishRingStart, 0.0f, 10.0f);
    fields.Float("introFlourishRingRadius", "一振りの煙の輪：塊の半径 (m)", flourishRingRadius, 0.05f, 3.0f);
    fields.Float("introFlourishRingDensity", "一振りの煙の輪：濃さ", flourishRingDensity, 0.0f, 10000.0f);
    fields.Float("introFlourishRingSeconds", "一振りの煙の輪：出す長さ (秒)", flourishRingSeconds, 0.0f, 2.0f);
    fields.Float("introFlourishRingSpeed", "一振りの煙の輪：広がる速さ (m/秒)", flourishRingSpeed, 0.0f, 40.0f);
    fields.Float("introFlourishFogPush", "一振りが地面の霧を外へ押す強さ (m/秒。0 で押さない)", flourishFogPush, 0.0f,
                 40.0f);
    fields.Float("introFlourishFogPushRadius", "一振りが地面の霧を押す半径 (m)", flourishFogPushRadius, 0.1f, 20.0f);
    fields.Float("introAccentShake", "手応え：刀が地面を離れる瞬間のカメラの揺れ幅 (m。0 で揺らさない)", accentShake,
                 0.0f, 1.0f);
    fields.Int("introAccentShakeFrames", "手応え：揺れの長さ (F)", accentShakeFrames, 0, 120);
    fields.Float("introAccentShakeFrequency", "手応え：揺れの速さ", accentShakeFrequency, 1.0f, 60.0f);
    fields.Float("introAccentFovDegrees", "手応え：強い拍で画角を一瞬広げる量（度。0 で動かさない）", accentFovDegrees,
                 0.0f, 20.0f);
    fields.Float("introAccentSoftShare", "手応え：一振りの輪の揺れ（抜く瞬間に対する割合。0 で揺らさない）",
                 accentSoftShare, 0.0f, 1.0f);
    fields.Int("introDrawHitStopFrames", "手応え：刀が地面を離れる瞬間に時間を止める長さ (F。0 で止めない)",
               drawHitStopFrames, 0, 60);
    fields.Float("introHitStopScale", "手応え：時間を止めている間の時間の速さ", hitStopScale, 0.0f, 1.0f);
    fields.Float("introAccentRumble", "手応え：振動の強さ（0 で出さない）", accentRumble, 0.0f, 1.0f);
    fields.Int("introAccentRumbleFrames", "手応え：振動の長さ (F)", accentRumbleFrames, 0, 120);
}

} // namespace Project_TD2_2
