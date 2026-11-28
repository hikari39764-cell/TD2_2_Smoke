#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "HIKARI/Game.h"
#include "HIKARI/Project/VfxTypes.h"
#include "BurnMap.h"

namespace Project_TD2_2 {

// 場の火の域に出す、見た目だけの塊の種類。
enum class BurnBlastKind : uint8_t {
    // 炎の刃が当たった所・刀の点火（小さな火球）。
    Hit,
    // 煙の刃が当たった所（暗い煙の塊に、少しだけ炎）。
    Smoke,
};
// 見た目のための 1 塊（位置と半径 m）。
struct BurnBlast {
    HIKARI::GameProjectFloat3 position{};
    float radius = 0.0f;
    BurnBlastKind kind = BurnBlastKind::Hit;
};
// 火の刃が当たった所を突き抜ける炎（BurnField::AddJet が書き、BurnFieldVisuals が進めて消す）。
struct BurnJet {
    // 燃料を入れている点（攻撃の向きへ進む）と、攻撃の向き（水平の単位ベクトル）。
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 direction{};
    float scale = 1.0f;
    // 出し続ける残りの秒数（0 以下は出していない）。
    float remaining = 0.0f;
};
// 描画フレームまでにためておける塊の数。あふれた塊は捨てる（規則は塊を読まないので、当たりは変わらない）。
inline constexpr size_t kBurnBlastCapacity = 64;

// 燃える地面を決まった大きさの升目（タイル）に分け、燃えているセルのあるタイルごとに、場の火の域へ燃料を入れる
// （BurnFieldVisuals が使う）。タイルは場所で決まるので、ほかの所の火が増えても減っても、そのタイルの炎の位置と太さは
// 変わらない。ここは、タイルごとにフレームをまたいで持つ分。
struct BurnTile {
    // 燃えているセルのかたまりの向き（長い方）。向きの符号を前のフレームに揃えるために持つ。
    float axisX = 1.0f;
    float axisZ = 0.0f;
    // 長い方へ行き来している今の位置（かたまりの中心からの距離 m）と、進んでいる向き（+1／-1）。
    float sweep = 0.0f;
    float sweepSign = 1.0f;
    // 升目が多くて順番に入れる時の、この升目の番（0〜間隔−1）。燃え始めた時に決め、燃えている間は変えない（ほかの所の
    // 火が増えても減っても、この升目に入る間隔が乱れない）。燃えていない升目は -1。
    int phase = -1;
    // 検証ログ用：最後に燃料を入れた描画フレーム（燃えていない間は 0）。
    uint32_t fedFrame = 0;
    // この升目に重ねている炎の層のエフェクト（c-3c908c。燃えている間だけ出す）と、その量（0〜1。燃え始めに上げ、
    // 消える時に下げてから止める）、最後に置いた所（薄れていく間はそこに残す）。
    HIKARI::GameProjectVfxPlaybackHandle flame{};
    float flameAmount = 0.0f;
    HIKARI::GameProjectTransform flameTransform{};
};
// タイルごとに、そのフレームの燃えているセルを足し合わせる作業場所（タイルの中心からの相対位置で足す）。
struct BurnTileSum {
    float x = 0.0f;
    float z = 0.0f;
    float xx = 0.0f;
    float zz = 0.0f;
    float xz = 0.0f;
    float remaining = 0.0f;
    int cells = 0;
};
// 1 フレームに出す燃料の点の数の上限。火の域が受け取れる注入は 32 個で、障害物・吸い込みの力・見た目の塊と分け合う。
inline constexpr size_t kBurnEmitterCapacity = 20;
// 一辺のタイルの数の上限（タイルの一辺を小さくしすぎても、これより細かくは分けない）。
inline constexpr int kBurnTilesPerSideMax = 64;

// 燃える地面。格子は持ち主の物体の世界位置（XZ）を中心に置き、その高さを床とする。
// 塗る・消す・当てる規則は BurnField（ほかの機能の入口）・BurnFieldRules・BurnFieldSystem が持ち、
// ここは調整値と実行時の状態だけを持つ。
struct BurnField : HIKARI::Component {
    HIKARI_COMPONENT(BurnField, "燃える地面")
    FIELD(bool, enabled, true, "有効", HIKARI::Group("燃える範囲"))
    // （c-69fc1e：闘技場を広げたので 48 → 88 m・闘技場の半径 22 → 40 m。火の場のエフェクト ArenaFireField も 88 m 角。）
    FIELD(float, fieldSize, 88.0f, "格子の一辺 (m。物体の位置が中心)", HIKARI::Range(1.0f, 200.0f))
    FIELD(float, cellSize, 0.5f, "セルの一辺 (m)", HIKARI::Range(0.1f, 4.0f))
    FIELD(float, arenaRadius, 40.0f, "闘技場の半径 (m。燃えている割合の分母)", HIKARI::Range(1.0f, 200.0f))
    NOTE("格子の大きさとセルの細かさは、Play を始めた時の値で作ります（Play 中に変えても次の Play から効きます）。"
            "物体の高さが床の高さです。")
    // 足元からこの半径の円に燃えているセルが掛かれば、火に触れている。
    FIELD(float, contactRadius, 0.3f, "火に触れたとみなす体の半径 (m。足元からこの円に燃えているセルが掛かれば触れている)", HIKARI::Range(0.0f, 2.0f), HIKARI::Group("火の手傷"))
    // 体の中心が床からこれより高いと、地面の火に触れない (m)。跳んでも届き、筋斗雲で飛べば届かない。
    FIELD(float, fireReachHeight, 4.0f, "火が届く高さ (m。体の中心が床からこれより高いと当たらない。筋斗雲で飛んでいる時など)", HIKARI::Range(0.0f, 100.0f))
    // 触れ続けて燃え移るまでの秒数と、火の中にいる間の炎上の減り方（/秒）。
    FIELD(float, damageDelay, 0.2f, "火に触れ続けてから燃え移るまで (秒)", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, damagePerSecond, 14.0f, "火の中にいる間に減る HP (/秒。プレイヤーの炎上の減り方の代わりに使う)", HIKARI::Range(0.0f, 1000.0f))
    NOTE("火に触れ続けるとプレイヤーが炎上します（炎上の長さ・火の外での減り方はプレイヤーの項目）。"
                    "火はのけぞり・吹っ飛び・ヒットストップ・カメラの揺れを起こさず、HP だけを少しずつ減らします。"
                    "無敵の間（ローリングの無敵・吹っ飛び・ダウン）は触れている時間を数えず、0 に戻します"
                    "（ローリングで火を抜ければ燃え移りません）。")
    FIELD(float, absorbRange, 5.0f, "吸い込みで火を消せる前方の距離 (m)", HIKARI::Range(0.0f, 50.0f), HIKARI::Group("火を吸う"))
    FIELD(float, absorbFanDegrees, 60.0f, "火を消せる扇の片側の角度 (度)", HIKARI::Range(0.0f, 180.0f))
    FIELD(float, absorbNear, 1.5f, "向きを問わず消せる足元の半径 (m)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, absorbAreaPerSecond, 12.0f, "吸い込みで火を消す速さ (m²/秒)", HIKARI::Range(0.0f, 1000.0f))
    FIELD(float, smokePerArea, 8.0f, "消した火 1 m² あたりヒョウタンに入る煙", HIKARI::Range(0.0f, 1000.0f))
    NOTE("扇はプレイヤーの位置から、プレイヤーの向きへ取ります（飛ぶ斬撃を吸う扇と同じ取り方）。"
                    "近い火から消し、ヒョウタンに入りきらない分は消しません（満杯なら消えません）。")
    FIELD(float, fuelRate, 20.0f, "火の強さ：燃料の点 1 つが中心に毎秒入れる燃料", HIKARI::Range(0.0f, 2000.0f), HIKARI::Group("火の見た目（流体）"))
    FIELD(float, fuelTemperature, 5.0f, "燃料の温度（場の火の域の着火温度より高くする）", HIKARI::Range(0.0f, 16.0f))
    FIELD(float, fuelRise, 0.0f, "燃料を上へ押す速さ (m/s。炎の背の高さ)", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, fuelHeight, 0.0f, "燃料を入れる高さ（床から m。床より下にすると、球の上側だけが床の上に出る）", HIKARI::Range(-2.0f, 3.0f))
    FIELD(float, fuelSweep, 45.0f, "燃料の点を、火の長い方へ行き来させる速さ (m/s。0 で動かさない＝球のまま。60 未満)", HIKARI::Range(0.0f, 55.0f))
    FIELD(float, fuelWave, 0.3f, "炎の背のうねり（0 で一様。点ごとに燃料をゆっくり増減させる割合）", HIKARI::Range(0.0f, 0.9f))
    FIELD(float, fuelWaveHz, 0.8f, "うねりの速さ (Hz)", HIKARI::Range(0.05f, 5.0f))
    // 燃えている升目が多くて順番に入れる時、待った回数ぶんの燃料をどれだけまとめて入れるか（待った回数の累乗。1 は全部＝
    // 前の作り、0 は増やさない）。小さいほど、広い火が白く飛ばない（c-3c908c）。
    FIELD(float, fuelCrowdPower, 0.5f, "火が広い時（升目へ順番に入れる時）に、待った分の燃料をまとめて入れる割合（1 で全部、小さいほど広い火が"
            "白く飛ばない）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, fuelRadiusScale, 1.15f, "炎の幅 (燃えている所の幅に対する倍。1 で踏むと痛い範囲とほぼ同じ幅)", HIKARI::Range(0.5f, 4.0f))
    FIELD(float, fadeSeconds, 2.0f, "消える前に火が細くなり始める残り秒数", HIKARI::Range(0.01f, 30.0f))
    FIELD(int, emitterCount, 16, "1 フレームに燃料を入れる所の数（これより燃えている升目が多い時は、数フレームに 1 回ずつ順番に入れる）", HIKARI::Range(1, static_cast<int>(kBurnEmitterCapacity)))
    FIELD(float, emitterTile, 2.0f, "燃料を入れる升目の一辺 (m。升目ごとに 1 か所。小さいほど火の形が細かいが、順番待ちが増える)", HIKARI::Range(0.5f, 8.0f))
    FIELD(float, fogBurnAbsorption, 5.0f, "火の下の霧を焼く率 (/s。0 は焼かない)", HIKARI::Range(0.0f, 1000.0f))
    FIELD(float, fogBurnRadiusScale, 1.8f, "霧を焼く半径 (燃料の点が受け持つ火の長さの半分に対する倍)", HIKARI::Range(0.0f, 6.0f))
    FIELD(float, fogUpdraft, 10.0f, "火のそばの霧を上へ持ち上げる加速度 (m/s²。0 は持ち上げない)", HIKARI::Range(0.0f, 60.0f))
    // 見た目：升目ごとに、流体の火の上へ重ねる炎の層のエフェクト（c-3c908c。空なら出さない）。量の倍率と、燃え始めに
    // 出そろうまで・消える時に薄れるまでの秒数。
    FIELD(HIKARI::VfxAsset, tileEffect, {}, "升目ごとに重ねる炎の層のエフェクト（炎の舌・熾火・立ちのぼる煙・地面の光。空なら流体の火だけ）")
    FIELD(float, tileEffectAmount, 1.0f, "…炎の層の量（倍）", HIKARI::Range(0.0f, 4.0f))
    FIELD(float, tileEffectRise, 0.2f, "…燃え始めに出そろうまで (秒)", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, tileEffectFade, 0.45f, "…消える時に薄れるまで (秒)", HIKARI::Range(0.0f, 5.0f))
    NOTE("炎そのものの色・明るさ・揺れ・煙の量は、エフェクト資産 ArenaFireField の流体域の層で決めます。"
            "ここは、燃えている所へ燃料をどう入れるかだけです。燃料の点は、燃えているセルのかたまりの上に居続けて、"
            "毎フレーム同じ所へ燃料を入れます（つながって燃える炎になります）。"
            "火が広がって点が足りなくなると、1 点が受け持つ半径を広げます（炎は太く、粗くなります）。")
    FIELD(float, blastFuel, 550.0f, "火球へ 1 フレームに入れる燃料の強さ", HIKARI::Range(0.0f, 10000.0f), HIKARI::Group("当たった所と体の炎の見た目"))
    FIELD(float, blastTemperature, 10.0f, "火球の燃料の温度", HIKARI::Range(0.0f, 64.0f))
    FIELD(float, blastRise, 2.0f, "火球を上へ押す速さ (m/s。煙の柱の高さ)", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, blastPush, 8.0f, "爆発が火と霧を外へ押す速さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, hitFuelScale, 0.3f, "炎の刃が当たった所・刀の点火の火球の燃料 (blastFuel に対する倍)", HIKARI::Range(0.0f, 4.0f))
    FIELD(float, smokeBlastStrength, 150.0f, "煙の塊へ 1 フレームに入れる煙の強さ", HIKARI::Range(0.0f, 10000.0f))
    FIELD(float, smokeBlastFuelShare, 0.0f, "煙の塊に混ぜる燃料の割合（煙の中の炎のひらめき）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, smokeBlastPush, 4.0f, "煙の塊が外へ押す速さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, jetFuel, 800.0f, "火の刃がプレイヤーに当たった所：体を突き抜ける炎へ 1 フレームに入れる燃料の強さ", HIKARI::Range(0.0f, 10000.0f))
    FIELD(float, jetSpeed, 13.0f, "…炎を攻撃の向きへ飛ばす速さ (m/s。大きいほど炎の舌が長い)", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, jetRadius, 0.55f, "…燃料の球の半径 (m。炎の舌の太さ)", HIKARI::Range(0.1f, 4.0f))
    FIELD(float, jetTemperature, 7.5f, "…燃料の温度", HIKARI::Range(0.0f, 64.0f))
    FIELD(float, jetSeconds, 0.18f, "…燃料を入れ続ける長さ (秒)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, jetTravel, 9.0f, "…燃料を入れる点が、体の手前から向こうへ進む速さ (m/s。60 未満)", HIKARI::Range(0.0f, 55.0f))
    FIELD(float, jetPush, 8.0f, "…同じ向きへ、火と場の霧を押す速さ (m/s。0 は押さない)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, bodyFireFuel, 260.0f, "炎上の間の体の炎：1 フレームに入れる燃料の強さ（0 は出さない）", HIKARI::Range(0.0f, 10000.0f))
    FIELD(float, bodyFireRadius, 0.32f, "…燃料の球の半径 (m)", HIKARI::Range(0.1f, 4.0f))
    FIELD(float, bodyFireOrbit, 0.42f, "…燃料を入れる 2 つの点が体の周りを回る半径 (m。体の外側になる大きさにする)", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, bodyFireTurns, 1.6f, "…回る速さ (回/秒)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, bodyFireSpread, 0.3f, "…2 つの点の高さの差の半分 (m。体の中心から上と下へ)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, bodyFireTemperature, 7.0f, "…燃料の温度", HIKARI::Range(0.0f, 64.0f))
    FIELD(float, bodyFireRise, 3.0f, "…燃料を上へ押す速さ (m/s。炎が体から立ちのぼる高さ)", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, bodyFireFadeSeconds, 0.8f, "…消える前に細らせる長さ (秒)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, bodyFireScatter, 5.0f, "…ローリング・吸い込みで消えた瞬間に、炎を外へ散らす速さ (m/s。0 は散らさない)", HIKARI::Range(0.0f, 100.0f))
    FIELD(int, traceEveryFrames, 0, "状態ログ間隔（0 は無効）", HIKARI::Range(0, 7200), HIKARI::Group("検証"))

    // 実行時の状態は保存しない。
    bool initialized = false;
    BurnMap map;
    // プレイヤーが燃えているセルに続けて触れている秒数（離れた tick・無敵の tick・吸い込みで火が消えた tick に 0 へ戻す）。
    float contactSeconds = 0.0f;
    // 吸い込みで消せる面積の持ち越し (m²)。セルは丸ごと消すので、1 tick の量が 1 セルに満たない分をためる。
    float absorbBudget = 0.0f;
    uint64_t tick = 0;
    // 検証ログ用の累計。fireDamageTotal はプレイヤーの炎上で減った HP の累計（Player::burnDamageTotal の写し）。
    double fireDamageTotal = 0.0;
    double absorbedAreaTotal = 0.0;
    double smokeGivenTotal = 0.0;
    // 見た目だけの状態。規則はここを読まない。
    // タイルごとの持ち越しと、そのフレームの足し合わせ（大きさが変わった時だけ確保し直す）。一辺のタイルの数と、
    // タイルの一辺のセルの数。
    std::vector<BurnTile> tiles;
    std::vector<BurnTileSum> tileSums;
    int tilesPerSide = 0;
    int tileCells = 0;
    // 検証ログ用：このフレームに燃料を入れたタイルの数・燃えているセルのあるタイルの数・何フレームに 1 回入れているか・
    // 最初に入れたタイルのかたまりの長さと幅 (m)。
    int activeEmitters = 0;
    int burningTiles = 0;
    int emitterStride = 1;
    // 検証ログ用：このフレームに燃料を入れた升目のうち、前に入れてから最も長く空いたフレーム数（燃え続けている升目だけ）。
    int maxFeedGap = 0;
    // 番ごとの升目の数（BurnFieldVisuals の作業場所）。
    std::vector<int> phaseCounts;
    float traceAlong = 0.0f;
    float traceAcross = 0.0f;
    uint32_t renderFrame = 0;
    float visualTime = 0.0f;
    bool fluidAccepted = true;
    bool vfxAccepted = true;
    // 爆ぜた塊の注入の番号を、毎フレーム入れ替える（BurnFieldVisuals が使う）。
    uint8_t blastFlip = 0;
    // 火の刃が当たった所を突き抜ける炎（プレイヤーは 1 人なので 1 つ。続けて当たれば新しい方で置き換える）。
    BurnJet jet;
    // 体の炎が消えた回数のうち、見た目が散らし終えた分（Player::burnEndings と比べる）。
    uint32_t bodyEndingsSeen = 0;
    // 見た目だけの塊（BurnField::AddBlast が固定 tick ごとに後ろへ足す。いっぱいなら捨てる）。読むのと
    // blastCount を 0 に戻すのは BurnFieldVisuals（描画フレーム）。規則は読まない。
    std::array<BurnBlast, kBurnBlastCapacity> blasts{};
    uint32_t blastCount = 0;
};

} // namespace Project_TD2_2
