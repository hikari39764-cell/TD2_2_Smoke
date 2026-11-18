#pragma once

#include <array>

#include "HIKARI/Project/GameComponent.h"
#include "../Combat/Feedback.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kPlayerType = "Project_TD2_2.Player";
inline constexpr std::string_view kPlayerSystem = "Project_TD2_2.PlayerSystem";
// 物体変数の ID。ProjectSettings/runtime_bindings.json の宣言と同じ名前・型（Float か Int）にする。
inline constexpr std::string_view kPlayerHealthVariable = "Player.Health";
inline constexpr std::string_view kPlayerMaxHealthVariable = "Player.MaxHealth";
inline constexpr std::string_view kPlayerStaminaVariable = "Player.Stamina";
inline constexpr std::string_view kPlayerMaxStaminaVariable = "Player.MaxStamina";
inline constexpr std::string_view kPlayerHealChargesVariable = "Player.HealCharges";
inline constexpr std::string_view kPlayerStateVariable = "Player.State";
// 炎上の残り秒数（燃えていなければ 0）。HUD が HP の条の色を変えるのに読む。
inline constexpr std::string_view kPlayerBurnVariable = "Player.Burn";

// 番号は物体変数 Player.State の値。足す時は末尾へ（Ride＝8 は筋斗雲に乗っている間。動きは CloudMountSystem が出す）。
enum class PlayerState { Normal, Roll, Heal, Stagger, Knockback, Down, GetUp, Dead, Ride };
// 食らった時の体の反応（のけぞり・吹っ飛び）。ダメージの種類（PlayerDamageType）とは別に選ぶ。
enum class PlayerDamageKind { Stagger, Knockback };
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
// 照準補助の候補（ボス本体＋出ている分身）を置いておける数。
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

class PlayerComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;
    // HUD などが読めるよう、HP・スタミナ・回復回数・状態を自分の物体の物体変数へ毎フレーム書く。
    void Update(HIKARI::GameComponentContext& context, float deltaTime) override;

    float maxHealth = 100.0f;
    // ふだんの歩き。走る（Gameplay.Sprint を押している間）は sprintSpeed（c-ff3e98。元は歩き 5.2、走りは無し）。
    float moveSpeed = 3.8f;
    float sprintSpeed = 5.2f;
    // 走っている間に使う精力（/秒）。走っている間は回復せず、止めてから staminaDelayFrames 待って回復する。
    float sprintStaminaPerSecond = 12.0f;
    // 通常・回復中に向きを変える速さ。試作の 12 rad/秒。
    float turnSpeed = 688.0f;
    float maxStamina = 100.0f;
    int staminaDelayFrames = 40;
    float staminaPerSecond = 45.0f;
    // 終わり際の硬直を短くして次の行動へ早く移れるようにする（c-a0a892。元の値は調整ガイド）。
    int rollFrames = 26;
    int rollMoveFrames = 22;
    // この経過 F 以降（移動の後）は、砲撃・吸い込みでローリングの残りを打ち切れる。
    int rollActionCancelFrame = 22;
    float rollDistance = 4.2f;
    // 進んだ距離 = rollDistance × (1 − (1 − 進み)^rollEasePower)。1 で等速、大きいほど出だしが速い。
    float rollEasePower = 1.6f;
    int rollInvincibleFrames = 14;
    float rollStamina = 25.0f;
    int inputBufferFrames = 12;
    int staggerFrames = 26;
    int staggerStepOneFrames = 9;
    int staggerStepTwoFrames = 9;
    float staggerStepOne = 0.75f;
    float staggerStepTwo = 0.3f;
    float staggerStepThree = 0.12f;
    int staggerHitStopFrames = 5;
    float knockbackDistance = 6.0f;
    int knockbackFrames = 30;
    // 進んだ距離 = knockbackDistance × (1 − (1 − 進み)^knockbackEasePower)。ローリングと同じ形で、前が速く後ろが遅い。
    float knockbackEasePower = 2.2f;
    // 吹っ飛びの放物線の頂点の高さ。knockbackApexAt（飛行の割合）で頂点に届き、knockbackLandAt で着地して、
    // 残りは地面を滑る。
    float knockbackHeight = 0.8f;
    float knockbackApexAt = 0.2f;
    float knockbackLandAt = 0.55f;
    int knockbackHitStopFrames = 8;
    // 着地の重み：小さなカメラの揺れ・手柄の振動・足元の霧を外へ押す一発。
    float knockbackLandShake = 0.06f;
    int knockbackLandShakeFrames = 6;
    float knockbackLandRumble = 0.5f;
    float knockbackLandPushRadius = 1.6f;
    float knockbackLandPush = 5.0f;
    float knockbackLandPushHeightOffset = -0.45f;
    // 吹っ飛ばされた時にカメラを少し引いて（画角を広げて）戻す。
    float knockbackCameraFov = 4.0f;
    int knockbackCameraFovFrames = 18;
    // 食らった時のカメラの揺れと振動（c-d330c0。止めは上の値のまま）。のけぞりは小さく短く、吹っ飛びは大きく長く。
    float hitCameraAmplitudeStagger = 0.05f;
    int hitCameraFramesStagger = 8;
    float hitCameraAmplitudeKnockback = 0.12f;
    int hitCameraFramesKnockback = 14;
    float hitCameraFrequency = 22.0f;
    float hitRumbleStagger = 0.5f;
    float hitRumbleKnockback = 0.9f;
    int hitRumbleFrames = 10;
    int downFrames = 40;
    int downRollFrame = 14;
    int getUpFrames = 22;
    // 起き上がりのこの F 目から、ローリングで起き上がりを打ち切って転がれる（0 は起き上がり始めから）。
    int getUpRollFrame = 0;
    // 炎上：付いてから消えるまでの秒数と、燃える地面の外で減る HP（/秒）。燃える地面の上では BurnField の値で減る。
    float burnSeconds = 4.0f;
    float burnDamagePerSecond = 6.0f;
    int maxHealCharges = 4;
    float healAmount = 40.0f;
    int healFrames = 60;
    int healApplyFrame = 36;
    float healMoveSpeed = 1.5f;
    float maximumFallSpeed = 55.0f;
    float rollFogRadius = 1.0f;
    float rollFogStrength = 3.0f;
    float rollFogHeightOffset = -0.45f;
    // ローリングの風が場の火を押す強さ（霧を押す強さに対する割合。0 で押さない）。
    float rollFireShare = 0.6f;
    // 場の火が体をよける大きさ（火の域にだけ効く障害物。半径 0 でよけない）。
    float fireObstacleRadius = 0.4f;
    float fireObstacleHeight = 1.7f;
    // 歩き出し・止まる時に速さを変える時間（0 で一瞬）。回避・被弾などの動作には掛からない。
    float moveAccelSeconds = 0.08f;
    float moveDecelSeconds = 0.06f;
    // 立ち止まっていても滑り落ちない坂の傾きの上限（度）。これより急な坂では、今までどおり重力で滑る。
    float standSlopeDegrees = 30.0f;
    uint64_t lockTargetId = 0;
    uint64_t cameraId = 0;
    // カメラ。姿勢は PlayerCamera が作る（Gameplay Camera の CameraFollow の値は使わない。画角だけカメラの値）。
    // 軸＝体の中心から cameraPivotHeight 上の点。視点は軸から cameraDistance 後ろ、cameraPitch だけ見下ろす。
    float cameraDistance = 6.0f;
    float cameraPivotHeight = 0.95f;
    float cameraPitch = 16.0f;
    // 肩越し。視点と注視点を一緒に横へずらす（負で左。プレイヤーが画面の中心より少し右に見える）。
    float cameraSideOffset = -0.6f;
    float cameraFollowSeconds = 0.09f;
    float cameraVerticalSeconds = 0.35f;
    float cameraVerticalDeadZone = 0.5f;
    float cameraMouseSensitivity = 0.12f;
    float cameraStickYawSpeed = 170.0f;
    float cameraStickPitchSpeed = 110.0f;
    float cameraMinPitch = -15.0f;
    float cameraMaxPitch = 55.0f;
    // 見上げた時に、視点が軸からどこまで下がってよいか (m)。地面へ潜らないようにする。
    float cameraEyeFloor = 1.5f;
    bool cameraTrace = false;
    // ロックオン中のカメラ。対象の向きへ lockYawSeconds でならして回り、回る速さは lockYawMaxSpeed まで。
    float lockYawSeconds = 0.16f;
    float lockYawMaxSpeed = 300.0f;
    // 遠い時の見下ろし角（度）。近い時は lockPitchNear（c-880340）。演出の向く点を追う間（ロックしていない）もこの値。
    float lockPitch = 17.0f;
    // 対象が高い所にいる時に見上げる割合（0 で見上げない、1 で対象の高さの角度だけ）。
    float lockPitchFollow = 0.6f;
    float lockPitchSeconds = 0.28f;
    // c-880340：距離に合わせた見せ方。対象との水平の距離が lockNearDistance 以下で「近い」の値、lockFarDistance 以上で
    // 「遠い」の値、その間はなめらかに（smoothstep）混ぜる。混ぜる割合そのものも lockFramingSeconds でならす。
    float lockNearDistance = 3.0f;
    float lockFarDistance = 15.0f;
    float lockFramingSeconds = 0.8f;
    // 軸から視点までの距離 (m)・軸を上げる高さ (m)・見せるだけの見上げ（視点はそのまま見る向きだけ上げる。度）。
    float lockCameraDistanceNear = 7.5f;
    float lockCameraDistanceFar = 6.5f;
    float lockPivotRiseNear = 0.9f;
    float lockPivotRiseFar = 0.3f;
    float lockLookUpNear = 6.0f;
    float lockLookUpFar = 4.0f;
    float lockPitchNear = 12.0f;
    // ロック中の見上げの限度（度、負で見上げ。ボスの跳躍・天降りで高い所にいる時）。
    float lockMinPitch = -10.0f;
    // 見る点の高さ：対象の中心から、体の高さの半分に対するこの割合だけ上（0.35 で胸のあたり）。照準も同じ点。
    float lockAimHeightShare = 0.35f;
    // 近くで回り過ぎない：対象が lockTurnNearDistance 以下で「近い」のならし・速さの上限、lockTurnFarDistance 以上で
    // lockYawSeconds・lockYawMaxSpeed。その間はなめらかに混ぜる。
    float lockTurnNearDistance = 2.5f;
    float lockTurnFarDistance = 7.0f;
    float lockYawSecondsNear = 0.4f;
    float lockYawMaxSpeedNear = 160.0f;
    // 消えた時：対象が隠れている間は向きを保ち、現れた（一瞬で lockJumpDistance 以上動いた・対象が変わった）後は
    // lockReacquireSeconds・lockReacquireMaxSpeed でゆっくり向き直す。切ると前の動き（隠れても追い、現れたらすぐ回る）。
    bool lockReacquireEnabled = true;
    float lockJumpDistance = 3.0f;
    float lockReacquireSeconds = 0.45f;
    float lockReacquireMaxSpeed = 120.0f;
    // ロックの印（見る点に出す小さいひし形）。大きさは 1920×1080 の画面での画素、不透明度 0 で出さない。
    float lockMarkerSize = 14.0f;
    float lockMarkerOpacity = 0.75f;
    HIKARI::GameProjectFloat3 lockMarkerColor{1.0f, 0.93f, 0.82f};

    // 実行時のデータは保存しない。ルールは PlayerRules と PlayerSystem が所有する。
    bool initialized = false;
    // 物体変数の書き込みが断られた警告を出したか（部品ごとに 1 回だけ出す）。
    bool variableWriteWarned = false;
    PlayerState state = PlayerState::Normal;
    int stateFrame = 0;
    bool advanceStateNextTick = false;
    float health = 0.0f;
    float stamina = 0.0f;
    int healCharges = 0;
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
    HIKARI::GameProjectObjectHandle lockTarget{};
    bool lockedOn = false;
    // ロック対象を変える入力（Q）が押された。対象の候補を知っている側（ボスの分身）が使ったら消す。
    bool lockSwitchRequested = false;
    // 照準補助の候補。ボス側が毎 tick 作り直す（分身が消えたら次の tick で外れる）。空なら lockTargetId だけを見る。
    std::array<HIKARI::GameProjectObjectHandle, kPlayerLockCandidateCapacity> lockCandidates{};
    size_t lockCandidateCount = 0;
    // ロック中のカメラが見る点を、対象の原点からずらす量（世界 m）。霧へ沈んだボスを見続ける時など、外部が毎 tick 入れる。
    HIKARI::GameProjectFloat3 lockPivotOffset{};
    // ロック対象の体の高さの半分 (m) と、隠れている（霧へ沈んでいる）か。対象を知っている側（ボス）が毎 tick 入れる。
    float lockTargetHalfHeight = 0.0f;
    bool lockTargetHidden = false;
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
    float burnRemaining = 0.0f;
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
