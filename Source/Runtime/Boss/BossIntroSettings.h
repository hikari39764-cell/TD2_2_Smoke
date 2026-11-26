#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/SequenceTypes.h"
#include "HIKARI/Project/VfxTypes.h"
#include "BossIntroActorState.h"

namespace Project_TD2_2 {

// 登場の段階（c-e0c500）。散らばって待つ（役者は最初の片段の最初の姿）→ 演技（役者が骨格の動画を演じる）→ 一息
// （戦いの体に替わった後、ゲームのカメラへ戻る間）→ 済み。規則（BossIntro）が固定 tick ごとに進め、見た目
// （BossIntroVisuals・BossIntroActor）が読む。
enum class BossIntroPhase { Scattered, Show, Settle, Done };

// 登場（c-e0c500）：戦いの前、鎧は地面に崩れ、兜と手甲は離れた所に落ちている。戦いが始まると、動きを捕まえた演技を、
// 役者（骨格と動画を持つ模型。Assets/Models/BossIntroActor）がエンジンの動画の機能で演じる：煙に引き起こされ、手甲が
// 付き、兜を被り、目が灯り、振り返って刀を抜き、右の肩に担ぐ。演出の資産（Sequence）が時計・カメラ・どの片段をいつ
// 流すか（事件段 Anim.<片段>）・部品がいつ飛ぶか（Part.<部品>.Fly）・効果の拍を受け持つ。演技が済むと、役者を隠して
// 戦いの体（骨格の体）に替える。
struct BossIntroSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 登場をするか。切ると、初めから組み上がって立っている。
    bool enabled = true;
    // Play の間に 1 度だけにする（負けてやり直した後は、初めから組み上がって立っている）。
    bool oncePerPlay = true;
    // 役者（骨格の動画で演じる体。模型と動画の部品を持つシーンの物体）と、付く前の部品の物体（兜・右の手甲・左の
    // 手甲。役者の子）。役者が無い・動画の機能が切ってある時は、登場をしない（初めから構えて立つ）。部品の物体の位置と
    // 向きは、シーンか演出の資産の物体の轨で決める（付く時は、コードが骨の所まで寄せる）。
    uint64_t actorObject = 0;
    uint64_t helmObject = 0;
    uint64_t gauntletRightObject = 0;
    uint64_t gauntletLeftObject = 0;
    // 演出の間だけ点ける灯（役者の子。置き場所・向き・明るさは、演出の資産の物体の轨がカットごとに決める）。0 なら使わない。
    uint64_t keyLightObject = 0;
    // 演出の資産（Sequence の GUID）。演技の時計・カメラ・片段・部品・効果の拍。空・流せない時は登場をしない。
    std::string sequence{"d66fc20a364f475699efb34fe4632336"};
    // 演技を流す速さの倍率（1 で資産のまま）。
    float playbackRate = 1.0f;
    // 演技が済んで戦いの体に替わった後、プレイヤーの入力を止めたまま待つ長さ (秒。ゲームのカメラへ戻り、肩に担いだ
    // 刀を待機の構えへ下ろす間)。
    float settleSeconds = 1.4f;
    // 一息の段に入ってから、役者を戦いの体に替えるまでの秒（settleSeconds より短く）。演技の後はカメラがゲームのカメラへ
    // 滑らかに引いて行くので、すぐ替えると、近くで体が入れ替わるのが見える。カメラが引いてから替える。0 ですぐ替える。
    float settleSwapSeconds = 0.8f;
    // 一息の段に入ってから、演出の灯をこの秒で 0 まで落とす（settleSwapSeconds より長ければ、替える時に 0 にそろえる）。
    // 灯を替える瞬間に急に消すと、ボスに当たる光が一気に変わって画が光ったように見え、動的な GI も作り直しで重くなる。
    float settleLightFadeSeconds = 0.7f;
    // 演出の動きのぼかし（c-bc5908）：演技の間、画面のカメラに掛ける動きのぼかし。シャッター角（度。180 で映画の
    // 標準、大きいほど長く流れる。0 で掛けない）と、ぼかしの長さの上限（1080p の画素）。一息の段では、ゲームのカメラへ
    // 渡すのに合わせて弱めて 0 にする。画質の設定の motionBlurQuality が Off なら出ない。カットの切り替わりでは
    // エンジンが前の画を捨てるので、切り替わりの画は流れない。既定は掛けない（SmokeArena の場面で 180°）。
    float motionBlurShutterDegrees = 0.0f;
    float motionBlurMaxPixels = 24.0f;
    // 部品が骨へ飛ぶ道：手甲は吸われる（初めゆっくり、最後に速い。この指数が大きいほど最後が速い）、途中で浮く高さ (m)。
    // 兜は煙に持ち上げられる（ゆっくり動き出してゆっくり止まる）、途中で外へふくらむ幅 (m)。
    float gauntletFlyPower = 2.6f;
    float gauntletFlyLift = 0.9f;
    float helmFlyBulge = 0.25f;
    // 確かめ用（正式シーンでは使わない）：0 以上なら、戦いの流れに関わらず、この tick に登場が始まる。
    int testStartFrames = -1;
    // 演出の後、ゲームのカメラへ渡す間にゲームのカメラが向くボスの高さ (m。戻り切った時、ボスを見ている)。
    float cameraFocusHeight = 4.0f;
    // 飛ばす：登場の間にこの入力（行動 id）を押すと、組み上がった待機の構えへ進む（空なら飛ばせない）。
    std::string skipAction = "Gameplay.Menu";

    // 登場の黒い煙（BossIntroSmoke）：コードからだけ煙を入れる流体の域（空なら煙を出さない）。量は「1 秒に入れる煙の総量」
    // （点の大きさや数を変えても、全体の濃さは変わらない）。
    HIKARI::GameProjectVfxAssetReference smokeEffect{"5c0e7a1b93d24f6e8a41b7d2c6f09e35",
                                                     "Assets/Vfx/BossIntroSmoke.hvfx"};
    // 地を走って鎧を起こす煙（拍 Smoke.Streak）：頭に付いて行く域のエフェクト（空なら出さない）、量の倍率、刀の向こう
    // 何 m から来るか、途中で左右へうねる幅 (m。鎧に近づくほど小さくなる)、鎧に着いた瞬間に舞台の域で鎧を包む煙の量。
    HIKARI::GameProjectVfxAssetReference streakEffect{"8f3b2c6d17a94e05b6d8c2a4e91f7350",
                                                      "Assets/Vfx/BossIntroStreak.hvfx"};
    float streakAmount = 1.0f;
    float streakLead = 30.0f;
    float streakSwerve = -1.2f;
    float wrapAmount = 6.0f;
    // 鎧を引き起こす煙（Smoke.Rise）：鎧の下の地面から、腰と胸を通って上へ抜ける柱の量。
    float riseAmount = 14.0f;
    // 腕の煙：手甲が付く前に肩から伸びる煙の腕（右 Smoke.Tendril・左 Smoke.TendrilLeft）の量、付いた後に鎧と手甲を
    // つなぐ細い煙の量。手甲が付いた瞬間の弾け（Hand.Right・Hand.Left）の量と、その時のカメラの揺れ。
    float tendrilAmount = 1.2f;
    float armAmount = 0.25f;
    float puffAmount = 4.0f;
    float attachShake = 0.3f;
    // 兜を持ち上げる煙の柱（Smoke.HelmLift）の量。
    float liftAmount = 0.6f;
    // 兜が頭に収まった瞬間（Helm.Seat。白黒の閃きと一緒に）：襟から噴く煙の量、衝撃の輪の大きさ（2 つ目は 1.4 倍）、
    // カメラの揺れの強さ、時間を止める長さ (F)。
    float seatBurstAmount = 10.0f;
    float seatShock = 2.6f;
    float seatShake = 1.2f;
    int seatHitStopFrames = 7;
    // 刀を握っている間（Sword.Grab）、根元から滲む煙の量。
    float seepAmount = 42.0f;
    // 刀を握っている間、画面の揺れが高まって行く強さ（握り終わりの強さ。0 で揺らさない）。
    float grabRumble = 0.55f;
    // 刀を握っている間、外から根元へ地面を這って集まって来る黒い霧の量。
    float gatherAmount = 26.0f;
    // 見得の黒い霧（抜刀の後から演技の終わりまで）：主役の後ろにゆっくり巻きながら立ち上がる壁と、両脇の地面を前へ
    // 這い出す霧の量。見得の間の、画の縁の締まり（全画面の効果。0 で出さない）。
    float backdropAmount = 90.0f;
    float finaleTension = 0.35f;
    // 見得の終わり（拍 Smoke.Disperse。音も衝撃も付けず、静かに晴れる）：黒い霧を主役から外へ流す加速 (m/秒²) と、
    // 場の霧を外へ押す速さ (m/秒)。演技の後も、ゲームのカメラへ渡す間、流しながら消し切る。
    float disperseBlow = 24.0f;
    float disperseFogPush = 2.5f;
    // 序幕の黒い煙（演技の前にプレイヤーを待つ間から、地を走る煙が来るまで。拍 Smoke.Ambient）：部品（鎧・兜・両の手甲・
    // 刀の根元）の周りを巡る黒い煙の筋の量（全部を合わせて 1 秒に入れる総量）、筋が走る速さ (m/秒。遅いと尾がつながって、
    // その場に溜まった塊に見える)、巡る輪の大きさ（部品ごとの半径に掛ける倍率）、煙の温度（上向きの加速 m/秒²。高いほど
    // 立ちのぼる。0 なら地面を這う）、輪を同じ向きへ回し続ける加速 (m/秒²)、部品の周りの場の霧を押しのける速さ
    // (m/秒。0 で押さない)。
    float ambientAmount = 18.0f;
    float ambientTurn = 2.2f;
    float ambientRing = 1.0f;
    float ambientHeat = 0.1f;
    float ambientStir = 0.8f;
    float ambientFogPush = 1.2f;
    // 筋の尾を薄める速さ (1/秒。大きいほど尾が短い。0 だと尾がつながって、丸い塊に溜まる)。
    float ambientFade = 1.2f;
    // 前触れ（拍 Omen）：煙の筋が自分の部品を離れて鎧の周りへ寄り、渦になる。その渦を回す加速 (m/秒²。拍の終わり)、
    // 画が暗くなる強さと縁が締まる強さ（全画面の効果。0 で出さない）、低く続く画面の揺れの強さ（拍の終わり。揺れの
    // 速さはボスの部品の introRumbleFrequency）。
    float omenPull = 4.0f;
    float omenDim = 0.5f;
    float omenTension = 0.45f;
    float omenRumble = 0.3f;
    // 落ちている兜の、面頬の隙間の瞬き（拍 Helm.Glint）：明るさ（画面基準。0 で出さない）。
    float helmGlint = 2.4f;
    // 刀身の光（登場の間だけ）：振り上げの頂点で剣先に出る星の明るさ（全画面の効果。0 で出さない）。刃口の細い線の
    // 明るさは、ボスの部品の introBladeEdgeGlow。
    float bladeStar = 1.0f;
    // 全画面の効果（BossImpactMono の衝撃と張りつめ。白黒の閃きと同じ効果の資産）：握っている間に高まる張りつめの強さ
    // （縁の減光と色ずれ。0 で出さない）、抜けた瞬間に根元から画の上を走る衝撃の輪の強さ（0 で出さない）と、輪が画面を
    // 走り切る長さ (秒。実時間)。輪の幅・歪み・ぼかし・色ずれは、資産 BossImpactMono.hposteffect の項目。
    float grabTension = 0.7f;
    float drawShockPost = 1.0f;
    float drawShockPostSeconds = 1.1f;
    // 目の炎（既定では出さない。目は兜の中の自発光の模型 BossHelmEyes で見せる：BossBodySettings の helmEyesObject）：
    // 目が灯ってから演技の終わりまで、面頬の左右の隙間から立つ青白い炎。兜の前に付いて動く細かい流体の域の
    // エフェクト（空なら出さない）と、量（片目の、1 秒に入れる煙の総量。目が灯る拍の初めは 4 倍。0 で出さない）。
    HIKARI::GameProjectVfxAssetReference eyeFireEffect{"b7e94c21d05a4f38a6c1e3f9027d5b84",
                                                       "Assets/Vfx/BossEyeFire.hvfx"};
    float eyeFireAmount = 0.0f;
    // 刀が地から抜けた瞬間（Sword.Out。白黒の閃きと一緒に）：噴き上げる煙の量と噴く長さ (秒)、柱が上がる速さ
    // (m/秒)、場の霧を外へ押す速さ (m/秒) と届く半径 (m)、刀身が引く煙の量、衝撃の輪の大きさ
    // （2 つ目は 1.4 倍）、カメラの揺れの強さ、時間を止める長さ (F)。
    float drawBurstAmount = 115.0f;
    float drawBurstSeconds = 1.4f;
    float drawJetSpeed = 4.0f;
    float drawPush = 3.0f;
    float drawPushRadius = 5.0f;
    float drawTrailAmount = 18.0f;
    float drawShock = 3.8f;
    float drawShake = 1.6f;
    int drawHitStopFrames = 9;
    // 刀が抜けた瞬間の停止の後、噴き上がる間のスローの長さ (F。間の時間の倍率は drawSlowScale。0 でスローにしない)、
    // 根元で弾ける閃光の大きさ（0 で出さない）。
    int drawSlowFrames = 0;
    float drawFlashScale = 3.2f;
    // 刀へ歩く一歩（拍 Step。動きの記録の足が着く時刻に置いてある）：カメラの揺れの強さと、足元に立つ煙の量。
    float stepShake = 0.32f;
    float stepAmount = 2.5f;
    // 煙を消す（Smoke.Clear と演技の後）：消す速さ (1/秒) と、演技の後に消し続けてから域を止めるまでの長さ (秒)。
    float clearAbsorption = 6.0f;
    float smokeLingerSeconds = 1.5f;
};

// 効果の拍（演出の資産の事件段）の種類。名前は読んだ後に 1 回だけ見分ける（毎フレーム文字を比べない）。
enum class BossIntroCueKind : uint8_t {
    Unread,
    Other,
    HelmSeat,
    SwordOut,
    Eyes,
    SmokeStreak,
    SmokeRise,
    SmokeTendril,
    SmokeTendrilLeft,
    HandRight,
    HandLeft,
    HelmLift,
    SwordGrab,
    SmokeClear,
    Step,
    SmokeDisperse,
    SmokeAmbient,
    Omen,
    HelmGlint
};

// 役者（BossIntroActor）が使う事件段の役：片段を流す（Anim.<片段>）、部品が骨へ飛ぶ（Part.<部品>.Fly）、落ちている部品が
// 痙攣する（Part.<部品>.Twitch）。
enum class BossIntroCueRole : uint8_t { None, Anim, PartFly, PartTwitch };

// 効果の拍 1 つ：事件段（名前・始め・長さ）と、入った・出たか。役者が使う段は、役と部品の番号（BossIntroPart。無ければ負）。
struct BossIntroCue {
    HIKARI::GameProjectSequenceEventClip clip{};
    BossIntroCueKind kind = BossIntroCueKind::Unread;
    bool entered = false;
    bool left = false;
    BossIntroCueRole role = BossIntroCueRole::None;
    int8_t part = -1;
};

// 登場の実行時の状態（保存しない）。
struct BossIntroState {
    BossIntroPhase phase = BossIntroPhase::Done;
    // いまの段に入ってからの F（固定 tick）。
    int frame = 0;
    // 舞台の枠：登場を始めた時のボスの足元と向き（Initialize が入れ、演技の前にプレイヤーが見えた tick に 1 度だけ
    // プレイヤーの側へ向ける）。演技の間、ボスの物体はここに止める（演出の資産のカメラは物体に付く）。
    HIKARI::GameProjectFloat3 origin{};
    float yaw = 0.0f;
    // 戦いの流れ（Flow）に止められたのを見たか（止められる前の最初の数 tick で始めない）。
    bool heldSeen = false;
    // 舞台の枠を、プレイヤーのいる側へ向けたか。
    bool faced = false;
    // 登場が始まった。
    bool started = false;
    // 役者と部品（BossIntroActor）。
    BossIntroActorState actor;
    // 演技の段の長さ (固定 tick。演出の資産の長さから)。
    int showTicks = 0;
    // 演出の資産：流している手綱（0 は流していない・断られた）。
    HIKARI::GameProjectSequencePlaybackHandle sequence{};
    // 見た目：最後に見た資産の時刻、その時刻のまま進まない秒、進まないので自分の時計に切り替えたか。
    float sequenceSeen = -1.0f;
    float sequenceStall = 0.0f;
    bool sequenceStalled = false;
    // 効果の拍（読んだ数だけ使う。入れ物は Begin で確保する）。役者が使う段を見分けたか、役者と部品を隠したか。
    std::vector<BossIntroCue> cues;
    int cueCount = 0;
    bool cueRolesRead = false;
    bool actorHidden = false;
    // 演技の最後に役者が担いでいた刀の置き場所（体に付く効果の置き場所の約束）。一息の段で、刀の効果をここから
    // 待機の構えへ移す。
    bool carryValid = false;
    HIKARI::GameProjectTransform carrySword{};
    float settleClock = 0.0f;
    // 見た目（BossIntroVisuals）：前に見た段、演技の時計（表の秒。描画のフレームで進める）、一息の段の秒、飛ばしたのを
    // 見たか、目が灯ったか（Eyes の拍から）。
    BossIntroPhase seenPhase = BossIntroPhase::Done;
    float showSeconds = 0.0f;
    float settleSeconds = 0.0f;
    bool skipSeen = false;
    bool eyesLit = false;
    // 登場の黒い煙（BossIntroSmoke）：流体の域の再生、演技の後に消し続けた秒、外へ 1 回押す頼み（兜が収まった・刀が抜けた）。
    HIKARI::GameProjectVfxPlaybackHandle smokeHandle{};
    float smokeLinger = 0.0f;
    // 地を走る煙の域：再生、道の始めと終わり（世界。拍の始めに決める）、拍の後に置いておいた秒。
    HIKARI::GameProjectVfxPlaybackHandle streakHandle{};
    HIKARI::GameProjectVfxPlaybackHandle eyeFireHandle{};
    bool streakSet = false;
    HIKARI::GameProjectFloat3 streakFrom{};
    HIKARI::GameProjectFloat3 streakTo{};
    float streakFade = 0.0f;
    bool smokeSeatPush = false;
    bool smokeDrawPush = false;
    // 刀身が引く煙：刀身を追いかける点の位置（世界）、もう置いたか、前に進めた時の演技の時刻 (秒)。
    std::array<HIKARI::GameProjectFloat3, 5> smokeTrail{};
    bool smokeTrailSet = false;
    float smokeTrailAt = 0.0f;
    // 編集器のプレビュー（Play の外）：流しているか、拍を読んだプレビューの手綱。
    bool previewing = false;
    HIKARI::GameProjectSequencePlaybackHandle previewHandle{};
    // 全画面の衝撃の輪（刀が抜けた瞬間）：始まってからの秒（負なら出していない）と、中心（世界）。
    float shockSeconds = -1.0f;
    HIKARI::GameProjectFloat3 shockAt{};
    // 剣先の星（全画面の効果）：始まってからの秒（負なら出していない）。
    float starSeconds = -1.0f;
    // 見得の終わり：霧を外へ流し始める 1 回の押しを頼んだか。
    bool dispersePush = false;
    // 序幕の黒い霧：点を巡らせる時計（秒。演技の前から進む。演技の時計は、始まるまで 0 のまま）。
    float ambientClock = 0.0f;
    // 落ちている兜の隙間の瞬き：頼まれた強さ（このフレームだけ。0〜1）。
    float helmGlint = 0.0f;
    // 演出の資産が流れている間の、最後に見たカメラの姿勢（演技の後、ここからゲームのカメラへ滑らかに渡す）。
    bool filmValid = false;
    HIKARI::GameProjectFloat3 filmEye{};
    HIKARI::GameProjectFloat3 filmTarget{};
    float filmFov = 1.0f;
    // 飛ばす：押された（次の tick に進める）・飛ばした。
    bool skipRequested = false;
    bool skipped = false;
    // 演出の動きのぼかしを頼んで受け付けられたか（変わった時だけ記録に書く）。
    bool motionBlurAccepted = false;
};

} // namespace Project_TD2_2
