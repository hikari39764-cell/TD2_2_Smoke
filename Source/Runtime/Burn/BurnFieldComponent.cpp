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
        fields.Float("fuelPerArea", "火の強さ：燃えている広さ 1 m² に毎秒入れる燃料", fuelPerArea, 0.0f, 200.0f);
        fields.Float("fuelRadius", "燃料の球の半径 (m。火の幅とぼけ方)", fuelRadius, 0.2f, 4.0f);
        fields.Float("fuelTemperature", "続けて配っている所の燃料の温度（場の火の域の着火温度より高くする）",
                     fuelTemperature, 0.0f, 16.0f);
        fields.Float("fuelIgniteTemperature", "冷えきった所へ入れる燃料の温度（着火用）", fuelIgniteTemperature, 0.0f,
                     64.0f);
        fields.Float("fuelRise", "燃料を上へ押す速さ (m/s。炎の背の高さ)", fuelRise, 0.0f, 20.0f);
        fields.Float("fuelHeight", "燃料を入れる高さ（床から m）", fuelHeight, 0.0f, 3.0f);
        fields.Float("fadeSeconds", "消える前に火が細くなり始める残り秒数", fadeSeconds, 0.01f, 30.0f);
        fields.Int("painterCount", "燃料を配る筆の最大数", painterCount, 1, 8);
        fields.Float("areaPerPainter", "筆 1 本が受け持つ広さ (m²)", areaPerPainter, 1.0f, 500.0f);
        fields.Float("fuelMarkRadius", "筆が燃料を置いた時に「配った」とみなす半径 (m)", fuelMarkRadius, 0.1f, 4.0f);
        fields.Float("fogBurnAbsorption", "火の下の霧を焼く率 (/s。筆が通る間だけ効く。0 は焼かない)",
                     fogBurnAbsorption, 0.0f, 1000.0f);
        fields.Float("fogBurnRadius", "霧を焼く半径 (m)", fogBurnRadius, 0.0f, 6.0f);
        fields.Note(
            "筆は 1 か所に 1 フレームしか燃料を置かず、注入はそこの温度を 1/60 秒に 13% しか燃料の温度へ近づけません。"
            "冷えきった所へは熱い燃料を入れて着火させます（低いと燃料が冷たいまま消えて、火が出ません）。"
            "狭い火で同じ所へ続けて配る時は fuelTemperature まで下げます（熱すぎると白く飛び、炎が高く噴きます）。");
        fields.Note(
            "炎そのものの色・明るさ・揺れ・煙の量は、エフェクト資産 ArenaFireField の流体域の層で決めます。"
            "ここは、燃えているセルへ燃料をどれだけ・どう配るかだけです。"
            "燃えている所が広いほど筆が同じ所へ戻るまでの時間が延びます（筆 1 本が 1 秒に配れるのは約 120 m²）。"
            "炎がまだらに明滅して見える時は、筆を増やすか areaPerPainter を下げます。");
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
