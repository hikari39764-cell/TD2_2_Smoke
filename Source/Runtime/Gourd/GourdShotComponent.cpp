#include "GourdShotComponent.h"

namespace Project_TD2_2 {

void GourdShotComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("砲撃の時間と消費", true)) {
        fields.Bool("enabled", "有効", enabled);
        fields.Int("windupFrames", "構えの前隙 (F)", windupFrames, 0, 7200);
        fields.Int("repeatFrames", "連射の間隔 (F)", repeatFrames, 1, 7200);
        fields.Int("recoveryFrames", "最後の発射からの後隙 (F)", recoveryFrames, 0, 7200);
        fields.Int("inputBufferFrames", "先行入力 (F)", inputBufferFrames, 1, 60);
        fields.Float("staminaCost", "一発のスタミナ消費（0 は制限なし）", staminaCost, 0.0f, 100000.0f);
        fields.Float("smokeCost", "一発の煙消費（0 は制限なし）", smokeCost, 0.0f, 100000.0f);
        fields.Float("recoilDistance", "プレイヤー反動距離 (m)", recoilDistance, 0.0f, 100.0f);
        fields.Int("recoilFrames", "プレイヤー反動時間 (F)", recoilFrames, 1, 600);
        fields.Float("gourdRecoilDistance", "ヒョウタン反動距離 (m)", gourdRecoilDistance, 0.0f, 10.0f);
        fields.Int("gourdRecoilFrames", "ヒョウタン反動時間 (F)", gourdRecoilFrames, 1, 600);
    }
    if (fields.Section("弾の数値", true)) {
        fields.Float("damage", "基礎威力", damage, 0.0f, 100000.0f);
        fields.Float("initialSpeed", "初速 (m/s)", initialSpeed, 0.1f, 1000.0f);
        fields.Float("range", "霧散する距離 (m)", range, 0.1f, 1000.0f);
        fields.Float("falloffStart", "威力・大きさ・濃さの減衰開始 (m)", falloffStart, 0.0f, 1000.0f);
        fields.Float("finalDamageRatio", "射程端の威力倍率", finalDamageRatio, 0.0f, 1.0f);
        fields.Float("finalSpeedRatio", "射程端の弾速倍率", finalSpeedRatio, 0.01f, 1.0f);
        fields.Float("projectileRadius", "弾の当たり半径 (m)", projectileRadius, 0.01f, 10.0f);
        fields.Float("finalRadiusScale", "射程端の大きさ倍率", finalRadiusScale, 1.0f, 10.0f);
        fields.Float("finalDensityRatio", "射程端の煙の濃さ倍率", finalDensityRatio, 0.0f, 1.0f);
        fields.Int("pelletsPerShot", "一発に含む弾の数", pelletsPerShot, 1, 8);
        fields.Float("spreadDegrees", "水平の散布角 (度)", spreadDegrees, 0.0f, 180.0f);
        fields.Note("既定は単発弾・散布 0。配列は同時 8 弾まで。空き不足なら消費せず発射を待ちます。");
    }
    if (fields.Section("煙の演出")) {
        fields.Vfx("trailEffect", "弾道の流体注入エフェクト", trailEffect);
        fields.Float("muzzleRadius", "砲口の押し広げ半径 (m)", muzzleRadius, 0.01f, 10.0f);
        fields.Float("muzzleStrength", "砲口の押し広げ速度 (m/s)", muzzleStrength, 0.0f, 100.0f);
        fields.Float("muzzleSmokeRate", "砲口の煙注入率 (/s)", muzzleSmokeRate, 0.0f, 10000.0f);
        fields.Float("impactRadius", "命中・霧散の爆開半径 (m)", impactRadius, 0.01f, 100.0f);
        fields.Float("impactStrength", "命中・霧散の爆開速度 (m/s)", impactStrength, 0.0f, 100.0f);
        fields.Float("residueRadius", "残り煙の注入半径 (m)", residueRadius, 0.01f, 100.0f);
        fields.Float("residueSmokeRate", "残り煙の注入率 (/s)", residueSmokeRate, 0.0f, 10000.0f);
        fields.Int("residueFrames", "残り煙を補い続ける時間 (F)", residueFrames, 0, 7200);
        fields.Float("fallbackTrailRate", "注入エフェクト未設定時の尾煙率 (/s)", fallbackTrailRate, 0.0f, 10000.0f);
        fields.Note("尾煙は input.amount を濃さ倍率に使います。VFX 注入源は全砲撃合計 8 個、Fluid 提出は合計 24 "
                    "個／描画フレームまで。");
    }
    if (fields.Section("検証"))
        fields.Int("traceEveryFrames", "弾道ログ間隔（0 はログ無効）", traceEveryFrames, 0, 7200);
}

} // namespace Project_TD2_2
