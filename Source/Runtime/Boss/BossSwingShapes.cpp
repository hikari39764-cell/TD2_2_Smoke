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
    fields.Float(k + "Crouch", l + "：腰を落とす量 (m。見た目だけ)", crouch, 0.0f, 3.0f);
    fields.Float(k + "Lean", l + "：前へ傾ける角度 (度。見た目だけ)", lean, -30.0f, 30.0f);
    fields.Float(k + "CrouchTo", l + "：振り終わりの腰の落とし (m。構えから寄せる。見た目だけ)", crouchTo, 0.0f, 3.0f);
    fields.Float(k + "LeanTo", l + "：振り終わりの前傾 (度。構えから寄せる。見た目だけ)", leanTo, -30.0f, 30.0f);
    fields.Float(k + "RaiseVia", l + "：振り上げの道（型の振りのこの進みを通って構えへ。0 は直に）", raiseVia, 0.0f,
                 1.0f);
    fields.Float(k + "ReturnLift", l + "：戻りの道を中段へ寄せる強さ（0〜1。0 は振り終わりからそのまま構えへ）",
                 returnLift, 0.0f, 1.0f);
    fields.Float(k + "ReturnPeakShare", l + "：戻りが最も速い所（戻りの長さに対する割合。小さいほど早く戻り始める）",
                 returnPeakShare, 0.1f, 0.9f);
    fields.Float(k + "EdgeRollDegrees", l + "：刃の面（斬る向きを刀身の軸のまわりに回す角度、度）", edgeRollDegrees,
                 -90.0f, 90.0f);
}

void BossSwingSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("斬りの型（両手で振る。当たりと見える刀の両方）", false))
        return;
    fields.Note(
        "振りの支点を通る軸のまわりに、両手の握りと刀を一緒に回します。当たりの線は握りから刀の向きへ伸びるので、"
        "見える刀とそのまま重なります。体の捻り・腰の落とし・前傾は見た目だけです。");
    horizontal.Describe(fields, "swingHorizontal", "横なぎ");
    kesa.Describe(fields, "swingKesa", "袈裟（逆袈裟は左右に映す）");
    vertical.Describe(fields, "swingVertical", "縦");
    dash.Describe(fields, "swingDash", "突進の払い（突進横なぎ。退き払いは左右に映す）");
    overhead.Describe(fields, "swingOverhead", "大上段（上段溜め）");
    fields.Float("swingSpeedLeanMaxDegrees", "両手の斬りの間、動く速さで体を傾ける上限 (度。見た目だけ)",
                 speedLeanMaxDegrees, 0.0f, 30.0f);
}

} // namespace Project_TD2_2
