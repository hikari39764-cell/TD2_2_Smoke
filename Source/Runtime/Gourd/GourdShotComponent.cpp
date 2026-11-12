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
        fields.Float("gourdWindupPull", "構え中にヒョウタンを引く距離 (m、0 は無効)", gourdWindupPull, 0.0f, 2.0f);
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
    if (fields.Section("炎の弾・砲口・命中のエフェクト")) {
        fields.Vfx("shellEffect", "飛んでいる火の弾（燃える流体の域）", shellEffect);
        fields.Float("shellScale", "火の弾の源の大きさ倍率（0 は無効）", shellScale, 0.0f, 8.0f);
        fields.Int("shellFlashFrames", "命中の瞬間に弾の域が熱い閃きを弾く時間 (F)", shellFlashFrames, 0, 30);
        fields.Int("shellFadeFrames", "命中・霧散の後に弾の域を消すまでの時間 (F)", shellFadeFrames, 0, 120);
        fields.Vfx("muzzleEffect", "砲口の発射エフェクト（火の輪）", muzzleEffect);
        fields.Float("muzzleScale", "砲口のエフェクトの源の大きさ倍率（0 は無効）", muzzleScale, 0.0f, 8.0f);
        fields.Vfx("impactEffect", "命中の爆発エフェクト", impactEffect);
        fields.Float("impactScale", "命中の爆発の源の大きさ倍率（0 は無効）", impactScale, 0.0f, 8.0f);
        fields.Float("impactFarScale", "射程端で命中した時の爆発の倍率", impactFarScale, 0.0f, 1.0f);
        fields.Float("impactStandoff", "爆発を出す位置を命中点から撃った側へ戻す距離 (m)", impactStandoff, 0.0f,
                     3.0f);
        fields.Note("流体の域は同時に 6 個までしか描けません。連射中は、砲口の炎 1・命中の爆発 2・弾 2 を超える分を"
                    "古いものから止めます（場の霧が消えないようにするため）。");
        fields.Note("弾と砲口の資産は +X が前、爆発は世界の向き。倍率は源（噴き出し口）だけに効き、流体の域の"
                    "大きさは資産の値です。炎の色・明るさ・消える速さ・画面の波紋は各資産で調整します。");
        fields.Note("爆発を敵の体に重ねて出すと隠れて小さく暗く見えるので、少し手前で出します。霧散（射程切れ）"
                    "では爆発を出しません。");
    }
    if (fields.Section("煙の演出")) {
        fields.Vfx("trailEffect", "弾道の流体注入エフェクト", trailEffect);
        fields.Float("muzzleRadius", "砲口の押し広げ半径 (m)", muzzleRadius, 0.01f, 10.0f);
        fields.Float("muzzleStrength", "砲口の押し広げ速度 (m/s)", muzzleStrength, 0.0f, 100.0f);
        fields.Float("muzzleSmokeRate", "砲口の煙注入率 (/s)", muzzleSmokeRate, 0.0f, 10000.0f);
        fields.Float("impactRadius", "命中・霧散で足元の霧を押し広げる半径 (m)", impactRadius, 0.01f, 100.0f);
        fields.Float("impactStrength", "命中・霧散で足元の霧を押し広げる速度 (m/s)", impactStrength, 0.0f, 100.0f);
        fields.Float("impactGroundReach", "足元の霧に届く高さ (m。これより高い所で爆ぜると霧は動かない)",
                     impactGroundReach, 0.1f, 100.0f);
        fields.Float("impactGroundOffset", "霧を押す中心の地面からの高さ (m)", impactGroundOffset, 0.0f, 5.0f);
        fields.Float("shellWakeStrength", "飛んでいる弾が足元の霧を左右へ割る加速度 (m/s2、0 は無効)",
                     shellWakeStrength, 0.0f, 200.0f);
        fields.Float("shellWakeRadius", "弾が霧を割る力の半径 (m)", shellWakeRadius, 0.05f, 10.0f);
        fields.Float("shellWakeSpread", "弾道の左右へ力を置く幅 (m)", shellWakeSpread, 0.0f, 5.0f);
        fields.Note("霧を押す力は、重なっている流体の域すべてに効きます。爆発の炎の域を吹き飛ばさないように、"
                    "爆ぜた点の真下の地面で押します（高い所ほど弱い）。");
        fields.Float("residueRadius", "残り煙の注入半径 (m)", residueRadius, 0.01f, 100.0f);
        fields.Float("residueSmokeRate", "残り煙の注入率 (/s)", residueSmokeRate, 0.0f, 10000.0f);
        fields.Int("residueFrames", "残り煙を補い続ける時間 (F)", residueFrames, 0, 7200);
        fields.Color3("smokeColor", "尾煙・残り煙の色", smokeColor);
        fields.Float("trailSmokeScale", "尾煙の注入量倍率", trailSmokeScale, 0.0f, 10.0f);
        fields.Float("fallbackTrailRate", "注入エフェクト未設定時の尾煙率 (/s)", fallbackTrailRate, 0.0f, 10000.0f);
        fields.Note("尾煙は input.amount を濃さ倍率に使います。VFX 注入源は全砲撃合計 8 個、Fluid 提出は合計 24 "
                    "個／描画フレームまで。");
        fields.Note("砲口の煙・残り煙・尾煙は既定で 0（切）です。場の霧へ入れる煙は炎の域にも入り、炎を暗い煙で"
                    "包んでしまうので、煙は炎の域が出す煤に任せています。");
    }
    if (fields.Section("撃った煙を場の霧へ戻す")) {
        fields.Float("fogReturnSmoke", "1 発が場の霧へ戻す煙の量（0 は戻さない）", fogReturnSmoke, 0.0f, 1000.0f);
        fields.Float("fogReturnTrailShare", "そのうち通り道に落とす割合（残りは終点）", fogReturnTrailShare, 0.0f,
                     1.0f);
        fields.Float("fogReturnTrailSpacing", "通り道に落とす間隔 (m)", fogReturnTrailSpacing, 0.5f, 20.0f);
        fields.Float("fogReturnRadius", "落とす煙の半径 (m)", fogReturnRadius, 0.2f, 10.0f);
        fields.Float("fogReturnHeight", "落とす煙の中心の地面からの高さ (m)", fogReturnHeight, 0.0f, 5.0f);
        fields.Int("fogReturnFrames", "1 か所の煙を出し切る時間 (F)", fogReturnFrames, 1, 600);
        fields.Int("fogReturnDelayFrames", "弾が終わってから落とし始めるまで (F)", fogReturnDelayFrames, 0, 600);
        fields.Int("fogReturnFallFrames", "終点の煙が爆ぜた高さから地面へ降りる時間 (F、0 は最初から地面)",
                   fogReturnFallFrames, 0, 600);
        fields.Float("fogReturnSink", "落とす煙を下へ押す速さ (m/s)", fogReturnSink, 0.0f, 20.0f);
        fields.Float("fogReturnWander", "落とす中心を回す半径 (m、0 は丸い塊のまま)", fogReturnWander, 0.0f, 5.0f);
        fields.Float("fogReturnSwirl", "落とす煙を横へ流す速さ (m/s、渦を巻いて広がる)", fogReturnSwirl, 0.0f, 20.0f);
        fields.Float("fogReturnClearance", "炎の域からこの距離 (m) 以内には落とさず、炎が消えるまで待つ",
                     fogReturnClearance, 0.0f, 20.0f);
        fields.Int("impactFireFrames", "命中の爆発の域が残る時間 (F、資産の長さに合わせる)", impactFireFrames, 0, 600);
        fields.Int("muzzleFireFrames", "砲口の炎の域が残る時間 (F、資産の長さに合わせる)", muzzleFireFrames, 0, 600);
        fields.Note("吸って撃った煙は、弾の通り道と終点（命中・射程切れ）の地面へ"
                    "冷たい煙として戻り、また吸えます。"
                    "戻す量は一発の煙消費とは別の値です（消費を 0 "
                    "にして試し撃ちしても戻ります）。");
        fields.Note("場の霧へ入れる煙は重なっている炎の域にも入るので、炎が消える"
                    "まで待ってから落とします。"
                    "同じ場所へ撃ち続けると、撃ち終えた後にまとめて湧きます。地面"
                    "に届かない高さで終わった弾の煙は戻りません。");
    }
    feedback.DescribeProperties(fields);
    if (fields.Section("検証"))
        fields.Int("traceEveryFrames", "弾道ログ間隔（0 はログ無効）", traceEveryFrames, 0, 7200);
}

} // namespace Project_TD2_2
