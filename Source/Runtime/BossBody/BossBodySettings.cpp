#include "BossBodySettings.h"

#include "HIKARI/Project/AssetKind.h"

namespace Project_TD2_2 {

void BossBodySettings::Describe(HIKARI::PropertyFields& fields) {
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
    fields.Note("刀と右手は、剣の姿勢（剣の付け根 Boss.swordShoulder と剣の向き）に付いて動きます。"
                "剣の長さは Boss.swordLength です。「握りの右手の模型」の 3 つは、模型を作り直した時にだけ、"
                "作るスクリプトが出す値へ合わせます。");
}

} // namespace Project_TD2_2
