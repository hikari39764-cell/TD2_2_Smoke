#include "BossCinematicFxSettings.h"

namespace Project_TD2_2 {

void BossCinematicFxSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("演出の効果（登場・フェーズの変わり目の演出が共通で使う閃光・白黒の閃き・揺れ・停止）", false))
        return;
    fields.Float("introDrawSlowScale", "効果：スローの間の時間の倍率（変わり目の閃光も使う）", drawSlowScale, 0.05f,
                 1.0f);
    fields.Float("introEyesFlare", "効果：目の閃きの強さ（0 で出さない）", eyesFlare, 0.0f, 8.0f);
    fields.Vfx("introSlashFlashEffect", "効果：刀身の上で弾ける閃光のエフェクト（空なら出さない）", slashFlashEffect);
    fields.Float("introSlashFlashScale", "効果：閃光の大きさ（0 で出さない）", slashFlashScale, 0.0f, 10.0f);
    fields.Asset("introSlashMonoProfile", "効果：白黒の閃きの効果のプロファイル（BossImpactMono。空なら出さない）",
                 HIKARI::GameProjectAssetKind::PostProfile, slashMonoProfile);
    fields.Float("introSlashMonoInvertSeconds", "白黒の閃き：反転した白黒を出す長さ（実時間の秒）",
                 slashMonoInvertSeconds, 0.0f, 1.0f);
    fields.Float("introSlashMonoHoldSeconds", "白黒の閃き：反転しない白黒を出す長さ（実時間の秒）",
                 slashMonoHoldSeconds, 0.0f, 1.0f);
    fields.Float("introSlashMonoFadeSeconds", "白黒の閃き：色へ戻る長さ（実時間の秒）", slashMonoFadeSeconds, 0.0f,
                 2.0f);
    fields.Float("introSlashMonoStrength", "白黒の閃き：強さ（0 で出さない）", slashMonoStrength, 0.0f, 1.0f);
    fields.Float("introAccentShake", "手応え：強い拍のカメラの揺れ幅 (m。0 で揺らさない)", accentShake, 0.0f, 1.0f);
    fields.Int("introAccentShakeFrames", "手応え：揺れの長さ (F)", accentShakeFrames, 0, 120);
    fields.Float("introAccentShakeFrequency", "手応え：揺れの速さ", accentShakeFrequency, 1.0f, 60.0f);
    fields.Float("introAccentFovDegrees", "手応え：強い拍で画角を一瞬広げる量（度。0 で動かさない）", accentFovDegrees,
                 0.0f, 20.0f);
    fields.Float("introHitStopScale", "手応え：時間を止めている間の時間の速さ", hitStopScale, 0.0f, 1.0f);
    fields.Float("introAccentRumble", "手応え：振動の強さ（0 で出さない）", accentRumble, 0.0f, 1.0f);
    fields.Int("introAccentRumbleFrames", "手応え：振動の長さ (F)", accentRumbleFrames, 0, 120);
    fields.Float("introRumbleFrequency", "続けて出す画面の揺れ（前触れ・刀を握る間）の速さ（1 秒の回数）",
                 rumbleFrequency, 0.5f, 30.0f);
    fields.Float("introBladeEdgeGlow", "刀身の光：刃口の細い線の明るさ（画面基準。登場・変わり目の演出）",
                 bladeEdgeGlow, 0.0f, 64.0f);
}

} // namespace Project_TD2_2
