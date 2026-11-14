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
    // 引爆で爆ぜた火（床の上の位置。火球と爆発のエフェクト）。
    Detonation,
    // 炎の刃が当たった所・刀の点火（小さな火球）。
    Hit,
    // 煙の刃が当たった所・霞隠れや分身の出入り（暗い煙の塊に、少しだけ炎）。
    Smoke,
};
// 見た目のための 1 塊（位置と半径 m）。引爆では、1 tick に爆ぜたセルをまとめた物。
struct BurnBlast {
    HIKARI::GameProjectFloat3 position{};
    float radius = 0.0f;
    BurnBlastKind kind = BurnBlastKind::Detonation;
    // 引爆の塊が、その引爆で何番目に出来たか（1 から。捨てた塊も数える）。引爆以外は 0。
    uint32_t order = 0;
    // 引爆の塊の強さ（場全体の引爆は 1。一か所だけの爆発は呼ぶ側が決める）：燃料と押す速さに掛け、1 より小さい分だけ
    // 燃料の温度を地面の火の温度（fuelTemperature）へ寄せる（弱い爆発は、白く飛ばずに橙の火球になる）。
    float power = 1.0f;
};
// 描画フレームまでにためておける塊の数。あふれた塊は捨てる（規則は塊を読まないので、当たりは変わらない）。
inline constexpr size_t kBurnBlastCapacity = 32;
// 爆発のエフェクトの上限（BurnField の blastEffectCap）に選べる最大。出した時刻をこの数だけ覚えておく。
inline constexpr size_t kBlastEffectCapMax = 256;

// 燃えている所に居続けて、場の火の域へ燃料を入れる点（BurnFieldVisuals が使う）。
struct BurnEmitter {
    bool active = false;
    float x = 0.0f;
    float z = 0.0f;
    // 受け持っているセルの少なさで燃料を絞る割合（0〜1）と、いちばん長く残る秒数（消え際に細らせる）。
    float share = 0.0f;
    float remaining = 0.0f;
    // 受け持っているセルのかたまりの向き（長い方）と、長さ・幅 (m)。火線なら線の向きと線の幅になる。
    float axisX = 1.0f;
    float axisZ = 0.0f;
    float along = 0.0f;
    float across = 0.0f;
    // 長い方へ行き来している今の位置（中心からの距離 m）と、進んでいる向き（+1／-1）。
    float sweep = 0.0f;
    float sweepSign = 1.0f;
};
// 燃料の点の数の上限。火の域が受け取れる注入は 32 個で、吸い込みの力・剣の炎・爆ぜた塊と分け合う。
inline constexpr size_t kBurnEmitterCapacity = 20;

// 燃える地面。格子は持ち主の物体の世界位置（XZ）を中心に置き、その高さを床とする。
// 塗る・消す・当てる規則は BurnFieldRules と BurnFieldSystem が持ち、ここは調整値と実行時の状態だけを持つ。
class BurnFieldComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    bool enabled = true;
    float fieldSize = 48.0f;
    float cellSize = 0.5f;
    float arenaRadius = 22.0f;
    float damageDelay = 0.25f;
    float damagePerSecond = 8.0f;
    float absorbRange = 5.0f;
    float absorbFanDegrees = 60.0f;
    float absorbNear = 1.5f;
    float absorbAreaPerSecond = 12.0f;
    float smokePerArea = 8.0f;
    int emitterCount = 16;
    float emitterCover = 1.1f;
    float emitterCoverMax = 3.5f;
    float fuelRate = 20.0f;
    float fuelRadiusScale = 1.15f;
    float fuelHeight = 0.0f;
    float fuelTemperature = 5.0f;
    float fuelRise = 0.0f;
    float fuelSweep = 45.0f;
    float fuelWave = 0.3f;
    float fuelWaveHz = 0.8f;
    float fadeSeconds = 2.0f;
    float fogBurnAbsorption = 3.0f;
    float fogBurnRadiusScale = 1.4f;
    float fogUpdraft = 10.0f;
    float chargeTemperature = 12.0f;
    float chargeShrink = 0.55f;
    float chargeFuelScale = 1.8f;
    float chargeRiseScale = 0.3f;
    float hotspotTemperature = 5.5f;
    float hotspotFuelScale = 1.5f;
    float hotspotRise = 3.0f;
    HIKARI::GameProjectVfxAssetReference blastEffect{"bd16110e0b1bf90ce62d5502eb61c0d3",
                                                     "Assets/Vfx/BossSlashBurst.hvfx"};
    float blastEffectScale = 0.6f;
    int blastEffectCap = 60;
    float blastEffectWindow = 1.4f;
    float blastRadiusScale = 1.4f;
    float blastFuel = 400.0f;
    float blastTemperature = 10.0f;
    float blastRise = 2.0f;
    float blastPush = 8.0f;
    float hitFuelScale = 0.3f;
    float smokeBlastStrength = 150.0f;
    float smokeBlastFuelShare = 0.0f;
    float smokeBlastPush = 4.0f;
    float blastBlockSize = 2.0f;
    int traceEveryFrames = 0;

    // 実行時の状態は保存しない。
    bool initialized = false;
    BurnMap map;
    // プレイヤーが燃えているセルに続けて立っている秒数（離れた tick に 0 へ戻す）。
    float contactSeconds = 0.0f;
    // 吸い込みで消せる面積の持ち越し (m²)。セルは丸ごと消すので、1 tick の量が 1 セルに満たない分をためる。
    float absorbBudget = 0.0f;
    uint64_t tick = 0;
    // 検証ログ用の累計。
    double fireDamageTotal = 0.0;
    double absorbedAreaTotal = 0.0;
    double smokeGivenTotal = 0.0;
    // 見た目だけの状態。規則はここを読まない。
    // 燃料の点と、1 点が受け持つ半径 (m。火が広がって点が足りなくなると広げる)。
    std::array<BurnEmitter, kBurnEmitterCapacity> emitters{};
    float cover = 0.0f;
    int activeEmitters = 0;
    uint32_t renderFrame = 0;
    float visualTime = 0.0f;
    bool fluidAccepted = true;
    bool vfxAccepted = true;
    // 爆ぜた塊の注入の番号を、毎フレーム入れ替える（BurnFieldVisuals が使う）。
    uint8_t blastFlip = 0;
    // 引爆から見た目への受け渡し。ボスの引爆（BurnFieldRules::SetCharge／Explode）が書くだけで、読むのと
    // blastCount を 0 に戻すのは BurnFieldVisuals（描画フレーム）。規則はどちらも読まない。
    // 溜めの進み（0〜1）。溜めていない間・崩された後・爆ぜ始めた後は 0。
    float charge = 0.0f;
    // 一か所だけの溜め（天降り斬りの着地の火。BurnFieldRules::SetHotspot）：真ん中・半径 (m)・進み（0〜1）。
    // その円の中の火だけが、溜めの見た目になる。半径か進みが 0 なら無し。
    HIKARI::GameProjectFloat3 hotspotCenter{};
    float hotspotRadius = 0.0f;
    float hotspotCharge = 0.0f;
    // 爆ぜた塊。固定 tick ごとに後ろへ足す（いっぱいなら捨てる）。
    std::array<BurnBlast, kBurnBlastCapacity> blasts{};
    uint32_t blastCount = 0;
    // 今の引爆で出ると見込んだ塊の数と、ここまでに出来た塊の数（規則が前線の最初の輪で見込み、塊ごとに数える）。
    uint32_t blastPlan = 0;
    uint32_t blastOrder = 0;
    // 見込みを数える作業場所（格子を作る時に一度だけ確保し、引爆のたびには確保しない）。
    std::vector<uint64_t> blastPlanKeys;
    // 爆発のエフェクトを出した時刻（visualTime。直近 kBlastEffectCapMax 個の輪）と、検証ログ用の累計：
    // 出した数・見込みに対して均等に間引いた数・窓の上限で止めた数・窓の中に同時にあった数の最大。
    std::array<float, kBlastEffectCapMax> blastEffectTimes{};
    uint32_t blastEffectsPlayed = 0;
    uint32_t blastEffectsThinned = 0;
    uint32_t blastEffectsHeld = 0;
    uint32_t blastEffectPeak = 0;
};

} // namespace Project_TD2_2
