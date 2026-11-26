#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "BurnMap.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kBurnFieldType = "Project_TD2_2.BurnField";
inline constexpr std::string_view kBurnFieldSystem = "Project_TD2_2.BurnFieldSystem";
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
class BurnFieldComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    bool enabled = true;
    // （c-69fc1e：闘技場を広げたので 48 → 88 m・闘技場の半径 22 → 40 m。火の場のエフェクト ArenaFireField も 88 m 角。）
    float fieldSize = 88.0f;
    float cellSize = 0.5f;
    float arenaRadius = 40.0f;
    // 足元からこの半径の円に燃えているセルが掛かれば、火に触れている。
    float contactRadius = 0.3f;
    // 体の中心が床からこれより高いと、地面の火に触れない (m)。跳んでも届き、筋斗雲で飛べば届かない。
    float fireReachHeight = 4.0f;
    // 触れ続けて燃え移るまでの秒数と、火の中にいる間の炎上の減り方（/秒）。
    float damageDelay = 0.2f;
    float damagePerSecond = 14.0f;
    float absorbRange = 5.0f;
    float absorbFanDegrees = 60.0f;
    float absorbNear = 1.5f;
    float absorbAreaPerSecond = 12.0f;
    float smokePerArea = 8.0f;
    int emitterCount = 16;
    float emitterTile = 2.0f;
    float fuelRate = 20.0f;
    float fuelRadiusScale = 1.15f;
    float fuelHeight = 0.0f;
    float fuelTemperature = 5.0f;
    float fuelRise = 0.0f;
    float fuelSweep = 45.0f;
    float fuelWave = 0.3f;
    float fuelWaveHz = 0.8f;
    float fadeSeconds = 2.0f;
    float fogBurnAbsorption = 5.0f;
    float fogBurnRadiusScale = 1.8f;
    float fogUpdraft = 10.0f;
    float blastFuel = 550.0f;
    float blastTemperature = 10.0f;
    float blastRise = 2.0f;
    float blastPush = 8.0f;
    float hitFuelScale = 0.3f;
    float smokeBlastStrength = 150.0f;
    float smokeBlastFuelShare = 0.0f;
    float smokeBlastPush = 4.0f;
    float jetFuel = 800.0f;
    float jetSpeed = 13.0f;
    float jetRadius = 0.55f;
    float jetTemperature = 7.5f;
    float jetSeconds = 0.18f;
    float jetTravel = 9.0f;
    float jetPush = 8.0f;
    float bodyFireFuel = 260.0f;
    float bodyFireRadius = 0.32f;
    float bodyFireOrbit = 0.42f;
    float bodyFireTurns = 1.6f;
    float bodyFireSpread = 0.3f;
    float bodyFireTemperature = 7.0f;
    float bodyFireRise = 3.0f;
    float bodyFireFadeSeconds = 0.8f;
    float bodyFireScatter = 5.0f;
    int traceEveryFrames = 0;

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
