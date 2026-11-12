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
// 当たった所の爆発と、霞隠れ・分身の霞の爆ぜ（押す力の無い爆発）の枠。分身を出す時の 4 つと当たりが重なっても足りる数。
inline constexpr size_t kBossBlastCapacity = 6;
// 霞分身で一度に出せる分身の数の上限（実際の数は表の判定サイズと、シーンに置いた分身の物体の数で決まる）。
inline constexpr size_t kBossCloneCapacity = 4;
// 斬撃が消えた所などに置く残り煙の枠。
inline constexpr size_t kBossDepositCapacity = 8;
// 連続攻撃の判定に覚えておく命中の数（試作と同じ 40）。
inline constexpr size_t kBossHitMemory = 40;

enum class BossState { Idle, Act, Stunned, Dead };
enum class BossActPhase { Pre, Act, Post };

// 飛んでいる霞の斬撃（三日月）か投げナイフか、回し蹴りの霞の輪。当たり判定は煙と別に持つので、場の霧を吸われても
// 斬撃は消えない（吸い込みの扇に入った斬撃・ナイフは、それ自体が吸われて消える）。
struct BossWave {
    bool active = false;
    bool knife = false;
    // 回し蹴りの霞の輪：position を中心（地面）に、radius から maxRadius まで speed で広がる。
    bool ring = false;
    float radius = 0.0f;
    float maxRadius = 0.0f;
    // 足元の霧を最後に押した半径（押す 1 点の半径ぶん広がるごとに 1 回押す。負はまだ押していない）。
    float pushedRadius = -1.0f;
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
    // 吸い込みで吸ったときにヒョウタンへ入る煙（試作 waveSmoke 25／knifeSmoke 6）。
    float absorbSmoke = 0.0f;
    // 飛んだ地面に残す火（技の行の「残す火」が線の斬撃だけ。幅 0 は塗らない）。放ってから消えるまで毎 tick 塗る。
    float burnWidth = 0.0f;
    float burnSeconds = 0.0f;
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
    // 霞隠れ・分身の消える／現れる所（煙の塊のエフェクト）。偽なら当たった所の爆発。
    bool puff = false;
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

// 引爆の 1 回ぶん。攻撃中に入った tick に始め、前線が火を通り過ぎたら（または打ち切られたら）終える。
struct BossDetonation {
    bool active = false;
    // 溜めの進みを燃える地面へ渡している間（溜めをやめた tick に 0 へ戻すため）。
    bool charging = false;
    // 前線の中心（攻撃中に入った時のボスの足元）と、その時に一番遠かった燃えているセルまでの距離 (m)。
    HIKARI::GameProjectFloat3 origin{};
    float farthest = -1.0f;
    uint32_t startCells = 0;
    uint32_t exploded = 0;
    uint32_t blasts = 0;
    uint32_t blastsDropped = 0;
    // 当たり（1 回の引爆で 1 回だけ）：当てた・無敵でよけられた。
    bool caught = false;
    bool hit = false;
    bool dodged = false;
};

// 行動ツリーの「反応の間隔」の節を、名前ごとに覚えておく数。
inline constexpr size_t kBossReactionMemory = 16;

// 「反応の間隔」1 つぶんの覚え（節の名前の欄で見分ける。同じ名前の節は間隔を分け合う）。時刻は boss.clockSeconds。
struct BossReactionMemory {
    size_t key = 0;
    // 反応の技を出した後、また反応できる時刻。
    float readySeconds = 0.0f;
    // 確率を振った後、次に振れる時刻（外れた時にすぐ振り直さない）。
    float rerollSeconds = 0.0f;
};

// ボスから見たローリングの向き。
enum class BossRollDirection { Away, Toward, Sideways };

// ボスが毎 tick 見ているプレイヤーの様子（行動ツリーの反応の節が読む）。時刻は boss.clockSeconds の秒。
struct BossPlayerWatch {
    bool present = false;
    bool healing = false;
    bool absorbing = false;
    float absorbSince = 0.0f;
    bool inFire = false;
    float fireSince = 0.0f;
    // 最後にローリングを始めた時刻（負はまだ見ていない）と、始めた時のボスから見た向き。
    float rollSeconds = -1.0f;
    BossRollDirection rollDirection = BossRollDirection::Away;
    // 前の tick にローリング中だったか・その経過 F（続けてもう一度転がったのを見分ける）。
    bool wasRolling = false;
    int rollFrame = 0;
    // 場の燃えている面積の割合 (%)。
    float burningPercent = 0.0f;
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
    // 分身が撃たれた：本体が突進系（瞬間突進）で来るまで立てておく（試作の forced）。突進系を始めると消える。
    bool cloneStruck = false;
    // 霞分身を次に出せる時刻（秒）。
    float cloneReadySeconds = 0.0f;
    // 反応の間隔の節ごとの覚え。
    std::array<BossReactionMemory, kBossReactionMemory> reactions{};
    size_t reactionCount = 0;
    bool reactionFullLogged = false;
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
    int recoveryGuardFrames = 30;
    float comboChancePercent = 65.0f;
    // ローリングの向きを分ける角度：ボスへ向かう・離れる向きからこの角度以内なら前後、それより外れたら横。
    float rollAxisDegrees = 45.0f;
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
    // 跳躍→X字2段（試作：前隙の 28% で屈み、残りで 3.6 m まで跳ぶ。後隙の最初の 1/3 で降りる）
    float jumpHeight = 3.6f;
    float jumpCrouchShare = 0.28f;
    // 回転→霞の輪（試作：半径 0.8 m から判定サイズまで、斬撃速度で広がる。消えた所の 0.7 倍の円に残り煙 6 か所）
    float kickRingStartRadius = 0.8f;
    float kickResidueScale = 0.6f;
    float kickResidueRadiusShare = 0.7f;
    // 霞隠れ→回り込み突進（試作：プレイヤーの向きの反対 ±100°、3.64 m に現れ、振りの間に 2.2 m 突っ込む）
    float ambushAppearDistance = 3.64f;
    float ambushAppearSpreadDegrees = 100.0f;
    float ambushLungeDistance = 2.2f;
    float ambushAppearTurnRate = 400.0f;
    // 霞に隠れる見せ方：霧へ沈む深さ（前隙の間に少し、隠れたら見えない所まで）と、沈む・浮かぶ長さ
    float hidePreSinkDepth = 1.0f;
    float hideSinkDepth = 8.0f;
    int hideSinkFrames = 6;
    int appearRiseFrames = 8;
    // 霞分身（試作：再使用 22 秒・消えるまで 25 秒。プレイヤーから 6〜16 m の円に、0.9〜1.2 rad ずつ離して並べる）
    float cloneCooldownSeconds = 22.0f;
    float cloneLifeSeconds = 25.0f;
    float cloneMinDistance = 6.0f;
    float cloneMaxDistance = 16.0f;
    float cloneSpreadRadians = 0.9f;
    float cloneSpreadJitter = 0.3f;
    // 霞隠れ・分身・消えた分身の霞（試作 puffResidue 25、半径 1.6 m。場の霧へ地面近くに入れる吸える煙）
    float puffResidue = 25.0f;
    float puffRadius = 1.6f;
    // 吸い込みで斬撃・ナイフを吸う範囲（試作 inhaleRange 5 m・inhaleAngle 片側 60°。1.5 m 以内は向きを問わない）
    float absorbCatchRange = 5.0f;
    float absorbCatchAngle = 60.0f;
    float absorbCatchNear = 1.5f;
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
    // 見た目：回し蹴りの霞の輪（広がる炎の輪。自前の流体域を 1 つ使う。足元の霧を外へ押す）
    HIKARI::GameProjectVfxAssetReference ringEffect{"cbffd59dc133951894e126d8243b0fd2", "Assets/Vfx/BossKickRing.hvfx"};
    float ringEffectRadius = 9.0f;
    float ringFadeSeconds = 0.35f;
    float ringHeight = 0.6f;
    float ringBurstScale = 0.6f;
    float ringPush = 6.0f;
    float ringPushRadius = 1.2f;
    // 見た目：霞隠れ・分身が消える／現れる所の煙の塊（流体域は使わない。場の霧へ煙は足さない。霧は押さない）
    HIKARI::GameProjectVfxAssetReference puffEffect{"fd1495a095abfc856a36e258cafbe9d9",
                                                    "Assets/Vfx/BossSmokePuff.hvfx"};
    float puffEffectScale = 1.0f;
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
    // 溜めの揺らぎ：前隙の windupHoldAt F 目で、あと windupHold F だけ構えたまま止まる。
    int windupHold = 0;
    int windupHoldAt = -1;
    // 前隙の間に当てられた回数（崩せる被弾数と比べる）。
    int windupHits = 0;
    // 追尾を止めている間（一撃を放つまで）。狙いは止めた時のプレイヤーの位置のまま。
    bool aimFrozen = false;
    HIKARI::GameProjectFloat3 aimPoint{};
    int glintFrames = 0;
    // 回し蹴りの回転（見た目の向きに足す rad）。霧へ沈んでいる深さ (m)。隠れている間は当たらない。
    float spin = 0.0f;
    float sink = 0.0f;
    bool hidden = false;
    // Damageable の無敵を隠れている間だけ立てたか（シーンで最初から無敵にしてある時に戻しすぎない）。
    bool hiddenApplied = false;
    // 霞分身：出す場所（分身の物体へ渡す）と、いま出ている数。
    std::array<HIKARI::GameProjectFloat3, kBossCloneCapacity> cloneSpots{};
    int cloneSpotCount = 0;
    bool cloneSpawnPending = false;
    int activeClones = 0;
    BossDetonation detonation;
    // 吸い込みで吸われた斬撃・ナイフの煙（この tick にヒョウタンへ渡す）。
    float absorbedSmoke = 0.0f;
    int wavesAbsorbed = 0;
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
    BossPlayerWatch watch;
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
    HIKARI::GameProjectVfxPlaybackHandle ringVfx{};
    uint64_t ringVfxId = 0;
    // 輪が消えた後、炎が消えるのを待っている残り時間（秒。負は輪が広がっている間）。
    float ringFade = -1.0f;
    float ringScale = 0.0f;
    HIKARI::GameProjectFloat3 ringCenter{};
    std::array<BossSlashEffect, kBossSlashEffectCapacity> slashEffects{};
    HIKARI::GameProjectVfxPlaybackHandle swordVfx{};
    bool vfxAccepted = true;
    bool fluidAccepted = true;
    int playerHits = 0;
    int playerDodges = 0;
};

} // namespace Project_TD2_2
