#include "GourdFeedback.h"

namespace Project_TD2_2 {

void GourdFeedback::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("発射の手応え（カメラ・振動）")) {
        fields.Int("shotCameraFrames", "発射のカメラ反動時間 (F、0 は無効)", cameraFrames, 0, 120);
        fields.Float("shotCameraKickDistance", "発射のカメラ後退幅 (m、0 は無効)", cameraKickDistance, 0.0f, 1.0f);
        fields.Float("shotCameraShakeAmplitude", "発射のカメラ揺れ幅 (m、0 は無効)", cameraShakeAmplitude, 0.0f, 1.0f);
        fields.Float("shotCameraShakeFrequency", "発射のカメラ揺れ周波数 (Hz)", cameraShakeFrequency, 0.0f, 60.0f);
        fields.Float("shotCameraFovDegrees", "発射で広がる視野角 (度、0 は無効)", cameraFovDegrees, 0.0f, 20.0f);
        fields.Int("shotChromaFrames", "発射の瞬間の色ずれの時間 (F、0 は無効)", chromaFrames, 0, 60);
        fields.Float("shotChromaPixels", "発射の瞬間の色ずれの幅 (画素)", chromaPixels, 0.0f, 24.0f);
        fields.Float("shotChromaContrast", "発射の瞬間に上げるコントラスト (0〜2)", chromaContrast, 0.0f, 2.0f);
        fields.Int("shotRumbleFrames", "発射のコントローラー振動時間 (F、0 は無効)", rumbleFrames, 0, 120);
        fields.Float("shotRumbleLow", "発射の振動：重いモーター (0〜1)", rumbleLow, 0.0f, 1.0f);
        fields.Float("shotRumbleHigh", "発射の振動：軽いモーター (0〜1)", rumbleHigh, 0.0f, 1.0f);
        fields.Note("プレイヤーの反動距離・時間は変えず、描画カメラと振動だけに短い反動を加えます。");
    }
    if (fields.Section("空撃ち（煙・スタミナ不足）の手応え")) {
        fields.Float("dryFireKickDistance", "空撃ちでヒョウタンが小さく跳ねる距離 (m、0 は無効)", dryFireKickDistance,
                     0.0f, 1.0f);
        fields.Int("dryFireKickFrames", "空撃ちで跳ねる時間 (F)", dryFireKickFrames, 1, 60);
        fields.Int("dryFireRumbleFrames", "空撃ちの振動時間 (F、0 は無効)", dryFireRumbleFrames, 0, 60);
        fields.Float("dryFireRumble", "空撃ちの振動の強さ (0〜1)", dryFireRumble, 0.0f, 1.0f);
    }
}

} // namespace Project_TD2_2
