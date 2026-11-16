#include "BossSwingShapes.h"

#include <string>

namespace Project_TD2_2 {

void BossSwingShape::Describe(HIKARI::PropertyFields& fields, std::string_view key, std::string_view label) {
    const std::string k(key), l(label);
    fields.Float3(k + "Pivot", l + "：振りの支点（体の足元から、局所 m）", pivot);
    fields.Float3(k + "Axis", l + "：振りの軸（局所の向き。この軸のまわりに手と刀が回る）", axis);
    fields.Float3(k + "Grip", l + "：振りの真ん中（0 度）の握り（体の足元から、局所 m）", grip);
    fields.Float3(k + "Direction", l + "：振りの真ん中の刀の向き（局所）", direction);
    fields.Float(k + "FromDegrees", l + "：構えの角度（振りの軸のまわり、度）", fromDegrees, -180.0f, 180.0f);
    fields.Float(k + "ToDegrees", l + "：振り終わりの角度（度）", toDegrees, -180.0f, 180.0f);
    fields.Float(k + "WristFromDegrees", l + "：構えで刀が手より回っている角度（負で遅れる、度）", wristFromDegrees,
                 -120.0f, 120.0f);
    fields.Float(k + "WristToDegrees", l + "：振り終わりで刀が手より回っている角度（度）", wristToDegrees, -120.0f,
                 120.0f);
    fields.Float(k + "TwistFromDegrees", l + "：構えの胴の捻り（度。+ で左へ）", twistFromDegrees, -90.0f, 90.0f);
    fields.Float(k + "TwistToDegrees", l + "：振り終わりの胴の捻り（度）", twistToDegrees, -90.0f, 90.0f);
    fields.Float(k + "Crouch", l + "：腰を落とす量 (m。見た目だけ)", crouch, 0.0f, 1.0f);
    fields.Float(k + "Lean", l + "：前へ傾ける角度 (度。見た目だけ)", lean, -30.0f, 30.0f);
}

void BossSwingSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("斬りの型（両手で振る。当たりと見える刀の両方）", false))
        return;
    fields.Note(
        "振りの支点を通る軸のまわりに、両手の握りと刀を一緒に回します。当たりの線は握りから刀の向きへ伸びるので、"
        "見える刀とそのまま重なります。体の捻り・腰の落とし・前傾は見た目だけです。");
    horizontal.Describe(fields, "swingHorizontal", "横なぎ");
}

} // namespace Project_TD2_2
