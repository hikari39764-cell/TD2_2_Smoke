#include "CloudMountComponent.h"

namespace Project_TD2_2 {

void CloudMountComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("召喚", true)) {
        fields.Note("Gameplay.Mount（F／LB）を押すたびに、乗る・降りるを切り替えます。地上でも落下中でも呼べます"
                    "（通常の状態で、砲撃などの動作の最中でない時）。");
        fields.Int("summonFrames", "浮き上がるまで (F)", summonFrames, 1, 600);
        fields.Float("rideHeight", "浮き上がる高さ（足元から地面まで m）", rideHeight, 0.0f, 20.0f);
        fields.Float("summonEasePower", "浮き上がりの出だしの速さ（1 は等速、大きいほど初め速くあとゆっくり）",
                     summonEasePower, 1.0f, 5.0f);
        fields.Float("summonGroundReach", "足元の地面を探す距離 (m。これより高い所ではその場で乗る)", summonGroundReach,
                     0.0f, 100.0f);
        fields.Int("dismissFrames", "降りる時にその場で止まる長さ (F。その後はいつもの重力で落ちる)", dismissFrames, 0,
                   600);
    }
    if (fields.Section("飛行", true)) {
        fields.Note("入力が無ければその場に浮きます（高さも変わりません）。前後はカメラの見ている向き"
                    "（見上げ・見下ろしを含む）、左右は水平の横、Jump は真上、Descend は真下です。"
                    "地形や物には当たって止まります。");
        fields.Float("cruiseSpeed", "前後左右の速さ (m/s)", cruiseSpeed, 0.0f, 100.0f);
        fields.Float("verticalSpeed", "上昇・下降の速さ (m/s)", verticalSpeed, 0.0f, 100.0f);
        fields.Float("accelSeconds", "最高速になるまで (秒、0 で一瞬)", accelSeconds, 0.0f, 5.0f);
        fields.Float("brakeSeconds", "入力を離して止まるまで (秒、0 で一瞬)", brakeSeconds, 0.0f, 5.0f);
        fields.Float("turnSpeed", "体を進む向きへ回す速さ (度/s)", turnSpeed, 1.0f, 5000.0f);
        fields.Float("levelPitch", "水平に飛ぶカメラの見下ろし角度 (度。0 でカメラの向きのまま)", levelPitch, -45.0f,
                     45.0f);
        fields.Note("前を押した時の上り下りの角度＝カメラの見下ろし角度 − この値。始めのカメラ（16 度見下ろす）で"
                    "水平に飛ぶよう 16 にしています。");
    }
    if (fields.Section("騎乗中のカメラ")) {
        fields.Note("騎乗中だけ、Player の「カメラ」の上下の追い方と見上げの限度をこの値に差し替えます。"
                    "距離・画角などは変えません。");
        fields.Float("cameraVerticalSeconds", "上下に追うならし (秒)", cameraVerticalSeconds, 0.0f, 3.0f);
        fields.Float("cameraVerticalDeadZone", "上下を追わない幅 (m)", cameraVerticalDeadZone, 0.0f, 10.0f);
        fields.Float("cameraMinPitch", "見上げる限度 (度、負で見上げ)", cameraMinPitch, -89.0f, 89.0f);
        fields.Float("cameraMaxPitch", "見下ろす限度 (度)", cameraMaxPitch, -89.0f, 89.0f);
    }
    if (fields.Section("仮の雲")) {
        fields.Note("流体の雲ができるまでの仮の見た目です。当たりの無い物体を足元へ動かし、"
                    "乗っていない間は地面の下へしまいます。");
        fields.SceneObject("cloudObject", "仮の雲の物体", cloudObjectId);
        fields.Float("cloudDiameter", "雲の直径 (m)", cloudDiameter, 0.01f, 50.0f);
        fields.Float("cloudThickness", "雲の厚み (m)", cloudThickness, 0.01f, 50.0f);
        fields.Float("cloudBelowFeet", "雲の中心を足元から下げる量 (m)", cloudBelowFeet, -5.0f, 5.0f);
        fields.Float("cloudParkDepth", "しまう深さ（体の中心から下へ m）", cloudParkDepth, 0.0f, 1000.0f);
    }
    if (fields.Section("確かめ")) {
        fields.Int("traceEveryFrames", "位置と速度をログに出す間隔 (F、0 は出さない)", traceEveryFrames, 0, 7200);
    }
}

} // namespace Project_TD2_2
