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
        fields.Float("lockTargetWeight", "画面内の対象の重み", lockTargetWeight, 0.0f, 1.0f);
        fields.Float("lockYawSpeed", "左右の追従 (度/s)", lockYawSpeed, 0.0f, 2000.0f);
        fields.Float("lockPitch", "見下ろし角度", lockPitch, -80.0f, 80.0f);
        fields.Float("lockPitchSpeed", "上下の追従 (度/s)", lockPitchSpeed, 0.0f, 2000.0f);
        fields.Float("lockDistanceScale", "対象距離による引き", lockDistanceScale, 0.0f, 2.0f);
        fields.Float("lockMaximumDistanceOffset", "最大追加距離 (m)", lockMaximumDistanceOffset, 0.0f, 30.0f);
    }
    if (fields.Section("カメラの構図")) {
        fields.Note("追従カメラ（Gameplay Camera）の結果に足すずらしです。距離・見下ろし角・注視の高さは "
                    "CameraFollow の値で調整します。");
        fields.Float("cameraSideOffset", "視点の横ずらし (m、負で左)", cameraSideOffset, -10.0f, 10.0f);
        fields.Float("cameraLookAhead", "注視点を前へ送る距離 (m)", cameraLookAhead, 0.0f, 20.0f);
        fields.Float("cameraLockLookAhead", "ロックオン中の注視点の前送り (m)", cameraLockLookAhead, 0.0f, 20.0f);
        fields.Float("cameraBlendSeconds", "ロックオンの切り替えをならす時間 (秒)", cameraBlendSeconds, 0.0f, 3.0f);
    }
}

} // namespace Project_TD2_2
