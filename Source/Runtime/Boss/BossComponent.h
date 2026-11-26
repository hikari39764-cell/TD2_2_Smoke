#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "BossCinematicFxSettings.h"
#include "BossGourdRead.h"
#include "BossKasumiStepSettings.h"
#include "BossMoveTable.h"
#include "BossSwordPose.h"
#include "BossWatch.h"
#include "../BossBody/BossAnimBody.h"
#include "../Player/Feedback.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kBossType = "Project_TD2_2.Boss";
inline constexpr std::string_view kBossSystem = "Project_TD2_2.BossSystem";
// 物体変数の ID。ProjectSettings/runtime_bindings.json の宣言と同じ名前・型（Float）にする。
inline constexpr std::string_view kBossHealthVariable = "Boss.Health";
inline constexpr std::string_view kBossMaxHealthVariable = "Boss.MaxHealth";
// HP の条を出すか（1＝出す、0＝出さない。登場の演出の間は見得まで 0）。
inline constexpr std::string_view kBossBarShownVariable = "Boss.BarShown";
// 登場の演出の間は 1（HUD がプレイヤーの条を引っ込める。c-e0c500）。
inline constexpr std::string_view kBossCinematicVariable = "Boss.Cinematic";
// いまのフェーズ（1 か 2。Int）と、そのフェーズ（HP の条）に入ってからの秒（Float）。HUD が条の色と、2 本目が満ちて
// 戻る見せ方に使う（c-3e0ffa）。Boss.Health・Boss.MaxHealth はいまの条の値。
inline constexpr std::string_view kBossPhaseVariable = "Boss.Phase";
inline constexpr std::string_view kBossPhaseSecondsVariable = "Boss.PhaseSeconds";
// 同時に飛ぶ斬撃・ナイフの枠。連撃 5 撃とナイフ 3 本が重なっても足りる数。
inline constexpr size_t kBossWaveCapacity = 12;
// 同時に見せる斬撃のエフェクトの枠。1 つが流体域を 1 つ使う。流体域は 1 フレームに 6 個まで（均衡画質）で、
// 場の霧・吸い口・砲撃と分け合う（出している間は砲撃の枠を同じ数だけ減らす）。
inline constexpr size_t kBossSlashEffectCapacity = 2;
// 当たった所の爆発の枠。
inline constexpr size_t kBossBlastCapacity = 6;
// 斬撃が消えた所などに置く残り煙の枠。爆発の跡の煙（炎が消えるまで待つ）も入るので 8 から増やした。
// 出している枠は 1 つが場の霧の域の干渉を 1 つ使う（BossSmoke の kEmitBudget の中）。
inline constexpr size_t kBossDepositCapacity = 12;
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
    // 吸い込みに捕まった：もう当たらず、火も残り煙も置かず、ヒョウタンの口へ引き込まれる。入りきった tick に、煙と
    // 吸い取りの知らせを渡して消える（途中で吸い込みをやめても最後まで進む）。
    bool captured = false;
    // 吸い込みの扇に入ったが、ヒョウタンが満杯で吸えなかった（記録を 1 回だけ出す。規則は読まない）。
    bool refused = false;
    // 捕まってからの tick と、入りきるまでの tick（同時に捕まえた分は、数 F ずつ遅らせて順に入れる）。
    int pullFrame = 0;
    int pullFrames = 0;
    // 捕まった時の位置・飛ぶ向き・幅の向き・幅。巻き込む向き（+1／−1）と、口へ向ける端（幅の向きの +1／−1 の側）。
    HIKARI::GameProjectFloat3 pullFrom{};
    HIKARI::GameProjectFloat3 pullDirection{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 pullAcross{1.0f, 0.0f, 0.0f};
    float pullWidth = 0.0f;
    float pullSide = 1.0f;
    float pullTip = 1.0f;
    // tick の始めの位置（引き込みの間、描画のフレームは前の tick との間を補間して置く）。
    HIKARI::GameProjectFloat3 previousPosition{};
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
    // 吸い込みに捕まって、口へ引き込まれている（入りきった後は、その場で短く消える）。
    bool captured = false;
    // 刃の霧の源の量（1＝飛んでいる時。引き込みの間は弱める）と、幅のほかの向きを細くする倍率（1＝そのまま）。
    float fly = 1.0f;
    float pinch = 1.0f;
    // 引き込みの進み（0〜1。入りきった後は 1）と、捕まった時の幅の向き・幅（域に残った両翼の霧を口へ押し寄せるのに使う）。
    float pullProgress = 0.0f;
    HIKARI::GameProjectFloat3 pullAcross{1.0f, 0.0f, 0.0f};
    float pullWidth = 0.0f;
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
    // 火を残す技の間に出た（剣が燃えている）。当たった所を火球にする。偽なら暗い煙の塊。
    bool fire = false;
    // プレイヤーに当たった所（direction は押した向き）。火の刃なら、火球の代わりに、体を突き抜ける炎と火花を出す。
    bool playerHit = false;
    // 爆発の後、真下の地面へ吸える煙を残す（BossWaves::AddBlast）。爆発がプレイヤーに当たった所（燃える地面の塊の側で
    // 残す）は残さない。
    bool leavesFog = true;
    float scale = 1.0f;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 direction{};
};

// 吸い込みで吸い取った斬撃・ナイフのうち、まだヒョウタンへ渡していない分。ボスはヒョウタンの中身を直に書き換えず、
// tick の終わりに BossSystem がまとめて渡す（煙・手応えの数・火弾の知らせ）。
struct BossCatchHandoff {
    // ヒョウタンへ入る煙（Boss.waveAbsorbSmoke・knifeAbsorbSmoke の合計）。
    float smoke = 0.0f;
    // 吸い取った数（ヒョウタンの手応え）と、そのうちの火の刃（火を残す斬撃・ナイフ。火弾の状態にする）の数。
    int count = 0;
    int fireCount = 0;
};

// 場の霧へ少しずつ入れる煙（斬撃の終点・ボスの通った跡・爆発の跡）。
struct BossSmokeDeposit {
    bool active = false;
    HIKARI::GameProjectFloat3 position{};
    float radius = 1.0f;
    float smokeLeft = 0.0f;
    float smokePerFrame = 0.0f;
    float rate = 0.0f;
    // 出し始めるまでの残りの tick（爆発の跡は、爆発の炎と光が消えるまで待つ。0 ならすぐ出す）。
    int waitFrames = 0;
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
    // 前の tick の剣の根元（剣の当たりを、前の tick から今の tick までの間でも確かめる）。
    HIKARI::GameProjectFloat3 lastBase{};
    // 刀が正面を斬った部分（振りの後ろ waveCutShare から先）の記録。斬撃は、ここから決めた所・面で放つ。
    // 刀の線が掃いた面の法線の合計、その部分の最初の剣先、前の tick の刀の向き。
    bool cutStarted = false;
    HIKARI::GameProjectFloat3 cutNormal{};
    HIKARI::GameProjectFloat3 cutFirstOuter{};
    HIKARI::GameProjectFloat3 lastBlade{};
    // 剣先がボスの前へ一番出た時の、前への距離と刀の線（そこを過ぎた tick に斬撃を放つ）。
    float reach = -1.0e9f;
    HIKARI::GameProjectFloat3 reachBase{};
    HIKARI::GameProjectFloat3 reachOuter{};
    // 放った斬撃の id（0 は無い）。同じ一撃なので、剣と斬撃のどちらかが当たったら、もう片方は当たらない。
    uint64_t waveId = 0;
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
    // この反応で出した技（とコンボ）がプレイヤーに当たった時刻（c-44fdc5。当てた直後は出にくくする）。
    float hitSeconds = -1.0e9f;
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
    // HUD などが読めるよう、HP と最大 HP を自分の物体の物体変数へ毎フレーム書く。
    void Update(HIKARI::GameComponentContext& context, float deltaTime) override;

    // 表
    std::string movesPath = "Boss/kasumi_bushi_moves_kujata.csv";
    std::string typesPath = "Boss/kasumi_bushi_move_types.csv";
    // モーションの技の区切り（c-7b1a08）：クリップの長さの表（Tools/BossAnim/export_clip_lengths.ps1 が書き出す）と、
    // 区間の表（技ごとの当たりの窓。クリップの何 F 目か）。空・無い時は、アニメーションの列があっても表の長さのまま。
    std::string clipsPath = "Boss/boss_anim_clips.csv";
    std::string segmentsPath;
    // 狙いのずれの表（c-884095。Tools/Verify/verify.py aim が書き出す。空なら体をねじらない）。
    std::string aimOffsetsPath = "Boss/boss_aim_offsets.csv";
    // ヒョウタンの読み（c-14392d。横の剣気を転がる癖の側へ寄せる）。
    BossGourdReadSettings gourdRead;
    // 斬撃の共通の値（試作の共通パラメータ）
    float range = 26.0f;
    int swingFrames = 10;
    int gapSmallFrames = 10;
    int gapMediumFrames = 20;
    int gapLargeFrames = 44;
    int followFrames = 6;
    int feintFrames = 24;
    // 構えで止まる F・振り上げる F（両手で頭上・肩の上まで上げるので、12 F では速すぎた。c-573c92 で 20/12 → 16/16。
    // 前隙の長さは変わらない）。
    int kamaeFrames = 16;
    int raiseFrames = 16;
    int swingInFrames = 6;
    // 振りの速さがいちばん速くなる所（当たりの窓の進み 0〜1）。ゆっくり動き始めて加速し、ここで最も速く、振り終わりへ
    // 減速して止まる（窓の途中で速さが落ちる・最速のまま止まることがない）。大きいほど溜めて遅く当たる。
    float swingPeakShare = 0.66f;
    float damageBase = 6.0f;
    float damagePerFrame = 0.15f;
    float thickness = 0.5f;
    // フェーズと気絶
    // HP の条はフェーズごとに 1 本（c-3e0ffa）。1 本目が空になるとフェーズ 2 の変わり目に入り、2 本目が満ちる。
    // 余ったダメージは 2 本目へ持ち越さない。2 本目が空で撃破。同じ物体の Damageable の最大 HP はこの値で上書きする。
    // 0 の時は、シーンで決めた Damageable の最大 HP（ボス全体の HP）を phaseOneShare : 1 - phaseOneShare に分ける
    // （シーンで HP を調整していれば、その合計のまま 2 本になる）。
    float phaseOneHealth = 0.0f;
    float phaseTwoHealth = 0.0f;
    float phaseOneShare = 0.4f;
    // 変わり目の長さ（1 本目が空になった tick から。無敵・AI は動かない）。変わり目の演出（同じ物体の
    // BossPhaseShiftComponent。c-8dd296・c-3a0f87）をする時は、その拍の長さの合計を使い、この値は演出が無い・切った時だけ使う。
    int phaseShiftFrames = 90;
    // 類魂の崩れにくさ（c-ff3e98）。気絶値 110 → 220：精力満タンの連射（4 発）だけでは気絶しない。
    // 気絶するたびに、次に要る気絶値を stunMax × stunMaxGrowth ずつ増やす（2 回目 275、3 回目 330）。
    float stunMax = 220.0f;
    float stunMaxGrowth = 0.25f;
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
    // 体勢（c-808641）：気絶値は技・連段の途中もたまるが打ち切らず、連段が終わった時（待機へ戻る時・新しく技を始める時）に
    // 決める。満ちていればひざまずく（いまの気絶）、poiseFlinchShare 以上なら小さく怯んで後ろへ跳ぶ（技の表の行
    // poiseFlinchMove。コンボ先で着地の後の剣気）。怯みの後 poiseFlinchCooldownSeconds 秒は次の怯みを出さない。
    // breakDuringWindup が入なら、前の作り（前隙に「崩せる被弾数」を当てると打ち切る）も使う。
    float poiseFlinchShare = 0.5f;
    float poiseFlinchCooldownSeconds = 8.0f;
    std::string poiseFlinchMove = "怯み跳び";
    bool breakDuringWindup = false;
    // 戻り（後隙）の間に当てた弾の倍率：威力・気絶値、当たりの手応え（押しの倍率・止めの F）。
    float recoveryDamageScale = 1.3f;
    float recoveryStunScale = 1.5f;
    float recoveryHitPushScale = 1.6f;
    int recoveryHitStopFrames = 4;
    // 手を止めると気絶値が早く散る（c-ff3e98。元は 6 秒後から 10/秒）。
    float stunDecayDelaySeconds = 4.0f;
    float stunDecayPerSecond = 20.0f;
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
    // プレイヤーを見る（c-59bd08）：前の弾からこの F 以内に撃った弾を「続けて撃った」と数える。狙いの線が体の軸から
    // 体の半径＋watchShotAimMargin m 以内を通れば「ボスへ向いている」。立ち位置のログ（SeeStance）の間隔 F（0 で出さない）。
    int watchShotChainFrames = 40;
    float watchShotAimMargin = 0.5f;
    int watchLogFrames = 30;
    // 咎められた技の変化（c-59bd08）：入なら、技の表の「変化確率%」を、その技の後隙に当てられた回数（punishSwitchMaxHits
    // 回まで）× punishSwitchPerHit 倍だけ増やす（例：0.5 で 2 回当てられると 2 倍）。既定は切（表のまま）。
    bool punishSwitchEnabled = false;
    float punishSwitchPerHit = 0.5f;
    int punishSwitchMaxHits = 4;
    // 体。身長 6.75 m・半径 1.35 m（5.0 m・1.0 m の 1.35 倍：c-a30eaf で 1.5 倍にした後、c-e97b18 でその 0.9 倍に）。
    // 以下の「体に付く値」は、同じ倍率で揃えてある：
    // 体の位置・高さに付く値と、体の中心どうしの距離（近づく・止まる距離）は、倍率を掛ける。
    // 体の大きさを変える時に一緒に変える所の一覧は、調整ガイド g-fe0396 の【大きさ】。
    float bodyHalfHeight = 3.375f;
    float bodyRadius = 1.35f;
    HIKARI::GameProjectFloat3 arenaCenter{0.0f, 3400.0f, 0.0f};
    // （闘技場を広げた：地形の平らな所が中心から約 40 m。21 → 40 m。c-69fc1e）
    float arenaRadius = 40.0f;
    float walkSpeed = 2.2f;
    float floatMin = 0.2f;
    float floatMax = 1.4f;
    float floatBob = 0.12f;
    float idleTurnRate = 240.0f;
    float preTurnRate = 360.0f;
    float overheadTurnRate = 220.0f;
    float stringGapTurnRate = 200.0f;
    // 狙いのずれ（c-884095）：剣の技の当たりの区間が始まる aimTwistLeadFrames F 前から、表の角度へ体を
    // aimTwistTurnRate 度/秒でねじる。
    // 角度が aimTwistMax 度を超える斬りはねじらない（横や背中を向いて振ることになるので）。
    float aimTwistTurnRate = 360.0f;
    float aimTwistMax = 60.0f;
    int aimTwistLeadFrames = 20;
    // 撃破された後、体を消すまでの待ち (F) と、消えるまでの長さ (F)。消える間に体が縮んで見えなくなる。
    int deadVanishDelayFrames = 60;
    int deadVanishFrames = 30;
    // 動きの型ごとの体の動き
    // 中心どうしの距離。体を大きくした時、c-146b66・c-f91bd1 は体の表面の隙間を保つよう半径の増分を足したが、
    // c-a30eaf（1.5 倍）・c-e97b18（その 0.9 倍）では距離ごと倍率を掛けた：振りの型の握りも同じ倍率で前へ出るので、
    // 半径の差を足すだけだと
    // プレイヤーが刀の根元に入り、横なぎの刀が頭上を通って当たらなかった（握りからプレイヤーまでの間を保つ）。
    float dashStopDistance = 4.266f;
    int dashSlideFrames = 20;
    float slashApproachSpeed = 1.2f;
    float stringApproachSpeed = 1.6f;
    float stringApproachStop = 3.942f;
    // 滑って入る（c-7b1a07）：表の「滑って入る m」がある斬りの技は、前隙の終わりのこの F で、プレイヤーからこの距離 (m)
    // の所へ低く滑って詰める（突進と同じ滑らかな速さ。もう近ければ動かない）。
    int slideInFrames = 16;
    float slideInStopDistance = 4.0f;
    float overheadLungeDistance = 2.43f;
    int overheadLeapFrames = 18;
    // 空中で終わった技（跳躍）から続いた斬りの技は、前隙の始めの comboLandFrames F で落ちて着地する（c-b1824c）。
    int comboLandFrames = 6;
    float overheadLeapHeight = 1.2f;
    float overheadLeapStop = 4.8f;
    float overheadLeapMax = 6.0f;
    float overheadLeapBack = 2.0f;
    float stepInClearance = 1.62f;
    float walkInStopDistance = 6.507f;
    float walkInSpeedScale = 1.25f;
    float backstepDistance = 4.725f;
    float backstepHeight = 0.84375f;
    // 後ろ跳びの重さ（c-95444e。後ろへ下がる技の跳び）：跳ぶ長さの始めの backstepTakeoffShare は動かずに蹴り、
    // 終わりの backstepLandShare は着地して止まる。間の空中は、速さが始めと終わりで 0 の曲線で、backstepPeakShare
    // （空中の長さに対する割合）で最も速い。
    float backstepTakeoffShare = 0.17f;
    float backstepLandShare = 0.2f;
    float backstepPeakShare = 0.35f;
    // 跳躍→X字2段（試作：前隙の 28% で屈み、残りで 3.6 m まで跳ぶ。後隙の最初の 1/3 で降りる）
    float jumpHeight = 3.6f;
    float jumpCrouchShare = 0.28f;
    // 回転→霞の輪（試作：半径 0.8 m から判定サイズまで、斬撃速度で広がる。消えた所の 0.7 倍の円に残り煙 6 か所）
    float kickRingStartRadius = 1.512f;
    float kickResidueScale = 0.6f;
    float kickResidueRadiusShare = 0.7f;
    // 地面に残す火の形（形そのものは技の表の「残す火」。交差の線の長さと、上段溜めの池を置く前の距離）
    float burnCrossLength = 7.0f;
    float burnPoolForward = 2.5f;
    float burnLineLag = 1.2f;
    // 吸い込みで斬撃・ナイフを吸う範囲（試作 inhaleRange 5 m・inhaleAngle 片側 60°。1.5 m 以内は向きを問わない）
    float absorbCatchRange = 5.0f;
    float absorbCatchAngle = 60.0f;
    float absorbCatchNear = 1.5f;
    // 捕まえた斬撃・ナイフを、ヒョウタンの口へ引き込む（c-3341a7）。入りきるまでの tick、初めゆっくり・終わり速くの強さ、
    // 巻き込む弧（出だしの向きを口への線から横へ振る角度）、入りきる時の大きさ、同時に捕まえた分を順に入れる間隔。
    int absorbPullFrames = 18;
    int knifePullFrames = 10;
    float absorbPullEasePower = 2.0f;
    float absorbPullSwirlDegrees = 30.0f;
    float absorbPullEndScale = 0.12f;
    int absorbPullStaggerFrames = 3;
    // 斬撃の三日月の端を口へ向ける割合（0＝向きを変えない、1＝端を口へ向けきる）。
    float absorbPullTurn = 0.0f;
    // 引き込みの見た目：刃の霧の源の量（飛んでいる時を 1 として）、幅のほかの向き（厚み・前後）を細くする倍率、
    // 入りきった後に残った霧と光が消えるまで（秒）。
    float absorbPullFly = 0.6f;
    float absorbPullPinch = 0.45f;
    float absorbPullFadeSeconds = 0.3f;
    // 域に残った三日月の両翼の霧を、刃の中心（口）へ押し寄せる力の強さ (m/s²) と半径 (m)。引き込みが進むほど強くなる。
    float absorbPullSqueeze = 700.0f;
    float absorbPullSqueezeRadius = 3.5f;
    // 体の見た目（体に付く効果の大きさと置き場所・胴の煙と火）は、同じ物体の体の見た目の部品（BossBodyComponent。
    // c-bd1a7a）。
    // 演出の効果の部品の既定（登場とフェーズの変わり目の演出が共通で使う。登場そのものは、同じ物体の
    // BossIntroComponent。c-eb3b08）。
    BossCinematicFxSettings cinematicFx;
    // 骨格の体（技をモーションで見せる体。登場の役者と同じ骨格。c-7b1a06）の物体・クリップ・骨の名前。規則が技の
    // クリップの刀を読むのに使う（流して見せるのは体の見た目の部品）。
    BossAnimBodySettings animBody;
    // 剣の線（付け根と長さ）。斬撃の煙・剣先のエフェクト・刀光が同じ線を使う。付け根は、骨格の体の刀をまだ読めて
    // いない間の休めの構え（BossPose::Rest）だけが使う（ほかはクリップの刀か見えている刀の骨）。
    // 体の倍率どおり（5 m の体の 1.35 倍）。
    HIKARI::GameProjectFloat3 swordShoulder{-1.44315f, 4.00275f, 0.313875f};
    // 剣の長さと剣先の霞（当たりの線は握りから swordLength＋swordHazeLength）。見える刀（BossBody.bodySwordVisibleLength）に
    // 合わせる：両手の斬りで当たりを見える刀に合わせた時（c-8c8d95）に、刀を 3.4 m へ大きくし、霞を剣先の少し先までにした。
    // 刀をもう一回り大きくした時（c-be27a9、見える刀 3.8 m）と、体を大きくした時（c-a30eaf・c-e97b18、見える刀 5.13 m）に、
    // 当たりの線も同じだけ伸ばした。
    float swordLength = 4.59f;
    float swordHazeLength = 0.6075f;
    // 霞歩（横っ飛び）とかわし。
    BossKasumiStepSettings kasumiStep;
    float waveForward = 1.324f;
    float waveMinHeight = 0.941f;
    float waveMaxHeight = 2.788f;
    // 斬撃を、振った刀に合わせて放つ（c-1ed669）：刀が正面を斬った部分（振りのこの割合から先）で、剣先が前へ一番出た所を
    // 過ぎた tick に、その時の刃の waveBladeShare の所（0＝付け根、1＝剣先）から、刀の線が掃いた面の向きで放つ。
    // 偽なら、振り抜いた tick に体の正面から放つ（前の作り）。
    bool waveFromBlade = true;
    float waveCutShare = 0.5f;
    float waveBladeShare = 0.5f;
    // 三日月の向きを、刀が斬った面へ合わせる割合（1＝面に合わせる。0＝剣先の最初と最後を結ぶ線＝前の作りの向き）。
    float wavePlaneMatch = 1.0f;
    int waveGrowFrames = 8;
    float knifeHeight = 4.70475f;
    float knifeForward = 1.56735f;
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
    float swordSmokeRadius = 0.7155f;
    float depositRadius = 1.5f;
    float depositHeight = 0.6f;
    int depositFrames = 24;
    // 爆発の跡に残す吸える煙（爆発の大きさ 1 の時の量と半径。量は大きさに、半径は大きさの平方根に比例する。
    // 爆発の炎と光が消えるまで待ってから、少しずつ出す）。
    float blastFogSmoke = 12.0f;
    float blastFogRadius = 1.2f;
    int blastFogDelayFrames = 30;
    int blastFogFrames = 40;
    float bodySmokeRadius = 1.3f;
    // 場の火が体をよける大きさ（火の域にだけ効く障害物。半径 0 でよけない）。
    float fireObstacleRadius = 1.38375f;
    float fireObstacleHeight = 6.75f;
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
    // 火の刃がプレイヤーに当たった所の火花（攻撃の向きへ飛ぶ。炎そのものは、場の火の域の流体で出す）。
    HIKARI::GameProjectVfxAssetReference fireHitEffect{"b0e523d7675b2f4320b92d7f1b0cda12",
                                                       "Assets/Vfx/PlayerFireHit.hvfx"};
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
    float igniteFlashRadius = 0.7f;
    float hitBlastRadius = 1.1f;
    float kamaeEmberScale = 0.6f;
    float knifeEffectAmount = 0.8f;
    // 見た目：投げナイフそのもの（刃と光点。局所 +X が飛ぶ向き。流体域は使わない）
    HIKARI::GameProjectVfxAssetReference knifeEffect{"26fa37413fbcc48cc52d0605225433bb", "Assets/Vfx/BossKnife.hvfx"};
    // 見た目：左手の技の霧の弾（c-4be37e）。入れると投げナイフの刃を出さず、尾の霞（swordEffect）を弾の大きさ
    // knifeMistScale 倍にして、霧の芯を knifeMistCore の明るさで灯して飛ばす。
    bool knifeMistBullet = false;
    float knifeMistScale = 2.5f;
    float knifeMistCore = 1.5f;
    // 見た目：振りで場の霧を揺らす（c-4be37e）。当たりの窓の間、刀の外側と内側の所で、刃が進む向きへ場の霧を押す力
    // (m/s²。0 は押さない) と、その力が届く半径 (m)。
    float swingPushForce = 0.0f;
    float swingPushRadius = 2.0f;
    // 見た目：回し蹴りの霞の輪（広がる炎の輪。自前の流体域を 1 つ使う。足元の霧を外へ押す）
    HIKARI::GameProjectVfxAssetReference ringEffect{"cbffd59dc133951894e126d8243b0fd2", "Assets/Vfx/BossKickRing.hvfx"};
    float ringEffectRadius = 9.0f;
    float ringFadeSeconds = 0.35f;
    float ringHeight = 0.6f;
    float ringBurstScale = 0.6f;
    int spinCoilFrames = 14;
    float spinCoilDegrees = 120.0f;
    float spinTurns = 1.0f;
    float spinRingShare = 0.45f;
    float ringPush = 6.0f;
    float ringPushRadius = 1.2f;
    // 確かめ用（正式シーンでは使わない）
    std::string testMove;
    bool testSequence = false;
    bool testLoop = false;
    int testStartFrames = 60;
    int testGapFrames = 40;
    bool testHoldStill = false;
    bool testChain = false;
    int testDamageAtFrame = -1;
    float testDamageAmount = 40.0f;
    int testDamageRepeatFrames = 0;
    int testDamageRepeatCount = 1;
    int traceEveryFrames = 0;
    int randomSeed = 7;
    // 間合いの計測（c-c010ef。BossReachProbe）：剣の技が当たる距離 × 角度を記録に出す。
    bool reachProbe = false;
    float reachProbeDistanceMin = 1.0f;
    float reachProbeDistanceMax = 10.0f;
    float reachProbeDistanceStep = 0.25f;
    int reachProbeAngleStep = 10;

    // 実行時の状態は保存しない。
    bool initialized = false;
    // 間合いの計測を済ませた（Play の間に 1 度だけ測る）。
    bool reachProbeDone = false;
    // 戦いの流れ（Flow）が、戦いが始まるまでボスの行動を止めている間 true。技を頼まず、歩かない。
    bool held = false;
    // 物体変数へ出す HP と最大 HP（Damageable の値を、ボスの固定 tick ごとに写す）。書き込みが断られた警告は 1 回だけ出す。
    float shownHealth = 0.0f;
    float shownMaxHealth = 0.0f;
    // HP の条を出すか（Boss.BarShown へ書く）。
    float shownBar = 1.0f;
    // 登場の演出の間か（Boss.Cinematic へ書く）。
    float shownCinematic = 0.0f;
    // Boss.Phase・Boss.PhaseSeconds へ書く値（固定 tick ごとに写す）。
    int shownPhase = 1;
    float shownPhaseSeconds = 0.0f;
    // HP の条（c-3e0ffa）：Damageable の最大 HP を 1 本目の値にしたか。いまの条に入った時刻（秒）。変わり目の残り F
    // （0 は変わり目でない）。
    bool healthBarsReady = false;
    // シーンで決めた Damageable の最大 HP（条の値が 0 の時に分ける元）。
    float authoredHealth = 0.0f;
    float phaseStartSeconds = 0.0f;
    int phaseShiftLeft = 0;
    bool variableWriteWarned = false;
    bool tableLoaded = false;
    BossMoveTable table;
    std::string loadedClipsText;
    std::string loadedSegmentsText;
    std::string loadedAimText;
    // プレイヤーへ当たった数（受け付けられた当たり。姿勢が「当てた後」を見る）。
    unsigned playerHitCount = 0;
    // 最後に読んだ表の中身。同じなら読み直さない（警告を何度も出さない）。
    std::string loadedMovesText;
    std::string loadedTypesText;
    HIKARI::GameProjectFloat3 ground{};
    float yaw = 0.0f;
    float height = 0.0f;
    // 技を始めた時の高さ（空中から続いた斬りの技が、前隙の始めに着地する元の高さ）。
    float landFrom = 0.0f;
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
    float strafe = 1.0f;
    float sideSign = 1.0f;
    // 規則の剣の向き（BossPose::Target）。固定 tick の終わりに BossSystem が 1 度だけ書き、煙・見た目・記録は
    // これを読む。tick の途中（振りの最中の BossMoveExecutor）は、状態が変わる前の向きが要るので現場で求める。
    BossSwordPose swordPose;
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
    // 構えの途中の変化（表の「変化先」）を、この技でもう振ったか（構えきる F で 1 回だけ）。
    bool switchRolled = false;
    // 前隙の間に当てられた回数（崩せる被弾数と比べる）。
    int windupHits = 0;
    // 追尾を止めている間（一撃を放つまで）。狙いは止めた時のプレイヤーの位置のまま。
    bool aimFrozen = false;
    HIKARI::GameProjectFloat3 aimPoint{};
    int glintFrames = 0;
    // 回し蹴りの回転（見た目の向きに足す rad）。
    float spin = 0.0f;
    // 狙いのずれのねじり（c-884095。見た目の向きと刀の当たりに足す rad。yaw はプレイヤーの方のまま）。いまの角度・目標・
    // 目標を決めた斬り（-1 は無し。振っている間・向きを止めている間は、その斬りの目標を変えない）。
    float aimTwist = 0.0f;
    float aimTwistTarget = 0.0f;
    int aimTwistStrike = -1;
    bool aimTwistHeld = false;
    // Damageable の無敵を、霞歩の跳び始め・登場・変わり目の間だけ立てたか（シーンで最初から無敵にしてある時に戻しすぎない）。
    bool shieldApplied = false;
    BossKasumiStepState kasumiStepState;
    // いまの技のどれかの一撃が、いまのフェーズで火を残す＝この技の間は剣が燃えている。技を始めた tick に決め、
    // 待機へ戻る・打ち切られる（気絶・崩れ・撃破）まで保つ。規則は読まず、見た目（BossVisuals）が読む。
    bool bladeLit = false;
    // 吸い込みで吸い取られた斬撃・ナイフの、ヒョウタンへの渡し分（BossWaves::DeliverCatch が足し、tick の終わりに
    // BossSystem が渡して空にする）。
    BossCatchHandoff caught;
    // 気絶
    float stun = 0.0f;
    int stunCount = 0;
    // 反応の枝が頼んだ技の名前の鍵（頼んだ時に覚え、技を始めた時に reactionMoveKey へ。反応でない技を始めると 0）。
    size_t pendingReactionKey = 0;
    size_t reactionMoveKey = 0;
    // 連段の途中で気絶値が満ちた（連段が終わったらひざまずく）。次に怯みを出せる時刻（秒）。戻りの手応えを強めている。
    bool stunPending = false;
    // 怯んだ回数（見た目が怯みの始まりを知るため）。
    uint32_t flinchCount = 0;
    float flinchReadySeconds = 0.0f;
    bool recoveryBoosted = false;
    // 崩れ・気絶に入った tick に立て、描画フレームの手応え（BossFeedback）が使ったら消す。
    bool breakFeedbackPending = false;
    Feedback::CameraShake breakShake{};
    float stunHitSeconds = -1000.0f;
    double seenBaseDamage = 0.0;
    float clockSeconds = 0.0f;
    uint64_t tick = 0;
    // 剣気（飛ぶ斬撃）を最後に放った tick と、それまでに放った数（見た目が放つ瞬間を知るため。c-ecd963）。
    uint64_t releaseTick = 0;
    uint32_t releaseCount = 0;
    int lastPhaseNumber = 1;
    // フェーズ 2 の見た目（灼けた鎧・火の目・刀の常時の火・場の色）を、変わり目の演出の閃光まで待っている（演出が
    // 立てて閃光で下ろす。BossBodyMath::PhaseTwoLook・BossRules::IsBladeLit が読む。c-3a0f87 で演出の状態から移した）。
    bool phaseTwoLookPending = false;
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
    // 爆発の跡に残した煙の累計（煙の単位。ログ用）。
    double blastFogTotal = 0.0;
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
    // この技で霞の輪をもう出したか（回転斬り）。
    bool spinRingOut = false;
    // クリップの回転斬りの型で、最後に輪を出した当たりの番号（当たりの窓ごとに 1 つ出す。-1 はまだ）。
    int spinRingHit = -1;
    // 輪が消えた後、炎が消えるのを待っている残り時間（秒。負は輪が広がっている間）。
    float ringFade = -1.0f;
    float ringScale = 0.0f;
    HIKARI::GameProjectFloat3 ringCenter{};
    std::array<BossSlashEffect, kBossSlashEffectCapacity> slashEffects{};
    HIKARI::GameProjectVfxPlaybackHandle swordVfx{};
    BossCinematicFxState cinematicFxState;
    // 技のクリップの刀（規則の側。PrepareSword が固定 tick の中で読む）。
    BossClipSwords clipSwords;
    // プレイヤーの位置（固定 tick ごとに書く。見た目だけが読む：近い時は胴の煙を薄くする）。
    HIKARI::GameProjectFloat3 lookTarget{};
    bool lookTargetSeen = false;
    // 補間：前の tick とこの tick の終わりに体の物体へ置いた位置と、覚えた tick の数（2 で補間できる）。
    // 固定 tick の終わりに BossTransform が書き、体の見た目が描画のフレームで間を補間する。
    HIKARI::GameProjectFloat3 tickFrom{};
    HIKARI::GameProjectFloat3 tickTo{};
    int ticksSeen = 0;
    bool vfxAccepted = true;
    bool fluidAccepted = true;
};

} // namespace Project_TD2_2
