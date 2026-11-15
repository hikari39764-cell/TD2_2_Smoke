#include "BossBodySettings.h"

#include "HIKARI/Project/AssetKind.h"

namespace Project_TD2_2 {

void BossBodySettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("外形（鎧・刀・手の模型）", false))
        return;
    fields.Asset("bodyModel", "外形の模型（部品がノードで分かれた 1 つの模型。空なら出さない）",
                 HIKARI::GameProjectAssetKind::Model, model);
    fields.Bool("bodyVisible", "外形の模型を出す", visible);
    fields.Float("bodySwordModelLength", "刀の模型の、握りの中央から剣先までの長さ (m。剣の長さに合うよう拡大する)",
                 swordModelLength, 0.1f, 20.0f);
    fields.Float("bodySwordGripAlong", "握りの位置：剣の付け根から剣の向きへ (m)", swordGripAlong, -1.0f, 2.0f);
    fields.Float("bodyHandScale", "手の大きさ（刀の拡大に掛ける倍率）", handScale, 0.1f, 5.0f);
    fields.Note("刀と手は、剣の姿勢（剣の付け根 Boss.swordShoulder と剣の向き）に付いて動きます。"
                "剣の長さは Boss.swordLength です。");
}

} // namespace Project_TD2_2
