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
        fields.Float("damageDelay", "燃えている所に立ってから減り始めるまで (秒)", damageDelay, 0.0f, 5.0f);
        fields.Float("damagePerSecond", "立っている間に減る HP (/秒)", damagePerSecond, 0.0f, 1000.0f);
        fields.Note("火はのけぞり・吹っ飛び・ヒットストップ・カメラの揺れを起こさず、HP だけを少しずつ減らします。"
                    "ローリングの無敵の間は減りませんが、立っている時間は数え続けます。");
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
        fields.Float("fuelRadiusScale", "炎の幅 (燃えている所の幅に対する倍。1 で踏むと痛い範囲とほぼ同じ幅)",
                     fuelRadiusScale, 0.5f, 4.0f);
        fields.Float("fadeSeconds", "消える前に火が細くなり始める残り秒数", fadeSeconds, 0.01f, 30.0f);
        fields.Int("emitterCount", "燃料の点の最大数", emitterCount, 1, static_cast<int>(kBurnEmitterCapacity));
        fields.Float("emitterCover", "点 1 つが受け持つ半径の最小 (m。小さいほど火の形が細かい)", emitterCover, 0.5f,
                     5.0f);
        fields.Float("emitterCoverMax", "…最大 (m。火が広くて点が足りない時に、ここまで広げる)", emitterCoverMax, 0.5f,
                     10.0f);
        fields.Float("fogBurnAbsorption", "火の下の霧を焼く率 (/s。0 は焼かない)", fogBurnAbsorption, 0.0f, 1000.0f);
        fields.Float("fogBurnRadiusScale", "霧を焼く半径 (燃料の点が受け持つ火の長さの半分に対する倍)",
                     fogBurnRadiusScale, 0.0f, 6.0f);
        fields.Note(
            "炎そのものの色・明るさ・揺れ・煙の量は、エフェクト資産 ArenaFireField の流体域の層で決めます。"
            "ここは、燃えている所へ燃料をどう入れるかだけです。燃料の点は、燃えているセルのかたまりの上に居続けて、"
            "毎フレーム同じ所へ燃料を入れます（つながって燃える炎になります）。"
            "火が広がって点が足りなくなると、1 点が受け持つ半径を広げます（炎は太く、粗くなります）。");
    }
    if (fields.Section("引爆の見た目")) {
        fields.Float("blastBlockSize", "爆ぜたセルを 1 つの塊にまとめる大きさ (m)", blastBlockSize, 0.5f, 20.0f);
        fields.Note(
            "ボスの引爆で同じ tick に爆ぜたセルを、この大きさの升ごとに 1 つの塊（爆発の見た目 1 つ）にまとめます。"
            "当たりはセルごとに見るので、ここを変えても当たりは変わりません。");
    }
    if (fields.Section("引爆の見た目")) {
        fields.Float("chargeTemperature", "溜めきった時の燃料の温度（高いほど火が白く締まる）", chargeTemperature, 0.0f,
                     64.0f);
        fields.Float("chargeShrink", "溜めきった時の火の幅 (倍。小さいほど内側へ縮む)", chargeShrink, 0.1f, 1.0f);
        fields.Float("chargeFuelScale", "溜めきった時の燃料の量 (倍)", chargeFuelScale, 0.0f, 10.0f);
        fields.Float("chargeRiseScale", "溜めきった時の、燃料を上へ押す速さ (倍。小さいほど炎が低く張り付く)",
                     chargeRiseScale, 0.0f, 4.0f);
        fields.Vfx("blastEffect", "爆ぜた所の爆発のエフェクト（光・衝撃の輪・火花・煙）", blastEffect);
        fields.Float("blastEffectScale", "…爆発のエフェクトの大きさ (火球の半径に対する倍)", blastEffectScale, 0.0f,
                     10.0f);
        fields.Float("blastRadiusScale", "火球の半径 (規則の塊の半径に対する倍)", blastRadiusScale, 0.1f, 10.0f);
        fields.Float("blastFuel", "火球へ 1 フレームに入れる燃料の強さ", blastFuel, 0.0f, 10000.0f);
        fields.Float("blastTemperature", "火球の燃料の温度", blastTemperature, 0.0f, 64.0f);
        fields.Float("blastRise", "火球を上へ押す速さ (m/s。煙の柱の高さ)", blastRise, 0.0f, 20.0f);
        fields.Float("blastPush", "爆発が火と霧を外へ押す速さ (m/s)", blastPush, 0.0f, 100.0f);
        fields.Float("hitFuelScale", "炎の刃が当たった所・刀の点火の火球の燃料 (引爆の火球に対する倍)", hitFuelScale,
                     0.0f, 4.0f);
        fields.Float("smokeBlastStrength", "煙の塊へ 1 フレームに入れる煙の強さ", smokeBlastStrength, 0.0f, 10000.0f);
        fields.Float("smokeBlastFuelShare", "煙の塊に混ぜる燃料の割合（煙の中の炎のひらめき）", smokeBlastFuelShare,
                     0.0f, 1.0f);
        fields.Float("smokeBlastPush", "煙の塊が外へ押す速さ (m/s)", smokeBlastPush, 0.0f, 100.0f);
        fields.Note("溜めは、火そのものが熱く・細く・低くなることで見せます（地面や画面の予兆は出しません）。"
                    "爆発は、規則が爆ぜさせた火の塊ごとに、火の域へ燃料と衝撃を入れ、爆発のエフェクトを 1 つ出します"
                    "（1 フレームに 3 つまで。残りは次のフレームへ回るので、広い火は波のように順に爆ぜます）。");
    }
    if (fields.Section("検証")) {
        fields.Int("traceEveryFrames", "状態ログ間隔（0 は無効）", traceEveryFrames, 0, 7200);
    }
}

} // namespace Project_TD2_2
