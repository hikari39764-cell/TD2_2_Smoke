#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "BossMoveTable.h"
#include "../Combat/Feedback.h"

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
    // 地面に残す火（幅 0 は塗らない）。斬撃は、この一撃に「線」が付く時に、放ってから消えるまで飛んだ地面を毎 tick 塗る。
    // 回し蹴りの輪は、この一撃に「輪」が付く時に、広がりきった所へ帯を 1 回塗る。
    float burnWidth = 0.0f;
    float burnSeconds = 0.0f;
    // 線をまだ塗っていない所の始まり。火は刃の後ろ burnLineLag m まで塗り、消える時に残りを塗る。
    HIKARI::GameProjectFloat3 burnFrom{};
    // この一撃に「交差」が付く時：斬撃が狙った所へ届いて爆ぜた時に塗る（放った時には塗らない）。吸われた斬撃は塗らない。
    bool crossPending = false;
    HIKARI::GameProjectFloat3 crossCenter{};
    HIKARI::GameProjectFloat3 crossToward{};
    float crossWidth = 0.0f;
    float crossSeconds = 0.0f;
    float crossReach = 0.0f;
    // この一撃が火を残す（線に限らず、表の「残す火」のどれかが付く）＝火の刃。規則は読まず、見た目（BossVisuals）が読む。
    bool fire = false;
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
    // 火を残す技の間に出た（剣が燃えている）。当たった所を火球にする。偽なら暗い煙の塊。
    bool fire = false;
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

// 天降り斬りの段階。屈む → 跳び上がる → 空中で溜める → 落ちる → 着地した（技が終わるまで）。技の外は None。
enum class BossSkyfallPhase { None, Crouch, Rise, Hang, Dive, Landed };

// 天降り斬りのいまの様子。規則（BossSkyfall）が固定 tick ごとに書き、見た目（エフェクト）は読むだけ。保存しない。
struct BossSkyfallState {
    BossSkyfallPhase phase = BossSkyfallPhase::None;
    // 空中の溜めの進み（溜め始めの tick で 0 から、落ち始める tick で 1）。溜めの揺らぎで延びた分も含めて進む。
    float charge = 0.0f;
    // 落ちる進み（落ち始めで 0 から、着地の tick で 1）。高さと水平の進みはこれに落ち方の曲線を掛けたもの。
    float dive = 0.0f;
    // 跳んだ所と落ちる所（どちらも地面の高さ）。落ちる所は、locked までは毎 tick の見込み、locked の後は決まった所。
    HIKARI::GameProjectFloat3 takeoff{};
    HIKARI::GameProjectFloat3 target{};
    bool locked = false;
    // ここから下は規則の内側の覚え（見た目は読まない）。
    // 跳ぶ間に流れる向き（跳んだ時のプレイヤーから離れる向き）と、落ち始めた所・高さ。
    HIKARI::GameProjectFloat3 away{};
    HIKARI::GameProjectFloat3 diveFrom{};
    float diveHeight = 0.0f;
    // 前隙の分け方（技を始めた時のボスの項目と前隙で決める）と、空中で溜めた tick の数。
    int crouchFrames = 0;
    int riseFrames = 1;
    int hangFrames = 0;
    int hangElapsed = 0;
    // 着地の当たり（1 回の着地で 1 回だけ）：判定に入った・当てた・無敵でよけられた。
    bool caught = false;
    bool hit = false;
    bool dodged = false;
};

// 退き溜めの溜め（退き終えてから斬撃を放つまで）。技を始めた tick に消す。
struct BossRetreatState {
    // 溜めている（溜め始めた tick に立て、放った・打ち切られた tick に下ろす）。崩せる被弾数はこの間だけ数える。
    bool charging = false;
    // 溜めの長さ（前隙の残り＋溜めの揺らぎ）と、溜めた tick の数。
    int chargeFrames = 0;
    int chargeElapsed = 0;
};

// 霞の大剣の段階。作り出す（前隙。最後の greatswordWindFrames で薙ぎ始めの構えへ下ろす）→ 薙ぐ（攻撃中）→
// ほどける（後隙の始めの greatswordDissolveFrames）。それ以外は None。
enum class BossGreatswordPhase { None, Conjure, Sweep, Dissolve };

// 霞の大剣のいまの形。規則（BossGreatsword）が固定 tick ごとに書き、見た目（エフェクト）は読むだけ。保存しない。
struct BossGreatswordState {
    BossGreatswordPhase phase = BossGreatswordPhase::None;
    // 柄から剣先へ満ちた割合（作り出す間に 0→1。薙ぐ・ほどける間は 1）。
    float fill = 0.0f;
    // 薙ぎ始めの構えへ下ろす進み（作り出す間の最後の greatswordWindFrames で 0→1。薙ぐ・ほどける間は 1）。
    float wind = 0.0f;
    // 薙ぐ進み（薙ぐ間に 0→1。ほどける間は 1）と、ほどける進み（ほどける間に 0→1）。
    float sweep = 0.0f;
    float dissolve = 0.0f;
    // 柄（握り）の世界の位置と、柄から剣先への向き（単位ベクトル）、剣の長さ (m)。
    HIKARI::GameProjectFloat3 hilt{};
    HIKARI::GameProjectFloat3 direction{0.0f, 1.0f, 0.0f};
    float length = 0.0f;
    // 炎の大剣か（技を始めた tick の bladeLit。フェーズ 2 で「残す火」が付く時）。
    bool fire = false;
    // ここから下は規則の内側の覚え（見た目は読まない）。
    // 薙ぐ向きの基準（薙ぎ始めの tick のボスの向き。rad）と、いまと 1 tick 前の剣の角度（基準からの度。右が負）。
    float yaw = 0.0f;
    float angle = 0.0f;
    float previousAngle = 0.0f;
    // 当たり（1 回の薙ぎで 1 回だけ）：判定に入った・当てた・無敵でよけられた。
    bool caught = false;
    bool hit = false;
    bool dodged = false;
    // 「弧」の火を塗り終えた所の数。
    int firePainted = 0;
};

// 天降り斬りの見た目の状態。見た目（BossSkyfallVisuals）だけが描画フレームごとに読み書きする。保存しない。
struct BossSkyfallVisualState {
    // 体と一緒に落ちる刃（斬撃のエフェクト）。
    BossSlashEffect blade{};
    // フェーズ 2 の着地で広がる炎の輪：エフェクト、出てからの秒、真ん中。
    HIKARI::GameProjectVfxPlaybackHandle ringVfx{};
    float ringAge = 0.0f;
    HIKARI::GameProjectFloat3 ringCenter{};
    // 炎の輪を次のフレームに出す（着地のフレームは、一緒に落ちた刃の域がまだ数に入っているので 1 フレーム待つ）。
    bool ringPending = false;
    // 前のフレームに見た段階と、落ちる所が決まっていたか（変わった瞬間を見つける）。
    BossSkyfallPhase seenPhase = BossSkyfallPhase::None;
    bool seenLock = false;
    // 落ちる所を決めた瞬間の剣の光の残り（1 から skyfallLockFlareSeconds で 0 へ）。剣先の霞のエフェクトが読む。
    float flare = 0.0f;
};

// 霞の大剣の見た目の状態。見た目（BossGreatswordVisuals）だけが描画フレームごとに読み書きする。保存しない。
struct BossGreatswordVisualState {
    HIKARI::GameProjectVfxPlaybackHandle vfx{};
    // 最後に送った変換（技が終わった後、消える間もそこに置く）と、消えるまでの残り秒。
    HIKARI::GameProjectTransform transform{};
    float fade = 0.0f;
    // 前のフレームに見た段階（変わった瞬間を見つける）。
    BossGreatswordPhase seenPhase = BossGreatswordPhase::None;
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
    // 溜めを崩した時・気絶した時の手応え（c-d330c0）。止め・カメラの揺れ・手柄の振動・本体の光り。
    // 本体の光りは Damageable の光りを頼むだけで、いまの体（色を読める材質の無いカプセル）では見えない。
    // 崩した合図は breakBlastScale の塊で見せる（体を模型にした時のために項目は残す）。
    int breakHitStopFrames = 6;
    float breakHitStopScale = 0.05f;
    float breakCameraAmplitude = 0.08f;
    int breakCameraFrames = 10;
    float breakCameraFrequency = 24.0f;
    float breakRumbleLow = 0.9f;
    float breakRumbleHigh = 0.7f;
    int breakRumbleFrames = 12;
    int breakFlashFrames = 12;
    // 崩した・気絶した瞬間に体の中心から弾ける塊の大きさ（当たった所の爆発に対する倍。0 で出さない）。
    // フェーズ 1 は霞の塊と青白い光、フェーズ 2 は火球と炎の光。
    float breakBlastScale = 1.8f;
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
    // 天降り斬り（前隙＝屈む F ＋跳び上がる F ＋残りが空中の溜め。攻撃中＝落ちる F ＋残りが着地の当たり。
    // 前隙・攻撃中・後隙・当たりの半径・威力・落ちる先を決める F（追尾を止める F）は技の表）
    float skyfallHeight = 6.0f;
    float skyfallBackDrift = 1.5f;
    int skyfallCrouchFrames = 8;
    int skyfallRiseFrames = 16;
    int skyfallDiveFrames = 14;
    float skyfallDiveEasePower = 2.0f;
    float skyfallStopDistance = 1.2f;
    float skyfallMaxTravel = 16.0f;
    int skyfallLandCrouchFrames = 30;
    float skyfallBlastScale = 1.8f;
    // 退き溜め（前隙＝退く F ＋残りが溜め。退く距離・高さは後ろへ下がると同じ backstepDistance／backstepHeight。
    // 溜めの長さの揺らぎ・追尾を止める F・崩せる被弾数・斬撃の幅と速さは技の表）
    int retreatStepFrames = 20;
    // 霞の大剣（前隙＝作り出す、攻撃中＝薙ぐ（表）。威力・被弾・追尾を止める F・崩せる被弾数・残す火は技の表）
    float greatswordLength = 6.5f;
    float greatswordHiltOffset = 0.8f;
    int greatswordFillFrames = 80;
    float greatswordRaiseTilt = 15.0f;
    float greatswordRaiseHeight = 2.6f;
    int greatswordWindFrames = 14;
    float greatswordSweepDegrees = 200.0f;
    float greatswordSweepHeight = 1.3f;
    float greatswordHitMarginDegrees = 4.0f;
    float greatswordInnerRadius = 1.0f;
    int greatswordDissolveFrames = 24;
    // ── 霞の大剣の見た目（規則は変えない） ──
    // 大剣のエフェクト：大剣の模型（Assets/Models/KasumiGreatsword.obj。握りから剣先まで 6.5 m）の輪郭へ流体を流し込む。
    // フェーズ 1 は霞、フェーズ 2（炎の大剣）は火。剣の長さ greatswordLength は、この模型の長さに合わせてある。
    HIKARI::GameProjectVfxAssetReference greatswordMistEffect{"c2e6f08a4b1d47c39a5e7d103f8b6a42",
                                                              "Assets/Vfx/BossGreatswordMist.hvfx"};
    HIKARI::GameProjectVfxAssetReference greatswordFireEffect{"d93a7b15e0c24f8ab46d2e9c7a01f5b3",
                                                              "Assets/Vfx/BossGreatswordFire.hvfx"};
    // 技が終わった・崩された後、残った霞が消えるまでエフェクトを止めずに待つ秒。
    float greatswordFadeSeconds = 0.6f;
    // 薙ぐ間に流し込む量（作る間に対する倍）。速く回る域の中で霞が薄まるのを補い、刃を明るく保つ。
    float greatswordSweepAmount = 2.5f;
    // 薙ぐ間の剣の平らな面の傾き（0＝真上を向く＝横から見ると線、1＝ボスの正面へ 45 度傾いて、前のプレイヤーから面が見える）。
    float greatswordSweepFaceTilt = 1.0f;
    // 薙ぐ間、剣が通った所に霞（フェーズ 2 は火の粉の光）を残す：剣先の霞のエフェクトを、握りから剣の長さのこの割合の所に
    // 置いて一緒に動かす。量と光は、ふだんの振りに対する倍（0 で出さない）。
    float greatswordTrailAlong = 0.6f;
    float greatswordTrailAmount = 4.0f;
    float greatswordTrailGlow = 2.0f;
    // 薙ぎ終わった時、剣が通った所の場の霧を外へ押す所の数と、ボスからの距離 m（0 で押さない）。
    int greatswordSweepPushes = 4;
    float greatswordSweepPushDistance = 4.5f;
    // 「弧」の火：ボスからこの距離の間に rows 本、薙いだ角度の step 度ごとに小さな円を塗る（円の直径は表の火の幅）。
    float greatswordFireNear = 3.0f;
    float greatswordFireFar = 7.0f;
    int greatswordFireRows = 3;
    float greatswordFireStepDegrees = 20.0f;
    // ── 溜める技の見た目（天降り斬りの空中・退き溜め。規則は変えない） ──
    // 溜める間（規則が書く chargeVisual が 0 より大きい間）、剣へ集まって来る光の粒
    // （フェーズ 1 は冷たい白、フェーズ 2 は橙）と、その大きさ。
    HIKARI::GameProjectVfxAssetReference gatherEffect{"b84f0c6d2e9a4f17a35d6c1e0f7b92d4", "Assets/Vfx/BossChargeGather.hvfx"};
    float chargeGatherScale = 1.3f;
    // 溜め切った時の、剣の光の大きさと霞の量（構えの時に対する倍）。
    float chargeGlowScale = 3.0f;
    float chargeHazeScale = 3.0f;
    // ── 天降り斬りの見た目（規則は変えない） ──
    // 落ちる所を決めた瞬間に剣がひときわ光る（落ちて来る合図。光の倍率と、戻るまでの秒）。
    float skyfallLockFlare = 2.5f;
    float skyfallLockFlareSeconds = 0.25f;
    // 跳び上がる時に足元から巻き上がる霞の塊の大きさ（消える所の煙の塊に対する倍。0 で出さない）。
    float skyfallTakeoffBlastScale = 1.4f;
    // 落ちる間、体と一緒に落ちる刃（斬撃のエフェクト）の幅 m（0 で出さない）。
    float skyfallBladeWidth = 6.0f;
    // 着地の周りに立つ霞（フェーズ 2 は火球）の塊：数・着地点からの距離 m・大きさ m。
    int skyfallLandPuffs = 5;
    float skyfallLandPuffDistance = 2.4f;
    float skyfallLandPuffRadius = 1.3f;
    // フェーズ 2 の着地で広がる炎の輪（回し蹴りの輪のエフェクト）：広がり切る半径 m と、広がる秒。
    float skyfallRingRadius = 4.5f;
    float skyfallRingSeconds = 0.22f;
    // 回転→霞の輪（試作：半径 0.8 m から判定サイズまで、斬撃速度で広がる。消えた所の 0.7 倍の円に残り煙 6 か所）
    float kickRingStartRadius = 0.8f;
    float kickResidueScale = 0.6f;
    float kickResidueRadiusShare = 0.7f;
    // 地面に残す火の形（形そのものは技の表の「残す火」。交差の線の長さと、上段溜めの池を置く前の距離）
    float burnCrossLength = 7.0f;
    float burnPoolForward = 2.5f;
    float burnLineLag = 1.2f;
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
    // 霞の刃（火を残さない斬撃）が当たった所に重ねるエフェクト（青白い光と火花。空なら blastEffect）。
    HIKARI::GameProjectVfxAssetReference mistBlastEffect{"c7a41d6be2f0483a9d15b0e36f7a2c18",
                                                         "Assets/Vfx/BossMistBurst.hvfx"};
    // 火を残さない斬撃の見た目（煙の刃）。空なら、どの斬撃も slashEffect（炎の刃）で出す。
    HIKARI::GameProjectVfxAssetReference slashSmokeEffect{"a86e3e830ef54289e4dd42a4ef0a6100",
                                                          "Assets/Vfx/BossSlashSmoke.hvfx"};
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
    float wavePartPush = 8.0f;
    float wavePartRadius = 1.5f;
    float blastPush = 9.0f;
    float blastPushRadius = 3.0f;
    float knifeBlastScale = 0.4f;
    // 見た目（流体注入のエフェクト。場の霧の域へ色の付いた霞を入れる）
    HIKARI::GameProjectVfxAssetReference swordEffect{"353886750fe7c2c7f8939f18e5df6b31",
                                                     "Assets/Vfx/BossSwordHaze.hvfx"};
    HIKARI::GameProjectFloat3 hazeColor{0.62f, 0.82f, 1.0f};
    float swordEffectAmount = 0.3f;
    float kamaeEffectAmount = 0.15f;
    // 剣先の火の粉の光の大きさ（倍）。構えの合図。煙と違って場に残らない。
    float swordEmberScale = 1.0f;
    // 見た目：火を残す技の間、剣が燃える（出す前の構えから、火を残す技だと見分けられる）
    float bladeLitEmber = 2.5f;
    HIKARI::GameProjectFloat3 bladeLitColor{1.0f, 0.45f, 0.14f};
    float bladeFuel = 160.0f;
    float bladeFuelRadius = 0.5f;
    float bladeFuelTemperature = 6.0f;
    float igniteFlashRadius = 0.7f;
    float hitBlastRadius = 1.1f;
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
    // 霞隠れ・分身の出入りの煙の塊は、場の火の域の流体で出す。ここは、その上に重ねるエフェクト（既定は無し）。
    HIKARI::GameProjectVfxAssetReference puffEffect{};
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
    // 瞬間突進の瞬間移動を終えた（跡の火を 1 回だけ塗る）。
    bool dashLanded = false;
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
    BossSkyfallState skyfall;
    BossRetreatState retreat;
    BossGreatswordState greatsword;
    // 溜めの見た目の進み（0＝溜めていない。溜め始めから 0→1）。退き溜めの溜めと天降り斬りの空中の溜めが固定 tick ごとに
    // 書き、それ以外の tick は 0（BossSystem が tick の始めに戻す）。規則は読まず、見た目（BossVisuals）が読む。
    float chargeVisual = 0.0f;
    // いまの技のどれかの一撃が、いまのフェーズで火を残す＝この技の間は剣が燃えている。技を始めた tick に決め、
    // 待機へ戻る・打ち切られる（気絶・崩れ・撃破）まで保つ。規則は読まず、見た目（BossVisuals）が読む。
    bool bladeLit = false;
    // 吸い込みで吸われた斬撃・ナイフの煙（この tick にヒョウタンへ渡す）。
    float absorbedSmoke = 0.0f;
    // この tick に吸われた斬撃・ナイフの数。BossSystem がヒョウタンへ知らせたら消す。
    int absorbedWavesPending = 0;
    // 気絶
    float stun = 0.0f;
    int stunCount = 0;
    // 崩れ・気絶に入った tick に立て、描画フレームの手応え（BossFeedback）が使ったら消す。
    bool breakFeedbackPending = false;
    Feedback::CameraShake breakShake{};
    float stunHitSeconds = -1000.0f;
    double seenBaseDamage = 0.0;
    float clockSeconds = 0.0f;
    uint64_t tick = 0;
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
    // 前のフレームに剣が燃えていたか（点火の瞬間を見つける。BossVisuals が使う）。
    bool bladeWasLit = false;
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
    // 溜める間（天降り斬りの空中・退き溜め）、剣へ集まって来る光の粒。
    HIKARI::GameProjectVfxPlaybackHandle gatherVfx{};
    BossSkyfallVisualState skyfallVisual;
    BossGreatswordVisualState greatswordVisual;
    bool vfxAccepted = true;
    bool fluidAccepted = true;
};

} // namespace Project_TD2_2
