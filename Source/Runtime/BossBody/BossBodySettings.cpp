#include "BossBodySettings.h"

#include "HIKARI/Project/AssetKind.h"

namespace Project_TD2_2 {

void BossBodySettings::Describe(HIKARI::PropertyFields& fields) {
    DescribeShape(fields);
    motion.Describe(fields);
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
    fields.Float("bodySwordModelLength", "刀の模型の、握りの中央から剣先までの長さ (m。剣の長さに合うよう拡大する)",
                 swordModelLength, 0.1f, 20.0f);
    fields.Float("bodySwordGripAlong", "握りの位置：剣の付け根から剣の向きへ (m)", swordGripAlong, -1.0f, 2.0f);
    fields.Float("bodyHandScale", "手の大きさ（刀の拡大に掛ける倍率）", handScale, 0.1f, 5.0f);
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
