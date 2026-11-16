#include "BossBodySettings.h"

#include "HIKARI/Project/AssetKind.h"

namespace Project_TD2_2 {

void BossBodySettings::Describe(HIKARI::PropertyFields& fields) {
    DescribeShape(fields);
    motion.Describe(fields);
    leftHand.Describe(fields);
    accents.Describe(fields);
}

void BossBodyAccentSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("外形：技の演出（見た目だけ）", false))
        return;
    fields.Note("技の点描の演出です。当たり・F 数は変えません。色・明るさ・消える時間は、演出の値の資産と各エフェクトの中に"
                "あります。");
    fields.Vfx("bodyBladeEffect", "刀に付くエフェクト（刀光の帯と、燃える刀身の炎・ゆがみ・光。空なら出さない）",
               bladeEffect);
    fields.Float("bodyBladeFromShare", "刀に付くエフェクト：刃の始まり（握りから剣先までの割合）", bladeFromShare,
                 0.0f, 0.9f);
    fields.Bool("bodyTrail", "刀光：振りの間、刃が通った面に光る帯とゆがみを出す", trail);
    fields.Float("bodyTrailFollowSeconds", "刀光：命中の窓の後も、振り抜きの間は出し続ける時間 (秒)",
                 trailFollowSeconds, 0.0f, 1.0f);
    fields.Float("bodyTrailMinTipSpeed", "刀光：振り抜きの間に出すのに要る剣先の速さ (m/秒)", trailMinTipSpeed, 0.0f,
                 100.0f);
    fields.Float("bodyTrailGroundCut", "刀光：剣先が地面からこの高さより下へ来たら切る (m)", trailGroundCut, 0.0f,
                 3.0f);
    fields.Float3("bodyTrailMistColor", "刀光：霞の技の色と明るさ（1 を超えると光る）", trailMistColor);
    fields.Float3("bodyTrailFireColor", "刀光：刀が燃えている技の色と明るさ", trailFireColor);
    fields.Float("bodySwingHaze", "振りのゆがみ（屈折）の強さ（0.1〜0.3。0.3 を超えると段が見える。0 で出さない）",
                 swingHaze, 0.0f, 0.5f);
    fields.Float("bodySwingHazeMinTipSpeed",
                 "振りのゆがみ：刀光より前から出し始める剣先の速さ (m/秒。打ち込みの間だけ。0 で刀光と同じ間)",
                 swingHazeMinTipSpeed, 0.0f, 300.0f);
    fields.Float("bodyBladeHeat", "燃える刀身：炎の明るさ（倍率。0 で出さない）", bladeHeat, 0.0f, 8.0f);
    fields.Float("bodyBladeHeatSeconds", "燃える刀身：火が付く・消える時間 (秒)", bladeHeatSeconds, 0.01f, 3.0f);
    fields.Vfx("bodyEyesEffect", "目の光のエフェクト（兜に付く。尾を引く光・暈・閃き。空なら出さない）", eyesEffect);
    fields.Float3("bodyEyesMistColor", "目の光：ふだんの色と明るさ", eyesMistColor);
    fields.Float3("bodyEyesFireColor", "目の光：フェーズ 2 の色と明るさ", eyesFireColor);
    fields.Float("bodyEyeFlare", "目の閃き：ふつうの技を出すと決めた瞬間の強さ（0 で出さない）", eyeFlare, 0.0f, 8.0f);
    fields.Float("bodyEyeFlareBig", "目の閃き：大技（引爆・天降り斬り・退き溜め・霞の大剣）の強さ", eyeFlareBig, 0.0f,
                 8.0f);
    fields.Float("bodyEyeFlareSeconds", "目の閃き：消えるまでの時間 (秒)", eyeFlareSeconds, 0.01f, 3.0f);
    fields.Float("bodyBladeRimKamae", "刀身の輪郭の光：両手の斬りの構え・溜めの強さ（0 で掛けない）", bladeRimKamae,
                 0.0f, 8.0f);
    fields.Float("bodyBladeRimSwing", "刀身の輪郭の光：振る間の強さ", bladeRimSwing, 0.0f, 8.0f);
    fields.Float("bodyBladeRimCharge", "刀身の輪郭の光：大きな溜め（溜めの見た目 1 あたり）で足す強さ", bladeRimCharge,
                 0.0f, 8.0f);
    fields.Float("bodyBladeRimHotShare", "刀身の輪郭の光：燃える刀身の間に弱める割合 (0〜1)", bladeRimHotShare, 0.0f,
                 1.0f);
    fields.Float("bodyBladeRimPower", "刀身の輪郭の光：鋭さ（大きいほど縁だけ細く光る）", bladeRimPower, 0.5f, 16.0f);
    fields.Float("bodyBladeRimPulse", "刀身の輪郭の光：明滅の速さ (rad/秒)", bladeRimPulse, 0.0f, 32.0f);
    fields.Float("bodyBladeRimRiseSeconds", "刀身の輪郭の光：灯る時間 (秒)", bladeRimRiseSeconds, 0.01f, 2.0f);
    fields.Float("bodyBladeRimFallSeconds", "刀身の輪郭の光：消える時間 (秒)", bladeRimFallSeconds, 0.01f, 2.0f);
    fields.Vfx("bodyGlintEffect", "刀身を走る光のエフェクト（刀を振る技を出すと決めた瞬間。空なら出さない）",
               glintEffect);
    fields.Float3("bodyGlintMistColor", "刀身を走る光：霞の技の色と明るさ", glintMistColor);
    fields.Float3("bodyGlintFireColor", "刀身を走る光：刀が燃えている技の色と明るさ", glintFireColor);
    fields.Vfx("bodyAuraEffect", "溜めの気配のエフェクト（溜めている間、体を包んで立ちのぼる。空なら出さない）",
               auraEffect);
    fields.Float3("bodyAuraMistColor", "溜めの気配：霞の技の色と明るさ", auraMistColor);
    fields.Float3("bodyAuraFireColor", "溜めの気配：刀が燃えている技の色と明るさ", auraFireColor);
    fields.Float("bodyAuraSeconds", "溜めの気配：立ち上がる・引く時間 (秒)", auraSeconds, 0.01f, 3.0f);
    fields.Float("bodyAuraConjure", "溜めの気配：霞の大剣を作り出す間の強さ（溜め切りを 1 として）", auraConjure, 0.0f,
                 2.0f);
    fields.Float("bodySmokeChargeAmount", "胴の煙：溜め切った時の量（倍率）", smokeChargeAmount, 0.0f, 8.0f);
    fields.Float("bodySmokeLitHeat", "胴の煙：フェーズ 1 で刀が燃えている間の熱さ（熱い煙は橙に光る。1 で光らない）",
                 smokeLitHeat, 0.0f, 20.0f);
    fields.Float("bodySmokeSwingPuff", "胴の煙：振り終わった直後に吹き出す量（倍率。1 で吹き出さない）", smokeSwingPuff,
                 1.0f, 8.0f);
    fields.Float("bodySmokeSwingPuffSeconds", "胴の煙：吹き出しが収まるまでの時間 (秒)", smokeSwingPuffSeconds, 0.01f,
                 3.0f);
    fields.Vfx("bodyShockEffect", "衝撃の輪のエフェクト（重い一撃が当たった瞬間、地面に沿って広がる。空なら出さない）",
               shockEffect);
    fields.Float3("bodyShockMistColor", "衝撃の輪：霞の技の色と明るさ", shockMistColor);
    fields.Float3("bodyShockFireColor", "衝撃の輪：刀が燃えている技の色と明るさ", shockFireColor);
    fields.Float("bodyShockSkyfall", "衝撃の輪の大きさ：天降り斬りの着地（倍率。0 で出さない）", shockSkyfall, 0.0f, 6.0f);
    fields.Float("bodyShockCleave", "衝撃の輪の大きさ：霞の大剣の叩きつけ", shockCleave, 0.0f, 6.0f);
    fields.Float("bodyShockKick", "衝撃の輪の大きさ：回し蹴り", shockKick, 0.0f, 6.0f);
    fields.Float("bodyShockDetonate", "衝撃の輪の大きさ：引爆の爆ぜ始め", shockDetonate, 0.0f, 6.0f);
    fields.Float("bodyShockRelease", "衝撃の輪の大きさ：溜めた一撃を放つ瞬間（退き溜め）", shockRelease, 0.0f, 6.0f);
    fields.Vfx("bodyDashEffect", "突進の尾のエフェクト（体が速く動く間の光の帯とゆがみ。空なら出さない）", dashEffect);
    fields.Float3("bodyDashMistColor", "突進の尾：ふだんの色と明るさ", dashMistColor);
    fields.Float3("bodyDashFireColor", "突進の尾：刀が燃えている間の色と明るさ", dashFireColor);
    fields.Float("bodyDashMinSpeed", "突進の尾：出始める体の速さ (m/秒)", dashMinSpeed, 0.0f, 100.0f);
    fields.Float("bodyDashFullSpeed", "突進の尾：出切る体の速さ (m/秒)", dashFullSpeed, 0.1f, 100.0f);
    fields.Vfx("bodyHandEffect", "左手の光のエフェクト（所作の間、掌に集まる光。空なら出さない）", handEffect);
    fields.Vfx("bodyHandBurstEffect", "左手の弾けのエフェクト（握る・印を切る・投げる瞬間。空なら出さない）",
               handBurstEffect);
    fields.Float3("bodyHandMistColor", "左手の光：霞の所作の色と明るさ", handMistColor);
    fields.Float3("bodyHandFireColor", "左手の光：火の所作の色と明るさ", handFireColor);
    fields.Float("bodyHandDetonate", "左手の光の強さ：引爆（掲げた掌。0 で出さない）", handDetonate, 0.0f, 8.0f);
    fields.Float("bodyHandSeal", "左手の光の強さ：刀印（霞分身・霞隠れ）", handSeal, 0.0f, 8.0f);
    fields.Float("bodyHandKnife", "左手の光の強さ：投げナイフ（引いた手）", handKnife, 0.0f, 8.0f);
    fields.Float("bodyHandWipe", "左手の光の強さ：刀身をなぞる（炎走り）", handWipe, 0.0f, 8.0f);
    fields.Float("bodyHandAim", "左手の光の強さ：狙い（退き溜めの溜め切り）", handAim, 0.0f, 8.0f);
    fields.Float("bodyHandSeconds", "左手の光：立ち上がる・引く時間 (秒)", handSeconds, 0.01f, 2.0f);
    fields.Float("bodyHandBurst", "左手の弾けの大きさ（倍率。0 で出さない）", handBurst, 0.0f, 6.0f);
    fields.Float3("bodyHandPoint", "左手の光を置く所（左手の模型の座標・実寸 m）", handPoint);
}

void BossBodyLeftHandSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("外形：左手の手振り（見た目だけ）", false))
        return;
    fields.Note("位置は体の足元から局所 m（+X が左、+Z が前）。向きは局所。左手の模型は +Z＝指先、+Y＝手の甲"
                "（掌は -Y）、親指は -X の側です。");
    fields.Float("bodyLeftFrequency", "手のばね：速さ (1 秒あたりの揺れの回数)", frequency, 0.2f, 30.0f);
    fields.Float("bodyLeftDamping", "手のばね：収まりの強さ", damping, 0.05f, 2.0f);
    fields.Float("bodyLeftSnapFrequency", "手のばね：投げる・拳を落とす時の速さ (1 秒あたりの回数)", snapFrequency,
                 0.2f, 40.0f);
    fields.Float("bodyLeftTurnSeconds", "手の向きを追う時間 (秒)", turnSeconds, 0.01f, 1.0f);
    fields.Float("bodyLeftAttachSeconds", "刀に手を添える・離す時間 (秒)", attachSeconds, 0.01f, 1.0f);
    fields.Float3("bodyLeftDetonateRaise", "引爆：掲げた手首の位置", detonateRaise);
    fields.Float3("bodyLeftDetonateChest", "引爆：溜めの終わりの手首の位置（胸の前）", detonateChest);
    fields.Float3("bodyLeftDetonateFist", "引爆：爆ぜ始めに拳を落とす手首の位置", detonateFist);
    fields.Float3("bodyLeftDetonateFingers", "引爆：指先の向き", detonateFingers);
    fields.Float3("bodyLeftDetonateBack", "引爆：手の甲の向き（掌はその反対＝プレイヤーへ）", detonateBack);
    fields.Float("bodyLeftDetonateRaiseShare", "引爆：前隙のこの割合で手を上げ切る (0〜1)", detonateRaiseShare, 0.01f,
                 1.0f);
    fields.Int("bodyLeftDetonateFistHoldFrames", "引爆：後隙へ入った後も拳を握ったままにする長さ (F)",
               detonateFistHoldFrames, 0, 120);
    fields.Float3("bodyLeftKnifePull", "投げナイフ：前隙に引く手首の位置", knifePull);
    fields.Float3("bodyLeftKnifePullFingers", "投げナイフ：引いた時の指先の向き", knifePullFingers);
    fields.Float3("bodyLeftKnifePullBack", "投げナイフ：引いた時の手の甲の向き", knifePullBack);
    fields.Float3("bodyLeftKnifeThrow", "投げナイフ：振り出した手首の位置", knifeThrow);
    fields.Float3("bodyLeftKnifeThrowFingers", "投げナイフ：振り出した指先の向き", knifeThrowFingers);
    fields.Float3("bodyLeftKnifeThrowBack", "投げナイフ：振り出した手の甲の向き", knifeThrowBack);
    fields.Float3("bodyLeftSealWrist", "印（霞分身・霞隠れ）：手首の位置", sealWrist);
    fields.Float3("bodyLeftSealFingers", "印：指先の向き", sealFingers);
    fields.Float3("bodyLeftSealBack", "印：手の甲の向き", sealBack);
    fields.Float("bodyLeftWipeStartShare", "炎走り：刀身をなぞり始める前隙の割合 (0〜1)", wipeStartShare, 0.0f, 1.0f);
    fields.Float("bodyLeftWipeEndShare", "炎走り：なぞり終える前隙の割合 (0〜1)", wipeEndShare, 0.0f, 1.0f);
    fields.Float("bodyLeftWipeFrom", "炎走り：なぞり始める刀身の割合（鍔が 0、剣先が 1）", wipeFrom, 0.0f, 1.0f);
    fields.Float("bodyLeftWipeTo", "炎走り：なぞり終える刀身の割合", wipeTo, 0.0f, 1.0f);
    fields.Float("bodyLeftBladeStart", "刀身の始まり（鍔）：握りから剣先の向きへ (m)", bladeStart, 0.0f, 2.0f);
    fields.Float3("bodyLeftPalmCenter", "左手の模型の掌の真ん中（模型の座標・実寸 m。刀身に付ける所）", palmCenter);
    fields.Float3("bodyLeftAimWrist", "退き溜め：手首の位置", aimWrist);
    fields.Float3("bodyLeftAimFingers", "退き溜め：指先の向き", aimFingers);
    fields.Float3("bodyLeftAimBack", "退き溜め：手の甲の向き（掌はその反対＝プレイヤーへ）", aimBack);
    fields.Float("bodyLeftTwoHandBehind", "両手持ち：握りから柄頭の側へ拳を置く距離 (m)", twoHandBehind, 0.0f, 2.0f);
    fields.Float3("bodyLeftTwoHandBack", "両手持ち：手の甲の向き（刀の座標。+X が刀の左、+Y が棟）", twoHandBack);
    fields.Float3("bodyLeftFistCenter", "左手の拳の真ん中（模型の座標・実寸 m。柄が通る所）", fistCenter);
    fields.Float3("bodyLeftKickWrist", "回し蹴り：手首の位置", kickWrist);
    fields.Float3("bodyLeftKickFingers", "回し蹴り：指先の向き", kickFingers);
    fields.Float3("bodyLeftKickBack", "回し蹴り：手の甲の向き", kickBack);
    fields.Float3("bodyLeftLimpWrist", "気絶・撃破：手首の位置", limpWrist);
    fields.Float3("bodyLeftLimpFingers", "気絶・撃破：指先の向き", limpFingers);
    fields.Float3("bodyLeftLimpBack", "気絶・撃破：手の甲の向き", limpBack);
}

void BossBodyMotionSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("外形：刀と手の動き（見た目だけ）", false))
        return;
    fields.Float("bodySwordFrequency", "刀のばね：戻る速さ (1 秒あたりの揺れの回数)", swordFrequency, 0.2f, 20.0f);
    fields.Float("bodySwordDamping", "刀のばね：収まりの強さ (小さいほど振り抜いた先へ行き過ぎて戻る。1 で戻さない)",
                 swordDamping, 0.05f, 2.0f);
    fields.Float("bodySwordHitFrequency", "刀のばね：振る直前に規則の線へ寄せる速さ (1 秒あたりの回数)",
                 swordHitFrequency, 1.0f, 40.0f);
    fields.Float("bodySwordHitDamping", "刀のばね：振る直前の収まりの強さ", swordHitDamping, 0.1f, 2.0f);
    fields.Float("bodyHitMaxTipGap", "振っている間に許す、剣先と規則の線のずれ (m)", hitMaxTipGap, 0.0f, 0.5f);
    fields.Float("bodyFollowFrequency",
                 "両手の斬りの間、刀と握りが規則の構えを追う速さ (1 秒あたりの回数。行き過ぎない)", followFrequency,
                 0.5f, 30.0f);
    fields.Float("bodyOvershootCarry", "振り抜いた勢いを持ち越す割合 (0〜1。大きいほど振り抜いた先へ行き過ぎる)",
                 overshootCarry, 0.0f, 1.0f);
    fields.Int("bodyLockLeadFrames", "振りの何 F 前から規則の線へ寄せるか (F。溜めの誇張もこの間に消す)",
               lockLeadFrames, 1, 60);
    fields.Float("bodyBlendSeconds", "振った後、手と刀を体の傾きへ戻す時間 (秒)", bodyBlendSeconds, 0.01f, 1.0f);
    fields.Float3("bodyIdleGrip", "待機の構え（下段）：握りの位置 (体の足元から、局所 m。+X が左、+Z が前)", idleGrip);
    fields.Float3("bodyIdleDirection", "待機の構え：剣先の向き (局所)", idleDirection);
    fields.Float("bodyRestBlendDegrees", "待機の構えを混ぜる範囲：規則の剣が下ろした構えから離れる角度 (度)",
                 restBlendDegrees, 1.0f, 180.0f);
    fields.Float("bodyBreathMeters", "待機の息：握りの上下 (m)", breathMeters, 0.0f, 0.3f);
    fields.Float("bodyBreathDegrees", "待機の息：剣先の上下 (度)", breathDegrees, 0.0f, 20.0f);
    fields.Float("bodyBreathFrequency", "待機の息：速さ (1 秒あたりの回数)", breathFrequency, 0.0f, 5.0f);
    fields.Float("bodyPostHoldShare", "後隙のうち、振り終わりの形を保つ割合 (0〜1。残りで待機の構えへ戻す)",
                 postHoldShare, 0.0f, 1.0f);
    fields.Float("bodyChargeReach", "溜め：握りを剣の向きへ送り出す長さ (m)", chargeReach, 0.0f, 2.0f);
    fields.Float("bodyChargeLift", "溜め：握りを持ち上げる高さ (m)", chargeLift, 0.0f, 2.0f);
    fields.Float("bodyChargeTremorMeters", "溜め：構えている間の握りの震え (m)", chargeTremorMeters, 0.0f, 0.2f);
    fields.Float("bodyChargeTremorDegrees", "溜め：構えている間の剣先の震え (度)", chargeTremorDegrees, 0.0f, 10.0f);
    fields.Float("bodyChargeTremorFrequency", "溜め：震えの速さ (1 秒あたりの回数)", chargeTremorFrequency, 0.0f,
                 40.0f);
    fields.Float("bodyGripFrequency", "握りのばね：速さ (1 秒あたりの揺れの回数)", gripFrequency, 0.2f, 20.0f);
    fields.Float("bodyGripDamping", "握りのばね：収まりの強さ", gripDamping, 0.05f, 2.0f);
    fields.Float("bodyArmReach", "振りの中ほどで握りを剣の線に沿って外へ伸ばす長さ (m)", armReach, 0.0f, 2.0f);
    fields.Float("bodyArmReachPeak", "腕が伸び切る、振りの進み (0〜1)", armReachPeak, 0.05f, 1.0f);
    fields.Float("bodyEdgeMinDegreesPerSecond", "刃の向き：動きへ向け始める剣先の速さ (度/秒。遅い時は前の向きを保つ)",
                 edgeMinDegreesPerSecond, 0.0f, 2000.0f);
    fields.Float("bodyEdgeTurnSeconds", "刃の向き：合わせる時間 (秒)", edgeTurnSeconds, 0.005f, 1.0f);
    fields.Float("bodyEdgeIdleSeconds", "刃の向き：待機の向きへ戻る時間 (秒)", edgeIdleSeconds, 0.01f, 5.0f);
    fields.Float3("bodyIdleEdge", "刃の向き：待機 (局所。刀に直交する成分を使う)", idleEdge);
    fields.Float3("bodyChargeEdgeToward", "刃の向き：溜めの間に向ける先＝これから斬る向き (局所)", chargeEdgeToward);
    fields.Float("bodyTwistDegrees", "胴の捻り：刀の横の向き (局所 x) 1 あたりの角度 (度)", twistDegrees, 0.0f, 90.0f);
    fields.Float("bodyTwistMaxDegrees", "胴の捻り：上限 (度)", twistMaxDegrees, 0.0f, 90.0f);
    fields.Float("bodyTwistSeconds", "胴の捻り：追う時間 (秒)", twistSeconds, 0.01f, 2.0f);
    fields.Float("bodyTassetTwistShare", "胴の捻りを草摺に掛ける割合 (0〜1)", tassetTwistShare, 0.0f, 1.0f);
    fields.Float("bodyHeadTurnMaxDegrees", "兜がプレイヤーを向く：横の上限 (度)", headTurnMaxDegrees, 0.0f, 120.0f);
    fields.Float("bodyHeadTurnSeconds", "兜がプレイヤーを向く：追う時間 (秒)", headTurnSeconds, 0.01f, 3.0f);
    fields.Float("bodyLeftBalanceMeters", "左手の釣り合い：刀が待機の構えから外れた量 1 あたり、反対へ動く長さ (m)",
                 leftBalanceMeters, 0.0f, 2.0f);
    fields.Float("bodyLeftBalanceMax", "左手の釣り合い：動く長さの上限 (m)", leftBalanceMax, 0.0f, 2.0f);
    fields.Float("bodyLeftBalanceSeconds", "左手の釣り合い：追う時間 (秒)", leftBalanceSeconds, 0.01f, 2.0f);
    fields.Float("bodyTipGroundClearance", "剣先を地面からこの高さより下へ入れない (m。負で切る)", tipGroundClearance,
                 -1.0f, 2.0f);
    fields.Float("bodyGreatswordGripAlong", "霞の大剣の間：握りを大剣の柄から剣の向きへずらす長さ (m)",
                 greatswordGripAlong, -2.0f, 5.0f);
    fields.Float("bodyGreatswordBlendSeconds", "霞の大剣の間：刀を大剣へ重ねる・離す時間 (秒)", greatswordBlendSeconds,
                 0.01f, 2.0f);
    fields.Bool("bodyTraceMotion", "確かめ用：振っている間、刃の向きと規則の線とのずれを毎フレーム記録に出す",
                traceMotion);
    fields.Note("刀は「見せる構え」をばねで追い、振っている間だけ規則の剣の線（当たり）へ重なります。"
                "当たり・F 数はここでは変わりません。");
}

void BossBodySettings::DescribeShape(HIKARI::PropertyFields& fields) {
    if (!fields.Section("外形（鎧・刀・手の模型）", false))
        return;
    fields.Asset("bodyModel", "外形の模型（部品がノードで分かれた 1 つの模型。空なら出さない）",
                 HIKARI::GameProjectAssetKind::Model, model);
    fields.Bool("bodyVisible", "外形の模型を出す", visible);
    fields.Bool("bodyCastShadow", "外形の模型が影を落とす（切ると軽くなる）", castShadow);
    fields.Float("bodySwordModelLength", "刀の模型の、握りの中央から剣先までの長さ (m。見える刀の長さに合うよう拡大する)",
                 swordModelLength, 0.1f, 20.0f);
    fields.Float("bodySwordVisibleLength",
                 "見える刀の長さ：剣の付け根から剣先まで (m。刀と手の大きさが決まる。当たりの線より先へは伸びない。0 で剣の長さ)",
                 swordVisibleLength, 0.0f, 20.0f);
    fields.Float("bodySwordGripAlong", "握りの位置：剣の付け根から剣の向きへ (m)", swordGripAlong, -1.0f, 2.0f);
    fields.Float("bodyHandScale", "手の大きさ（刀の拡大に掛ける倍率）", handScale, 0.1f, 5.0f);
    fields.Float("bodyScale", "体の大きさ（倍率。鎧・肩・左手の置き場所・待機の握りに掛ける。当たりの型には掛けない）",
                 bodyScale, 0.2f, 5.0f);
    fields.Float3("bodyGripWrist", "握りの右手の模型：手首の位置（刀の座標、模型の実寸 m）", gripWrist);
    fields.Float3("bodyGripElbow", "握りの右手の模型：肘の向き（刀の座標）", gripElbow);
    fields.Float3("bodyGripBack", "握りの右手の模型：手の甲の向き（刀の座標）", gripBack);
    fields.Float3("bodyLeftRestWrist", "左手の待機：手首の位置（体の足元から、局所 m。+X が左、+Z が前）",
                  leftRestWrist);
    fields.Float3("bodyLeftRestFingers", "左手の待機：指先の向き（局所）", leftRestFingers);
    fields.Float3("bodyLeftRestBack", "左手の待機：手の甲の向き（局所）", leftRestBack);
    fields.Float3("bodyShoulder", "肩の位置（体の足元から、局所 m。右肩。左肩は左右を反転）：籠手の筒が向く先",
                  shoulder);
    fields.Float("bodyCuffGripBlend", "右の籠手の筒の向きに、握りの手の模型の肘の向きを混ぜる割合", cuffGripBlend, 0.0f,
                 1.0f);
    fields.Float("bodyCuffModelLength", "籠手の筒の模型の長さ (m。模型を作るスクリプトの値)", cuffModelLength, 0.05f, 5.0f);
    fields.Float("bodyCuffShoulderClearance",
                 "籠手の筒の先を、肩からこれだけ手前で止める (m。体の大きさを掛ける。0 で詰めない)",
                 cuffShoulderClearance, 0.0f, 5.0f);
    fields.Float("bodyCuffMinLength", "籠手の筒を詰める下限（模型の長さに対する割合）", cuffMinLength, 0.0f, 1.0f);
    fields.Float3("bodyTorsoCenter", "鎧：胴の中心（体の足元から、局所 m）", torsoCenter);
    fields.Float3("bodyHeadCenter", "鎧：兜の中心（体の足元から、局所 m）", headCenter);
    fields.Float("bodyHeadScale", "鎧：兜の大きさ（倍率）", headScale, 0.2f, 4.0f);
    fields.Float3("bodySodeHang", "鎧：大袖を吊る所（右の袖。体の足元から、局所 m。左は左右を反転）", sodeHang);
    fields.Float("bodySodeYawDegrees", "鎧：大袖を外へ向ける角度 (度。90 で真横、小さいほど前へ向く)", sodeYawDegrees,
                 0.0f, 180.0f);
    fields.Float("bodySodeFlareDegrees", "鎧：大袖の裾を外へ開く角度 (度)", sodeFlareDegrees, -30.0f, 80.0f);
    fields.Int("bodyTassetCount", "鎧：草摺の枚数", tassetCount, 0, kBossBodyTassetCapacity);
    fields.Float("bodyTassetHangHeight", "鎧：草摺を吊る高さ (m、体の足元から)", tassetHangHeight, 0.0f, 10.0f);
    fields.Float("bodyTassetRadiusSide", "鎧：草摺を吊る腰の半径・横 (m)", tassetRadiusSide, 0.05f, 5.0f);
    fields.Float("bodyTassetRadiusFront", "鎧：草摺を吊る腰の半径・前後 (m)", tassetRadiusFront, 0.05f, 5.0f);
    fields.Float("bodyTassetFlareDegrees", "鎧：草摺の裾を外へ開く角度 (度)", tassetFlareDegrees, -30.0f, 80.0f);
    fields.Vfx("bodySmokeEffect", "胴の煙のエフェクト（ボスの足元に付く流体。空なら出さない）", smokeEffect);
    fields.Float("bodySmokeAmount", "胴の煙の量（倍率）", smokeAmount, 0.0f, 8.0f);
    fields.Float("bodySmokeHeatPhaseTwo", "フェーズ 2 で胴の煙を熱くする倍率（熱い煙は橙に光る。1 で光らない）",
                 smokeHeatPhaseTwo, 0.0f, 64.0f);
    fields.Float("bodySmokeAmountStunned", "気絶の間の胴の煙の量（倍率）", smokeAmountStunned, 0.0f, 8.0f);
    fields.Float("bodySwayDegreesPerSpeed", "揺れ：体の速さ 1 m/s あたり、大袖・草摺が後ろへ振れる角度 (度)",
                 swayDegreesPerSpeed, 0.0f, 30.0f);
    fields.Float("bodySwayMaxDegrees", "揺れ：振れの上限 (度)", swayMaxDegrees, 0.0f, 90.0f);
    fields.Float("bodySwayFrequency", "揺れ：戻る速さ (1 秒あたりの揺れの回数)", swayFrequency, 0.1f, 10.0f);
    fields.Float("bodySwayDamping", "揺れ：収まりの強さ (小さいほど長く揺れ、1 で揺れ戻さない)", swayDamping, 0.05f,
                 2.0f);
    fields.Float("bodyFlutterDegrees", "揺れ：止まっている時の小さな揺らぎ (度)", flutterDegrees, 0.0f, 20.0f);
    fields.Float("bodyFlutterFrequency", "揺れ：揺らぎの速さ (1 秒あたりの回数)", flutterFrequency, 0.0f, 10.0f);
    fields.Float("bodyHitKickDegrees", "揺れ：撃たれた時に後ろへ弾かれる角度 (度)", hitKickDegrees, 0.0f, 90.0f);
    fields.Note("刀と右手は、振っている間は剣の姿勢（剣の付け根 Boss.swordShoulder と剣の向き）に重なり、"
                "ほかの時は「刀と手の動き」の節の構えを追います。"
                "剣の長さは Boss.swordLength です。「握りの右手の模型」の 3 つは、模型を作り直した時にだけ、"
                "作るスクリプトが出す値へ合わせます。");
}

} // namespace Project_TD2_2
