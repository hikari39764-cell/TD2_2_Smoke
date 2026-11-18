#pragma once

#include <array>
#include <string>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "../Combat/Feedback.h"

namespace Project_TD2_2 {

// 登場の段階。散らばって待つ → 煙が鎧に憑いて引き寄せる → 煙の中で形になる → 刀を抜く → 一振りして構える → 一息 → 済み。
// 規則（BossIntro）が固定 tick ごとに進め、見た目（BossIntroVisuals）が読む。
enum class BossIntroPhase { Scattered, Gather, Assemble, Draw, Flourish, Settle, Done };

// とぐろの黒い煙の帯の数の上限。
inline constexpr int kBossIntroCoilCapacity = 6;
// 登場の間に吹く煙の塊（刀が抜ける瞬間・一振りの輪）を同時に持てる数と、鎧の部品の数の上限。
inline constexpr int kBossIntroPuffCapacity = 24;
inline constexpr int kBossIntroPartCapacity = 24;
inline constexpr int kBossIntroTrailMarkCapacity = 8;

// 登場（c-429242）：戦いの前、鎧の部品は地面に散らばり、刀は地面に刺さり、黒い煙が広く立ち込めて、中央でとぐろを巻いて
// いる。戦いが始まると、周りの煙が中央へ流れ、煙の筋が鎧の部品へ這って憑き、引きずって集め、煙の塊の中で浮かび上がって
// ゆるく形になり、しだいに締まって武者の姿になる。最後に右手で刀を抜いて、待機の構えに収まる。
// 主役は煙で、鎧は煙に運ばれる殻（部品が自分で飛んで嵌まる動きにはしない）。
// 位置は、ボスの始めの足元から、プレイヤーのいる側を前にした局所 m（+X が左、+Z が前）。体の大きさは掛けない。
struct BossIntroSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 登場をするか。切ると、初めから組み上がって立っている。
    bool enabled = true;
    // Play の間に 1 度だけにする（負けてやり直した後は、初めから組み上がって立っている）。
    bool oncePerPlay = true;
    // 各段の長さ (F)：煙が憑いて引き寄せる・煙の中で形になる・刀を抜く・一振りして構える・構えてから動き出すまで。
    int gatherFrames = 270;
    int assembleFrames = 270;
    int drawFrames = 96;
    int flourishFrames = 168;
    int settleFrames = 24;
    // 確かめ用（正式シーンでは使わない）：0 以上なら、戦いの流れに関わらず、この tick に登場が始まる。
    int testStartFrames = -1;

    // 立ち込める黒い煙：広い・低い流体域のエフェクト（場の雰囲気。全部は使わない）。登場が始まると湧き止み、中央へ
    // 流れて薄れる（周りの煙が敵になっていく）。空なら出さない。
    HIKARI::GameProjectVfxAssetReference hazeEffect{"9d2b6f40c7a14e83b5f0a6c1d8e37f12",
                                                    "Assets/Vfx/BossIntroHaze.hvfx"};
    // 立ち上がる瞬間まで湧き続け、その間、中央へゆっくり流れる：流す速さ (m/秒。届く半径の所での速さ)、届く半径 (m)。
    // 登場が済んでから域を止めるまで (秒)。
    float hazeDrain = 0.2f;
    float hazeDrainRadius = 25.0f;
    // 立ち上がって吹き払った後、登場の煙を消す速さ（毎秒の割合。大きいほど早く消える。戦いに煙を残さない）：
    // 立ち込める煙と、とぐろの輪。
    float hazeClearRate = 7.0f;
    float coilClearRate = 10.0f;
    float hazeFadeSeconds = 1.0f;

    // とぐろの黒い煙：世界に固定の流体域のエフェクト（資産の輪の源が帯を作り、その上に、巡る頭の位置へコードから濃い煙と
    // 熱を入れる）。
    HIKARI::GameProjectVfxAssetReference coilEffect{"5e0c7a91b34d4c28a6f1d9e2037b84c5",
                                                    "Assets/Vfx/BossIntroCoil.hvfx"};
    // 巡る頭の数、いちばん下の輪の半径 (m)、上の輪ほど半径を変える量 (m。負で、上ほどすぼまる)、半径のうねり (m)。
    int coilStreams = 3;
    float coilRadius = 7.6f;
    float coilRadiusStep = -0.6f;
    float coilRadiusWave = 0.4f;
    // いちばん下の輪の高さ (m)、上の輪ほど高くする量 (m)、上下のうねり (m)。
    float coilHeight = 0.7f;
    float coilHeightStep = 0.8f;
    float coilHeightWave = 0.3f;
    // 輪の傾き（度。輪ごとに傾く向きが違い、帯どうしが斜めに交わる）と、傾く向きがゆっくり回る速さ (度/秒)。
    float coilTiltDegrees = 5.0f;
    float coilTiltTurnDegrees = 4.0f;
    // 頭が巡る速さ (m/秒)。
    float coilSpeed = 2.4f;
    // 頭が入れる煙：半径 (m)、中心の毎秒の密度、温度（内側の暗い赤の光。0 で光らない）、頭の速さを新しい煙に持たせる割合、
    // 濃さのむら（0 で一様）。
    float coilEmitRadius = 0.6f;
    float coilDensity = 35.0f;
    float coilHeat = 0.0f;
    float coilCarry = 0.35f;
    float coilFlicker = 0.3f;
    // 鎌首：頭の 1 つが、この間隔 (秒) ごとに、この長さ (秒) をかけて、この高さ (m) まで持ち上がって沈む。
    float coilRearSeconds = 5.0f;
    float coilRearEverySeconds = 16.0f;
    float coilRearHeight = 3.0f;
    // 引き寄せる間：頭が寄る先の半径 (m)、昇る高さ (m)、巡りが速まる倍率。
    float gatherRadius = 1.0f;
    float gatherHeight = 3.0f;
    float gatherSpeedScale = 1.6f;
    // 頭が、プレイヤーの側（舞台の前）を通る間に入れる煙の量（後ろに対する割合。小さいほど、前が開いて舞台が見える）。
    float coilFrontShare = 0.1f;
    // 引き寄せる間、頭が寄る先を、ボスの後ろへずらす量 (m。体の前に煙が立たない)。
    float mantleBack = 1.8f;
    // 引き寄せ切った時の、とぐろの煙の量（待つ間に対する割合。減らして、憑かれた鎧が見えるようにする）。
    float gatherCoilShare = 0.15f;
    // 引き寄せる間、とぐろの煙を中央へ吸い寄せる流れ：速さ (m/秒。0 で吸わない)、届く半径 (m)、吸う点の高さ (m)。
    float gatherPull = 0.0f;
    float gatherPullRadius = 9.0f;
    float gatherPullHeight = 1.8f;
    // 形になる段のこの割合の間に、とぐろの煙を出し終える（胴の煙へ引き継ぐ）。出し終えてから域を止めるまで (秒)。
    float coilHandoffShare = 0.5f;
    float coilFadeSeconds = 1.0f;

    // 刀：剣先が刺さる地面の点、柄が傾く向き（局所の yaw。度。0 で前へ倒れる）、傾き（度。0 でまっすぐ立つ）、
    // 地面に埋まる長さ (m)、刃が向く向き（局所の yaw。度）。
    HIKARI::GameProjectFloat3 swordGround{-1.5f, 0.0f, 1.5f};
    float swordLeanYawDegrees = 174.0f;
    float swordLeanDegrees = 12.0f;
    float swordBuried = 1.0f;
    float swordEdgeYawDegrees = -90.0f;
    // 鎧の部品が地面に置いてある所（位置と、向き＝X・Y・Z まわりの度。Y → X → Z の順に回す）。
    HIKARI::GameProjectFloat3 torsoRest{0.9f, 0.62f, -0.9f};
    HIKARI::GameProjectFloat3 torsoRestTurn{-78.0f, 35.0f, 8.0f};
    HIKARI::GameProjectFloat3 headRest{-0.7f, 0.50f, 3.2f};
    HIKARI::GameProjectFloat3 headRestTurn{18.0f, 150.0f, 62.0f};
    HIKARI::GameProjectFloat3 sodeRestRight{-3.4f, 0.14f, 0.4f};
    HIKARI::GameProjectFloat3 sodeRestRightTurn{-86.0f, 70.0f, 0.0f};
    HIKARI::GameProjectFloat3 sodeRestLeft{2.9f, 0.14f, 1.5f};
    HIKARI::GameProjectFloat3 sodeRestLeftTurn{-84.0f, 200.0f, 0.0f};
    HIKARI::GameProjectFloat3 cuffRestRight{-2.5f, 0.2f, -1.6f};
    HIKARI::GameProjectFloat3 cuffRestRightTurn{0.0f, 40.0f, 0.0f};
    HIKARI::GameProjectFloat3 cuffRestLeft{1.6f, 0.2f, 3.3f};
    HIKARI::GameProjectFloat3 cuffRestLeftTurn{0.0f, 250.0f, 0.0f};
    // 草摺：始めの足元の周りへ、この半径 (m) の間に散らす（向き・距離は tassetSeed から決まる）。地面からの高さ (m)。
    float tassetRestRadiusMin = 2.2f;
    float tassetRestRadiusMax = 4.6f;
    float tassetRestHeight = 0.1f;
    int tassetSeed = 11;

    // 憑く：煙の筋が部品に届く時刻の幅（引き寄せる段の中の割合。胴がいちばん早く、兜がいちばん遅い）、筋が這う長さ（割合）、
    // 憑かれた部品が浮く高さ (m)。
    float possessFirstShare = 0.05f;
    float possessLastShare = 0.42f;
    float tendrilShare = 0.12f;
    float possessLift = 0.16f;
    // 引きずる：横へふらつく幅 (m)、運ばれる高さ (m)、中央で溜まる所の半径の幅 (m)。
    float dragWobble = 0.45f;
    float dragCarryHeight = 0.35f;
    float pileRadiusMin = 0.9f;
    float pileRadiusMax = 1.9f;
    // 憑いた部品が出す黒い煙（引きずった跡に尾が残る）：半径 (m)、中心の毎秒の密度、温度（隙間の暗い赤の光）。
    float pieceSmokeRadius = 0.38f;
    float pieceSmokeDensity = 30.0f;
    float pieceSmokeHeat = 0.0f;
    // 形になる：浮き上がった部品の、付く所からのずれ (m) と傾き（度）。立ち上がる勢いで looseAfterBurst まで引き締まり、
    // 形になる段のこの割合で締まり切る。
    float looseOffset = 0.5f;
    float looseTiltDegrees = 24.0f;
    float looseAfterBurst = 0.15f;
    float looseSettleEndShare = 0.86f;
    // うずくまった姿から一気に立ち上がる：うずくまった高さ（立った高さに対する割合）、前へのめる量 (m)、前へ傾く角度
    // （度）、立ち上がり始める・立ち上がり切る割合（形になる段の中で。この間が短いほど勢いがある）。
    float slumpHeight = 0.42f;
    float slumpLean = 0.8f;
    float slumpPitchDegrees = 26.0f;
    float riseStartShare = 0.50f;
    float riseEndShare = 0.74f;
    // 立ち上がる間に、ばねを硬くする倍率（締まっていく）。立った高さを少し行き過ぎて戻る量（立った高さに対する割合）。
    float burstFollowScale = 1.8f;
    float burstOvershoot = 0.03f;
    // 演出のエフェクト（見える刀と体に付く、シェーダーの層のエフェクトを使う。Boss の「外形：技の演出」の資産）。
    // 目が灯る瞬間の閃きの強さ（0 で出さない）。
    float eyesFlare = 2.4f;
    // 刀が地面を離れる瞬間：地面の衝撃の輪の大きさと、刀身を走る光の明るさ（0 で出さない）。
    float drawShock = 1.2f;
    float drawGlint = 2.0f;
    // 部品が煙に遅れて付いて行くばね（1 秒あたりの揺れの回数・収まりの強さ。小さいほど柔らかく、行き過ぎて戻る）と、
    // 向きが追う時間 (秒)。
    float followFrequency = 1.3f;
    float followDamping = 0.6f;
    float turnSeconds = 0.3f;
    // 兜が持ち上がる間に、左右を見回す角度（度）。
    float headLookDegrees = 32.0f;
    // 胴の煙：形になる段の始めに濃く立つ倍率と、この割合で普段の量に落ち着く。
    float smokeColumnScale = 1.0f;
    // 胴の煙：立ち上がる前（うずくまっている間）の量（倍率。胴の煙は立った高さで湧くので、少なくしておく）。
    float smokeCrouchScale = 0.15f;
    // 胴の煙：立ち上がった瞬間に、鎧の継ぎ目から噴き出す量（普段の量に足す倍率。すぐ収まる）。
    float smokeBurstScale = 0.4f;
    float smokeSettleShare = 0.8f;

    // 握る前の右手の置き場所（1 倍の体での値。体の大きさを掛ける）。
    HIKARI::GameProjectFloat3 rightRestWrist{-1.0f, 1.85f, 0.35f};
    // 抜く（刀を抜く段の中の割合）：袖口の煙が柄へ流れ始める・手が柄に届く・力を込め終わる（ここから一気に斬り上げる）・
    // 八相に構え切る（残りで構えが決まる）。
    float drawTendrilShare = 0.04f;
    float drawReachShare = 0.36f;
    float drawHoldShare = 0.58f;
    float drawRipShare = 0.84f;
    // 斬り上げの道：地面に刺さった所 → 体の右の外へ振り出す所（正面から見て大きな弧になる） → 八相（右の肩の上に刀を立てた構え）。握りの位置（1 倍の体での
    // 値。体の大きさを掛ける）と剣先の向き（局所。右が -X、前が +Z）。
    HIKARI::GameProjectFloat3 drawRipGrip{-1.10f, 2.40f, 0.50f};
    HIKARI::GameProjectFloat3 drawRipTip{-1.0f, 0.10f, 0.30f};
    HIKARI::GameProjectFloat3 highGrip{-0.71f, 3.26f, 0.27f};
    HIKARI::GameProjectFloat3 highTip{0.12f, 0.88f, -0.46f};
    // 斬り上げが八相を行き過ぎる量（道のりに対する割合。すぐ戻って構えが決まる）。
    float drawRipOvershoot = 0.06f;
    // 斬り上げの間に刀の跡に残る黒い煙の濃さ（一振りの slashTrailScale と同じ倍率。体の前に煙の柱が立って体を隠すので、既定は 0）。
    float drawTrailScale = 0.0f;
    // 体の動き：力を込める間（刀の方へ捻り、前へ傾け、腰を落とす）と、八相の構え。捻りは度で、正が左、負が右（刀の側）。
    // 傾きは度で、正が前。腰を落とす量は立った高さに対する割合。
    float strainTwistDegrees = -14.0f;
    float strainLeanDegrees = 10.0f;
    float strainSink = 0.05f;
    float highTwistDegrees = -28.0f;
    float highLeanDegrees = -4.0f;
    // 斬る前の溜め（刀を抜く段の始めから振り始めまで、だんだん強く）：周りの黒い煙が渦を巻いてボスへ集まり続ける。
    // 外から巻き込む煙の筋の数・筋が出る半径と巻き込み終わる半径 (m)・1 本が巻き込むのにかかる秒・筋の濃さと塊の半径 (m)・
    // 高さ（外・内 m）・1 回の巻き込みで回る角度（度）。とぐろの煙をボスの足元へ引き込む速さ (m/秒)、立ち込める煙を中央へ
    // 流す速さ (m/秒。普段の introHazeDrain に足す)。
    int chargeStreams = 8;
    float chargeOuterRadius = 8.5f;
    float chargeInnerRadius = 2.0f;
    float chargeSeconds = 0.9f;
    float chargeDensity = 60.0f;
    float chargeRadius = 0.7f;
    float chargeOuterHeight = 0.8f;
    float chargeInnerHeight = 3.2f;
    float chargeSpinDegrees = 160.0f;
    float chargePull = 0.8f;
    float chargeHazePull = 2.0f;
    // 刀が地面から抜ける瞬間に、裂け目から噴く黒い煙：半径 (m)、密度、噴く長さ (秒)、噴き上がる速さ (m/秒)。
    float drawBurstRadius = 0.8f;
    float drawBurstDensity = 340.0f;
    float drawBurstSeconds = 0.4f;
    float drawBurstSpeed = 5.0f;
    // 抜いてから振り下ろすまで、刀身が引く煙の弧：半径 (m)、密度、温度（暗い赤の光）。
    float bladeTrailRadius = 0.28f;
    float bladeTrailDensity = 70.0f;
    float bladeTrailHeat = 0.0f;
    // 一振り（登場の締め。両手の袈裟斬りで、周りに集まった煙を吹き払う）。一振りの段の中の割合：振り始める（それまでは
    // 八相で溜め、左手を柄へ添え、少し引く）・振り抜く（振り始めとの間が短いほど速い）・残心が終わる（このあと見得へ）。
    float slashHoldShare = 0.214f;
    float slashStrikeShare = 0.262f;
    float slashRestShare = 0.44f;
    // 一振りの道：八相 → 体の前を通る所 → 左下へ振り抜いた所。握りの位置（1 倍の体での値）と剣先の向き（局所）。
    HIKARI::GameProjectFloat3 slashMidGrip{-0.15f, 2.45f, 0.95f};
    HIKARI::GameProjectFloat3 slashMidTip{0.35f, 0.55f, 0.76f};
    HIKARI::GameProjectFloat3 slashEndGrip{0.35f, 1.75f, 1.00f};
    HIKARI::GameProjectFloat3 slashEndTip{0.62f, -0.50f, 0.60f};
    // 振り抜いた体：捻り（度。正が左）、前への傾き（度）、腰を落とす量（立った高さに対する割合）。
    float slashTwistDegrees = 30.0f;
    float slashLeanDegrees = 16.0f;
    float slashSink = 0.08f;
    // 締めの見得（血振り）：残心の後、刀を体の右の外へ一気に振り払い、腕を伸ばして刀身を横へ出したまま胸を張って止まる。
    // 一振りの段の中の割合：振り払い終える・見得を解く（残りで待機の構えへ戻る）。握りの位置（1 倍の体での値）と剣先の
    // 向き（局所）、体の捻り・前への傾き（度）。
    float poseShare = 0.50f;
    float poseHoldShare = 0.83f;
    HIKARI::GameProjectFloat3 poseGrip{-1.45f, 2.55f, 0.45f};
    HIKARI::GameProjectFloat3 poseTip{-0.92f, -0.30f, -0.10f};
    float poseTwistDegrees = -10.0f;
    float poseLeanDegrees = -5.0f;
    // 溜めの間に刃口が明るくなる強さ、体を包む気配（ゆがみ）の強さ。
    float slashEdge = 2.5f;
    float slashAura = 2.0f;
    // 振り抜いた瞬間（登場でいちばん強い拍）：衝撃の輪の大きさ（1 つ目・遅れて出る 2 つ目。1 で半径 約 3.4 m）、刀身を走る
    // 光の明るさ、刀身の上で弾ける閃光のエフェクトとその大きさ、周りの煙を吹き払う強さ (m/秒) と半径 (m)、時間を止める長さ (F)。
    float slashShock = 3.8f;
    float slashShockSecond = 2.4f;
    float slashGlint = 2.2f;
    HIKARI::GameProjectVfxAssetReference slashFlashEffect{"c7a41d6be2f0483a9d15b0e36f7a2c18",
                                                          "Assets/Vfx/BossMistBurst.hvfx"};
    float slashFlashScale = 2.0f;
    float slashBlast = 22.0f;
    float slashBlastRadius = 14.0f;
    int slashHitStopFrames = 8;
    // 振り抜いた瞬間に、ボスのまわりに集まった煙（とぐろの域）を外へ吹き飛ばす強さ (m/秒。0 で押さない)。
    float slashCoilBlast = 32.0f;
    // 振り抜いた瞬間の白黒の閃き（全画面の効果。Assets/Post/BossImpactMono）：プロファイル（空なら出さない）、反転した
    // 白黒の長さ・反転しない白黒の長さ・色へ戻る長さ（実時間の秒。時間の停止の間も進む）、強さ（0〜1）。白と黒の境・硬さ・
    // 明るさは、効果の資産の項目で変える。
    std::string slashMonoProfile{"9e45ffde0ff4c6ee90345798c55b262f"};
    float slashMonoInvertSeconds = 0.035f;
    float slashMonoHoldSeconds = 0.05f;
    float slashMonoFadeSeconds = 0.12f;
    float slashMonoStrength = 1.0f;
    // 振る間、刀の通った跡に残る黒い煙の弧：濃さ（刀身が引く煙に対する倍率。0 で出さない）、塊の半径 (m)、外へ広がる
    // 速さ (m/秒。弧が波のように広がって、輪の所で消える)。
    float slashTrailScale = 5.0f;
    float slashTrailRadius = 0.55f;
    float slashTrailSpeed = 3.0f;
    // 振り下ろした時に、足元から広がる煙の輪（体が煙を吐く）：塊の数、出る半径 (m)、塊の半径 (m)、密度、長さ (秒)、
    // 広がる速さ (m/秒)。
    int flourishRingCount = 8;
    float flourishRingStart = 2.4f;
    float flourishRingRadius = 0.6f;
    float flourishRingDensity = 100.0f;
    float flourishRingSeconds = 0.16f;
    float flourishRingSpeed = 12.0f;
    // 一振りが地面の霧を外へ押す強さ (m/秒) と、押す半径 (m)。0 で押さない。
    float flourishFogPush = 5.0f;
    float flourishFogPushRadius = 3.0f;
    // 手応え：刀が地面を離れる瞬間（登場でただ 1 つの強い拍）のカメラの揺れ（揺れ幅 m・長さ F・速さ）、時間を止める
    // 長さ (F) とその間の時間の速さ、振動（コントローラー）の強さと長さ (F)。一振りの輪は accentSoftShare 倍の揺れ。
    float accentShake = 0.14f;
    int accentShakeFrames = 16;
    float accentShakeFrequency = 20.0f;
    // 手応え：強い拍で、画角を一瞬広げる量（度。0 で動かさない）。
    float accentFovDegrees = 2.5f;
    float accentSoftShare = 0.35f;
    int drawHitStopFrames = 2;
    float hitStopScale = 0.1f;
    float accentRumble = 0.6f;
    int accentRumbleFrames = 12;
};

// 登場の実行時の状態（保存しない）。
struct BossIntroState {
    BossIntroPhase phase = BossIntroPhase::Done;
    // いまの段に入ってからの F。
    int frame = 0;
    // 散らばりの基準：始めの足元と向き（Initialize が入れ、プレイヤーが見えた tick にプレイヤーの側へ向ける）。
    HIKARI::GameProjectFloat3 origin{};
    float yaw = 0.0f;
    // 戦いの流れ（Flow）に止められたのを見たか（止められる前の最初の数 tick で始めない）。
    bool heldSeen = false;
    // 散らばりの基準を、プレイヤーのいる側へ向けたか。
    bool faced = false;
    // 登場が始まった。
    bool started = false;
    // 見た目（BossIntroVisuals）：前に見た段と、その段に入ってからの秒（描画のフレームで進める）。
    BossIntroPhase seenPhase = BossIntroPhase::Done;
    float phaseSeconds = 0.0f;
    // 立ち込める煙・とぐろの煙：エフェクト、時計 (秒)、出し終えてからの秒。
    HIKARI::GameProjectVfxPlaybackHandle hazeVfx{};
    float hazeIdleSeconds = 0.0f;
    HIKARI::GameProjectVfxPlaybackHandle coilVfx{};
    std::array<float, kBossIntroCoilCapacity> coilAngle{};
    bool coilReady = false;
    float coilClock = 0.0f;
    float coilIdleSeconds = 0.0f;
    // 鎧の部品：ばねで追っている位置・速さ・向き（世界）と、煙を出す所・出す重み（0 で出さない）。
    struct Part {
        bool ready = false;
        HIKARI::GameProjectFloat3 position{};
        HIKARI::GameProjectFloat3 velocity{};
        HIKARI::GameProjectQuaternion rotation{};
        HIKARI::GameProjectFloat3 smokeAt{};
        float smoke = 0.0f;
    };
    std::array<Part, kBossIntroPartCapacity> parts{};
    // 袖口から柄へ流れる煙（抜く前）：出す所と重み。
    HIKARI::GameProjectFloat3 sleeveAt{};
    float sleeve = 0.0f;
    // 煙の塊（置く所・吹く速さ・残りの秒・半径・密度・温度）。
    struct Puff {
        HIKARI::GameProjectFloat3 position{};
        HIKARI::GameProjectFloat3 velocity{};
        float seconds = 0.0f;
        float radius = 0.5f;
        float density = 0.0f;
        float heat = 0.0f;
    };
    std::array<Puff, kBossIntroPuffCapacity> puffs{};
    int puffCursor = 0;
    // 刀が地面を離れたか・一振りの輪を出したか。一振りが場の霧を押すのを、次の描画のフレームで 1 回だけ出す。
    bool drawBurst = false;
    bool flourishRing = false;
    bool fogPushPending = false;
    // 一振りで周りの煙を吹き払ったか・目が灯った拍を出したか。周りの煙を吹き払うのを、次の描画のフレームで 1 回だけ出す。
    bool fogBlown = false;
    // 一振りで吹き払ってからの秒（登場の煙を消し始める時を計る）。
    float clearSeconds = 0.0f;
    // 演出のエフェクトへの頼み（BossBodyAccents が読んで出す）：体を包む気配の強さ、衝撃の輪（置く所・大きさ・出すまでの
    // 秒。大きさ 0 は頼んでいない）、刀身を走る光の明るさ（0 は頼んでいない）、閃光を出すか。
    float aura = 0.0f;
    struct Shock {
        HIKARI::GameProjectFloat3 at{};
        float size = 0.0f;
        float delay = 0.0f;
    };
    std::array<Shock, 2> shocks{};
    float glint = 0.0f;
    bool flashPending = false;
    HIKARI::GameProjectFloat3 flashAt{};
    bool eyesAccent = false;
    bool blastPending = false;
    // 刀身が引く煙：剣先と刀身の中ほどの位置（世界）と、出す重み（0 で出さない）。
    HIKARI::GameProjectFloat3 trailTip{};
    HIKARI::GameProjectFloat3 trailMiddle{};
    float trailWeight = 0.0f;
    // 前のフレームに煙を入れた剣先・中ほどの位置（速い一振りでも、弧が点に切れないように間を埋める）。
    HIKARI::GameProjectFloat3 trailTipFrom{};
    HIKARI::GameProjectFloat3 trailMiddleFrom{};
    bool trailLinked = false;
    // 一振りの煙の弧を入れた所の目印（吹き払った後、そこへ吸収を置いて弧を消す）。
    HIKARI::GameProjectFloat3 trailMarks[kBossIntroTrailMarkCapacity]{};
    int trailMarkCount = 0;
    // 一振りの間の、刀に付くエフェクトへの頼み（BossBodyAccents が読む）：刃口の光の強さ・刀光とゆがみを出すか。
    float bladeEdge = 0.0f;
    bool bladeSwing = false;
    // 振る間、刀の跡に残す黒い煙の濃さの倍率（斬り上げと一振りで違う）。
    float trailScale = 0.0f;
    // 溜めに入った合図（目の閃き・刀身の光）を出したか。
    bool slashCharge = false;
    // 斬る前の溜めの、煙の筋の時計（秒）。とぐろの域の煙を吹き飛ばすのを次の描画で 1 回だけ出す。白黒の閃きを次の
    // 描画のフレームで始める・始めてからの実時間の秒（負は出していない）。見得に止まった拍を出したか。
    float chargeClock = 0.0f;
    bool coilBlastPending = false;
    bool monoPending = false;
    float monoSeconds = -1.0f;
    bool poseAccent = false;
    // 確かめ用（Boss.bodyTraceMotion）：前のフレームの剣先の位置。
    HIKARI::GameProjectFloat3 traceTip{};
    bool traceReady = false;
    // 手応え：頼まれた強さ（0〜1。描画のフレームで使ったら 0 に戻す）、時間を止める F、カメラの揺れ。
    float accentPending = 0.0f;
    int hitStopPending = 0;
    Feedback::CameraShake shake{};
};

} // namespace Project_TD2_2
