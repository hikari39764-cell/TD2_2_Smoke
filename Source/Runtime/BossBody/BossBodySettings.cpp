#include "BossBodySettings.h"

namespace Project_TD2_2 {

void BossBodySettings::Describe(HIKARI::PropertyFields& fields) {
    DescribeShape(fields);
    accents.Describe(fields);
}

void BossBodyAccentSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("体：技の演出（見た目だけ）", false))
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
    fields.Float("bodyBladeEmbers", "燃える刀身：刀身から立つ火の粉の量（倍率。0 で出さない）", bladeEmbers, 0.0f,
                 8.0f);
    fields.Vfx("bodyBladeFireEffect",
               "燃える刀身：流体の炎のエフェクト（刃に付いて動く流体域。空ならシェーダーの炎の帯だけ）",
               bladeFireEffect);
    fields.Float("bodyBladeFireAmount", "…流体の炎の燃料の量（倍率。0 で出さない）", bladeFireAmount, 0.0f, 4.0f);
    fields.Float("bodyBladeFlameShare", "…流体の炎を出している間に残す、シェーダーの炎の帯の割合（0 で消す）",
                 bladeFlameShare, 0.0f, 1.0f);
    fields.Float("bodyBladeFireGlowShare", "…流体の炎を出している間に残す、刀身の光の層の割合", bladeFireGlowShare,
                 0.0f, 1.0f);
    fields.Float(
        "bodyBladeFlameFastSpeed",
        "…剣先がこの速さ (m/秒) に近づくほど、シェーダーの炎の帯を元の強さへ戻す（流体の炎は速い振りで剥がれる。"
        "0 で戻さない）",
        bladeFlameFastSpeed, 0.0f, 200.0f);
    fields.Float("bodyBladeFlameReturnSeconds", "…振り終わった後、シェーダーの炎の帯を下げて流体の炎へ渡す時間 (秒)",
                 bladeFlameReturnSeconds, 0.01f, 3.0f);
    fields.Vfx("bodyEyesEffect", "目の光のエフェクト（兜に付く。尾を引く光・暈・閃き。空なら出さない）", eyesEffect);
    fields.Float3("bodyEyesMistColor", "目の光：ふだんの色と明るさ", eyesMistColor);
    fields.Float3("bodyEyesFireColor", "目の光：フェーズ 2 の色と明るさ", eyesFireColor);
    fields.Float("bodyEyeFlare", "目の閃き：ふつうの技を出すと決めた瞬間の強さ（0 で出さない）", eyeFlare, 0.0f, 8.0f);
    fields.Float("bodyEyeFlareSeconds", "目の閃き：消えるまでの時間 (秒)", eyeFlareSeconds, 0.01f, 3.0f);
    fields.SceneObject("bodyHelmSpotObject", "兜の中の灯：前を向くスポットライトの物体（無ければ貼り付けの光を出す）",
                       helmSpotObject);
    fields.SceneObject("bodyHelmFillObject", "兜の中の灯：兜の中を満たす点光の物体", helmFillObject);
    fields.Float("bodyHelmGlowStrength", "面頬の隙間の中の光：登場の間の明るさ（画面基準。1 で白）", helmGlowStrength,
                 0.0f, 64.0f);
    fields.Float("bodyHelmSpotIntensity", "兜の中の灯：スポットライトの明るさ（場面の光に対する倍率）",
                 helmSpotIntensity, 0.0f, 100.0f);
    fields.Float("bodyHelmFillIntensity", "兜の中の灯：点光の明るさ（場面の光に対する倍率）", helmFillIntensity, 0.0f,
                 100.0f);
    fields.Float3("bodyHelmSpotLocal", "兜の中の灯：スポットライトの置き場所（兜の局所。+Z が前）", helmSpotLocal);
    fields.Float3("bodyHelmFillLocal", "兜の中の灯：点光の置き場所（兜の局所）", helmFillLocal);
    fields.Float("bodyHelmSpotPitchDegrees", "兜の中の灯：スポットライトを下へ向ける角度（度）", helmSpotPitchDegrees,
                 -45.0f, 45.0f);
    fields.Float("bodyHelmFlareGain", "兜の中の灯：閃きを明るさに足す倍率", helmFlareGain, 0.0f, 10.0f);
    fields.Float("bodyHelmLightFlicker", "兜の中の灯：ゆらぎの幅（0 で揺らがない）", helmLightFlicker, 0.0f, 1.0f);
    fields.Float3("bodyEdgeMistColor", "刃口の光：霞の色と明るさ（段階の倍率を掛ける）", edgeMistColor);
    fields.Float3("bodyEdgeFireColor", "刃口の光：刀が燃えている時の色と明るさ", edgeFireColor);
    fields.Float("bodyEdgeIdle", "刃口の光：待機の明るさ（倍率。弱いが必ず見える）", edgeIdle, 0.0f, 8.0f);
    fields.Float("bodyEdgeWindup", "刃口の光：前隙の終わり（構え切った所）の明るさ", edgeWindup, 0.0f, 8.0f);
    fields.Float("bodyEdgeSwing", "刃口の光：振る間（命中の窓）の明るさ", edgeSwing, 0.0f, 8.0f);
    fields.Float("bodyEdgeStunned", "刃口の光：気絶の間の明るさ", edgeStunned, 0.0f, 8.0f);
    fields.Float("bodyEdgeRampPower", "刃口の光：前隙の立ち上がりの曲がり（1 で一定、大きいほど終わりに急に明るく）",
                 edgeRampPower, 0.2f, 6.0f);
    fields.Float("bodyEdgePulseDepth", "刃口の光：前隙の脈の深さ（0〜1。0 で脈打たない）", edgePulseDepth, 0.0f, 1.0f);
    fields.Float("bodyEdgePulseFrequency", "刃口の光：前隙の脈の速さ（1 秒あたりの回数）", edgePulseFrequency, 0.0f,
                 10.0f);
    fields.Float("bodyEdgeHotShare", "刃口の光：燃える刀身の間に掛ける割合 (0〜1)", edgeHotShare, 0.0f, 1.0f);
    fields.Float("bodyEdgeRiseSeconds", "刃口の光：明るくなる時間 (秒)", edgeRiseSeconds, 0.005f, 2.0f);
    fields.Float("bodyEdgeFallSeconds", "刃口の光：暗くなる時間 (秒)", edgeFallSeconds, 0.005f, 2.0f);
    fields.Float("bodyEdgeDark", "刃のまわりの暗い縁：濃さ（倍率。0 で出さない）", edgeDark, 0.0f, 2.0f);
    fields.Float("bodyHazeBoost", "霞の技（剣気を飛ばす）の間、刃文の流れ・霞の帯を強める倍率", hazeBoost, 0.0f, 8.0f);
    fields.Float("bodyRealEdgeShare", "実体の刀の技の間、刃口の光に掛ける割合", realEdgeShare, 0.0f, 1.0f);
    fields.Float("bodyRealFlowShare", "実体の刀の技の間、刃文の流れ・霞の帯に掛ける割合", realFlowShare, 0.0f, 1.0f);
    fields.Float3("bodyFlowMistColor", "刃文の流れ：霞の色と明るさ", flowMistColor);
    fields.Float3("bodyFlowFireColor", "刃文の流れ：刀が燃えている時の色と明るさ", flowFireColor);
    fields.Float("bodyFlowIdle", "刃文の流れ：明るさ（倍率。0 で出さない）", flowIdle, 0.0f, 8.0f);
    fields.Float3("bodyMistMistColor", "霞の帯：霞の色と明るさ", mistMistColor);
    fields.Float3("bodyMistFireColor", "霞の帯：刀が燃えている時の色と明るさ", mistFireColor);
    fields.Float("bodyMistIdle", "霞の帯：明るさ（倍率。0 で出さない）", mistIdle, 0.0f, 8.0f);
    fields.Float3("bodyWakeMistColor", "剣先の残光：霞の色と明るさ", wakeMistColor);
    fields.Float3("bodyWakeFireColor", "剣先の残光：刀が燃えている時の色と明るさ", wakeFireColor);
    fields.Float("bodyWakeGain", "剣先の残光：明るさ（倍率。0 で出さない）", wakeGain, 0.0f, 8.0f);
    fields.Float("bodyWakeMinTipSpeed", "剣先の残光：出始める剣先の速さ (m/秒)", wakeMinTipSpeed, 0.0f, 100.0f);
    fields.Float("bodyWakeFullTipSpeed", "剣先の残光：いちばん明るくなる剣先の速さ (m/秒)", wakeFullTipSpeed, 0.1f,
                 200.0f);
    fields.Float("bodyWakeGroundFade", "剣先の残光：地面の近くで弱める高さ (m)", wakeGroundFade, 0.0f, 10.0f);
    fields.Float("bodyReleaseGlint", "剣気を放つ瞬間：刀身を走る光の明るさ（倍率。0 で出さない）", releaseGlint, 0.0f,
                 8.0f);
    fields.Int("bodyReleaseHoldTicks", "剣気を放つ瞬間：時間を止める長さ (F。0 で止めない)", releaseHoldTicks, 0, 10);
    fields.Float("bodyReleaseHoldScale", "剣気を放つ瞬間：止める間の時間の倍率 (0〜1)", releaseHoldScale, 0.0f, 1.0f);
    fields.Vfx("bodyGlintEffect", "刀身を走る光のエフェクト（刀を振る技を出すと決めた瞬間。空なら出さない）",
               glintEffect);
    fields.Float3("bodyGlintMistColor", "刀身を走る光：霞の技の色と明るさ", glintMistColor);
    fields.Float3("bodyGlintFireColor", "刀身を走る光：刀が燃えている技の色と明るさ", glintFireColor);
    fields.Vfx("bodyAuraEffect", "溜めの気配のエフェクト（溜めている間、体を包んで立ちのぼる。空なら出さない）",
               auraEffect);
    fields.Float3("bodyAuraMistColor", "溜めの気配：霞の技の色と明るさ", auraMistColor);
    fields.Float3("bodyAuraFireColor", "溜めの気配：刀が燃えている技の色と明るさ", auraFireColor);
    fields.Float("bodyAuraSeconds", "溜めの気配：立ち上がる・引く時間 (秒)", auraSeconds, 0.01f, 3.0f);
    fields.Float("bodySmokeChargeAmount", "胴の煙：溜め切った時の量（倍率）", smokeChargeAmount, 0.0f, 8.0f);
    fields.Float("bodySmokeNearScale", "胴の煙：プレイヤーが近い時の量（倍率。画面を塞がないように）", smokeNearScale,
                 0.0f, 1.0f);
    fields.Float("bodySmokeNearMeters", "胴の煙：この距離 (m) 以内で近い時の量", smokeNearMeters, 0.0f, 40.0f);
    fields.Float("bodySmokeFarMeters", "胴の煙：この距離 (m) 以遠で元の量", smokeFarMeters, 0.0f, 40.0f);
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
    fields.Float("bodyShockKick", "衝撃の輪の大きさ：回し蹴り", shockKick, 0.0f, 6.0f);
    fields.Vfx("bodyDashEffect", "突進の尾のエフェクト（体が速く動く間の光の帯とゆがみ。空なら出さない）", dashEffect);
    fields.Float3("bodyDashMistColor", "突進の尾：ふだんの色と明るさ", dashMistColor);
    fields.Float3("bodyDashFireColor", "突進の尾：刀が燃えている間の色と明るさ", dashFireColor);
    fields.Float("bodyDashMinSpeed", "突進の尾：出始める体の速さ (m/秒)", dashMinSpeed, 0.0f, 100.0f);
    fields.Float("bodyDashFullSpeed", "突進の尾：出切る体の速さ (m/秒)", dashFullSpeed, 0.1f, 100.0f);
    fields.Vfx("bodyHandEffect", "左手の光のエフェクト（投げナイフの前隙の間、掌に集まる光。空なら出さない）",
               handEffect);
    fields.Vfx("bodyHandBurstEffect", "左手の弾けのエフェクト（ナイフを投げる瞬間。空なら出さない）", handBurstEffect);
    fields.Float3("bodyHandMistColor", "左手の光：色と明るさ", handMistColor);
    fields.Float("bodyHandKnife", "左手の光の強さ（投げナイフの引いた手）", handKnife, 0.0f, 8.0f);
    fields.Float("bodyHandSeconds", "左手の光：立ち上がる・引く時間 (秒)", handSeconds, 0.01f, 2.0f);
    fields.Float("bodyHandBurst", "左手の弾けの大きさ（倍率。0 で出さない）", handBurst, 0.0f, 6.0f);
    fields.Float3("bodyHandPoint", "左手の光を置く所（左手の模型の座標・実寸 m）", handPoint);
    fields.Vfx("bodyGatherMistEffect",
               "範囲爆発の溜め：左手へ霧を集めるエフェクト（手に付いて動く流体域。空なら出さない）", gatherMistEffect);
    fields.Vfx("bodyGatherFireEffect", "…剣が燃えている技で、火を集めるエフェクト（空なら霧と同じ）", gatherFireEffect);
    fields.Vfx("bodyGatherPinchEffect", "…掌へ押し縮める瞬間のエフェクト（内へ縮む輪・一閃。空なら出さない）",
               gatherPinchEffect);
    fields.Int("bodyGatherLeadFrames", "…叩きつけの何 F 前から集め始めるか", gatherLeadFrames, 1, 600);
    fields.Int("bodyGatherPinchFrames", "…叩きつけの何 F 前に押し縮めるか（集めるのを止める）", gatherPinchFrames, 0,
               600);
    fields.Float("bodyGatherCore", "…掌の芯の光：集め終わりの明るさ（倍率）", gatherCore, 0.0f, 8.0f);
    fields.Float("bodyGatherCorePinched", "…掌の芯の光：押し縮めた後の明るさ（倍率）", gatherCorePinched, 0.0f, 8.0f);
    fields.Float("bodyGatherCoreFlicker", "…押し縮めた後の芯の細かい明滅の深さ（0 で明滅なし）", gatherCoreFlicker,
                 0.0f, 1.0f);
    fields.Float3("bodyGatherMistColor", "…押し縮める瞬間の色（霞）", gatherMistColor);
    fields.Float3("bodyGatherFireColor", "…押し縮める瞬間の色（火）", gatherFireColor);
    fields.Float("bodyGatherFogPull", "…集めている間、足元の霧と場の火を手の真下へ寄せる加速度 (m/s²。0 は寄せない)",
                 gatherFogPull, 0.0f, 400.0f);
    fields.Float("bodyGatherFogPullRadius", "…寄せる輪の半径 (m)", gatherFogPullRadius, 0.5f, 20.0f);
    fields.Int("bodyGatherArms", "…掌へ流れ込む腕の本数", gatherArms, 1, 8);
    fields.Float("bodyGatherArmStrength", "…腕の濃さ（中心での毎秒の密度）", gatherArmStrength, 0.0f, 5000.0f);
    fields.Float("bodyGatherArmRadius", "…腕の太さ (m)", gatherArmRadius, 0.05f, 2.0f);
    fields.Float("bodyGatherArmReach", "…腕が立ちのぼる地面の輪の半径（手の真下から m）", gatherArmReach, 0.5f, 10.0f);
    fields.Float("bodyGatherArmTurns", "…掌へ着くまでに回る回数", gatherArmTurns, 0.0f, 4.0f);
    fields.Float("bodyGatherArmRate", "…1 秒に流れ込む回数（集め始め）", gatherArmRate, 0.1f, 10.0f);
    fields.Float("bodyGatherArmRateEnd", "…1 秒に流れ込む回数（押し縮める直前。大きいほど終わりが速い）",
                 gatherArmRateEnd, 0.1f, 10.0f);
    fields.Float("bodyGatherArmHeatMist", "…腕の温度（霞。高いほど光る）", gatherArmHeatMist, 0.0f, 10.0f);
    fields.Float("bodyGatherArmHeatFire", "…腕の温度（火）", gatherArmHeatFire, 0.0f, 10.0f);
    fields.Float("bodyGatherArmLift", "…腕の昇り方（大きいほど、地面を長く這ってから一気に昇る）", gatherArmLift, 0.2f,
                 6.0f);
    fields.Float("bodyGatherArmSpread", "…地面の輪の半径のばらつき（0〜1）", gatherArmSpread, 0.0f, 1.0f);
}

void BossBodySettings::DescribeShape(HIKARI::PropertyFields& fields) {
    if (!fields.Section("体（大きさ・胴の煙と火・フェーズ 2 の場）", false))
        return;
    fields.Bool("bodyInterpolate", "体を前の tick との間で補間して置く（描画が tick より速い時のがくつきを消す）",
                interpolate);
    fields.Float("bodySwordModelLength", "刀の模型の、握りの中央から剣先までの長さ (m。見える刀の長さに合うよう拡大する)",
                 swordModelLength, 0.1f, 20.0f);
    fields.Float("bodySwordVisibleLength",
                 "見える刀の長さ：剣の付け根から剣先まで (m。刀と手の大きさが決まる。当たりの線より先へは伸びない。0 で剣の長さ)",
                 swordVisibleLength, 0.0f, 20.0f);
    fields.Float("bodySwordGripAlong", "握りの位置：剣の付け根から剣の向きへ (m)", swordGripAlong, -1.0f, 2.0f);
    fields.Float("bodyHandScale", "手の大きさ（刀の拡大に掛ける倍率）", handScale, 0.1f, 5.0f);
    fields.Float("bodyScale", "体の大きさ（倍率。鎧に付く効果の大きさに掛ける。当たりの型には掛けない）", bodyScale,
                 0.2f, 5.0f);
    fields.Float("bodyCuffModelLength", "籠手の筒の模型の長さ (m。模型を作るスクリプトの値)", cuffModelLength, 0.05f,
                 5.0f);
    fields.Float("bodyHeadScale", "鎧：兜の大きさ（倍率）", headScale, 0.2f, 4.0f);
    fields.Int("bodyTassetCount", "鎧：草摺の枚数", tassetCount, 0, kBossBodyTassetCapacity);
    fields.Vfx("bodySmokeEffect", "胴の煙のエフェクト（ボスの足元に付く流体。空なら出さない）", smokeEffect);
    fields.Float("bodySmokeAmount", "胴の煙の量（倍率）", smokeAmount, 0.0f, 8.0f);
    fields.Float("bodySmokeHeatPhaseTwo", "フェーズ 2 で胴の煙を熱くする倍率（熱い煙は橙に光る。1 で光らない）",
                 smokeHeatPhaseTwo, 0.0f, 64.0f);
    fields.Float("bodySmokeAmountStunned", "気絶の間の胴の煙の量（倍率）", smokeAmountStunned, 0.0f, 8.0f);
    fields.Float("bodySpineSmoke", "骨に付いて行く煙：背骨（腰から胸）の濃さ（毎秒。0 で出さない）", spineSmoke, 0.0f,
                 500.0f);
    fields.Float("bodySpineSmokeRadius", "骨に付いて行く煙：背骨の半径（1 倍の体での m）", spineSmokeRadius, 0.02f,
                 2.0f);
    fields.Float("bodyArmSmoke", "骨に付いて行く煙：両腕（肩から袖口）の濃さ（毎秒。0 で出さない）", armSmoke, 0.0f,
                 500.0f);
    fields.Float("bodyArmSmokeRadius", "骨に付いて行く煙：腕の半径（1 倍の体での m）", armSmokeRadius, 0.02f, 2.0f);
    fields.Float("bodySmokeTilt", "胴の煙を体の傾きに付ける割合（変わり目の演出の間。0 倒さない 〜 1 背骨の向きまで）",
                 smokeTilt, 0.0f, 1.0f);
    fields.Float("bodySmokeTiltMaxDegrees", "胴の煙を倒す角度の上限（度）", smokeTiltMaxDegrees, 0.0f, 85.0f);
    fields.Float("bodySmokeTiltFollow", "胴の煙が体の傾きに寄る速さ（1/秒）", smokeTiltFollow, 0.5f, 60.0f);
    fields.Float("bodySmokeCalmFollow", "胴の煙が、体の細かい揺れへ寄る速さ（1/秒。小さいほど煙が落ち着く）",
                 smokeCalmFollow, 0.0f, 20.0f);
    fields.Float("bodySmokeHipsHeight", "胴の煙の資産の中の腰の高さ（足元から m。ここを軸に倒す）", smokeHipsHeight,
                 0.0f, 8.0f);
    fields.Float("bodySmokeTiltCombat", "戦いの間も、胴の煙を体の傾きに付ける割合（0 立ったまま 〜 1 背骨の向きまで）",
                 smokeTiltCombat, 0.0f, 1.0f);
    fields.Float("bodySmokeTiltCombatFollow", "戦いの間、胴の煙が体の傾きに寄る速さ（1/秒）", smokeTiltCombatFollow,
                 0.5f, 60.0f);
    fields.Float("bodyArmSmokeFlow", "骨に付いて行く煙：腕の煙が手の方へ流れる速さ (m/秒)", armSmokeFlow, 0.0f, 10.0f);
    fields.Bool("bodyArmorHot", "フェーズ 2 の本体の鎧を灼けた鎧（継ぎ目と隙間が熾火に光る）に差し替える", armorHot);
    fields.Float("bodyFireRiseSeconds", "フェーズ 2 の燃える胴：火が付く・消える時間 (秒)", fireRiseSeconds, 0.0f,
                 10.0f);
    fields.Float("bodySmokeAmountPhaseTwo", "フェーズ 2 の燃える胴：胴の煙の量（倍率）", smokeAmountPhaseTwo, 0.0f,
                 4.0f);
    fields.Float("bodyFireBurn", "フェーズ 2 の燃える胴：火の粉と熱のゆがみの量（倍率。0 で出さない）", fireBurn, 0.0f,
                 4.0f);
    fields.Float("bodyFireStunnedShare", "フェーズ 2 の燃える胴：気絶の間の火の強さ（割合）", fireStunnedShare, 0.0f,
                 1.0f);
    fields.Float3("bodyFireCollar", "火の舌：襟の口（胴の中心から、1 倍の体の局所 m。右。左は左右を反転）", fireCollar);
    fields.Float("bodyFireCollarSpread", "火の舌：襟の片側 2 本の前後の間隔 (1 倍の体の m。0 で 1 本に重なる)",
                 fireCollarSpread, 0.0f, 1.0f);
    fields.Float("bodyFireCuffAlong", "火の舌：袖口の位置（籠手の筒の手首 0 〜 肘の側の口 1）", fireCuffAlong, 0.0f,
                 1.5f);
    fields.Float("bodyFireTongueDensity", "火の舌：1 秒に入れる燃料の濃さ（襟。0 で出さない）", fireTongueDensity, 0.0f,
                 500.0f);
    fields.Float("bodyFireTongueRadius", "火の舌：入れる半径 (1 倍の体の m。体の大きさを掛ける)", fireTongueRadius,
                 0.02f, 2.0f);
    fields.Float("bodyFireTongueHeat",
                 "火の舌：入れる燃料の温度（0.3 で燃え始める。燃えて熱くなった煤が 1.5 から光り、4.5 で橙黄）",
                 fireTongueHeat, 0.0f, 50.0f);
    fields.Float("bodyFireTongueRise", "火の舌：上へ吹き出す速さ (m/秒)", fireTongueRise, 0.0f, 20.0f);
    fields.Float("bodyFireCuffShare", "火の舌：袖口の強さ（襟に対する割合。0 で出さない）", fireCuffShare, 0.0f, 4.0f);
    fields.Float("bodyFireHemShare", "火の舌：腰の周りの 3 本の強さ（襟に対する割合。0 で出さない）", fireHemShare,
                 0.0f, 4.0f);
    fields.Float("bodyFireHemRadius", "火の舌：腰の周りの火の半径（襟の火の半径に対する倍率）", fireHemRadius, 0.2f,
                 4.0f);
    fields.Float("bodyFireHemRing", "火の舌：腰の周りの火を置く、腰からの距離 (1 倍の体の m)", fireHemRing, 0.0f, 2.0f);
    fields.Float("bodyFireHemDrop", "火の舌：腰の周りの火を、腰から下げる高さ (1 倍の体の m)", fireHemDrop, 0.0f, 2.0f);
    fields.Vfx("bodyFieldEffect", "フェーズ 2 の場：漂う火の粉と焦げた輪のエフェクト（空なら出さない）", fieldEffect);
    fields.Float("bodyFieldWarmSeconds", "フェーズ 2 の場：場の色が変わる・戻る時間 (秒)", fieldWarmSeconds, 0.0f,
                 30.0f);
    fields.Float3("bodyFieldShadowsTint", "フェーズ 2 の場の色：暗部の色味（倍率。1 で変えない）", fieldShadowsTint);
    fields.Float3("bodyFieldMidtonesTint", "フェーズ 2 の場の色：中間の色味（倍率。1 で変えない）", fieldMidtonesTint);
    fields.Float3("bodyFieldHighlightsTint", "フェーズ 2 の場の色：明部の色味（倍率。1 で変えない）",
                  fieldHighlightsTint);
    fields.Float("bodyFieldEmbers", "フェーズ 2 の場：漂う火の粉の量（倍率。0 で出さない）", fieldEmbers, 0.0f, 8.0f);
    fields.Float("bodyFieldScorch", "フェーズ 2 の場：焦げた輪の濃さ（0 で出さない）", fieldScorch, 0.0f, 1.0f);
    fields.Float("bodyFieldScorchGlow", "フェーズ 2 の場：焦げた輪に残る熾火の明るさ（倍率）", fieldScorchGlow, 0.0f,
                 8.0f);
    fields.Float("bodyFieldScorchSize", "フェーズ 2 の場：焦げた輪の大きさ（倍率。1 で直径 約 9 m）", fieldScorchSize,
                 0.1f, 4.0f);
    fields.Float("bodyFieldScorchSeconds", "フェーズ 2 の場：焦げた輪が現れる時間 (秒)", fieldScorchSeconds, 0.0f,
                 30.0f);
    fields.Note("体は骨格の体（「骨格の体」の節）が描きます。ここの値は、体に付く効果（刀光・胴の煙・火の舌・兜の光）の"
                "大きさと置き場所です。剣の長さは Boss.swordLength です。");
}

} // namespace Project_TD2_2
