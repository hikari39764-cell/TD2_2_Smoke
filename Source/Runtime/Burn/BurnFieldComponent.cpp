#include "BurnFieldComponent.h"

namespace Project_TD2_2 {

void BurnFieldComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("燃える範囲", true)) {
        fields.Bool("enabled", "有効", enabled);
        fields.Float("fieldSize", "格子の一辺 (m。物体の位置が中心)", fieldSize, 1.0f, 200.0f);
        fields.Float("cellSize", "セルの一辺 (m)", cellSize, 0.1f, 4.0f);
        fields.Float("arenaRadius", "闘技場の半径 (m。燃えている割合の分母)", arenaRadius, 1.0f, 200.0f);
        fields.Note(
            "格子の大きさとセルの細かさは、Play を始めた時の値で作ります（Play 中に変えても次の Play から効きます）。"
            "物体の高さが床の高さです。");
    }
    if (fields.Section("火の手傷", true)) {
        fields.Float("contactRadius",
                     "火に触れたとみなす体の半径 (m。足元からこの円に燃えているセルが掛かれば触れている)",
                     contactRadius, 0.0f, 2.0f);
        fields.Float("fireReachHeight",
                     "火が届く高さ (m。体の中心が床からこれより高いと当たらない。筋斗雲で飛んでいる時など)",
                     fireReachHeight, 0.0f, 100.0f);
        fields.Float("damageDelay", "火に触れ続けてから燃え移るまで (秒)", damageDelay, 0.0f, 5.0f);
        fields.Float("damagePerSecond", "火の中にいる間に減る HP (/秒。プレイヤーの炎上の減り方の代わりに使う)",
                     damagePerSecond, 0.0f, 1000.0f);
        fields.Note("火に触れ続けるとプレイヤーが炎上します（炎上の長さ・火の外での減り方はプレイヤーの項目）。"
                    "火はのけぞり・吹っ飛び・ヒットストップ・カメラの揺れを起こさず、HP だけを少しずつ減らします。"
                    "無敵の間（ローリングの無敵・吹っ飛び・ダウン）は触れている時間を数えず、0 に戻します"
                    "（ローリングで火を抜ければ燃え移りません）。");
    }
    if (fields.Section("火を吸う", true)) {
        fields.Float("absorbRange", "吸い込みで火を消せる前方の距離 (m)", absorbRange, 0.0f, 50.0f);
        fields.Float("absorbFanDegrees", "火を消せる扇の片側の角度 (度)", absorbFanDegrees, 0.0f, 180.0f);
        fields.Float("absorbNear", "向きを問わず消せる足元の半径 (m)", absorbNear, 0.0f, 10.0f);
        fields.Float("absorbAreaPerSecond", "吸い込みで火を消す速さ (m²/秒)", absorbAreaPerSecond, 0.0f, 1000.0f);
        fields.Float("smokePerArea", "消した火 1 m² あたりヒョウタンに入る煙", smokePerArea, 0.0f, 1000.0f);
        fields.Note("扇はプレイヤーの位置から、プレイヤーの向きへ取ります（飛ぶ斬撃を吸う扇と同じ取り方）。"
                    "近い火から消し、ヒョウタンに入りきらない分は消しません（満杯なら消えません）。");
    }
    if (fields.Section("火の見た目（流体）")) {
        fields.Float("fuelRate", "火の強さ：燃料の点 1 つが中心に毎秒入れる燃料", fuelRate, 0.0f, 2000.0f);
        fields.Float("fuelTemperature", "燃料の温度（場の火の域の着火温度より高くする）", fuelTemperature, 0.0f, 16.0f);
        fields.Float("fuelRise", "燃料を上へ押す速さ (m/s。炎の背の高さ)", fuelRise, 0.0f, 20.0f);
        fields.Float("fuelHeight", "燃料を入れる高さ（床から m。床より下にすると、球の上側だけが床の上に出る）",
                     fuelHeight, -2.0f, 3.0f);
        fields.Float("fuelSweep", "燃料の点を、火の長い方へ行き来させる速さ (m/s。0 で動かさない＝球のまま。60 未満)",
                     fuelSweep, 0.0f, 55.0f);
        fields.Float("fuelWave", "炎の背のうねり（0 で一様。点ごとに燃料をゆっくり増減させる割合）", fuelWave, 0.0f,
                     0.9f);
        fields.Float("fuelWaveHz", "うねりの速さ (Hz)", fuelWaveHz, 0.05f, 5.0f);
        fields.Float(
            "fuelCrowdPower",
            "火が広い時（升目へ順番に入れる時）に、待った分の燃料をまとめて入れる割合（1 で全部、小さいほど広い火が"
            "白く飛ばない）",
            fuelCrowdPower, 0.0f, 1.0f);
        fields.Float("fuelRadiusScale", "炎の幅 (燃えている所の幅に対する倍。1 で踏むと痛い範囲とほぼ同じ幅)",
                     fuelRadiusScale, 0.5f, 4.0f);
        fields.Float("fadeSeconds", "消える前に火が細くなり始める残り秒数", fadeSeconds, 0.01f, 30.0f);
        fields.Int(
            "emitterCount",
            "1 フレームに燃料を入れる所の数（これより燃えている升目が多い時は、数フレームに 1 回ずつ順番に入れる）",
            emitterCount, 1, static_cast<int>(kBurnEmitterCapacity));
        fields.Float("emitterTile",
                     "燃料を入れる升目の一辺 (m。升目ごとに 1 か所。小さいほど火の形が細かいが、順番待ちが増える)",
                     emitterTile, 0.5f, 8.0f);
        fields.Float("fogBurnAbsorption", "火の下の霧を焼く率 (/s。0 は焼かない)", fogBurnAbsorption, 0.0f, 1000.0f);
        fields.Float("fogBurnRadiusScale", "霧を焼く半径 (燃料の点が受け持つ火の長さの半分に対する倍)",
                     fogBurnRadiusScale, 0.0f, 6.0f);
        fields.Float("fogUpdraft", "火のそばの霧を上へ持ち上げる加速度 (m/s²。0 は持ち上げない)", fogUpdraft, 0.0f,
                     60.0f);
        fields.Vfx("tileEffect",
                   "升目ごとに重ねる炎の層のエフェクト（炎の舌・熾火・立ちのぼる煙・地面の光。空なら流体の火だけ）",
                   tileEffect);
        fields.Float("tileEffectAmount", "…炎の層の量（倍）", tileEffectAmount, 0.0f, 4.0f);
        fields.Float("tileEffectRise", "…燃え始めに出そろうまで (秒)", tileEffectRise, 0.0f, 5.0f);
        fields.Float("tileEffectFade", "…消える時に薄れるまで (秒)", tileEffectFade, 0.0f, 5.0f);
        fields.Note(
            "炎そのものの色・明るさ・揺れ・煙の量は、エフェクト資産 ArenaFireField の流体域の層で決めます。"
            "ここは、燃えている所へ燃料をどう入れるかだけです。燃料の点は、燃えているセルのかたまりの上に居続けて、"
            "毎フレーム同じ所へ燃料を入れます（つながって燃える炎になります）。"
            "火が広がって点が足りなくなると、1 点が受け持つ半径を広げます（炎は太く、粗くなります）。");
    }
    if (fields.Section("当たった所と体の炎の見た目")) {
        fields.Float("blastFuel", "火球へ 1 フレームに入れる燃料の強さ", blastFuel, 0.0f, 10000.0f);
        fields.Float("blastTemperature", "火球の燃料の温度", blastTemperature, 0.0f, 64.0f);
        fields.Float("blastRise", "火球を上へ押す速さ (m/s。煙の柱の高さ)", blastRise, 0.0f, 20.0f);
        fields.Float("blastPush", "爆発が火と霧を外へ押す速さ (m/s)", blastPush, 0.0f, 100.0f);
        fields.Float("hitFuelScale", "炎の刃が当たった所・刀の点火の火球の燃料 (blastFuel に対する倍)", hitFuelScale,
                     0.0f, 4.0f);
        fields.Float("smokeBlastStrength", "煙の塊へ 1 フレームに入れる煙の強さ", smokeBlastStrength, 0.0f, 10000.0f);
        fields.Float("smokeBlastFuelShare", "煙の塊に混ぜる燃料の割合（煙の中の炎のひらめき）", smokeBlastFuelShare,
                     0.0f, 1.0f);
        fields.Float("smokeBlastPush", "煙の塊が外へ押す速さ (m/s)", smokeBlastPush, 0.0f, 100.0f);
        fields.Float("jetFuel", "火の刃がプレイヤーに当たった所：体を突き抜ける炎へ 1 フレームに入れる燃料の強さ",
                     jetFuel, 0.0f, 10000.0f);
        fields.Float("jetSpeed", "…炎を攻撃の向きへ飛ばす速さ (m/s。大きいほど炎の舌が長い)", jetSpeed, 0.0f, 60.0f);
        fields.Float("jetRadius", "…燃料の球の半径 (m。炎の舌の太さ)", jetRadius, 0.1f, 4.0f);
        fields.Float("jetTemperature", "…燃料の温度", jetTemperature, 0.0f, 64.0f);
        fields.Float("jetSeconds", "…燃料を入れ続ける長さ (秒)", jetSeconds, 0.0f, 2.0f);
        fields.Float("jetTravel", "…燃料を入れる点が、体の手前から向こうへ進む速さ (m/s。60 未満)", jetTravel, 0.0f,
                     55.0f);
        fields.Float("jetPush", "…同じ向きへ、火と場の霧を押す速さ (m/s。0 は押さない)", jetPush, 0.0f, 100.0f);
        fields.Float("bodyFireFuel", "炎上の間の体の炎：1 フレームに入れる燃料の強さ（0 は出さない）", bodyFireFuel,
                     0.0f, 10000.0f);
        fields.Float("bodyFireRadius", "…燃料の球の半径 (m)", bodyFireRadius, 0.1f, 4.0f);
        fields.Float("bodyFireOrbit", "…燃料を入れる 2 つの点が体の周りを回る半径 (m。体の外側になる大きさにする)",
                     bodyFireOrbit, 0.0f, 3.0f);
        fields.Float("bodyFireTurns", "…回る速さ (回/秒)", bodyFireTurns, 0.0f, 10.0f);
        fields.Float("bodyFireSpread", "…2 つの点の高さの差の半分 (m。体の中心から上と下へ)", bodyFireSpread, 0.0f,
                     2.0f);
        fields.Float("bodyFireTemperature", "…燃料の温度", bodyFireTemperature, 0.0f, 64.0f);
        fields.Float("bodyFireRise", "…燃料を上へ押す速さ (m/s。炎が体から立ちのぼる高さ)", bodyFireRise, 0.0f, 20.0f);
        fields.Float("bodyFireFadeSeconds", "…消える前に細らせる長さ (秒)", bodyFireFadeSeconds, 0.0f, 10.0f);
        fields.Float("bodyFireScatter", "…ローリング・吸い込みで消えた瞬間に、炎を外へ散らす速さ (m/s。0 は散らさない)",
                     bodyFireScatter, 0.0f, 100.0f);
    }
    if (fields.Section("検証")) {
        fields.Int("traceEveryFrames", "状態ログ間隔（0 は無効）", traceEveryFrames, 0, 7200);
    }
}

} // namespace Project_TD2_2
