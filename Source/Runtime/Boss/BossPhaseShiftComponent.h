#pragma once

#include <string_view>

#include "HIKARI/Game.h"
#include "BossPhaseShiftSettings.h"

namespace Project_TD2_2 {

// ボスのフェーズの変わり目の演出の部品（c-3a0f87 でボスの部品から分けた）：ボスと同じ物体に付ける。無ければ演出を
// しない（演出を切った時と同じ。変わり目は Boss.phaseShiftFrames の間、待機のまま）。進めるのはボスの System
// （BossSystem。ボスの tick の中で、前と同じ順に呼ぶ）、規則は BossPhaseShift、見た目は BossPhaseShiftVisuals。
// フェーズ 2 の見た目へ切り替える時（閃光）は、ボスの部品の phaseTwoLookPending に書く（体の効果・場の色が読む）。
struct BossPhaseShift : HIKARI::Component {
    HIKARI_COMPONENT(BossPhaseShift, "ボスの変わり目の演出")
    // 演出をするか。切ると、変わり目は Boss.phaseShiftFrames の間、待機のまま。
    FIELD(bool, shiftEnabled, true, "演出をする（切ると、変わり目は phaseShiftFrames の間、待機のまま）", HIKARI::Group("フェーズの変わり目の演出（打ち退け → 太陽を掴む → 燃え上がる → 刀に火 → 一閃）"))
    NOTE("1 本目の条が空になると、ボスが打ち退けられて座り込み、左手を太陽へ伸ばして掴みます。拳から火が回って胴の煙が"
        "炎に、目が橙赤に、場の色が変わり、立ち上がって刀を拭って火を移し、一閃して待機の構えへ戻ります。動きは骨格の体の"
        "クリップ、カメラと効果の時刻は演出の資産（事件段 Shift.<拍>）が受け持ちます。舞台の向きは太陽で決め、"
        "プレイヤーは演出の始めにボスの正面へ置き直します。演出の間、ボスは当たらず、プレイヤーは動けず当たりません。"
        "HP の条は一閃の所から満ちます。")
    // 演出の資産（時計・カメラ・事件段）と、骨格の体に流すクリップ。資産に Anim.<クリップ名> の段が無ければ、0 秒から
    // clipSeconds の長さで流す。資産を流す速さ。
    FIELD(HIKARI::Sequence, shiftSequence, (HIKARI::Sequence{"f417d9926c1bae81a991107f933b1e26"}), "演出の資産（時計・カメラ・事件段）")
    FIELD(std::string, shiftClip, "Shift_Fire", "骨格の体に流すクリップ（模型の中の名前）")
    FIELD(float, shiftClipSeconds, 18.933f, "クリップの長さ (秒。資産に Anim.<クリップ> の段が無い時に使う)", HIKARI::Range(0.1f, 120.0f))
    FIELD(float, shiftPlaybackRate, 1.0f, "資産を流す速さ（1 で等速）", HIKARI::Range(0.1f, 4.0f))
    // 資産のカメラが付く舞台の印の名前（資産のバインディングの名前。骨格の体の物体を結ぶ）。
    FIELD(std::string, shiftStageSlot, "Stage", "カメラが付く舞台の印（資産のバインディングの名前。骨格の体の物体を結ぶ）")
    // 舞台の向き：太陽（環境の太陽の向き）が、ボスの正面からこの角度（度。正は左）に来るように向ける。切ると
    // プレイヤーのいる側を前にする。太陽が地平より下（夜）の時も、プレイヤーの側。
    FIELD(bool, shiftFaceSun, true, "舞台の向きを太陽で決める（切るとプレイヤーのいる側を前にする）")
    FIELD(float, shiftSunStageYawDegrees, 0.0f, "太陽が来る方位（ボスの正面からの度。正は左。演出の資産を作った値と同じに）", HIKARI::Range(-180.0f, 180.0f))
    // 舞台の真ん中（ボスの足元）を、場の縁（Boss.arenaRadius）からこれだけ内側へ寄せる (m。カメラが場の外へ出ないように)。
    FIELD(float, shiftStageEdgeMargin, 16.0f, "舞台の真ん中を場の縁からこれだけ内側へ寄せる (m)", HIKARI::Range(0.0f, 60.0f))
    // プレイヤーを置き直す：ボスの正面のこの距離 (m)、場の縁からの内側の余白 (m)、置く高さ（足元の高さに足す m）。
    FIELD(bool, shiftPlacePlayer, true, "プレイヤーをボスの正面へ置き直す")
    FIELD(float, shiftPlayerDistance, 13.0f, "…ボスの正面のこの距離 (m)", HIKARI::Range(2.0f, 60.0f))
    FIELD(float, shiftPlayerArenaMargin, 3.0f, "…場の縁からの内側の余白 (m)", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, shiftPlayerLift, 0.5f, "…置く高さ（置く所の地面から足元までの m。ここから落ちて地面に立つ。置く中心の高さは、これにプレイヤーの碰撞体の半分の高さを足した所）", HIKARI::Range(0.0f, 5.0f))
    // 渡し：演出の最後のカメラの姿勢からゲームのカメラへ混ぜる長さ (秒)、その間ゲームのカメラが見ておく高さ (m)。
    // クリップの層を外す時に待機の姿へ混ぜる長さ (秒)。
    FIELD(float, shiftSettleSeconds, 1.4f, "渡し：最後のカメラからゲームのカメラへ混ぜる長さ (秒)", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, shiftCameraFocusHeight, 3.6f, "渡し：ゲームのカメラが見ておく、ボスの足元からの高さ (m)", HIKARI::Range(0.0f, 12.0f))
    FIELD(float, shiftClipBlendOutSeconds, 0.35f, "渡し：クリップから待機の姿へ混ぜる長さ (秒)", HIKARI::Range(0.0f, 2.0f))
    // 演出の後、すぐには攻めて来ない間 (F。この間は技を選ばない)。
    FIELD(int, shiftCalmFrames, 45, "演出の後、技を選ばない間 (F)", HIKARI::Range(0, 600))
    // 飛ばす：演出の間にこの入力（行動 id）を押すと、フェーズ 2 の構えへ進む。
    FIELD(bool, shiftSkippable, true, "飛ばせる")
    FIELD(std::string, shiftSkipAction, "Gameplay.Menu", "飛ばす入力（行動 id）")
    // 打ち退け（Hit）：時間を止める長さ (F)・その間の速さ・揺れ（0〜1）。
    FIELD(int, shiftHitStopFrames, 9, "打ち退け：時間を止める長さ (F)", HIKARI::Range(0, 60))
    FIELD(float, shiftHitStopScale, 0.05f, "打ち退け：止める間の時間の速さ", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, shiftHitShake, 0.7f, "打ち退け：揺れ（0〜1）", HIKARI::Range(0.0f, 2.0f))
    // 打ち退けの瞬間、胴の煙が体から弾け出る速さ (m/秒) と半径 (1 倍の体での m)。0 で出さない。
    FIELD(float, shiftHitBurst, 3.5f, "打ち退け：胴の煙が体から弾け出る速さ (m/秒)", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, shiftHitBurstRadius, 0.9f, "打ち退け：煙が弾ける半径 (m)", HIKARI::Range(0.1f, 6.0f))
    // 崩れ落ちた瞬間（刀が地面を打つ）：揺れ（0〜1）と、胴の煙が弾ける速さ (m/秒)。
    FIELD(float, shiftFallShake, 0.45f, "崩れ落ち：揺れ（0〜1）", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, shiftFallBurst, 2.0f, "崩れ落ち：胴の煙が弾ける速さ (m/秒)", HIKARI::Range(0.0f, 60.0f))
    // 演出の間のカメラの動きのぼけ（速いカメラの動き・速い体の動きが流れて見える）：シャッター角（度。180 が映画の標準、
    // 大きいほど強い）と、ぼけの長さの上限（1080p の画素。0〜64）。渡しの間は、ゲームのカメラへ戻るのに合わせて弱める。
    FIELD(bool, shiftMotionBlur, true, "演出の間、カメラの動きのぼけを強める")
    FIELD(float, shiftMotionBlurShutterDegrees, 270.0f, "動きのぼけ：シャッター角（度。180 が映画の標準）", HIKARI::Range(0.0f, 360.0f))
    FIELD(float, shiftMotionBlurMaxPixels, 40.0f, "動きのぼけ：長さの上限（1080p の画素）", HIKARI::Range(0.0f, 64.0f))
    // 継ぎ目から漏れる黒い煙（Leak。胴の煙の域）：中心の毎秒の密度・半径（1 倍の体での m）・外へ吹く速さ (m/秒)。
    FIELD(float, shiftLeakDensity, 35.0f, "座り込み：継ぎ目から漏れる黒い煙の濃さ（毎秒の密度。0 で出さない）", HIKARI::Range(0.0f, 5000.0f))
    FIELD(float, shiftLeakRadius, 0.22f, "座り込み：黒い煙の半径（1 倍の体での m）", HIKARI::Range(0.02f, 2.0f))
    FIELD(float, shiftLeakSpeed, 1.2f, "座り込み：黒い煙が外へ吹く速さ (m/秒)", HIKARI::Range(0.0f, 20.0f))
    // 打ち退けられてから火が回るまで、胴の煙の量をこの割合まで薄くする（0〜1。崩れて煙が抜けた姿：兜と胴が煙に隠れない）。
    FIELD(float, shiftSlumpSmoke, 0.35f, "座り込み：火が回るまで、胴の煙の量をこの割合まで薄くする（0〜1）", HIKARI::Range(0.0f, 1.0f))
    // 太陽へ伸ばす間（Reach）：太陽の所に出す画面の星の明るさ（終わりにここまで）。
    FIELD(float, shiftReachStar, 0.9f, "太陽へ伸ばす：太陽の所の画面の星の明るさ", HIKARI::Range(0.0f, 4.0f))
    // 掴む（Grab の終わり）：小さな揺れ（0〜1）、拳へ吸い込む輪の強さと出始めの大きさ（画面の高さを 1）、吸い込む長さ (秒)、
    // 画が暗くなる強さ（0〜1）と長さ (秒)。
    FIELD(float, shiftGrabShake, 0.3f, "掴む：揺れ（0〜1。小さく）", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, shiftGrabShock, 1.0f, "掴む：光を拳へ吸い込む輪の強さ", HIKARI::Range(0.0f, 4.0f))
    FIELD(float, shiftGrabShockRadius, 0.9f, "掴む：吸い込む輪の出始めの大きさ（画面の高さを 1）", HIKARI::Range(0.05f, 3.0f))
    FIELD(float, shiftGrabShockSeconds, 0.32f, "掴む：吸い込む長さ (秒)", HIKARI::Range(0.05f, 2.0f))
    FIELD(float, shiftGrabDim, 0.55f, "掴む：画が暗くなる強さ（0〜1）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, shiftGrabDimSeconds, 0.7f, "掴む：暗さが戻る長さ (秒)", HIKARI::Range(0.05f, 4.0f))
    // 拳の火（Ignite）：燃料の強さ・半径（1 倍の体での m）・温度。
    FIELD(float, shiftFistFuel, 150.0f, "拳の火：燃料の強さ", HIKARI::Range(0.0f, 2000.0f))
    FIELD(float, shiftFistFuelRadius, 0.3f, "拳の火：半径（1 倍の体での m）", HIKARI::Range(0.02f, 2.0f))
    FIELD(float, shiftFistFuelHeat, 3.0f, "拳の火：温度", HIKARI::Range(0.0f, 20.0f))
    // 火が回る（Spread）：腕を伝う火の燃料の強さ、胴の煙の火の強さを Spread の終わりにここまで上げる（0〜1）。
    FIELD(float, shiftSpreadFuel, 240.0f, "火が回る：腕を伝う火の燃料の強さ", HIKARI::Range(0.0f, 2000.0f))
    FIELD(float, shiftSpreadFire, 0.9f, "火が回る：胴の煙の火の強さを、Spread の終わりにここまで（0〜1）", HIKARI::Range(0.0f, 1.0f))
    // 火が回る（Spread）：胴の煙の柱を、胸から足元へ降りていく火の燃料の強さ（0 で出さない）と半径（1 倍の体での m）。
    // Spread が終わった後も、演出の終わりまで、この割合で燃やし続ける（0 で Spread と一緒に止む）。
    FIELD(float, shiftColumnFuel, 620.0f, "火が回る：胴の煙の柱を降りていく火の燃料の強さ（0 で出さない）", HIKARI::Range(0.0f, 2000.0f))
    FIELD(float, shiftColumnFuelRadius, 0.42f, "火が回る：柱を降りる火の半径（1 倍の体での m）", HIKARI::Range(0.02f, 2.0f))
    FIELD(float, shiftColumnSustain, 0.3f, "火が回る：回り切った後も、演出の終わりまで柱を燃やし続ける割合（0 で Spread と一緒に止む）", HIKARI::Range(0.0f, 1.0f))
    // 燃え上がり（火の先が胸に届いた瞬間。c-280b71）：胴の煙の柱へ一気に入れる燃料の強さ（0 で出さない）・半径（1 倍の
    // 体での m）・長さ (秒)、外へ膨らませる速さ (m/秒)、小さな揺れ、火の粉を何倍にするか・元へ戻るまでの秒。
    FIELD(float, shiftFlareFuel, 650.0f, "燃え上がり：火が胸に届いた瞬間に、胴へ一気に入れる燃料の強さ（0 で出さない）", HIKARI::Range(0.0f, 5000.0f))
    FIELD(float, shiftFlareRadius, 0.6f, "燃え上がり：燃料を入れる半径（1 倍の体での m）", HIKARI::Range(0.02f, 3.0f))
    FIELD(float, shiftFlareSeconds, 0.5f, "燃え上がり：燃料を入れ続ける長さ (秒)", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, shiftFlarePush, 2.0f, "燃え上がり：炎を外へ膨らませる速さ (m/秒。0 で押さない)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, shiftFlareShake, 0.5f, "燃え上がり：揺れの強さ", HIKARI::Range(0.0f, 4.0f))
    FIELD(float, shiftFlareEmbers, 3.0f, "燃え上がり：火の粉を何倍にするか", HIKARI::Range(1.0f, 10.0f))
    FIELD(float, shiftFlareEmbersSeconds, 1.4f, "燃え上がり：火の粉が元の量へ戻るまでの秒", HIKARI::Range(0.0f, 6.0f))
    // 場の色（Field の終わりに、フェーズ 2 の色のこの割合まで）。
    FIELD(float, shiftFieldWarm, 0.85f, "場の色：Field の終わりにフェーズ 2 の色のこの割合まで（残りは一閃で）", HIKARI::Range(0.0f, 1.0f))
    // 刀に火を移す（Fuse）：火の先が左手より先へ進む割合（1 で手と同じ所、2 で倍の速さ）、刀身の燃料の強さ・半径 (m)・温度。
    FIELD(float, shiftFuseLead, 1.1f, "刀に火を移す：火の先が左手より先へ進む割合（1 で手の所）", HIKARI::Range(0.5f, 5.0f))
    FIELD(float, shiftBladeFuel, 90.0f, "刀に火を移す：刀身の燃料の強さ", HIKARI::Range(0.0f, 2000.0f))
    FIELD(float, shiftBladeFuelRadius, 0.3f, "刀に火を移す：刀身の火の半径 (m)", HIKARI::Range(0.02f, 2.0f))
    FIELD(float, shiftBladeFuelHeat, 3.0f, "刀に火を移す：刀身の火の温度", HIKARI::Range(0.0f, 20.0f))
    // 火の壁（Wall）：ボスの正面に立ち上がる炎の壁（場の火の域）。舞台の足元から正面へこの距離 (m)、壁の半分の幅 (m)、
    // 燃える地面として塗る幅 (m)、真ん中から端へ広がる長さ (秒)。炎を持ち上げる燃料：点の数・強さ・半径 (m)・温度・上へ
    // 吹く速さ (m/秒)。一閃で壁が爆ぜる：壁に沿って外へ走る炎の塊の半径 (m。0 で出さない) と、走る速さ (m/秒)。
    FIELD(float, shiftWallDistance, 6.2f, "火の壁：舞台の足元から正面へこの距離 (m)", HIKARI::Range(1.0f, 30.0f))
    FIELD(float, shiftWallHalfWidth, 7.5f, "火の壁：半分の幅 (m)", HIKARI::Range(0.5f, 30.0f))
    FIELD(float, shiftWallPaintWidth, 1.6f, "火の壁：燃える地面として塗る幅 (m。0 で塗らない)", HIKARI::Range(0.0f, 6.0f))
    FIELD(float, shiftWallRiseSeconds, 0.4f, "火の壁：真ん中から端へ広がる長さ (秒)", HIKARI::Range(0.05f, 4.0f))
    FIELD(int, shiftWallPoints, 10, "火の壁：炎を持ち上げる燃料の点の数（0 で出さない）", HIKARI::Range(0, 16))
    FIELD(float, shiftWallFuel, 85.0f, "火の壁：燃料の強さ", HIKARI::Range(0.0f, 2000.0f))
    FIELD(float, shiftWallFuelRadius, 0.9f, "火の壁：燃料の半径 (m)", HIKARI::Range(0.1f, 4.0f))
    FIELD(float, shiftWallFuelHeat, 6.0f, "火の壁：燃料の温度", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, shiftWallFuelRise, 5.0f, "火の壁：上へ吹く速さ (m/秒)", HIKARI::Range(0.0f, 30.0f))
    FIELD(float, shiftWallBurstRadius, 2.0f, "一閃：火の壁に沿って走る炎の塊の半径 (m。0 で出さない)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, shiftWallBurstSpeed, 45.0f, "一閃：炎の塊が壁に沿って走る速さ (m/秒)", HIKARI::Range(1.0f, 200.0f))
    // 振りかぶり（Charge）：刃口の光と体を包む気配（ゆがみ）の強さ、画の張りつめ（0〜1）。
    FIELD(float, shiftChargeEdge, 2.4f, "振りかぶり：刃口の光の強さ", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, shiftChargeAura, 1.6f, "振りかぶり：体を包む気配（ゆがみ）の強さ", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, shiftChargeTension, 0.55f, "振りかぶり：画の張りつめ（0〜1）", HIKARI::Range(0.0f, 1.0f))
    // 一閃（Slash の始め）：白黒の閃き・衝撃の輪（1 つ目・遅れて出る 2 つ目）・刀身を走る光・時間の停止 (F・速さ)・揺れ。
    FIELD(bool, shiftSlashMono, true, "一閃：白黒の閃き")
    FIELD(float, shiftSlashShock, 1.3f, "一閃：衝撃の輪の強さ（1 つ目。0 で出さない）", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, shiftSlashShockSecond, 0.8f, "一閃：遅れて出る衝撃の輪の強さ", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, shiftSlashGlint, 2.6f, "一閃：刀身を走る光の明るさ", HIKARI::Range(0.0f, 10.0f))
    FIELD(int, shiftSlashHitStopFrames, 7, "一閃：時間を止める長さ (F)", HIKARI::Range(0, 60))
    FIELD(float, shiftSlashHitStopScale, 0.04f, "一閃：止める間の時間の速さ", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, shiftSlashShake, 1.0f, "一閃：揺れ（0〜1）", HIKARI::Range(0.0f, 2.0f))
    // 一閃の炎の塊（場の火の域。見た目だけ）：剣先の落ちた所の塊の半径 (m。0 で出さない)、刀身に沿って地面に並べる数と
    // その先へ続ける数、続けて出す回数と間 (秒)。
    FIELD(float, shiftSlashBlastRadius, 2.4f, "一閃：剣先の落ちた所の炎の塊の半径 (m。0 で出さない)", HIKARI::Range(0.0f, 10.0f))
    FIELD(int, shiftSlashBlastAlong, 2, "一閃：刀身に沿って地面に並べる炎の塊の数", HIKARI::Range(0, 8))
    FIELD(int, shiftSlashBlastAhead, 3, "一閃：剣先の先へ続ける炎の塊の数", HIKARI::Range(0, 8))
    FIELD(int, shiftSlashBlastWaves, 3, "一閃：炎の塊を続けて出す回数", HIKARI::Range(1, 8))
    FIELD(float, shiftSlashBlastInterval, 0.06f, "一閃：続けて出す間 (秒)", HIKARI::Range(0.01f, 0.5f))
    // 火の輪（Slash）：広がり切る半径 (m。0 で出さない)・広がる長さ (秒)・光を下げて消す長さ (秒)・資産の細い輪の明るさ。
    FIELD(float, shiftRingRadius, 13.0f, "一閃：火の輪が広がり切る半径 (m。0 で出さない)", HIKARI::Range(0.0f, 40.0f))
    FIELD(float, shiftRingSeconds, 0.26f, "一閃：火の輪が広がる長さ (秒)", HIKARI::Range(0.05f, 3.0f))
    FIELD(float, shiftRingFadeSeconds, 0.18f, "一閃：火の輪の光を下げて消す長さ (秒)", HIKARI::Range(0.05f, 3.0f))
    FIELD(float, shiftRingThinGlow, 0.0f, "一閃：資産の細い輪の明るさ（0 で出さない。細い輪は地面の予告に見える）", HIKARI::Range(0.0f, 1.0f))
    // 一閃で地面の霧を外へ押す強さ (m/秒) と押す半径 (m)。
    FIELD(float, shiftFogPush, 9.0f, "一閃：地面の霧を外へ押す強さ (m/秒)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, shiftFogPushRadius, 12.0f, "一閃：霧を押す半径 (m)", HIKARI::Range(0.0f, 40.0f))
    // 飛ばした時：演出で入れた火を吸い消す長さ (秒)・球の半径（1 倍の体での m）・消す速さ (1/秒)。
    FIELD(float, shiftSkipQuenchSeconds, 0.3f, "飛ばした時：演出で入れた火を吸い消す長さ (秒。0 で消さない)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, shiftSkipQuenchRadius, 0.5f, "飛ばした時：吸い消す球の半径（1 倍の体での m）", HIKARI::Range(0.05f, 2.0f))
    FIELD(float, shiftSkipQuenchRate, 40.0f, "飛ばした時：消す速さ (1/秒)", HIKARI::Range(0.0f, 1000.0f))

    BossPhaseShiftState state;
};

} // namespace Project_TD2_2
