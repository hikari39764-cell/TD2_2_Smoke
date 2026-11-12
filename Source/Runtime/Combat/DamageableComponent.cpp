#include "DamageableComponent.h"

namespace Project_TD2_2 {

void DamageableComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("被弾と HP", true)) {
        fields.Float("maxHealth", "最大 HP", maxHealth, 1.0f, 1000000.0f);
        fields.Bool("invulnerable", "無敵（ダメージを受けない）", invulnerable);
        fields.Note("開始時は最大 HP。現在 HP と最後の被弾は DamageableRules の問い合わせで取得できます。");
    }
    if (fields.Section("被弾の手応え")) {
        fields.Int("hitStopFrames", "命中の停止時間 (F、0 は無効)", hitStopFrames, 0, 30);
        fields.Float("hitStopScale", "停止中の時間倍率（1 は停止なし）", hitStopScale, 0.0f, 1.0f);
        fields.Int("hitCameraFrames", "命中のカメラ揺れ時間 (F、0 は無効)", hitCameraFrames, 0, 120);
        fields.Float("hitCameraAmplitude", "命中のカメラ揺れ幅 (m、0 は無効)", hitCameraAmplitude, 0.0f, 1.0f);
        fields.Float("hitCameraFrequency", "命中のカメラ揺れ周波数 (Hz)", hitCameraFrequency, 0.0f, 60.0f);
        fields.Float("hitCameraKickDistance", "命中でカメラが攻撃の向きへ押される幅 (m、0 は無効)",
                     hitCameraKickDistance, 0.0f, 1.0f);
        fields.Float("hitCameraFovDegrees", "命中で広がる視野角 (度、0 は無効)", hitCameraFovDegrees, 0.0f, 20.0f);
        fields.SceneObject("feedbackCamera", "命中のカメラ（未指定は操作カメラ）", feedbackCameraId);
        fields.Int("hitRumbleFrames", "命中のコントローラー振動時間 (F、0 は無効)", hitRumbleFrames, 0, 120);
        fields.Float("hitRumbleLow", "命中の振動：重いモーター (0〜1)", hitRumbleLow, 0.0f, 1.0f);
        fields.Float("hitRumbleHigh", "命中の振動：軽いモーター (0〜1)", hitRumbleHigh, 0.0f, 1.0f);
        fields.Int("hitFlashFrames", "被弾の発光時間 (F、0 は無効)", hitFlashFrames, 0, 120);
        fields.Float("hitFlashStrength", "被弾の発光強度（0 は無効）", hitFlashStrength, 0.0f, 16.0f);
        fields.Color3("hitFlashColor", "被弾の発光色", hitFlashColor);
        fields.Int("hitMaterialIndex", "被弾で光らせる材質の番号", hitMaterialIndex, 0, 255);
        fields.Note(
            "成功した被弾だけで作動します。F は 1/60 秒、演出は停止中も実時間で戻ります。形状・衝突は変えません。");
        fields.Note("被弾の発光は、材質の色を読めるモデルだけに出ます。命中点の爆ぜ・火花・光は攻撃側"
                    "（砲撃の命中エフェクト）が出します。");
    }
    if (fields.Section("検証")) {
        fields.Bool("traceEveryHit", "受けたダメージを毎回ログへ記録", traceEveryHit);
    }
}

} // namespace Project_TD2_2
