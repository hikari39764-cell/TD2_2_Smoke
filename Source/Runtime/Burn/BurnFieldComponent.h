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
// 引爆で 1 tick に爆ぜたセルを、見た目のためにまとめた 1 塊（床の上の位置と、セルの数から出した半径 m）。
struct BurnBlast {
    HIKARI::GameProjectFloat3 position{};
    float radius = 0.0f;
};
// 描画フレームまでにためておける塊の数。あふれた塊は捨てる（規則は塊を読まないので、当たりは変わらない）。
inline constexpr size_t kBurnBlastCapacity = 32;

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
    int painterCount = 6;
    float areaPerPainter = 18.0f;
    float fuelPerArea = 1.6f;
    float fuelRadius = 1.0f;
    float fuelMarkRadius = 0.8f;
    float fuelHeight = 0.15f;
    float fuelTemperature = 3.0f;
    float fuelIgniteTemperature = 6.0f;
    float fuelRise = 2.0f;
    float fadeSeconds = 2.0f;
    float fogBurnAbsorption = 30.0f;
    float fogBurnRadius = 1.4f;
    float chargeTemperature = 12.0f;
    float chargeShrink = 0.55f;
    float chargeFuelScale = 1.8f;
    float chargeRiseScale = 0.3f;
    HIKARI::GameProjectVfxAssetReference blastEffect{"bd16110e0b1bf90ce62d5502eb61c0d3",
                                                     "Assets/Vfx/BossSlashBurst.hvfx"};
    float blastEffectScale = 0.6f;
    float blastRadiusScale = 1.4f;
    float blastFuel = 600.0f;
    float blastTemperature = 10.0f;
    float blastRise = 2.0f;
    float blastPush = 8.0f;
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
    // 筆ごとの、毎フレーム入れ替える注入の番号（BurnFieldVisuals が使う）。
    std::array<uint8_t, 8> painterFlip{};
    // セルごとの、最後に燃料をもらってからの秒数（筆が次に行くセルを選ぶのに使う）。
    std::vector<float> fueledAgo;
    int activePainters = 0;
    bool fluidAccepted = true;
    bool vfxAccepted = true;
    // 爆ぜた塊の注入の番号を、毎フレーム入れ替える（BurnFieldVisuals が使う）。
    uint8_t blastFlip = 0;
    // 引爆から見た目への受け渡し。ボスの引爆（BurnFieldRules::SetCharge／Explode）が書くだけで、読むのと
    // blastCount を 0 に戻すのは BurnFieldVisuals（描画フレーム）。規則はどちらも読まない。
    // 溜めの進み（0〜1）。溜めていない間・崩された後・爆ぜ始めた後は 0。
    float charge = 0.0f;
    // 爆ぜた塊。固定 tick ごとに後ろへ足す（いっぱいなら捨てる）。
    std::array<BurnBlast, kBurnBlastCapacity> blasts{};
    uint32_t blastCount = 0;
};

} // namespace Project_TD2_2
