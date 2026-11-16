#pragma once

#include <cstdint>
#include <string>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "../Combat/Feedback.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kCloudMountType = "Project_TD2_2.CloudMount";
inline constexpr std::string_view kCloudMountSystem = "Project_TD2_2.CloudMountSystem";

// 筋斗雲の段階。Gameplay.Mount を押すたびに、乗る（Summoning → Riding）と降りる（Dismissing → None）を切り替える。
enum class CloudMountState { None, Summoning, Riding, Dismissing };

// 飛行の入力。描画フレームの入力から入れ、固定 tick が使う。
struct CloudMountInput {
    // Gameplay.Move の左右（右が正）と前後（前が正）。
    float strafe = 0.0f;
    float forward = 0.0f;
    // Gameplay.Jump（上昇）と Gameplay.Descend（下降）を押している。
    bool ascend = false;
    bool descend = false;
    // Gameplay.Sprint（ブースト）を押している。
    bool boost = false;
};

// 飛行の手応えの値。カメラ（④ c-db5975）と見た目（⑤〜⑧）が読むために毎 tick 書く。読む側は書き換えない。保存しない。
struct CloudMountFlightReadout {
    // いまの速さ (m/s)。騎乗していなければ 0。
    float speed = 0.0f;
    // この tick の速さの変わり方 (m/s²。速くなる時が正)。カメラの加速の遅れに使う。
    float acceleration = 0.0f;
    // ブースト中（Gameplay.Sprint を押し、精力が残っている間）。
    bool boosting = false;
    // ブーストの弾き出しの残り（押した瞬間 1、boostKickSeconds で 0）。
    float boostKick = 0.0f;
    // 旋回の強さ（−1〜1、右へ回る時が正）。いまの速さで回れる最大の速さで回っている時が ±1。
    float turn = 0.0f;
    // 急降下の度合い（0〜1）。下向きの角度が diveStartDegrees で 0、diveFullDegrees で 1。前後左右の入力がある時だけ。
    float dive = 0.0f;
    // 巡航・ブーストの速さを超えている分 (m/s)。急降下の後に overspeedDecaySeconds かけて 0 へ戻る。
    float overspeed = 0.0f;
    // 地面すれすれの持ち上げをこの tick に掛けた（騎乗中に地面に触れたまま速く進んでいる）。
    bool groundSkim = false;
};

// 召喚と解除の演出の実行時の状態（CloudMountVisuals が描画フレームごとに進める。保存しない）。
struct CloudMountArrivalState {
    // 前の描画フレームに見た段階。段階の変わり目で、飛来・散りを始める。
    CloudMountState lastState = CloudMountState::None;
    // 飛んで来ている間か、飛び始めからの秒、飛ぶ長さ（秒）、飛び始めてからの描画フレームの数。
    bool flying = false;
    float age = 0.0f;
    float flySeconds = 0.0f;
    int flyFrames = 0;
    // 空中（落下の受け止め・持ち上げの無い召喚）の飛来か。下から受け止め、深く沈む。
    bool catching = false;
    // 足元から見た飛び始めの位置と、道の曲がりを決める点（世界の向き、m）。
    HIKARI::GameProjectFloat3 from{};
    HIKARI::GameProjectFloat3 bend{};
    // 前の描画フレームの根の位置と、そこから測った根の速さ (m/s。確かめ用)。
    HIKARI::GameProjectFloat3 lastRoot{};
    bool hasLastRoot = false;
    float rootSpeed = 0.0f;
    // 飛んでいる間の雲の向き（度）。着いた後はここから体の向きへ戻す。
    float yaw = 0.0f;
    float pitch = 0.0f;
    // 着いてからの秒（負は着いていない）と、その時の沈む深さ (m)。
    float landedAge = -1.0f;
    float sinkDepth = 0.0f;
    // 軌跡の帯の明るさ（0〜1）。
    float trail = 0.0f;
    // 散り始めてからの秒（負は散っていない）と、その間に留める根の変換。
    float scatterAge = -1.0f;
    HIKARI::GameProjectTransform scatterRoot{};
    // 前の描画フレームに送った根の変換。
    HIKARI::GameProjectTransform lastTransform{};
    // 着いた瞬間のカメラの沈み。
    Feedback::CameraShake cameraDip{};
    // 着いた瞬間の飾りの下準備を済ませた。
    bool prewarmed = false;
};

// プレイヤーの筋斗雲。Player と同じ物体に付ける。状態と飛行の運動の規則は CloudMountRules、入力を読んで運動を出すのは
// CloudMountSystem。見た目（CloudMountVisuals）とカメラの要求（CloudMountCamera）は別の所に置く。ここは調整値と実行時の状態だけ。
class CloudMountComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    // 召喚：summonFrames F で、足元を地面から rideHeight 浮いた高さへ持ち上げる（今より低くはしない）。
    int summonFrames = 30;
    float rideHeight = 1.2f;
    // 持ち上げの出だしの速さ（1 は等速、大きいほど初め速くあとゆっくり）。
    float summonEasePower = 2.0f;
    // 召喚の時に足元の地面を探す距離 (m)。これより下に地面が無ければ、その場の高さで乗る（落下中の召喚）。
    float summonGroundReach = 4.0f;
    // 解除：dismissFrames F その場で止まってから、いつもの重力で落ちる。
    int dismissFrames = 12;
    // 飛行：入力が無ければその場に浮く。前後はカメラの見ている向き、左右は水平の横、上昇・下降は真上・真下。
    float cruiseSpeed = 16.0f;
    float verticalSpeed = 10.0f;
    float accelSeconds = 0.35f;
    float brakeSeconds = 0.5f;
    // 体を水平の進む向きへ回す速さ（度/秒）。
    float turnSpeed = 540.0f;
    // このカメラの見下ろし角度（度）を水平の飛行とみなす。0 でカメラの向きのまま（始めのカメラは 16 度見下ろす）。
    float levelPitch = 16.0f;
    // どの状態でも超えない速さ (m/s)。⑤ の雲の流体の域は 1 フレームに 60 m/s を超える動きを瞬間移動と見なす。
    float maxFlightSpeed = 45.0f;
    // 地面すれすれの持ち上げ：騎乗中に地面に触れ、水平の速さが groundSkimMinSpeed 以上なら、その tick の縦の速さの
    // 下向きを捨てて groundSkimSpeed (m/s) 上へ出す。地面に触れたまま速く進むと運動学ソルバーが平らに近い所で止まる
    // 件（エンジンへの要望 e-dca15c）を避け、起伏を滑るように越える。groundSkimSpeed 0 で切る。
    // 持ち上げた後 groundSkimHoldSeconds の間は下へ向かわない（地面へ向けて飛び続けても、持ち上げと下降を毎 tick
    // 繰り返して震えず、地面に沿って滑る）。
    float groundSkimSpeed = 3.0f;
    float groundSkimMinSpeed = 6.0f;
    float groundSkimHoldSeconds = 0.2f;
    // ブースト：Gameplay.Sprint を押している間の最高速。押した瞬間は boostKickSeconds で boostKickSpeed まで弾き出す。
    float boostSpeed = 32.0f;
    float boostKickSpeed = 28.0f;
    float boostKickSeconds = 0.15f;
    // ブースト中に使う精力（/秒）。尽きたら入力を離すまでブーストしない。0 で使い放題。
    float boostStaminaPerSecond = 12.0f;
    // 急降下：下向きに進んでいる間、下向きの度合いに応じて速さが伸びる（度合い 1 で diveAccel、上限 diveMaxSpeed）。
    // 度合いは下向きの角度が diveStartDegrees で 0、diveFullDegrees で 1。浅い下りでは伸びない（速さが溜まり続け
    // ないように）。前で飛べる一番急な下りは「騎乗中のカメラの見下ろす限度 − levelPitch」（既定 75 − 16 = 59 度）。
    float diveAccel = 12.0f;
    float diveMaxSpeed = 45.0f;
    float diveStartDegrees = 10.0f;
    float diveFullDegrees = 55.0f;
    // 巡航・ブーストより速い分は、急降下をやめてからこの秒数で戻る（その間は速いまま上へ抜けられる）。
    float overspeedDecaySeconds = 1.5f;
    // 旋回：速度の向きを入力の向きへ回す速さ（度/秒）。巡航の速さ以下で turnRateCruise、ブーストの速さ以上で
    // turnRateBoost、間は速さで補間する。旋回の半径＝速さ ÷ 回る速さ（ラジアン/秒）なので速いほど大回りになる。
    float turnRateCruise = 180.0f;
    float turnRateBoost = 90.0f;
    // 落下の受け止め：乗っていない時、足場を離れて catchFallMeters 落ちたら自動で召喚し、下向きの速さを
    // catchBrakeSeconds でならして止める。自分で降りた後の落下は、catchAfterDismiss が true の時だけ受け止める。
    bool autoCatch = true;
    float catchFallMeters = 12.0f;
    float catchBrakeSeconds = 0.4f;
    bool catchAfterDismiss = false;
    // 飛べる範囲：場の原点から水平の半径 boundsRadius、高さ boundsMinY〜boundsMaxY（世界の Y）。境界の手前
    // boundsSoftMeters から外向きの速さを弱め、境界の上で 0 にする（壁に当たったようには止めない）。
    // 高さの既定は NimbusSky の島の床（Y 3400）から −150〜+300 m。
    float boundsRadius = 400.0f;
    float boundsMinY = 3250.0f;
    float boundsMaxY = 3700.0f;
    float boundsSoftMeters = 30.0f;
    // 騎乗中のカメラ（PlayerCamera への要求。姿勢を作るのは PlayerCamera だけ）。上下の追い方と見上げの限度は
    // Player の「カメラ」の値の代わりに使う。cameraVerticalSeconds は止まっている時の上下のならしで、水平の速さが巡航へ
    // 近づくほど水平のならし（加速の遅れを含む）へ寄せる（体が飛ぶ向きのまま画面の奥へ離れ、上下へ外れない）。
    float cameraVerticalSeconds = 0.08f;
    float cameraVerticalDeadZone = 0.0f;
    float cameraMinPitch = -70.0f;
    float cameraMaxPitch = 75.0f;
    // 見上げた時に、視点が軸からどこまで下がってよいか (m)。Player の値（1.5）のままだと、見上げて上る時に距離が
    // 縮んで引きが効かない。
    float cameraEyeFloor = 4.0f;
    // 距離 (m) と縦の画角（度）：止まっている・巡航・ブースト。間は速さで補間し、ブースト中はブーストの値。
    // 巡航・ブーストより速い時（急降下）はブーストの値。距離は cameraZoomSeconds、画角は cameraFovSeconds でならす。
    float cameraDistanceStop = 6.0f;
    float cameraDistanceCruise = 7.5f;
    float cameraDistanceBoost = 9.5f;
    float cameraZoomSeconds = 0.35f;
    float cameraFovStop = 60.0f;
    float cameraFovCruise = 68.0f;
    float cameraFovBoost = 82.0f;
    float cameraFovSeconds = 0.15f;
    // ブーストの出だしに、ならさずに足す画角（度）と、その長さ（秒）。初めの半分で上がり、残りで戻る。その間は画角の
    // ならしを短くしてブーストの画角へ素早く届かせるので、ブーストの画角を一度これだけ行き過ぎて戻る。
    float cameraBoostFovKick = 6.0f;
    float cameraBoostFovKickSeconds = 0.25f;
    // 傾き：旋回の強さ（−1〜1）× 上限（度）。上限は巡航の速さで cameraRollCruise、ブーストで cameraRollBoost、
    // 巡航より遅い時は速さに比例して小さくする。cameraRollSeconds でならす。
    bool cameraRollEnabled = true;
    float cameraRollCruise = 12.0f;
    float cameraRollBoost = 20.0f;
    float cameraRollSeconds = 0.25f;
    // 水平に追うならし（秒）。速さが増えている間は、増え方が cameraFollowFullAccel (m/s²) で cameraFollowAccelSeconds
    // まで伸ばし（体が先に画面の奥へ出る）、減っている間は cameraFollowBrakeSeconds まで縮める。伸ばした分は
    // cameraFollowRecoverSeconds で戻す（カメラが追いつく）。
    float cameraFollowSeconds = 0.09f;
    float cameraFollowAccelSeconds = 0.22f;
    float cameraFollowBrakeSeconds = 0.06f;
    float cameraFollowFullAccel = 60.0f;
    float cameraFollowRecoverSeconds = 0.05f;
    // 急降下の度合い 1 で足す、見せるだけの見下ろし（度。進む向きは変えない）。
    float diveLookDegrees = 6.0f;
    // 動きのぼかし：ブースト中と急降下の度合いに合わせて強める（TAA／DLAA／DLSS の時だけ効く）。強さは cameraBlurSeconds
    // でならす。シャッター角 0 で切る。
    float cameraBlurShutterDegrees = 90.0f;
    float cameraBlurMaxPixels = 8.0f;
    float cameraBlurSeconds = 0.2f;
    // ブースト中だけの細かい震え：揺れ幅 (m)・速さ (Hz)。離すと cameraBoostShakeFadeSeconds で消える。0 で切る。
    float cameraBoostShake = 0.015f;
    float cameraBoostShakeFrequency = 26.0f;
    float cameraBoostShakeFadeSeconds = 0.15f;
    // 降りた後、距離・画角・傾きと、騎乗中だけ許した見上げ・見下ろしを普段の値へ戻すならし（秒）。
    float cameraReleaseSeconds = 0.4f;
    // 雲の見た目（CloudMountVisuals）：足元に付いて動く流体の雲のエフェクト。空なら出さない。資産の入力は
    // input.appear（体の雲の量）・input.speed（尾の量）・input.boost（吹き出しと金の縁の明るさ）。
    HIKARI::GameProjectVfxAssetReference cloudEffect{"467fbd610a584de599d6946884fcdb67", "Assets/Vfx/NimbusCloud.hvfx"};
    // エフェクトの原点（資産の足元）を、体の足元からどれだけ下げるか (m)。
    float cloudBelowFeet = 0.0f;
    // 体の雲の量（倍率）。出す・消すは cloudAppearSeconds・cloudVanishSeconds で 0〜1 をならす（⑥ の演出の口）。
    float cloudAmount = 1.0f;
    float cloudAppearSeconds = 0.3f;
    float cloudVanishSeconds = 0.25f;
    // 尾の量：速さ ÷ cloudTailFullSpeed（上限 cloudTailLimit）× cloudTailAmount。尾の長さは資産の相対風と消散で決まる。
    float cloudTailAmount = 1.0f;
    float cloudTailFullSpeed = 32.0f;
    float cloudTailLimit = 1.4f;
    // 吹き出しと金の縁の強さ：ブースト中 cloudBoostAmount、弾き出しの間 cloudBoostKickAmount を足し、急降下の度合い ×
    // cloudDiveAmount を足す。cloudBoostSeconds でならす。
    float cloudBoostAmount = 1.0f;
    float cloudBoostKickAmount = 1.0f;
    float cloudDiveAmount = 0.6f;
    float cloudBoostSeconds = 0.12f;
    // 止まっている時の上下の揺れ（雲だけ。体は動かさない）：揺れ幅 (m) と 1 往復の秒。水平の速さ cloudIdleSpeed で 0 になる。
    float cloudBobHeight = 0.08f;
    float cloudBobSeconds = 2.4f;
    float cloudIdleSpeed = 6.0f;
    // 上り下りで雲の前を進む向きへ傾ける割合（0〜1）と上限（度）、ならし（秒）。水平の速さが cloudIdleSpeed 以上で全部掛け、
    // 真上・真下だけの上昇・下降では傾けない。急降下で相対風が雲を体の上へ吹き上げず、尾が進む向きの後ろへ流れる。
    float cloudPitchFollow = 0.8f;
    float cloudPitchMaxDegrees = 45.0f;
    float cloudPitchSeconds = 0.25f;
    // 旋回で雲を内側へ傾ける角度（旋回の強さ ±1 で、度）と、そのならし（秒）。
    float cloudBankDegrees = 18.0f;
    float cloudBankSeconds = 0.2f;
    // 乗っていない間もエフェクトを出したままにする（中は空で域は眠る。召喚のたびに出し直す詰まりを避ける）。false なら
    // 雲が消えてから cloudLingerSeconds 後に止める。
    bool cloudKeepAlive = true;
    float cloudLingerSeconds = 1.5f;
    // 召喚の演出（CloudMountArrival・CloudMountVisuals）：雲（本体のエフェクトの根）が、体の向きの斜め後ろ上（後ろへ
    // summonFromDistance・上へ summonFromHeight、真後ろから横へ summonFromSideDegrees）から尾を引いて足元へ飛んで来る。
    // 着くのは召喚の summonArriveFraction（summonFrames の割合）の時。落下中の受け止めでは足元の下へ summonCatchDip 回り
    // 込んで下から受け止める。飛ぶ速さが summonFlySpeedLimit を超える時は飛び始めを近くする（流体は 1 フレームに 60 m/s を
    // 超える動きを瞬間移動と見なし、尾が引けない）。
    float summonFromDistance = 25.0f;
    float summonFromHeight = 10.0f;
    float summonFromSideDegrees = 60.0f;
    float summonArriveFraction = 0.8f;
    float summonFlyEasePower = 1.3f;
    float summonFlySpeedLimit = 55.0f;
    float summonApproachHeight = 1.0f;
    float summonCatchDip = 1.5f;
    // 飛んで来る間の雲の量・尾の量（倍率）と、軌跡の帯の明るさ。帯は着いてから summonTrailFadeSeconds で消す。
    float summonFlyAmount = 0.7f;
    float summonFlyTail = 0.4f;
    // 飛んで来る間の渦の強さ（域の乱流の倍率）。相対風でなめらかに伸びる煙を巻いて、綿の縁を保つ。着いた後は膨らみと
    // 一緒に 1 へ戻る。
    float summonFlySwirl = 2.5f;
    float summonTrailAmount = 1.0f;
    float summonTrailFadeSeconds = 0.15f;
    // 着いた時：雲だけが沈んで戻る（体は動かさない。地上 summonSinkMeters、受け止め summonCatchSinkMeters を
    // summonSinkSeconds で）、雲の量を一度 summonPuffAmount 足して膨らませる（summonPuffSeconds で戻る）、向きを飛ぶ向きから
    // 体の向きへ summonSettleSeconds で戻す。
    float summonSinkMeters = 0.15f;
    float summonCatchSinkMeters = 0.4f;
    float summonSinkSeconds = 0.3f;
    float summonPuffAmount = 1.0f;
    float summonPuffSeconds = 0.3f;
    float summonSettleSeconds = 0.2f;
    // 着いた瞬間の飾り（空気のゆがみの輪・光・粒。流体の域は持たない）と、輪の強さ・大きさの倍率。
    HIKARI::GameProjectVfxAssetReference arriveEffect{"5c1e7a3f2b8d4e60a9f4d2c7b1e08a36",
                                                      "Assets/Vfx/NimbusArrive.hvfx"};
    float arriveRingStrength = 1.0f;
    float arriveRingScale = 1.0f;
    // 場面の始めに飾りを見えない強さで一度出し、画面のゆがみの合成の初回の準備を済ませる（最初の召喚で止まらない）。
    bool arrivePrewarm = true;
    // 着いた瞬間のカメラの手応え：下へ summonCameraDip (m) 沈んで summonCameraDipSeconds で戻る。0 で切る。
    float summonCameraDip = 0.05f;
    float summonCameraDipSeconds = 0.25f;
    // 解除の演出：雲はその場に残り（体と一緒に落ちない）、disperseSeconds の間だけ外へ押し広げながら吸って散る。
    // disperseHoldSeconds 後に根を体の足元へ戻す（その時は空）。散る瞬間に disperseEffect（光の粒）を出す。
    HIKARI::GameProjectVfxAssetReference disperseEffect{"a7d93e15c4f2480bb6e1f0a2d95c3b74",
                                                        "Assets/Vfx/NimbusDisperse.hvfx"};
    float disperseSeconds = 0.5f;
    float disperseHoldSeconds = 1.0f;
    // 散らす間の渦の強さ（域の乱流の倍率）。煙を筋にちぎって散らす。
    float disperseSwirl = 3.0f;
    // 速度感（全画面の効果 FlightSpeed：進む向きを中心に、縁が放射状に流れる・ぼかし・わずかな色ずれ・縁の減光）。
    // 強さは速さの曲線（speedFxStartSpeed まで 0 → 巡航 speedFxCruise → ブーストの速さ speedFxBoost → 急降下の上限の速さ
    // speedFxDive）と、ブースト中 speedFxBoost・急降下の度合い × speedFxDive の大きい方。強まる時 speedFxRiseSeconds、
    // 弱まる時 speedFxFallSeconds でならし、speedFxLimit で抑える（0 で出さない。酔いやすい人向け）。強さ 0 の間は
    // 効果を一切出さない。見た目（ぼかしの長さ・筋・色ずれ・減光・中心の澄んだ広さ）は効果資産の値。
    std::string speedFxProfile{"0b62bd7baa7d48e4a65dc8867fd3d990"};
    float speedFxLimit = 1.0f;
    float speedFxStartSpeed = 4.0f;
    float speedFxCruise = 0.15f;
    float speedFxBoost = 0.75f;
    float speedFxDive = 1.0f;
    float speedFxRiseSeconds = 0.25f;
    float speedFxFallSeconds = 0.5f;
    // 流れの中心（進む向きを画面へ写した点）のならし（秒）と、画面の中心からの限度（画面の端が 1）。横へ飛ぶ時は
    // 限度の所に置き、流れはほぼ平行になる。
    float speedFxCenterSeconds = 0.1f;
    float speedFxCenterLimit = 1.4f;
    // 体と雲を守る球：体の中心から speedFxGuardDrop (m) 下げた所の、半径 speedFxGuardRadius (m)。画面で守る楕円の
    // 内は流さない（縁のぼかし具合は効果資産の guardFeather）。半径 0 で守らない。
    float speedFxGuardRadius = 1.5f;
    float speedFxGuardDrop = 0.35f;
    // 確かめ用：この F ごとに位置・速度をログに出す（0 は出さない）。
    int traceEveryFrames = 0;

    // 実行時のデータは保存しない。
    CloudMountState state = CloudMountState::None;
    // いまの段階に入ってから進んだ固定 tick の数。
    int stateFrame = 0;
    // Mount が押された（固定 tick が使ったら消す）。
    bool mountPressed = false;
    CloudMountInput input{};
    // 飛行の速度（世界 m/s）。縦はこの値をソルバーの重力の差し替えで出す。
    HIKARI::GameProjectFloat3 velocity{};
    // 外へ知らせる飛行の値（④〜⑧が読む）。
    CloudMountFlightReadout flight{};
    // ブーストが尽きた（入力を離すまでブーストしない）。
    bool boostExhausted = false;
    // 弾き出しの残り (秒)。
    float boostKickLeft = 0.0f;
    // 速い分を戻す速さ (m/s²)。戻し始めた時の「速い分 ÷ overspeedDecaySeconds」。戻していない間は 0。
    float overspeedDecayRate = 0.0f;
    // 前の固定 tick の体の中心と、そこから実際に動いた速さ（当たって止められた分の速さを持ち越さないために使う）。
    HIKARI::GameProjectFloat3 lastPosition{};
    bool hasLastPosition = false;
    float achievedSpeed = 0.0f;
    // 落下の受け止め：足場を離れてからの一番高い体の中心の高さ。受け止める落下か（自分で降りた後は catchAfterDismiss）。
    float fallPeakY = 0.0f;
    bool catchArmed = true;
    // 受け止めた時の下向きの速さ (m/s、正) と、ならしの残り (秒)。catching は受け止めの召喚の間（縦は持ち上げの輪郭
    // ではなく、落ちる勢いのならしで出す）。
    float catchFallSpeed = 0.0f;
    float catchBrakeLeft = 0.0f;
    bool catching = false;
    // 地面すれすれの持ち上げの後、下へ向かわない残り (秒)。
    float groundSkimHoldLeft = 0.0f;
    // カメラの演出（CloudMountCamera が描画フレームごとに進める）。いまの水平に追うならし（負は騎乗の始め）、
    // ブーストの出だしからの秒（負は無し）、前のフレームのブースト、動きのぼかしの強さ（0〜1）、震え。
    float cameraFollowNow = -1.0f;
    float cameraKickAge = -1.0f;
    bool cameraWasBoosting = false;
    float cameraBlurWeight = 0.0f;
    Feedback::CameraShake cameraShake{};
    // 召喚の始めと終わりの体の中心の高さ。
    float summonStartY = 0.0f;
    float summonTargetY = 0.0f;
    // 立った時の、体の中心から足元までの高さ。地面に立っている間に測る（測る前は Player のカプセルの半分）。
    float standOffset = 0.85f;
    // ログ用：この tick に提出した縦の速さと重力の差し替え、提出が受け入れられたか。
    float submittedVertical = 0.0f;
    float submittedGravity = 0.0f;
    bool motionAccepted = true;
    // 雲の見た目（CloudMountVisuals が描画フレームごとに進める）：再生、出し入れ（0〜1）、吹き出しの強さ、旋回の傾きと上り下りの傾き（度）、
    // 揺れの時間、雲が消えてからの秒、送った入力が受け付けられたか。
    HIKARI::GameProjectVfxPlaybackHandle cloudVfx{};
    float cloudAppear = 0.0f;
    float cloudBoost = 0.0f;
    float cloudBank = 0.0f;
    float cloudPitch = 0.0f;
    float cloudBobTime = 0.0f;
    float cloudHiddenSeconds = 0.0f;
    bool cloudAccepted = true;
    // 召喚と解除の演出（CloudMountVisuals が描画フレームごとに進める）。
    CloudMountArrivalState arrival{};
    // 速度感（CloudMountSystem が描画フレームごとに進める）：ならした強さ（上限の前）、流れの中心（UV）と置いたか、
    // 前へ向かう度合い、最後の提出が受け付けられたか、受け付けられた提出の数と、出していない所から出し始めた数
    // （確かめ用）。
    float speedFxStrength = 0.0f;
    float speedFxCenterU = 0.5f;
    float speedFxCenterV = 0.5f;
    bool speedFxHasCenter = false;
    // 前へ向かう度合い（CloudMountSpeedFx::HeadingOnScreen）。止まって向きが決まらない間は前の値のまま。
    float speedFxFacing = 1.0f;
    bool speedFxAccepted = false;
    uint64_t speedFxSubmits = 0;
    uint64_t speedFxStarts = 0;
};

} // namespace Project_TD2_2
