#pragma once

#include <array>
#include <cstdint>

#include "HIKARI/Extend/World.h"
#include "Feedback.h"

namespace Project_TD2_2 {

// 新しいゲーム層の型名（TD2_2.Player）。旧い System をこの部品に結ぶのに使う。
inline constexpr std::string_view kPlayerType = "TD2_2.Player";
inline constexpr std::string_view kPlayerSystem = "Project_TD2_2.PlayerSystem";
// ゲーム全体の値（Params）の名前。UI の型紙の変数名と同じ（HUD が読む）。
inline constexpr std::string_view kPlayerHealthVariable = "Player.Health";
inline constexpr std::string_view kPlayerMaxHealthVariable = "Player.MaxHealth";
inline constexpr std::string_view kPlayerStaminaVariable = "Player.Stamina";
inline constexpr std::string_view kPlayerMaxStaminaVariable = "Player.MaxStamina";
inline constexpr std::string_view kPlayerHealChargesVariable = "Player.HealCharges";
inline constexpr std::string_view kPlayerStateVariable = "Player.State";
// 炎上の残り秒数（燃えていなければ 0）。HUD が HP の条の色を変えるのに読む。
inline constexpr std::string_view kPlayerBurnVariable = "Player.Burn";

// 番号は Params の Player.State の値。足す時は末尾へ（Ride＝8 は筋斗雲に乗っている間。動きは CloudMountSystem が出す）。
enum class PlayerState { Normal, Roll, Heal, Stagger, Knockback, Down, GetUp, Dead, Ride };
HIKARI_ENUM(PlayerState, Normal, Roll, Heal, Stagger, Knockback, Down, GetUp, Dead, Ride)
// 食らった時の体の反応（のけぞり・吹っ飛び）。ダメージの種類（PlayerDamageType）とは別に選ぶ。
enum class PlayerDamageKind { Stagger, Knockback };
HIKARI_ENUM(PlayerDamageKind, Stagger, Knockback)
// ダメージの種類。ログと、炎上の起こり方・見た目（次のカードの炎）を分けるために持つ。
// Slash＝刃・斬撃・剣が当たった、Burn＝燃えている間に続けて減る（炎上・燃える地面）、Explosion＝爆発が当たった。
enum class PlayerDamageType { Slash, Burn, Explosion };
// 炎上が付いたきっかけ（ログ用）。Ground＝燃える地面に立ち続けた、Slash＝火の刃が当たった、Explosion＝爆発が当たった。
enum class PlayerBurnCause { Ground, Slash, Explosion };
// 炎上が消えた理由。None は一度も消えていない。
enum class PlayerBurnEnd { None, Roll, Absorb, Timeout, Death };
// 外部の動作（砲撃）が、その固定 tick のローリングをどう扱うか。
// Hold は押されたローリングを消さずに待たせ、Cancel は動作ロック中でもローリングで打ち切らせる。
enum class PlayerRollGate { None, Hold, Cancel };
// 外部（筋斗雲など）が PlayerRules::RequestCamera で出す、1 描画フレームぶんのカメラの要求。
// 要求が無いフレームは Player の「カメラ」の値をそのまま使う。角度は度。姿勢を出すのは PlayerCamera だけで、
// 要求は値を渡すだけ（距離・画角・傾き・見下ろしのならしも PlayerCamera が持つ）。負の値は「Player の値のまま」。
struct PlayerCameraRequest {
    // 上下に追うならし（秒）と、上下を追わない幅 (m)。
    float verticalSeconds = 0.35f;
    float verticalDeadZone = 0.5f;
    // 見上げ（負）・見下ろしの限度。
    float minPitch = -15.0f;
    float maxPitch = 55.0f;
    // 水平に追うならし（秒）。
    float followSeconds = -1.0f;
    // 軸から視点までの距離 (m)、縦の画角、見上げた時に視点が軸から下がれる限度 (m)。
    float distance = -1.0f;
    float fovDegrees = -1.0f;
    float eyeFloor = -1.0f;
    // ならさずに足す画角（ブーストの出だしの行き過ぎ）。
    float fovKickDegrees = 0.0f;
    // 傾き（右へ傾くのが正）と、見せるだけの見下ろし（移動の向きには入れない）。
    float rollDegrees = 0.0f;
    float lookDownDegrees = 0.0f;
    // ならし（秒）：距離・見下ろし・視点の下限、画角、傾き。
    float shapeSeconds = 0.35f;
    float fovSeconds = 0.35f;
    float rollSeconds = 0.25f;
    // 要求が終わった後、距離・画角・傾き・見下ろしと、限度の外の見上げ・見下ろしを Player の値へ戻すならし（秒）。
    float releaseSeconds = 0.4f;
};
// 要求で変わる見せ方の、いまの値（PlayerCamera が持つ）。角度は rad。
struct PlayerCameraShape {
    float distance = 0.0f;
    float fovRadians = 0.0f;
    float eyeFloor = 0.0f;
    float rollRadians = 0.0f;
    float lookDownRadians = 0.0f;
};
// 演出のカメラ（ボスの登場）が入れる、1 描画フレームぶんの姿勢。PlayerCamera は、ゲームのカメラの姿勢と gameWeight
// で混ぜて出す（1 でゲームのカメラそのもの）。使ったら消す（入れた側が消えても残さない）。
struct PlayerCinematicRequest {
    HIKARI::GameProjectFloat3 eye{};
    HIKARI::GameProjectFloat3 target{0.0f, 0.0f, 1.0f};
    float fovRadians = 1.0f;
    float gameWeight = 0.0f;
    // 演出の間、ゲームのカメラが向いておく点（ロックオンしていない時。戻った時にそちらを見ている）。
    HIKARI::GameProjectFloat3 focus{};
};
// 照準補助の候補（ボスの側が渡す）を置いておける数。
inline constexpr size_t kPlayerLockCandidateCapacity = 8;

// 外部の動作（ヒョウタンの吸い込み・砲撃）が、PlayerRules を通して出す 1 固定 tick ぶんの要求。
// 外部は自分の PreFixedUpdate（PlayerSystem より後）で毎 tick 入れ直し、PlayerSystem は次の tick の PreFixedUpdate で
// 使い終えた所で全部消す（外部のシステムが消えても要求を残さない）。
// 歩く倍率・向き直り・歩ける・ローリングを待たせるは同じ tick の StepPlayer が、回復の停止とローリングの扱いは
// 次の tick の先行入力（StartBufferedAction）が使う。
struct PlayerExternalRequests {
    float moveScale = 1.0f;
    // 向きを入力の方向へ回す速さ（度/秒。0 は普段の向き直り）。
    float turnSpeed = 0.0f;
    // 動作ロック中でも moveScale で歩ける（砲撃の構え・後隙の後半）。
    bool walkAllowed = false;
    bool healBlocked = false;
    PlayerRollGate rollGate = PlayerRollGate::None;
};

// プレイヤー（新しいゲーム層の部品）。調整値は FIELD、規則は PlayerRules、進めるのは PlayerSystem。
struct Player : HIKARI::Component {
    HIKARI_COMPONENT(Player)

    FIELD(float, maxHealth, 100.0f, "最大 HP", HIKARI::Range(1.0f, 10000.0f), HIKARI::Group("移動と体力"))
    // ふだんの歩き。走る（Gameplay.Sprint を押している間）は sprintSpeed（c-ff3e98。元は歩き 5.2、走りは無し）。
    FIELD(float, moveSpeed, 3.8f, "歩く速さ (m/s)", HIKARI::Range(0.0f, 50.0f))
    FIELD(float, sprintSpeed, 5.2f, "走る速さ (m/s、Shift／L3 を押している間)", HIKARI::Range(0.0f, 50.0f))
    // 走っている間に使う精力（/秒）。走っている間は回復せず、止めてから staminaDelayFrames 待って回復する。
    FIELD(float, sprintStaminaPerSecond, 12.0f, "走っている間のスタミナ消費 (/s)", HIKARI::Range(0.0f, 1000.0f),
          HIKARI::Tip("走っている間はスタミナが回復しません。尽きたら、走る入力を離して押し直すまで歩きます。"
                      "ロックオン中に走ると、体は進む向きを向きます。砲撃・吸い込み・回復の間は走りません。"))
    // 通常・回復中に向きを変える速さ。試作の 12 rad/秒。
    FIELD(float, turnSpeed, 688.0f, "振り向きの速さ (度/s、688 ≒ 12 rad/s)", HIKARI::Range(1.0f, 5000.0f))
    FIELD(float, maxStamina, 100.0f, "最大スタミナ", HIKARI::Range(1.0f, 10000.0f))
    FIELD(int, staminaDelayFrames, 40, "スタミナ回復待ち (F)", HIKARI::Range(0, 600))
    FIELD(float, staminaPerSecond, 45.0f, "スタミナ回復 (/s)", HIKARI::Range(0.0f, 1000.0f))
    FIELD(float, maximumFallSpeed, 55.0f, "落下速度上限 (m/s)", HIKARI::Range(0.0f, 200.0f))
    FIELD(int, inputBufferFrames, 12, "先行入力 (F)", HIKARI::Range(1, 60))
    // 歩き出し・止まる時に速さを変える時間（0 で一瞬）。回避・被弾などの動作には掛からない。
    FIELD(float, moveAccelSeconds, 0.08f, "歩き出しで最高速になるまで (秒、0 で一瞬)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, moveDecelSeconds, 0.06f, "入力を離して止まるまで (秒、0 で一瞬)", HIKARI::Range(0.0f, 2.0f))
    // 立ち止まっていても滑り落ちない坂の傾きの上限（度）。これより急な坂では、今までどおり重力で滑る。
    FIELD(float, standSlopeDegrees, 30.0f, "立ち止まっても滑らない坂の傾き (度、これより急なら滑る)",
          HIKARI::Range(0.0f, 89.0f),
          HIKARI::Tip("入力が無く止まっている間だけ、この傾きまでの地面の上では重力で坂を滑り落ちません。"
                      "歩き・ローリング・被弾・押されて動く時の重力はそのままです。"))
    // 終わり際の硬直を短くして次の行動へ早く移れるようにする（c-a0a892。元の値は調整ガイド）。
    FIELD(int, rollFrames, 26, "全体 (F)", HIKARI::Range(1, 600), HIKARI::Group("ローリング"),
          HIKARI::Tip("移動 F は全体 F までに制限します。無敵は 0F から指定した F 数です。"))
    FIELD(int, rollMoveFrames, 22, "移動 (F)", HIKARI::Range(1, 600))
    FIELD(float, rollDistance, 4.2f, "距離 (m)", HIKARI::Range(0.0f, 30.0f))
    // 進んだ距離 = rollDistance × (1 − (1 − 進み)^rollEasePower)。1 で等速、大きいほど出だしが速い。
    FIELD(
        float, rollEasePower, 1.6f, "出だしの速さ（1 は等速、大きいほど初め速くあとゆっくり）",
        HIKARI::Range(1.0f, 5.0f),
        HIKARI::Tip("方向入力が無いとき、ロックオン中は対象から離れる向き、そうでなければ今の向きの後ろへ転がります。"))
    FIELD(int, rollInvincibleFrames, 14, "無敵 (F)", HIKARI::Range(0, 600))
    // この経過 F 以降（移動の後）は、砲撃・吸い込みでローリングの残りを打ち切れる。
    FIELD(int, rollActionCancelFrame, 22, "砲撃・吸い込みで打ち切れる最初の経過 F", HIKARI::Range(0, 600),
          HIKARI::Tip("打ち切れる F より前に押した砲撃・吸い込みは、それぞれの先行入力の長さだけ覚えておき、"
                      "打ち切れる F に始まります。"))
    FIELD(float, rollStamina, 25.0f, "スタミナ消費", HIKARI::Range(0.0f, 1000.0f))
    FIELD(float, rollFogRadius, 1.0f, "霧を押す半径 (m)", HIKARI::Range(0.01f, 10.0f))
    FIELD(float, rollFogStrength, 3.0f, "霧を押す強さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, rollFogHeightOffset, -0.45f, "霧の中心高さ補正 (m)", HIKARI::Range(-5.0f, 5.0f))
    // ローリングの風が場の火を押す強さ（霧を押す強さに対する割合。0 で押さない）。
    FIELD(float, rollFireShare, 0.6f, "場の火を押す強さ (霧を押す強さに対する割合。0 で押さない)",
          HIKARI::Range(0.0f, 4.0f))
    // 場の火が体をよける大きさ（火の域にだけ効く障害物。半径 0 でよけない）。
    FIELD(
        float, fireObstacleRadius, 0.4f, "火が体をよける半径 (m。0 でよけない)", HIKARI::Range(0.0f, 5.0f),
        HIKARI::Group("場の火"),
        HIKARI::Tip(
            "場の火（燃える地面）が体をよけて割れ、体の動きに引かれます。霧の側は FluidObstacleComponent が受け持ちます。"))
    FIELD(float, fireObstacleHeight, 1.7f, "火が体をよける高さ (m)", HIKARI::Range(0.0f, 10.0f))
    FIELD(int, staggerFrames, 26, "のけぞり全体 (F)", HIKARI::Range(3, 600), HIKARI::Group("被弾"),
          HIKARI::Tip("三歩の時間は全体 F の中に収めます。三歩目は残りの F 数です。"))
    FIELD(int, staggerStepOneFrames, 9, "一歩目 (F)", HIKARI::Range(1, 600))
    FIELD(int, staggerStepTwoFrames, 9, "二歩目 (F)", HIKARI::Range(1, 600))
    FIELD(float, staggerStepOne, 0.75f, "一歩目の距離 (m)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, staggerStepTwo, 0.3f, "二歩目の距離 (m)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, staggerStepThree, 0.12f, "三歩目の距離 (m)", HIKARI::Range(0.0f, 10.0f))
    FIELD(int, staggerHitStopFrames, 5, "のけぞりヒットストップ (F)", HIKARI::Range(0, 60))
    FIELD(float, knockbackDistance, 6.0f, "吹っ飛び距離 (m)", HIKARI::Range(0.0f, 30.0f),
          HIKARI::Tip(
              "吹っ飛びの距離は、ボスの技の表の「吹っ飛びの距離 m」が空ならこの値です。総距離と F は変わりません。"))
    FIELD(int, knockbackFrames, 30, "吹っ飛び (F)", HIKARI::Range(1, 600))
    // 進んだ距離 = knockbackDistance × (1 − (1 − 進み)^knockbackEasePower)。ローリングと同じ形で、前が速く後ろが遅い。
    FIELD(float, knockbackEasePower, 2.2f, "吹っ飛びの出だしの速さ（1 は等速、大きいほど前が速く後ろがゆっくり）",
          HIKARI::Range(1.0f, 5.0f))
    // 吹っ飛びの放物線の頂点の高さ。knockbackApexAt（飛行の割合）で頂点に届き、knockbackLandAt で着地して、
    // 残りは地面を滑る。
    FIELD(float, knockbackHeight, 0.8f, "吹っ飛びの放物線の高さ (m、0 は地面を滑る)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, knockbackApexAt, 0.2f, "頂点に届く時（飛行の割合）", HIKARI::Range(0.05f, 0.9f))
    FIELD(float, knockbackLandAt, 0.55f, "着地する時（飛行の割合。以降は残りを地面で滑る）", HIKARI::Range(0.1f, 1.0f))
    FIELD(int, knockbackHitStopFrames, 8, "吹っ飛びヒットストップ (F)", HIKARI::Range(0, 60))
    // 着地の重み：小さなカメラの揺れ・手柄の振動・足元の霧を外へ押す一発。
    FIELD(float, knockbackLandShake, 0.06f, "着地のカメラ揺れ幅 (m、0 は無効)", HIKARI::Range(0.0f, 1.0f))
    FIELD(int, knockbackLandShakeFrames, 6, "着地のカメラ揺れ・振動の時間 (F)", HIKARI::Range(0, 120))
    FIELD(float, knockbackLandRumble, 0.5f, "着地の振動の強さ (0〜1、0 は無効)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, knockbackLandPushRadius, 1.6f, "着地で足元の霧を外へ押す半径 (m)", HIKARI::Range(0.01f, 10.0f))
    FIELD(float, knockbackLandPush, 5.0f, "着地で霧を外へ押す強さ (m/s、0 は無効)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, knockbackLandPushHeightOffset, -0.45f, "着地の霧の押しの中心高さ補正 (m)", HIKARI::Range(-5.0f, 5.0f))
    // 吹っ飛ばされた時にカメラを少し引いて（画角を広げて）戻す。
    FIELD(float, knockbackCameraFov, 4.0f, "吹っ飛びでカメラを引く画角 (度、0 は無効)", HIKARI::Range(0.0f, 30.0f))
    FIELD(int, knockbackCameraFovFrames, 18, "引いたカメラが戻るまで (F)", HIKARI::Range(0, 120))
    // 食らった時のカメラの揺れと振動（c-d330c0。止めは上の値のまま）。のけぞりは小さく短く、吹っ飛びは大きく長く。
    FIELD(float, hitCameraAmplitudeStagger, 0.05f, "のけぞりのカメラ揺れ幅 (m、0 は無効)", HIKARI::Range(0.0f, 1.0f))
    FIELD(int, hitCameraFramesStagger, 8, "のけぞりのカメラ揺れ時間 (F)", HIKARI::Range(0, 120))
    FIELD(float, hitCameraAmplitudeKnockback, 0.12f, "吹っ飛びのカメラ揺れ幅 (m、0 は無効)", HIKARI::Range(0.0f, 1.0f))
    FIELD(int, hitCameraFramesKnockback, 14, "吹っ飛びのカメラ揺れ時間 (F)", HIKARI::Range(0, 120))
    FIELD(float, hitCameraFrequency, 22.0f, "被弾のカメラ揺れ周波数 (Hz)", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, hitRumbleStagger, 0.5f, "のけぞりの振動の強さ (0〜1、0 は無効)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, hitRumbleKnockback, 0.9f, "吹っ飛びの振動の強さ (0〜1、0 は無効)", HIKARI::Range(0.0f, 1.0f))
    FIELD(int, hitRumbleFrames, 10, "被弾の振動時間 (F、0 は無効)", HIKARI::Range(0, 120))
    // 動きのぼかし（c-e0a9c3 の続き）：戦いの間はふだんから少し掛け（カメラの振り・ボスの速い動き・斬撃が流れて見える）、
    // 食らった瞬間と回避の出だしで一瞬だけ強める。強さはシャッター角 (度) と、ぼかしの長さの上限 (1080p の画素)。
    // 演出のカメラの間は、ふだんの分を出さない（演出は自分のぼかしを持つ）。
    FIELD(float, combatBlurShutter, 120.0f, "戦いの間の動きのぼかし：シャッター角 (度、0 は無効)",
          HIKARI::Range(0.0f, 360.0f))
    FIELD(float, combatBlurPixels, 12.0f, "…ぼかしの長さの上限 (1080p の画素、0 は無効)", HIKARI::Range(0.0f, 64.0f))
    FIELD(float, hitBlurShutter, 300.0f, "食らった瞬間の動きのぼかし：シャッター角 (度、0 は無効)",
          HIKARI::Range(0.0f, 360.0f))
    FIELD(float, hitBlurPixels, 36.0f, "…ぼかしの長さの上限 (1080p の画素)", HIKARI::Range(0.0f, 64.0f))
    FIELD(int, hitBlurFrames, 12, "…ぼかしが戻るまで (F)", HIKARI::Range(0, 120))
    FIELD(float, rollCameraFov, 2.5f, "回避の出だしでカメラを引く画角 (度、0 は無効)", HIKARI::Range(0.0f, 30.0f))
    FIELD(int, rollCameraFovFrames, 16, "…引いたカメラが戻るまで (F)", HIKARI::Range(0, 120))
    FIELD(float, rollBlurShutter, 220.0f, "回避の出だしの動きのぼかし：シャッター角 (度、0 は無効)",
          HIKARI::Range(0.0f, 360.0f))
    FIELD(float, rollBlurPixels, 24.0f, "…ぼかしの長さの上限 (1080p の画素)", HIKARI::Range(0.0f, 64.0f))
    FIELD(int, rollBlurFrames, 14, "…ぼかしが戻るまで (F)", HIKARI::Range(0, 120))
    FIELD(int, downFrames, 40, "ダウン (F)", HIKARI::Range(1, 600))
    FIELD(int, downRollFrame, 14, "ダウン回避開始 (F)", HIKARI::Range(0, 600))
    FIELD(int, getUpFrames, 22, "起き上がり (F)", HIKARI::Range(1, 600))
    // 起き上がりのこの F 目から、ローリングで起き上がりを打ち切って転がれる（0 は起き上がり始めから）。
    FIELD(int, getUpRollFrame, 0, "起き上がり回避開始 (F。ここからローリングで打ち切れる)", HIKARI::Range(0, 600))
    // 炎上：付いてから消えるまでの秒数と、燃える地面の外で減る HP（/秒）。燃える地面の上では BurnField の値で減る。
    FIELD(float, burnSeconds, 4.0f, "燃えている長さ (秒)", HIKARI::Range(0.1f, 30.0f), HIKARI::Group("炎上"),
          HIKARI::Tip(
              "燃える地面に立ち続ける・火の刃や爆発を食らうと燃え、HP が少しずつ減ります（のけぞり・ヒットストップ・"
              "揺れは起こしません。吹っ飛び・ダウン中も減ります）。ローリング・吸い込みで消え、時間が来ても消えます。"
              "燃える地面の上にいる間は、その地面の減り方で減り、残りの秒数が満タンに戻ります。"))
    FIELD(float, burnDamagePerSecond, 6.0f, "燃える地面の外で減る HP (/秒)", HIKARI::Range(0.0f, 1000.0f))
    FIELD(int, maxHealCharges, 4, "回復回数", HIKARI::Range(0, 99), HIKARI::Group("回復"),
          HIKARI::Tip("回復発生は全体 F までに制限します。使用回数は動作開始時に消費します。"))
    FIELD(float, healAmount, 40.0f, "HP 回復量", HIKARI::Range(0.0f, 10000.0f))
    FIELD(int, healFrames, 60, "回復動作全体 (F)", HIKARI::Range(1, 600))
    FIELD(int, healApplyFrame, 36, "HP 回復発生 (F)", HIKARI::Range(1, 600))
    FIELD(float, healMoveSpeed, 1.5f, "回復中の移動 (m/s)", HIKARI::Range(0.0f, 50.0f))
    FIELD(HIKARI::Entity, lockTarget, {}, "ロック対象", HIKARI::Group("ロックオン"))
    FIELD(HIKARI::Entity, camera, {}, "ゲームカメラ",
          HIKARI::Tip("ロックオン中、カメラは対象の向きへ回ります。回る速さに上限があるので、"
                      "対象が背後へ現れても振り回されません。対象との距離に合わせて、近い時は少し引いて上げ、見上げを"
                      "足します（大きいボスの頭と振りかぶった刀が入るように）。遠い時は前に近い見せ方です。"))
    // ロックオン中のカメラ。対象の向きへ lockYawSeconds でならして回り、回る速さは lockYawMaxSpeed まで。
    FIELD(float, lockYawSeconds, 0.16f, "対象の向きへ回るならし (秒、小さいほど素早く追う)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, lockYawMaxSpeed, 300.0f, "回る速さの上限 (度/s)", HIKARI::Range(10.0f, 2000.0f))
    // 遠い時の見下ろし角（度）。近い時は lockPitchNear（c-880340）。演出の向く点を追う間（ロックしていない）もこの値。
    FIELD(float, lockPitch, 17.0f, "見下ろし角度：遠い時 (度)", HIKARI::Range(-80.0f, 80.0f))
    // 対象が高い所にいる時に見上げる割合（0 で見上げない、1 で対象の高さの角度だけ）。
    FIELD(float, lockPitchFollow, 0.6f, "対象が高い時に見上げる割合 (0 で見上げない)", HIKARI::Range(0.0f, 1.5f))
    FIELD(float, lockPitchSeconds, 0.28f, "見上げ・見下ろしのならし (秒)", HIKARI::Range(0.0f, 2.0f))
    // ロック中の見上げの限度（度、負で見上げ。ボスの跳躍で高い所にいる時）。
    FIELD(float, lockMinPitch, -10.0f, "見上げの限度 (度、負で見上げ。跳躍で高い時)", HIKARI::Range(-89.0f, 89.0f))
    // 見る点の高さ：対象の中心から、体の高さの半分に対するこの割合だけ上（0.35 で胸のあたり）。照準も同じ点。
    FIELD(float, lockAimHeightShare, 0.35f, "見る点の高さ（対象の中心から、体の高さの半分に対する割合。照準も同じ点）",
          HIKARI::Range(-1.0f, 1.0f))
    // c-880340：距離に合わせた見せ方。対象との水平の距離が lockNearDistance 以下で「近い」の値、lockFarDistance 以上で
    // 「遠い」の値、その間はなめらかに（smoothstep）混ぜる。混ぜる割合そのものも lockFramingSeconds でならす。
    FIELD(float, lockNearDistance, 3.0f, "近い：この距離まで (m)", HIKARI::Range(0.0f, 50.0f),
          HIKARI::Group("ロックオン：距離に合わせた見せ方"),
          HIKARI::Tip(
              "対象との水平の距離が「近い」以下で近い時の値、「遠い」以上で遠い時の値、その間はなめらかに混ぜます。"
              "見上げは視点を動かさず、見る向きだけを上げます（プレイヤーが画面の下の方に残ります）。"))
    FIELD(float, lockFarDistance, 15.0f, "遠い：この距離から (m)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, lockFramingSeconds, 0.8f, "見せ方が変わるならし (秒)", HIKARI::Range(0.0f, 3.0f))
    // 軸から視点までの距離 (m)・軸を上げる高さ (m)・見せるだけの見上げ（視点はそのまま見る向きだけ上げる。度）。
    FIELD(float, lockCameraDistanceNear, 7.5f, "軸から視点までの距離：近い (m)", HIKARI::Range(0.5f, 30.0f))
    FIELD(float, lockCameraDistanceFar, 6.5f, "軸から視点までの距離：遠い (m)", HIKARI::Range(0.5f, 30.0f))
    FIELD(float, lockPivotRiseNear, 0.9f, "軸を上げる高さ：近い (m)", HIKARI::Range(-3.0f, 5.0f))
    FIELD(float, lockPivotRiseFar, 0.3f, "軸を上げる高さ：遠い (m)", HIKARI::Range(-3.0f, 5.0f))
    FIELD(float, lockPitchNear, 12.0f, "見下ろし角度：近い (度)", HIKARI::Range(-80.0f, 80.0f))
    FIELD(float, lockLookUpNear, 6.0f, "見る向きだけの見上げ：近い (度)", HIKARI::Range(-30.0f, 30.0f))
    FIELD(float, lockLookUpFar, 4.0f, "見る向きだけの見上げ：遠い (度)", HIKARI::Range(-30.0f, 30.0f))
    // 近くで回り過ぎない：対象が lockTurnNearDistance 以下で「近い」のならし・速さの上限、lockTurnFarDistance 以上で
    // lockYawSeconds・lockYawMaxSpeed。その間はなめらかに混ぜる。
    FIELD(float, lockTurnNearDistance, 2.5f, "近い：この距離まで (m)", HIKARI::Range(0.0f, 50.0f),
          HIKARI::Group("ロックオン：近くで回り過ぎない・消えた時"),
          HIKARI::Tip(
              "対象が近いほど向きが速く変わるので、近い時は追うならしを長く、回る速さの上限を低くします。"
              "対象が隠れている間は向きを保ち、一瞬で離れた所へ現れた時と対象を切り替えた時は、ゆっくり向き直します"
              "（切ると前の動き）。"))
    FIELD(float, lockTurnFarDistance, 7.0f, "普段の追い：この距離から (m)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, lockYawSecondsNear, 0.4f, "近い時の追うならし (秒)", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, lockYawMaxSpeedNear, 160.0f, "近い時の回る速さの上限 (度/s)", HIKARI::Range(10.0f, 2000.0f))
    // 消えた時：対象が隠れている間は向きを保ち、現れた（一瞬で lockJumpDistance 以上動いた・対象が変わった）後は
    // lockReacquireSeconds・lockReacquireMaxSpeed でゆっくり向き直す。切ると前の動き（隠れても追い、現れたらすぐ回る）。
    FIELD(bool, lockReacquireEnabled, true, "消えた・現れた時にゆっくり向き直す")
    FIELD(float, lockJumpDistance, 3.0f, "現れたと見なす 1 フレームの移動 (m)", HIKARI::Range(0.5f, 100.0f))
    FIELD(float, lockReacquireSeconds, 0.45f, "向き直すならし (秒)", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, lockReacquireMaxSpeed, 120.0f, "向き直す速さの上限 (度/s)", HIKARI::Range(10.0f, 2000.0f))
    // ロックの印（見る点に出す小さいひし形）。大きさは 1920×1080 の画面での画素、不透明度 0 で出さない。
    FIELD(
        float, lockMarkerSize, 14.0f, "大きさ (画素、1920×1080 の画面で)", HIKARI::Range(0.0f, 100.0f),
        HIKARI::Group("ロックオン：印"),
        HIKARI::Tip(
            "ロック中、見る点に小さいひし形を出します（どれをロックしているかが分かるように。攻撃の予告ではありません）。"))
    FIELD(float, lockMarkerOpacity, 0.75f, "不透明度 (0 で出さない)", HIKARI::Range(0.0f, 1.0f))
    FIELD(HIKARI::Color, lockMarkerColor, (HIKARI::Color{1.0f, 0.93f, 0.82f, 1.0f}), "色")
    // カメラ。姿勢は PlayerCamera が作る（Gameplay Camera の CameraFollow の値は使わない。画角だけカメラの値）。
    // 軸＝体の中心から cameraPivotHeight 上の点。視点は軸から cameraDistance 後ろ、cameraPitch だけ見下ろす。
    FIELD(float, cameraDistance, 6.0f, "軸から視点までの距離 (m)", HIKARI::Range(0.5f, 30.0f), HIKARI::Group("カメラ"),
          HIKARI::Tip(
              "カメラの姿勢はここの値で決まります（Gameplay Camera の CameraFollow の値は使いません。"
              "画角だけ Gameplay Camera の値です）。軸＝体の中心から少し上の点で、視点は軸の後ろ・上にあります。"))
    FIELD(float, cameraPivotHeight, 0.95f, "軸の高さ（体の中心から上へ m）", HIKARI::Range(-5.0f, 10.0f))
    FIELD(float, cameraPitch, 16.0f, "始めの見下ろし角度 (度)", HIKARI::Range(-80.0f, 80.0f))
    // 肩越し。視点と注視点を一緒に横へずらす（負で左。プレイヤーが画面の中心より少し右に見える）。
    FIELD(float, cameraSideOffset, -0.6f, "肩越しの横ずらし (m、負で左。0 で真後ろ)", HIKARI::Range(-10.0f, 10.0f))
    FIELD(float, cameraFollowSeconds, 0.09f, "水平に追うならし (秒、大きいほど遅れてついてくる)",
          HIKARI::Range(0.0f, 2.0f))
    FIELD(float, cameraVerticalSeconds, 0.35f, "上下に追うならし (秒)", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, cameraVerticalDeadZone, 0.5f, "上下を追わない幅 (m)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, cameraMouseSensitivity, 0.12f, "マウスの感度 (度/画素)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, cameraStickYawSpeed, 170.0f, "スティックの左右の速さ (度/s)", HIKARI::Range(0.0f, 3600.0f))
    FIELD(float, cameraStickPitchSpeed, 110.0f, "スティックの上下の速さ (度/s)", HIKARI::Range(0.0f, 3600.0f))
    FIELD(float, cameraMinPitch, -15.0f, "見上げる限度 (度、負で見上げ)", HIKARI::Range(-89.0f, 89.0f))
    FIELD(float, cameraMaxPitch, 55.0f, "見下ろす限度 (度)", HIKARI::Range(-89.0f, 89.0f))
    // 見上げた時に、視点が軸からどこまで下がってよいか (m)。地面へ潜らないようにする。
    FIELD(float, cameraEyeFloor, 1.5f, "見上げた時に視点が軸から下がれる限度 (m)", HIKARI::Range(0.0f, 10.0f))
    FIELD(bool, cameraTrace, false, "カメラの姿勢を毎フレーム記録する（確かめ用）")

    // HUD などが読めるよう、HP・スタミナ・回復回数・状態をゲーム全体の値（Params）へ毎フレーム書く。
    void Update(float dt) override;

    // 実行時のデータは保存しない。ルールは PlayerRules と PlayerSystem が所有する。
    bool initialized = false;
    STATE(PlayerState, state, PlayerState::Normal)
    STATE(int, stateFrame, 0)
    bool advanceStateNextTick = false;
    STATE(float, health, 0.0f)
    STATE(float, stamina, 0.0f)
    STATE(int, healCharges, 0)
    int staminaAge = 0;
    int rollBuffer = 0;
    int healBuffer = 0;
    // 走る入力を押している（描画フレームの入力から毎フレーム入れる）。
    bool sprintHeld = false;
    // この固定 tick に走っている（歩きより先に決める。向きとスタミナの規則が読む）。
    bool sprinting = false;
    // 精力が尽きて走れなくなった。走る入力を離すまで走らない（0 付近で走る・歩くを繰り返さない）。
    bool sprintExhausted = false;
    int actionLockFrames = 0;
    int recoilFramesLeft = 0;
    int recoilFrames = 0;
    float recoilDistance = 0.0f;
    float recoilEasePower = 1.0f;
    HIKARI::GameProjectFloat3 recoilDirection{};
    HIKARI::GameProjectFloat3 moveInput{};
    HIKARI::GameProjectFloat3 cameraForward{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 facing{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 actionDirection{};
    // いまロックしている物体（lockTarget はロックオンを押した時に狙う相手）。
    HIKARI::GameProjectObjectHandle lockTargetHandle{};
    bool lockedOn = false;
    // ロック対象を変える入力（Q）が押された。対象の候補を知っている側（ボスの BossLockTarget）が使ったら消す。
    bool lockSwitchRequested = false;
    // ロックオンの入れ切りを押した（Update で覚え、次の固定 tick の始めに入れ切りする。c-164cab）。
    bool lockToggleRequested = false;
    // 照準補助の候補。ボス側が毎 tick 作り直す（消えた候補は次の tick で外れる）。空なら lockTarget だけを見る。
    std::array<HIKARI::GameProjectObjectHandle, kPlayerLockCandidateCapacity> lockCandidates{};
    size_t lockCandidateCount = 0;
    // ロック対象の体の高さの半分 (m)。対象を知っている側（ボス）が毎 tick 入れる。
    float lockTargetHalfHeight = 0.0f;
    bool rollFogPending = false;
    bool healApplied = false;
    bool motionAccepted = true;
    bool cameraAccepted = true;
    bool fluidAccepted = true;
    bool hitStopAccepted = true;
    int pendingHitStopFrames = 0;
    // 食らった tick に立て、描画フレームの手応え（カメラ・振動）が使ったら消す。
    bool pendingHitFeedback = false;
    bool pendingHitFeedbackKnockback = false;
    Feedback::CameraShake hitShake{};
    // 吹っ飛びの画角の引き。揺れとは別の送り手で出す（同時に出せるように）。
    Feedback::CameraShake knockbackFov{};
    // 回避に入った tick に立て、描画フレームの手応え（画角の引き・ぼかし）が使ったら消す。
    bool pendingRollFeedback = false;
    Feedback::CameraShake rollFov{};
    // 一瞬の動きのぼかし（PlayerRules::KickBlur で頼む。戦いの間のふだんの分に重ねて、StepHitFeedback が出す）。
    Feedback::BlurPulse blurPulse{};
    // いまの吹っ飛びの距離（食らった時に決まる。表の「吹っ飛びの距離 m」が空なら knockbackDistance）と、始めた位置。
    float knockbackDistanceNow = 0.0f;
    HIKARI::GameProjectFloat3 knockbackOrigin{};
    // 着地した tick に立て、描画フレームの手応え（揺れ・振動）と霧の押しが使ったら消す。
    bool pendingLandFeedback = false;
    bool pendingLandFog = false;
    // 最後に受け入れた被弾（ApplyDamage）の種類と、被弾の通し番号（見た目が「新しく当たった」を知る）。
    PlayerDamageType lastHitType = PlayerDamageType::Slash;
    uint32_t hitCount = 0;
    // 炎上。burnRemaining が 0 なら燃えていない。点いた・消えた回数は見た目（次のカードの炎）が差分で読む。
    STATE(float, burnRemaining, 0.0f)
    // いまの炎上が点いてからの秒数と、いまの炎上で減った HP（消えた時のログ用）。
    float burnElapsed = 0.0f;
    float burnDamage = 0.0f;
    uint32_t burnIgnitions = 0;
    uint32_t burnEndings = 0;
    PlayerBurnCause burnCause = PlayerBurnCause::Ground;
    PlayerBurnEnd burnEndReason = PlayerBurnEnd::None;
    // 炎上で減った HP の累計（燃える地面の検証ログ用）。
    double burnDamageTotal = 0.0;
    // 燃える地面からの知らせ：その固定 tick にプレイヤーが火の中にいる（触れ続けて damageDelay を越えた）と、その間の
    // 減り方（/秒）。BurnFieldSystem が毎 tick 入れ、PlayerSystem の PostFixedUpdate が使ったら消す。
    bool burnInGroundFire = false;
    float burnGroundDamagePerSecond = 0.0f;
    PlayerExternalRequests external{};
    // 射撃の構え中に向く方向。毎 tick 外部が要求し、PlayerSystem が使ったら消す。
    bool aimTurnPending = false;
    float aimTurnSpeed = 0.0f;
    HIKARI::GameProjectFloat3 aimTurnDirection{0.0f, 0.0f, 1.0f};
    // いまの歩く速度（歩き出し・止まる時のならし）。
    HIKARI::GameProjectFloat3 moveVelocity{};
    // 描画のフレームで体の位置を補間するための、いまの固定 tick の始めの体の位置（PlayerSystem の PreFixedUpdate が毎 tick
    // 書く）。PlayerRules::PresentedPosition が、エンジンが体を描く位置と同じ補間に使う。
    HIKARI::GameProjectFloat3 tickStartPosition{};
    bool tickStartKnown = false;
    // カメラの状態（PlayerCamera が持つ）。角度は rad。yaw は水平の向き、pitch は見下ろす角度。
    bool cameraReady = false;
    float cameraYaw = 0.0f;
    float cameraPitchNow = 0.0f;
    float cameraYawVelocity = 0.0f;
    float cameraPitchVelocity = 0.0f;
    float cameraFovRadians = 0.0f;
    HIKARI::GameProjectFloat3 cameraPivot{};
    HIKARI::GameProjectFloat3 cameraPivotVelocity{};
    // ロックの見せ方（c-880340）：ロックの重み（0 で自由視点の見せ方、1 でロックの見せ方）と近さ（1 で近い、0 で遠い）の
    // いまの値とならしの速さ。向き直しの間か、前のフレームの見る点と対象（跳んだ・切り替わったを知る）。
    float lockAmount = 0.0f;
    float lockAmountVelocity = 0.0f;
    float lockNearness = 0.0f;
    float lockNearnessVelocity = 0.0f;
    bool lockReacquiring = false;
    bool lockPointKnown = false;
    HIKARI::GameProjectFloat3 lockPointLast{};
    HIKARI::GameProjectObjectHandle lockTargetLast{};
    // 出したロックの印の位置（1920×1080 の画面の画素。出していない時は負）。確かめの記録（cameraTrace）用。
    float lockMarkerX = -1.0f;
    float lockMarkerY = -1.0f;
    // カメラの見ている向き（見上げ・見下ろしを含む単位ベクトル。揺れ・肩越しのずらしは含まない）。筋斗雲が進む向きに使う。
    HIKARI::GameProjectFloat3 cameraLook{0.0f, 0.0f, 1.0f};
    // 外部のカメラの要求。PlayerCamera が使ったら消す（要求した側が消えても残さない）。
    bool cameraRequestPending = false;
    PlayerCameraRequest cameraRequest{};
    // 演出のカメラ（PlayerCinematicRequest）。PlayerCamera が使ったら消す。
    bool cinematicPending = false;
    PlayerCinematicRequest cinematic{};
    // 演出の間（0 より大きい間）：入力を受け付けず、ダメージを受けない。演出の側が毎 tick 入れ直し、PlayerSystem が
    // tick ごとに 1 減らす。
    int cinematicHold = 0;
    // 要求で変えた見せ方と、そのならしの速さ。cameraShaped は要求の値から Player の値へ戻り切るまで true
    // （要求が一度も無ければ見せ方は Player の値そのまま）。戻すならしは最後の要求の releaseSeconds。
    PlayerCameraShape cameraShape{};
    PlayerCameraShape cameraShapeVelocity{};
    bool cameraShaped = false;
    float cameraReleaseSeconds = 0.4f;
    // 確かめ用（cameraTrace）：前のフレームに出した視点。GetControlPose は前のフレームに解決した姿勢を返すので、その差が
    // 揺れなどの補正の大きさ。
    HIKARI::GameProjectFloat3 cameraTraceEye{};
};

} // namespace Project_TD2_2
