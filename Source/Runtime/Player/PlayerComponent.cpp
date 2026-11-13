#include "PlayerComponent.h"

namespace Project_TD2_2 {

void PlayerComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("移動と体力", true)) {
        fields.Float("maxHealth", "最大 HP", maxHealth, 1.0f, 10000.0f);
        fields.Float("moveSpeed", "移動速度 (m/s)", moveSpeed, 0.0f, 50.0f);
        fields.Float("turnSpeed", "振り向きの速さ (度/s、688 ≒ 12 rad/s)", turnSpeed, 1.0f, 5000.0f);
        fields.Float("maxStamina", "最大スタミナ", maxStamina, 1.0f, 10000.0f);
        fields.Int("staminaDelayFrames", "スタミナ回復待ち (F)", staminaDelayFrames, 0, 600);
        fields.Float("staminaPerSecond", "スタミナ回復 (/s)", staminaPerSecond, 0.0f, 1000.0f);
        fields.Float("maximumFallSpeed", "落下速度上限 (m/s)", maximumFallSpeed, 0.0f, 200.0f);
        fields.Int("inputBufferFrames", "先行入力 (F)", inputBufferFrames, 1, 60);
        fields.Float("moveAccelSeconds", "歩き出しで最高速になるまで (秒、0 で一瞬)", moveAccelSeconds, 0.0f, 2.0f);
        fields.Float("moveDecelSeconds", "入力を離して止まるまで (秒、0 で一瞬)", moveDecelSeconds, 0.0f, 2.0f);
    }
    if (fields.Section("ローリング", true)) {
        fields.Note("移動 F は全体 F までに制限します。無敵は 0F から指定した F 数です。");
        fields.Int("rollFrames", "全体 (F)", rollFrames, 1, 600);
        fields.Int("rollMoveFrames", "移動 (F)", rollMoveFrames, 1, 600);
        fields.Float("rollDistance", "距離 (m)", rollDistance, 0.0f, 30.0f);
        fields.Float("rollEasePower", "出だしの速さ（1 は等速、大きいほど初め速くあとゆっくり）", rollEasePower, 1.0f,
                     5.0f);
        fields.Note("方向入力が無いとき、ロックオン中は対象から離れる向き、そうでなければ今の向きの後ろへ転がります。");
        fields.Int("rollInvincibleFrames", "無敵 (F)", rollInvincibleFrames, 0, 600);
        fields.Int("rollActionCancelFrame", "砲撃・吸い込みで打ち切れる最初の経過 F", rollActionCancelFrame, 0, 600);
        fields.Note("打ち切れる F より前に押した砲撃・吸い込みは、それぞれの先行入力の長さだけ覚えておき、"
                    "打ち切れる F に始まります。");
        fields.Float("rollStamina", "スタミナ消費", rollStamina, 0.0f, 1000.0f);
        fields.Float("rollFogRadius", "霧を押す半径 (m)", rollFogRadius, 0.01f, 10.0f);
        fields.Float("rollFogStrength", "霧を押す強さ (m/s)", rollFogStrength, 0.0f, 100.0f);
        fields.Float("rollFogHeightOffset", "霧の中心高さ補正 (m)", rollFogHeightOffset, -5.0f, 5.0f);
    }
    if (fields.Section("被弾")) {
        fields.Note("三歩の時間は全体 F の中に収めます。三歩目は残りの F 数です。");
        fields.Int("staggerFrames", "のけぞり全体 (F)", staggerFrames, 3, 600);
        fields.Int("staggerStepOneFrames", "一歩目 (F)", staggerStepOneFrames, 1, 600);
        fields.Int("staggerStepTwoFrames", "二歩目 (F)", staggerStepTwoFrames, 1, 600);
        fields.Float("staggerStepOne", "一歩目の距離 (m)", staggerStepOne, 0.0f, 10.0f);
        fields.Float("staggerStepTwo", "二歩目の距離 (m)", staggerStepTwo, 0.0f, 10.0f);
        fields.Float("staggerStepThree", "三歩目の距離 (m)", staggerStepThree, 0.0f, 10.0f);
        fields.Int("staggerHitStopFrames", "のけぞりヒットストップ (F)", staggerHitStopFrames, 0, 60);
        fields.Float("knockbackDistance", "吹っ飛び距離 (m)", knockbackDistance, 0.0f, 30.0f);
        fields.Int("knockbackFrames", "吹っ飛び (F)", knockbackFrames, 1, 600);
        fields.Float("knockbackHeight", "吹っ飛びの放物線の高さ (m、0 は地面を滑る)", knockbackHeight, 0.0f, 10.0f);
        fields.Int("knockbackHitStopFrames", "吹っ飛びヒットストップ (F)", knockbackHitStopFrames, 0, 60);
        fields.Float("hitCameraAmplitudeStagger", "のけぞりのカメラ揺れ幅 (m、0 は無効)", hitCameraAmplitudeStagger, 0.0f,
                     1.0f);
        fields.Int("hitCameraFramesStagger", "のけぞりのカメラ揺れ時間 (F)", hitCameraFramesStagger, 0, 120);
        fields.Float("hitCameraAmplitudeKnockback", "吹っ飛びのカメラ揺れ幅 (m、0 は無効)", hitCameraAmplitudeKnockback,
                     0.0f, 1.0f);
        fields.Int("hitCameraFramesKnockback", "吹っ飛びのカメラ揺れ時間 (F)", hitCameraFramesKnockback, 0, 120);
        fields.Float("hitCameraFrequency", "被弾のカメラ揺れ周波数 (Hz)", hitCameraFrequency, 0.0f, 60.0f);
        fields.Float("hitRumbleStagger", "のけぞりの振動の強さ (0〜1、0 は無効)", hitRumbleStagger, 0.0f, 1.0f);
        fields.Float("hitRumbleKnockback", "吹っ飛びの振動の強さ (0〜1、0 は無効)", hitRumbleKnockback, 0.0f, 1.0f);
        fields.Int("hitRumbleFrames", "被弾の振動時間 (F、0 は無効)", hitRumbleFrames, 0, 120);
        fields.Int("downFrames", "ダウン (F)", downFrames, 1, 600);
        fields.Int("downRollFrame", "ダウン回避開始 (F)", downRollFrame, 0, 600);
        fields.Int("getUpFrames", "起き上がり (F)", getUpFrames, 1, 600);
    }
    if (fields.Section("回復")) {
        fields.Note("回復発生は全体 F までに制限します。使用回数は動作開始時に消費します。");
        fields.Int("maxHealCharges", "回復回数", maxHealCharges, 0, 99);
        fields.Float("healAmount", "HP 回復量", healAmount, 0.0f, 10000.0f);
        fields.Int("healFrames", "回復動作全体 (F)", healFrames, 1, 600);
        fields.Int("healApplyFrame", "HP 回復発生 (F)", healApplyFrame, 1, 600);
        fields.Float("healMoveSpeed", "回復中の移動 (m/s)", healMoveSpeed, 0.0f, 50.0f);
    }
    if (fields.Section("ロックオン")) {
        fields.SceneObject("lockTarget", "ロック対象", lockTargetId);
        fields.SceneObject("camera", "ゲームカメラ", cameraId);
        fields.Note("ロックオン中、カメラは対象の向きへ回ります。回る速さに上限があるので、"
                    "対象が背後へ現れても振り回されません。距離は変わりません。");
        fields.Float("lockYawSeconds", "対象の向きへ回るならし (秒、小さいほど素早く追う)", lockYawSeconds, 0.0f, 2.0f);
        fields.Float("lockYawMaxSpeed", "回る速さの上限 (度/s)", lockYawMaxSpeed, 10.0f, 2000.0f);
        fields.Float("lockPitch", "見下ろし角度 (度)", lockPitch, -80.0f, 80.0f);
        fields.Float("lockPitchFollow", "対象が高い時に見上げる割合 (0 で見上げない)", lockPitchFollow, 0.0f, 1.5f);
        fields.Float("lockPitchSeconds", "見上げ・見下ろしのならし (秒)", lockPitchSeconds, 0.0f, 2.0f);
    }
    if (fields.Section("カメラ")) {
        fields.Note(
            "カメラの姿勢はここの値で決まります（Gameplay Camera の CameraFollow の値は使いません。"
            "画角だけ Gameplay Camera の値です）。軸＝体の中心から少し上の点で、視点は軸の後ろ・上にあります。");
        fields.Float("cameraDistance", "軸から視点までの距離 (m)", cameraDistance, 0.5f, 30.0f);
        fields.Float("cameraPivotHeight", "軸の高さ（体の中心から上へ m）", cameraPivotHeight, -5.0f, 10.0f);
        fields.Float("cameraPitch", "始めの見下ろし角度 (度)", cameraPitch, -80.0f, 80.0f);
        fields.Float("cameraSideOffset", "肩越しの横ずらし (m、負で左。0 で真後ろ)", cameraSideOffset, -10.0f, 10.0f);
        fields.Float("cameraFollowSeconds", "水平に追うならし (秒、大きいほど遅れてついてくる)", cameraFollowSeconds,
                     0.0f, 2.0f);
        fields.Float("cameraVerticalSeconds", "上下に追うならし (秒)", cameraVerticalSeconds, 0.0f, 3.0f);
        fields.Float("cameraVerticalDeadZone", "上下を追わない幅 (m)", cameraVerticalDeadZone, 0.0f, 10.0f);
        fields.Float("cameraMouseSensitivity", "マウスの感度 (度/画素)", cameraMouseSensitivity, 0.0f, 10.0f);
        fields.Float("cameraStickYawSpeed", "スティックの左右の速さ (度/s)", cameraStickYawSpeed, 0.0f, 3600.0f);
        fields.Float("cameraStickPitchSpeed", "スティックの上下の速さ (度/s)", cameraStickPitchSpeed, 0.0f, 3600.0f);
        fields.Float("cameraMinPitch", "見上げる限度 (度、負で見上げ)", cameraMinPitch, -89.0f, 89.0f);
        fields.Float("cameraMaxPitch", "見下ろす限度 (度)", cameraMaxPitch, -89.0f, 89.0f);
        fields.Float("cameraEyeFloor", "見上げた時に視点が軸から下がれる限度 (m)", cameraEyeFloor, 0.0f, 10.0f);
        fields.Bool("cameraTrace", "カメラの姿勢を毎フレーム記録する（確かめ用）", cameraTrace);
    }
}

} // namespace Project_TD2_2
