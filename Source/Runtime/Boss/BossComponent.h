#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "BossMoveTable.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kBossType = "Project_TD2_2.Boss";
inline constexpr std::string_view kBossSystem = "Project_TD2_2.BossSystem";
// 同時に飛ぶ斬撃・ナイフの枠。連撃 5 撃とナイフ 3 本が重なっても足りる数。
inline constexpr size_t kBossWaveCapacity = 12;
// 同時に見せる斬撃のエフェクトの枠。1 つが流体域を 1 つ使う。流体域は 1 フレームに 6 個まで（均衡画質）で、
// 場の霧・吸い口・砲撃と分け合う（出している間は砲撃の枠を同じ数だけ減らす）。
inline constexpr size_t kBossSlashEffectCapacity = 2;
// 当たった所の爆発の枠（連撃とナイフ 3 本が続けて当たっても足りる数）。
inline constexpr size_t kBossBlastCapacity = 4;
// 斬撃が消えた所などに置く残り煙の枠。
inline constexpr size_t kBossDepositCapacity = 8;
// 連続攻撃の判定に覚えておく命中の数（試作と同じ 40）。
inline constexpr size_t kBossHitMemory = 40;

enum class BossState { Idle, Act, Stunned, Dead };
enum class BossActPhase { Pre, Act, Post };

// 飛んでいる霞の斬撃（三日月）か投げナイフ。当たり判定は煙と別に持つので、煙を吸われても斬撃は消えない。
struct BossWave {
    bool active = false;
    bool knife = false;
    // 当たった・無敵で抜けられた後は、もう当たらない（煙は飛び続ける）。
    bool done = false;
    bool dodged = false;
    // プレイヤーに当たった（エフェクトが刃を砕けさせる）。
    bool struck = false;
    bool blow = false;
    int move = -1;
    int strike = 0;
    uint64_t id = 0;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 direction{0.0f, 0.0f, 1.0f};
    // 三日月の幅の向き（飛ぶ向きに直交）。
    HIKARI::GameProjectFloat3 across{1.0f, 0.0f, 0.0f};
    float width = 0.0f;
    float fullWidth = 0.0f;
    float grow = 1.0f;
    float growStart = 1.0f;
    float thickness = 0.5f;
    float speed = 0.0f;
    float travelled = 0.0f;
    int damage = 0;
    // この tick に場の霧へ入れる煙の量（描画フレームで注入する。煙の単位）。
    float smokeRate = 0.0f;
    // 放った直後は斬撃のエフェクトの域と重なるので入れず、ここに貯めて後から足す。
    float smokeOwed = 0.0f;
    // 吸い込みで吸ったときに増える煙（後のカードの吸い込みが読む。ここでは持つだけ）。
    float absorbSmoke = 0.0f;
};

// 飛んでいる斬撃 1 つを見せるエフェクト（自前の流体域を持つ三日月の霧）。斬撃が消えた後も、残った霧が消えるまで飛ばす。
struct BossSlashEffect {
    HIKARI::GameProjectVfxPlaybackHandle handle{};
    uint64_t waveId = 0;
    bool flying = false;
    bool struck = false;
    float fadeSeconds = 0.0f;
    float impactSeconds = 0.0f;
    // 光の層の大きさ（1＝出ている、0＝消えた）。刃を出すのをやめたら数フレームで 0 へ縮める。
    float glow = 1.0f;
    float travelled = 0.0f;
    float scale = 1.0f;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 velocity{};
    HIKARI::GameProjectQuaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
};

// 斬撃・ナイフ・剣がプレイヤーに当たった所の爆発。光のエフェクトを 1 回出し、足元の霧を 1 回外へ押す。
struct BossBlast {
    bool pushPending = false;
    bool effectPending = false;
    float scale = 1.0f;
    HIKARI::GameProjectFloat3 position{};
};

// 場の霧へ少しずつ入れる煙（斬撃の終点・ボスの通った跡）。
struct BossSmokeDeposit {
    bool active = false;
    HIKARI::GameProjectFloat3 position{};
    float radius = 1.0f;
    float smokeLeft = 0.0f;
    float smokePerFrame = 0.0f;
    float rate = 0.0f;
};

// 剣を振っている一撃（判定区間ごと）。振り抜いた瞬間に、剣先の霞の弧から斬撃を飛ばす。
struct BossSwing {
    bool active = false;
    bool released = false;
    // 振っている剣そのものの当たり。飛び立った斬撃と同じ一撃なので、当たるのは 1 回だけ。
    bool done = false;
    int segment = -1;
    int strike = 0;
    int samples = 0;
    float heightSum = 0.0f;
    HIKARI::GameProjectFloat3 firstOuter{};
    HIKARI::GameProjectFloat3 lastOuter{};
};

// 技の選び方が覚えていること（行動ツリーの節が BossSelection を通して読み書きする）。
struct BossBrainMemory {
    // 直前に始動した技の行（中立の技選びで重みを減らす）。無ければ -1。
    int lastStarted = -1;
    // 待機中・移動中に撃たれた回数。突進系を出すと 0 に戻る。
    int moveHits = 0;
    // ボスに当たった時刻（秒）。新しいものから kBossHitMemory 個まで（輪にして使う）。
    std::array<float, kBossHitMemory> hitSeconds{};
    size_t hitCount = 0;
    size_t hitNext = 0;
    uint64_t seenHits = 0;
    // 連続攻撃へ次に反応できる時刻（秒）。
    float pressureReadySeconds = 0.0f;
    // 行動ツリーを最後に進めた時刻（秒）。木の中の時間（Wait・Cooldown）はこの差で進む。
    float thoughtSeconds = 0.0f;
    // 表に始動技としてあるのに、木に節が無い行を一度だけ知らせるための記録（表を読み直すと消す）。
    uint64_t scoredRows = 0;
    bool coverageChecked = false;
    // 木の節が出そうとして表に無かった名前（名前ごとに一度だけ知らせる）。
    std::array<size_t, 8> unknownNames{};
    size_t unknownCount = 0;
    bool treeProblemLogged = false;
};

// ボス「霞武士」の本体。HP は同じ物体の Damageable が持ち、ここは技の実行・気絶・浮遊・斬撃を持つ。
// 技の表は Data/ の CSV。どの技を出すかは、同じ物体の行動ツリー（Behavior Tree、Update＝GameCode）が
// BossRules::RequestMove で頼む。
class BossComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    // 表
    std::string movesPath = "Boss/kasumi_bushi_moves.csv";
    std::string typesPath = "Boss/kasumi_bushi_move_types.csv";
    // 斬撃の共通の値（試作の共通パラメータ）
    float range = 26.0f;
    int swingFrames = 10;
    int gapSmallFrames = 10;
    int gapMediumFrames = 20;
    int gapLargeFrames = 44;
    int followFrames = 6;
    int feintFrames = 24;
    int kamaeFrames = 20;
    int raiseFrames = 12;
    int swingInFrames = 6;
    float damageBase = 6.0f;
    float damagePerFrame = 0.15f;
    float thickness = 0.5f;
    // フェーズと気絶
    float phaseTwoHealthPercent = 60.0f;
    float stunMax = 110.0f;
    int stunLimit = 0;
    int stunFrames = 180;
    float stunDamageScale = 1.5f;
    float stunDecayDelaySeconds = 6.0f;
    float stunDecayPerSecond = 10.0f;
    float stunPerShot = 22.0f;
    float stunReferenceDamage = 40.0f;
    // 技の選び方（行動ツリーの節が使う共通の値。試作の共通パラメータ）
    float selectFarDistance = 20.0f;
    float selectMinWeight = 0.05f;
    float selectPower = 2.0f;
    float selectRepeatScale = 0.5f;
    float moveHitBoost = 1.0f;
    int moveHitMax = 5;
    float interruptPower = 3.0f;
    int comboMaxChain = 3;
    float comboChancePercent = 65.0f;
    // 体
    float bodyHalfHeight = 1.7f;
    float bodyRadius = 0.68f;
    HIKARI::GameProjectFloat3 arenaCenter{0.0f, 3400.0f, 0.0f};
    float arenaRadius = 21.0f;
    float walkSpeed = 2.2f;
    float floatMin = 0.2f;
    float floatMax = 1.4f;
    float floatBob = 0.12f;
    float idleTurnRate = 240.0f;
    float preTurnRate = 360.0f;
    float overheadTurnRate = 220.0f;
    float stringGapTurnRate = 200.0f;
    float leanFromSpeed = 0.05f;
    float leanMaxRadians = 0.3f;
    float poseLeanDegrees = 9.0f;
    float poseSwayDegrees = 7.0f;
    float crouchSquash = 0.18f;
    float stunLeanRadians = 0.45f;
    // 動きの型ごとの体の動き
    float dashStopDistance = 2.84f;
    int dashTeleportFrames = 12;
    float slashApproachSpeed = 1.2f;
    float stringApproachSpeed = 1.6f;
    float stringApproachStop = 2.6f;
    float overheadLungeDistance = 1.8f;
    float stepInClearance = 1.2f;
    float walkInStopDistance = 4.5f;
    float walkInSpeedScale = 1.25f;
    float backstepDistance = 3.5f;
    float backstepHeight = 0.5f;
    // 剣（いまは見えない剣。当たりと煙の位置だけに使う）
    HIKARI::GameProjectFloat3 swordShoulder{-0.727f, 2.372f, 0.158f};
    float swordLength = 2.253f;
    float swordHazeLength = 0.6f;
    float waveForward = 0.9f;
    float waveMinHeight = 0.8f;
    float waveMaxHeight = 2.37f;
    int waveGrowFrames = 8;
    float knifeHeight = 2.37f;
    float knifeForward = 0.79f;
    float knifeSpreadDegrees = 14.0f;
    float knifeWidth = 0.3f;
    float knifeThickness = 0.2f;
    // プレイヤーの当たり（プレイヤーの原点からの高さ）
    float playerHitLow = -0.5f;
    float playerHitHigh = 0.55f;
    float playerHitRadius = 0.34f;
    float playerAimHeight = 0.15f;
    // 残る煙（試作の residue／bossResidue）と吸い込みの煙（試作の waveSmoke／knifeSmoke）
    float residuePerTwoMeters = 10.0f;
    float knifeEndResidueScale = 0.5f;
    float bossResidue = 3.0f;
    float bossResidueStep = 1.2f;
    float waveAbsorbSmoke = 25.0f;
    float knifeAbsorbSmoke = 6.0f;
    float swingSmoke = 6.0f;
    float kamaeSmoke = 2.0f;
    int kamaeSmokeFrames = 15;
    float waveSmokeRadius = 1.2f;
    float waveSmokeSink = 1.5f;
    float waveSmokeDrag = 0.1f;
    float waveResidueLag = 5.0f;
    float waveResidueHeight = 0.7f;
    float swordSmokeRadius = 0.45f;
    float depositRadius = 1.5f;
    float depositHeight = 0.6f;
    int depositFrames = 24;
    float bodySmokeRadius = 1.3f;
    // 見た目：飛ぶ斬撃（自前の流体域を持つ三日月の霧。局所 +X が飛ぶ向き、刃は局所 XY 面）
    HIKARI::GameProjectVfxAssetReference slashEffect{"6e539e255514847188410bf7c2d81a0c",
                                                     "Assets/Vfx/BossSlashWave.hvfx"};
    float slashEffectWidth = 11.0f;
    float slashFadeSeconds = 0.25f;
    float slashImpactSeconds = 0.2f;
    float slashHitSpeedScale = 0.15f;
    float slashGlowFadeSeconds = 0.1f;
    // 見た目：当たった所の爆発（光・火花・爆炎のエフェクトと、足元の霧を外へ押す力）
    HIKARI::GameProjectVfxAssetReference blastEffect{"bd16110e0b1bf90ce62d5502eb61c0d3",
                                                     "Assets/Vfx/BossSlashBurst.hvfx"};
    // 当たった斬撃の炎を消すまでの時間。爆発の光に隠して、ほぼすぐ消す（残すと、薄い域の炎が板のように広がって画面を塞ぐ）。
    float slashHitFadeSeconds = 0.07f;
    float blastPush = 9.0f;
    float blastPushRadius = 3.0f;
    float knifeBlastScale = 0.4f;
    // 見た目（流体注入のエフェクト。場の霧の域へ色の付いた霞を入れる）
    HIKARI::GameProjectVfxAssetReference swordEffect{"353886750fe7c2c7f8939f18e5df6b31",
                                                     "Assets/Vfx/BossSwordHaze.hvfx"};
    HIKARI::GameProjectFloat3 hazeColor{0.86f, 0.87f, 0.9f};
    float swordEffectAmount = 0.3f;
    float kamaeEffectAmount = 0.15f;
    // 剣先の火の粉の光の大きさ（倍）。構えの合図。煙と違って場に残らない。
    float swordEmberScale = 1.0f;
    float kamaeEmberScale = 0.6f;
    float knifeEffectAmount = 0.8f;
    // 見た目：投げナイフそのもの（刃と光点。局所 +X が飛ぶ向き。流体域は使わない）
    HIKARI::GameProjectVfxAssetReference knifeEffect{"26fa37413fbcc48cc52d0605225433bb", "Assets/Vfx/BossKnife.hvfx"};
    // 確かめ用（正式シーンでは使わない）
    std::string testMove;
    bool testSequence = false;
    bool testLoop = false;
    int testStartFrames = 60;
    int testGapFrames = 40;
    bool testHoldStill = false;
    int testDamageAtFrame = -1;
    float testDamageAmount = 40.0f;
    int testDamageRepeatFrames = 0;
    int testDamageRepeatCount = 1;
    int traceEveryFrames = 0;
    int randomSeed = 7;

    // 実行時の状態は保存しない。
    bool initialized = false;
    bool tableLoaded = false;
    BossMoveTable table;
    // 最後に読んだ表の中身。同じなら読み直さない（警告を何度も出さない）。
    std::string loadedMovesText;
    std::string loadedTypesText;
    HIKARI::GameProjectFloat3 previousGround{};
    HIKARI::GameProjectFloat3 ground{};
    float yaw = 0.0f;
    float height = 0.0f;
    float floatHeight = 0.5f;
    float floatTarget = 0.5f;
    int floatTimer = 0;
    float floatClock = 0.0f;
    HIKARI::GameProjectFloat3 velocity{};
    BossState state = BossState::Idle;
    BossActPhase phase = BossActPhase::Pre;
    int stateFrame = 0;
    int idleFrames = 0;
    int move = -1;
    int chain = 0;
    int actFrames = 0;
    int lastSegment = -1;
    float crouch = 0.0f;
    float strafe = 1.0f;
    float sideSign = 1.0f;
    float leanForward = 0.0f;
    float leanSide = 0.0f;
    float poseX = 70.0f;
    float poseY = -10.0f;
    HIKARI::GameProjectFloat3 stepFrom{};
    bool dashStarted = false;
    HIKARI::GameProjectFloat3 dashFrom{};
    HIKARI::GameProjectFloat3 dashTo{};
    std::vector<bool> segmentDone;
    std::array<BossSwing, 16> swings{};
    int glintFrames = 0;
    // 気絶
    float stun = 0.0f;
    int stunCount = 0;
    float stunHitSeconds = -1000.0f;
    double seenBaseDamage = 0.0;
    float clockSeconds = 0.0f;
    uint64_t tick = 0;
    bool defeatedLogged = false;
    int lastPhaseNumber = 1;
    // 技の要求（行動ツリーや確かめ用が入れる）
    int requestedMove = -1;
    bool requestedAsCombo = false;
    BossBrainMemory brain;
    int sequenceCursor = 0;
    int testDamageDone = 0;
    uint32_t random = 0;
    // 斬撃と煙
    std::array<BossWave, kBossWaveCapacity> waves{};
    uint64_t nextWaveId = 1;
    std::array<BossSmokeDeposit, kBossDepositCapacity> deposits{};
    std::array<BossBlast, kBossBlastCapacity> blasts{};
    size_t blastCursor = 0;
    float bodyTrail = 0.0f;
    float bodySmokeRate = 0.0f;
    HIKARI::GameProjectFloat3 lastBodyPosition{};
    // 描画フレームで使う剣の位置（固定 tick で決める）
    bool swordSmoke = false;
    float swordSmokeRate = 0.0f;
    HIKARI::GameProjectFloat3 swordInner{};
    HIKARI::GameProjectFloat3 swordOuter{};
    std::array<HIKARI::GameProjectVfxPlaybackHandle, kBossWaveCapacity> waveVfx{};
    std::array<HIKARI::GameProjectVfxPlaybackHandle, kBossWaveCapacity> knifeVfx{};
    std::array<uint64_t, kBossWaveCapacity> waveVfxId{};
    std::array<BossSlashEffect, kBossSlashEffectCapacity> slashEffects{};
    HIKARI::GameProjectVfxPlaybackHandle swordVfx{};
    bool vfxAccepted = true;
    bool fluidAccepted = true;
    int playerHits = 0;
    int playerDodges = 0;
};

} // namespace Project_TD2_2
