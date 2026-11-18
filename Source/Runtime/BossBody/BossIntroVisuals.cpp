#include "BossIntroVisuals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/FluidView.h"
#include "BossBodyMath.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossIntro.h"
#include "../Boss/BossVfx.h"
#include "../Combat/Feedback.h"
#include "../Common/FluidChannels.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;

// 固定 tick の数（1 秒あたり）。段の長さ (F) を秒に直す。
constexpr float kFramesPerSecond = 60.0f;
constexpr float kTau = 2.0f * kPi;
// 流体への提出 ID（持ち主はボスの物体。ほかのボスの提出と重ねない）：とぐろの頭、煙の塊（kBossIntroPuffCapacity 個）、
// 刀身が引く煙（剣先・中ほど）、一振りが場の霧を押す力（kFogPushCount 個）、とぐろの吸い寄せ、立ち込める煙を流す力、
// 袖口の煙、鎧の部品の煙（kBossIntroPartCapacity 個）。
constexpr uint64_t kCoilHeadBase = 1400;
constexpr uint64_t kPuffBase = 1430;
constexpr uint64_t kTrailBase = 1460;
constexpr uint64_t kFogPushBase = 1470;
constexpr uint64_t kGatherPullId = 1480;
constexpr uint64_t kHazeDrainId = 1481;
constexpr uint64_t kSleeveId = 1482;
constexpr uint64_t kBlastBase = 1484;
// 登場の煙を消す吸収：立ち込める煙、とぐろの輪（kCoilClearCount 個を輪に並べる）、とぐろの域の全体。
constexpr uint64_t kHazeClearId = 1530;
constexpr uint64_t kCoilClearBase = 1532;
constexpr uint64_t kCoilWipeId = 1545;
constexpr uint64_t kTrailClearBase = 1550;
constexpr uint64_t kChargeBase = 1560;
constexpr uint64_t kChargePullId = 1569;
constexpr uint64_t kCoilBlastBase = 1570;
constexpr uint64_t kSplitBase = 1580;
constexpr int kCoilClearCount = 8;
constexpr uint64_t kPartSmokeBase = 1500;
constexpr int kFogPushCount = 8;
// 手応えの出し元の番号（ほかの手応えと重ねない）と、優先度（崩しの手応えより下）。
constexpr uint64_t kAccentCameraSource = 0x5444324A11ull;
constexpr uint64_t kAccentHitStopSource = 0x5444324A12ull;
// 白黒の閃き（Action Post）の送り手。口は 1 つなので、ほかの全画面の効果（速度感）より高い優先度で出す。
constexpr uint64_t kMonoSource = 0x5444324A13ull;
constexpr int32_t kMonoPriority = 10;
constexpr int kAccentPriority = Feedback::kBreakPriority - 10;
// 場の霧を押す高さ（地面から m）。立ち込める煙を中央へ流す力の点の高さ（地面から m）。
constexpr float kFogPushHeight = 0.3f;
constexpr float kHazeDrainHeight = 0.8f;

// 部品が浮き上がり始める割合と、浮き上がる長さ（形になる段の中で）。胴が先、草摺が続き、大袖・籠手、兜が最後。
// 拍にはしない：長く重ねて、いつの間にか形になっているように見せる。
struct Lift {
    float start;
    float length;
};
constexpr Lift kTorsoLift{0.00f, 0.30f};
constexpr float kTassetLiftFirst = 0.04f;
constexpr float kTassetLiftStep = 0.025f;
constexpr float kTassetLiftLength = 0.30f;
constexpr Lift kSodeLifts[2]{{0.12f, 0.30f}, {0.16f, 0.30f}};
constexpr Lift kCuffLifts[2]{{0.20f, 0.30f}, {0.23f, 0.30f}};
constexpr Lift kHeadLift{0.28f, 0.30f};
// 手が煙から現れる割合（形になる段の中で）。目が灯る割合。
constexpr Lift kHandLift{0.40f, 0.18f};
constexpr float kEyesShare = 0.90f;
// 立ち上がった後、継ぎ目から噴く胴の煙が収まる長さ（形になる段の中の割合）。
constexpr float kSurgeFade = 0.14f;
// 引きずる終わりの、鞭のように引き込まれる強さ（0 でなだらか）。
constexpr float kDragWhip = 0.45f;
// 周りの煙を吹き払う押しの数。
constexpr int kBlastCount = 12;
// 一振り：振り始める前に八相からさらに引く量（道のりに対する割合）と、引き始める、振り始めより前の長さ（一振りの段の中の
// 割合）。振る間の速まり方（0 で一定、1 で止まった所から速まる）。振り抜いた後に行き過ぎる量。振る長さのこの進みで、
// 拍（吹き払い・輪・停止）を出す。振り抜いた後、刀光を残す長さ（一振りの段の中の割合）。
constexpr float kSlashCock = 0.08f;
constexpr float kSlashCockLead = 0.08f;
constexpr float kSlashAccel = 0.5f;
constexpr float kSlashFollow = 0.10f;
constexpr float kSlashFire = 0.8f;
constexpr float kSwingTail = 0.04f;
// 八相：引く間に、さらに右へ捻る割合と腰を落とす量。左手が柄へ添え終わる割合（一振りの段の中。ここで目が閃く）。
// 待機へ戻る長さのうち、左手を離し終わる割合。一振りの段の始めで、抜いた道から一振りの道へ移す長さ。
constexpr float kCockTwist = 0.15f;
constexpr float kCockSink = 0.03f;
constexpr float kTwoHandArrive = 0.15f;
constexpr float kTwoHandRelease = 0.6f;
constexpr float kHighBlend = 0.08f;
// 残心の間に、さらに前へ傾く量 (rad)。斬った後、体を包む気配が引く長さ（一振りの段の中の割合）。
constexpr float kZanshinLean = 0.04f;
constexpr float kAuraFall = 0.08f;
// 斬り上げ：動き出しの間に、刺さっていた時の棟の向きから移す長さ（斬り上げの長さに対する割合）。道の向きを測る幅。
constexpr float kRipSpineBlend = 0.3f;
constexpr float kPathStep = 0.03f;
// 捻り・傾きに付いて回る割合：兜（捻り）・草摺（捻り・傾き）。胴・大袖・左手は丸ごと付く。傾きの軸の高さ（1 倍の体での
// 値。腰）。
constexpr float kHeadTwistShare = 0.45f;
constexpr float kTassetTwistShare = 0.5f;
constexpr float kTassetLeanShare = 0.3f;
constexpr float kLeanPivot = 1.9f;
// 確かめ用の記録：剣先がこれより動いたフレームだけ、刃の向きを測る (m)。
constexpr float kTraceMinStep = 0.05f;
// 見得に移る終わりで、少し行き過ぎて止まる量（道のりに対する割合）。
constexpr float kPoseSettle = 0.05f;
// 溜めの煙の筋：数の上限、出だしで濃くなる・終わりで薄れる進み（巻き込みの中の割合）、筋が煙を運ぶ割合、筋ごとの
// 出だしのずれ（巻き込みの中の割合）。とぐろの煙を引き込む所の高さ (m) と半径 (m)。
constexpr int kChargeCapacity = 8;
constexpr float kChargeFadeIn = 0.12f;
constexpr float kChargeFadeOut = 0.8f;
constexpr float kChargeCarry = 0.4f;
constexpr float kChargeStagger = 0.618f;
constexpr float kChargePullHeight = 0.8f;
constexpr float kChargePullRadius = 9.0f;
// 振り抜いた瞬間に、とぐろの域の煙を外へ押す数・輪の半径 (m)・高さ (m)・1 つの半径 (m)・上への割合。正面（プレイヤーの
// 側）へ押す強さを弱める割合（煙を正面へ押し寄せさせない）。
constexpr int kCoilBlastCount = 6;
constexpr float kCoilBlastFrontSoften = 0.7f;
// 正面の煙を左右へ割る押し：組の数、最初の組と組の間隔（ボスから正面へ m）、斬った線からの横のずれ (m)、1 つの半径 (m)。
constexpr int kSplitPairs = 3;
constexpr float kSplitFirst = 2.0f;
constexpr float kSplitStep = 2.5f;
constexpr float kSplitOffset = 1.4f;
constexpr float kSplitReach = 3.0f;
constexpr float kCoilBlastRing = 2.5f;
constexpr float kCoilBlastHeight = 1.0f;
constexpr float kCoilBlastReach = 5.0f;
constexpr float kCoilBlastLift = 0.0f;
// 吹き払ってから、煙を消し始めるまでの間（秒。輪が外へ広がるのを見せてから消す）と、消す強さが上がり切るまで（秒）。
constexpr float kClearDelay = 0.1f;
constexpr float kClearRamp = 0.25f;
// 立ち込める煙を消す吸収の半径（中央へ流す力の半径に対する倍率。縁でも効くように広く取る）。
constexpr float kHazeClearReach = 1.8f;
// とぐろの輪を消す吸収を並べる半径 (m)・高さ (m)・1 つの半径 (m)。舞台の中央（刀の煙）には掛けない。
constexpr float kCoilClearRing = 7.0f;
constexpr float kCoilClearHeight = 1.3f;
constexpr float kCoilClearRadius = 4.2f;
// 吹き払った後、舞台の中央（足元の煙の輪・刀の跡の煙の弧）に掛ける吸収の強さ（輪を消す強さに対する割合）。
constexpr float kCenterClearShare = 1.6f;
// その吸収の中心の高さ (m) と、半径（輪の半径に対する倍率）。
constexpr float kCenterClearHeight = 3.0f;
constexpr float kCenterClearReach = 1.3f;
// 2 つ目の衝撃の輪の遅れ (秒)。
constexpr float kSecondShockDelay = 0.14f;
// 憑く順（0＝最初、1＝最後）：胴、大袖、籠手、兜。草摺は種から決まる（kTassetOrderFrom〜kTassetOrderTo の間）。
constexpr float kTorsoOrder = 0.0f;
constexpr float kSodeOrders[2]{0.45f, 0.6f};
constexpr float kCuffOrders[2]{0.3f, 0.75f};
constexpr float kHeadOrder = 1.0f;
constexpr float kTassetOrderFrom = 0.1f;
constexpr float kTassetOrderTo = 0.9f;
// 憑く：煙が届いてから憑き終わるまでの長さ・引きずり始めるまでの間（引き寄せる段の中の割合）。
constexpr float kPossessBlend = 0.05f;
constexpr float kDragDelay = 0.05f;
// 引きずる：中央へ着く時刻の幅（引き寄せる段の中の割合。部品ごとにばらつく）、途中で滑り戻る量（道のりに対する割合）。
constexpr float kDragEndFrom = 0.74f;
constexpr float kDragEndTo = 1.0f;
constexpr float kDragSlip = 0.05f;
// 憑かれた部品の揺れ（rad）と速さ (rad/秒)。浮き上がった部品の漂いの速さ (rad/秒)。
constexpr float kPossessSway = 0.22f;
constexpr float kPossessSwayRate = 1.4f;
constexpr float kDriftRate = 0.9f;
// 煙の筋が中央を出る所（中央からの距離 m・地面からの高さ m）と、這う間のうねりの幅 (m)。
constexpr float kTendrilFrom = 1.2f;
constexpr float kTendrilHeight = 0.4f;
constexpr float kTendrilWave = 0.7f;
// 部品の煙が消える割合（形になる段の中で、ここから消え始めて、長さの間に消える）。
constexpr Lift kPartSmokeFade{0.42f, 0.18f};
// 構えた後（刀を抜く段の始め）、ばねで追っていた位置を付く所へ重ねる時間 (秒)。
constexpr float kLockSeconds = 0.5f;
// 握る前の右手：握りの軸（剣先の側）と手の甲の向き（局所）。
constexpr Vec3 kFreeHandAxis{0.05f, -0.45f, 0.89f};
constexpr Vec3 kFreeHandSpine{0.0f, 0.89f, 0.45f};
// 握って力を込める間の、刀の震え (m) と速さ（1 秒あたりの回数）。
constexpr float kDrawStrain = 0.02f;
constexpr float kStrainFrequency = 19.0f;
// 鎌首の間、帯が中央へ寄る割合。引き寄せる間、頭が昇り直す速さ（1 秒あたりの回数）。
constexpr float kRearPull = 0.35f;
constexpr float kClimbRate = 0.3f;
// 頭ごとのうねりの速さ (rad/秒) と、頭ごとの位相のずれ。濃さのむらの速さ (rad/秒)。
constexpr float kRadiusWaveRate = 0.37f;
constexpr float kHeightWaveRate = 0.83f;
constexpr float kStreamPhase = 2.1f;
constexpr float kFlickerRate = 1.3f;
// 頭の高さの下限（地面から m）。頭 1 つが煙を入れる点の数と、点の間隔（入れる半径に対する割合）。
constexpr float kCoilFloor = 0.35f;
constexpr int kCoilHeadParts = 3;
constexpr float kCoilPartSpacing = 0.4f;
// 手を描かない間の、手の大きさの下限（割合。0 にすると、手に付くエフェクトの変換が断られる）。
constexpr float kHiddenHandScale = 0.02f;
// 刀身が引く煙：刀身の中ほどの点（握りから剣先までの割合）。前のフレームとの間を埋める点の数（剣先・中ほどのそれぞれ）。
constexpr float kTrailMiddleShare = 0.6f;
constexpr int kTrailParts = 5;
// 一振りの煙の弧を消す吸収：目印を置く間隔 (m)・1 つの半径 (m)・強さ（輪を消す強さに対する倍率）。
constexpr float kTrailMarkSpacing = 5.0f;
constexpr float kTrailClearRadius = 4.0f;
constexpr float kTrailClearShare = 2.0f;

Quat AroundX(float radians) { return {std::sin(radians * 0.5f), 0.0f, 0.0f, std::cos(radians * 0.5f)}; }
Quat AroundY(float radians) { return {0.0f, std::sin(radians * 0.5f), 0.0f, std::cos(radians * 0.5f)}; }
Quat AroundZ(float radians) { return {0.0f, 0.0f, std::sin(radians * 0.5f), std::cos(radians * 0.5f)}; }
Quat AroundAxis(Vec3 axis, float radians) {
    const Vec3 n = Normalize(axis, {0.0f, 1.0f, 0.0f});
    const float s = std::sin(radians * 0.5f);
    return {n.x * s, n.y * s, n.z * s, std::cos(radians * 0.5f)};
}

// 向きを近い側で混ぜる（正規化した線形補間）。
Quat Blend(Quat a, Quat b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    const float sign = dot < 0.0f ? -1.0f : 1.0f;
    Quat q{a.x + (b.x * sign - a.x) * t, a.y + (b.y * sign - a.y) * t, a.z + (b.z * sign - a.z) * t,
           a.w + (b.w * sign - a.w) * t};
    const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length < kTiny)
        return b;
    return {q.x / length, q.y / length, q.z / length, q.w / length};
}

float Unit(float value) { return std::clamp(value, 0.0f, 1.0f); }

// 0〜1 の乱数（同じ鍵からは同じ値）。
float Hash(int key, int salt) {
    uint32_t h = static_cast<uint32_t>(key) * 374761393u + static_cast<uint32_t>(salt) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>((h ^ (h >> 16)) & 0xFFFFFFu) / 16777216.0f;
}

// 登場の基準（始めの足元と向き）の局所を世界へ。
struct Ground {
    const BossIntroState& state;

    Vec3 Point(Vec3 local) const { return Add(state.origin, RotateY(local, state.yaw)); }
    Vec3 Direction(Vec3 local) const { return RotateY(local, state.yaw); }
    Quat Turn(Vec3 degrees) const {
        return Multiply(AroundY(state.yaw + degrees.y * kRadiansPerDegree),
                        Multiply(AroundX(degrees.x * kRadiansPerDegree), AroundZ(degrees.z * kRadiansPerDegree)));
    }
};

// いまの段 phase の進み（0〜1）。まだ来ていない段は 0、過ぎた段は 1。
float Progress(const BossComponent& boss, BossIntroPhase phase) {
    const BossIntroState& state = boss.introState;
    if (state.phase != phase)
        return static_cast<int>(state.phase) > static_cast<int>(phase) ? 1.0f : 0.0f;
    const int frames = BossIntro::PhaseFrames(boss, phase);
    return frames > 0 ? Unit(state.phaseSeconds * kFramesPerSecond / static_cast<float>(frames)) : 1.0f;
}

// 斬る前の溜めの進み（0〜1）：刀を抜く段の始めから振り始めまで。吹き払った後は 0。
float ChargeOf(const BossComponent& boss) {
    const BossIntroSettings& s = boss.intro;
    const BossIntroState& state = boss.introState;
    if (state.fogBlown || (state.phase != BossIntroPhase::Draw && state.phase != BossIntroPhase::Flourish))
        return 0.0f;
    const float draw = static_cast<float>(BossIntro::PhaseFrames(boss, BossIntroPhase::Draw));
    const float hold =
        static_cast<float>(BossIntro::PhaseFrames(boss, BossIntroPhase::Flourish)) * Unit(s.slashHoldShare);
    const float done = Progress(boss, BossIntroPhase::Draw) * draw +
                       Progress(boss, BossIntroPhase::Flourish) *
                           static_cast<float>(BossIntro::PhaseFrames(boss, BossIntroPhase::Flourish));
    return Unit(done / std::max(1.0f, draw + hold));
}

float LiftProgress(float share, Lift lift) { return Smooth((share - lift.start) / std::max(kTiny, lift.length)); }

// 地面に置いてある部品（大きさは、付く先と同じ）。
BossBodyPlacement Rest(const Ground& ground, Vec3 position, Vec3 turn, const BossBodyPlacement& target) {
    BossBodyPlacement rest = target;
    rest.position = ground.Point(position);
    rest.rotation = ground.Turn(turn);
    rest.lengthScale = 1.0f;
    return rest;
}

// 形になる段の流れ：締まり具合・立ち上がり。
struct Shape {
    // ゆるさ（1＝ばらばらに漂う、0＝付く所に締まる）、高さ（うずくまった高さの割合〜1）、立ち上がりの進み（0〜1。
    // 時間に比例）と、その勢いの付いた進み（0〜1。出だしが速い）。
    float loose = 1.0f;
    float height = 1.0f;
    float burst = 0.0f;
    float rise = 0.0f;
};

// 低くうずくまった塊としてゆっくり集まり、重さを持って立ち上がりながら締まっていく（溜めや衝撃は付けない）。
Shape ShapeOf(const BossComponent& boss) {
    const BossIntroSettings& s = boss.intro;
    const float share = Progress(boss, BossIntroPhase::Assemble);
    const float span = std::max(kTiny, s.riseEndShare - s.riseStartShare);
    Shape shape;
    shape.burst = Unit((share - s.riseStartShare) / span);
    shape.rise = Smooth(shape.burst);
    // 立った高さを少し行き過ぎて、戻る。
    const float over = Unit((share - s.riseStartShare) / (1.5f * span));
    shape.height = Lerp(s.slumpHeight, 1.0f, shape.rise) + s.burstOvershoot * std::sin(kPi * over) * shape.rise;
    const float settle = Smooth((share - s.riseEndShare) / std::max(kTiny, s.looseSettleEndShare - s.riseEndShare));
    shape.loose = Lerp(1.0f, s.looseAfterBurst, shape.rise) * (1.0f - settle);
    return shape;
}

// うずくまった姿：付く所を、足元へ向けて低くし、前へのめらせ、前へ傾ける（height 1 で立った姿のまま）。
BossBodyPlacement Slump(const BossComponent& boss, BossBodyPlacement placement, float height) {
    if (height == 1.0f)
        return placement;
    const BossIntroSettings& s = boss.intro;
    const BossIntroState& state = boss.introState;
    const Vec3 forward = Forward(state.yaw);
    const float full = std::max(0.1f, boss.bodyHalfHeight * 2.0f);
    Vec3 rel = Sub(placement.position, state.origin);
    const float high = Unit(rel.y / full);
    rel.y *= height;
    placement.position = Add(Add(state.origin, rel), Mul(forward, s.slumpLean * std::max(0.0f, 1.0f - height) * high));
    // 上が前へ倒れる向きの軸（上 × 前）。
    const Vec3 axis{forward.z, 0.0f, -forward.x};
    placement.rotation = Multiply(
        AroundAxis(axis, s.slumpPitchDegrees * kRadiansPerDegree * std::max(0.0f, 1.0f - height)), placement.rotation);
    return placement;
}

// 鎧の部品 1 つ：煙の筋が這って来て憑く → 引きずられて中央へ → 煙の中で浮き上がってゆるく付く → 締まる。
// ばねで追うので、煙（付く所）の動きに遅れて付いて行き、行き過ぎて戻る。部品の煙を出す所と重みも決める。
BossBodyPlacement Possess(BossComponent& boss, int index, const BossBodyPlacement& rest, BossBodyPlacement target,
                          float order, Lift lift, const Shape& shape, float dt) {
    const BossIntroSettings& s = boss.intro;
    BossIntroState& state = boss.introState;
    BossIntroState::Part& part = state.parts[static_cast<size_t>(index)];
    const float gather = Progress(boss, BossIntroPhase::Gather);
    const float share = Progress(boss, BossIntroPhase::Assemble);
    const float clock = state.coilClock;
    const float k = static_cast<float>(index);

    // 憑く。
    const float possessAt = Lerp(s.possessFirstShare, s.possessLastShare, Unit(order));
    const float possessed = Smooth((gather - possessAt) / kPossessBlend);
    // 引きずる：止まりかけては進み、横へふらつく。
    const float dragStart = possessAt + kDragDelay;
    const float dragEnd = std::max(dragStart + 0.1f, Lerp(kDragEndFrom, kDragEndTo, Hash(index, 7)));
    const float d = Unit((gather - dragStart) / (dragEnd - dragStart));
    // 終わりは、鞭のように一気に引き込まれる。
    const float along =
        Unit(Lerp(Smooth(d), d * d * d, kDragWhip) - kDragSlip * std::sin(2.0f * kTau * d) * (1.0f - d));
    const Vec3 out = Normalize(Vec3{rest.position.x - state.origin.x, 0.0f, rest.position.z - state.origin.z},
                               Forward(kTau * Hash(index, 6)));
    const Vec3 side{out.z, 0.0f, -out.x};
    Vec3 pile = Add(state.origin, Mul(out, Lerp(s.pileRadiusMin, s.pileRadiusMax, Hash(index, 8))));
    pile.y = state.origin.y + s.dragCarryHeight;
    const float bob = std::sin(clock * 2.1f + 1.7f * k);
    Vec3 dragged = Lerp(rest.position, pile, along);
    dragged =
        Add(dragged, Mul(side, s.dragWobble * std::sin(d * kPi * 3.0f + kTau * Hash(index, 9)) * std::sin(kPi * d)));
    dragged.y = Lerp(rest.position.y + possessed * s.possessLift * (0.7f + 0.3f * bob), pile.y + 0.08f * bob, along);
    // 憑かれると、置いてあった向きのまま、ゆらゆら揺れる。
    const float sway = possessed * kPossessSway;
    const Quat draggedTurn =
        Multiply(AroundY(sway * std::sin(clock * kPossessSwayRate + 2.3f * k)),
                 Multiply(rest.rotation, AroundX(sway * 0.7f * std::sin(clock * kPossessSwayRate * 1.3f + 1.1f * k))));

    // 煙の中で浮き上がる：付く所はうずくまった姿から立ち上がり、部品はそこからずれて漂う（ゆるさが 0 になると締まる）。
    target = Slump(boss, target, shape.height);
    const float e = LiftProgress(share, lift);
    const float loose = shape.loose;
    const Vec3 scatter{Hash(index, 11) - 0.5f, Hash(index, 12) - 0.3f, Hash(index, 13) - 0.5f};
    const Vec3 drift{std::sin(clock * kDriftRate * (1.0f + 0.21f * k) + 1.3f * k),
                     std::sin(clock * kDriftRate * (0.8f + 0.13f * k) + 2.9f * k),
                     std::sin(clock * kDriftRate * (1.15f + 0.17f * k) + 0.6f * k)};
    const Vec3 offset = Mul(Add(Mul(scatter, 1.2f), Mul(drift, 0.4f)), s.looseOffset * loose);
    const float tilt = s.looseTiltDegrees * kRadiansPerDegree * loose;
    const Quat looseTurn =
        Multiply(AroundY(tilt * (Hash(index, 14) - 0.5f) * 2.0f + tilt * 0.4f * drift.x),
                 Multiply(target.rotation, AroundX(tilt * (Hash(index, 15) - 0.5f) * 2.0f + tilt * 0.4f * drift.z)));
    const Vec3 desired = Lerp(dragged, Add(target.position, offset), e);
    const Quat desiredTurn = Blend(draggedTurn, looseTurn, e);

    // ばねで追う。待つ間と、始めて置く時は、そのまま置く。
    if (!part.ready || state.phase == BossIntroPhase::Scattered || dt <= 0.0f) {
        part.position = desired;
        part.velocity = {};
        part.rotation = desiredTurn;
        part.ready = true;
    } else {
        // 立ち上がる間は、ばねを硬くする（遅れずに一気に引き上げられ、引き締まる）。
        SpringPoint(part.position, part.velocity, desired,
                    s.followFrequency * Lerp(1.0f, s.burstFollowScale, shape.rise), s.followDamping, dt);
        part.rotation = Blend(part.rotation, desiredTurn, Follow(dt, s.turnSeconds));
    }
    BossBodyPlacement placement = target;
    placement.position = part.position;
    placement.rotation = part.rotation;
    placement.lengthScale = Lerp(1.0f, target.lengthScale, e);
    // 構えた後は、付く所へ重ねる（握った手に付く籠手が、遅れて離れない）。
    if (static_cast<int>(state.phase) >= static_cast<int>(BossIntroPhase::Draw)) {
        const float lock = state.phase == BossIntroPhase::Draw ? Smooth(state.phaseSeconds / kLockSeconds) : 1.0f;
        placement.position = Lerp(part.position, target.position, lock);
        placement.rotation = Blend(part.rotation, target.rotation, lock);
        placement.lengthScale = target.lengthScale;
    }

    // 煙：憑く前は、煙の筋が中央から這って来る。憑いた後は、部品のいる所から煙が湧く（引きずった跡に尾が残る）。
    if (gather < possessAt) {
        const float crawl = (gather - (possessAt - s.tendrilShare)) / std::max(kTiny, s.tendrilShare);
        Vec3 from = Add(state.origin, Mul(out, kTendrilFrom));
        from.y = state.origin.y + kTendrilHeight;
        part.smokeAt = Add(Lerp(from, rest.position, Smooth(crawl)),
                           Mul(side, kTendrilWave * std::sin(Unit(crawl) * kPi * 2.5f + kTau * Hash(index, 10)) *
                                         std::sin(kPi * Unit(crawl))));
        part.smoke = crawl > 0.0f ? 0.8f : 0.0f;
    } else {
        part.smokeAt = placement.position;
        part.smoke = static_cast<int>(state.phase) >= static_cast<int>(BossIntroPhase::Draw)
                         ? 0.0f
                         : 1.0f - LiftProgress(share, kPartSmokeFade);
    }
    return placement;
}

// 刀の姿勢（世界）：握りの位置、剣先の向き、棟の向き。
struct Held {
    Vec3 grip{};
    Vec3 tip{0.0f, -1.0f, 0.0f};
    Vec3 spine{0.0f, 0.0f, 1.0f};
};

Held Mix(const Held& a, const Held& b, float t) {
    Held held;
    held.grip = Lerp(a.grip, b.grip, t);
    held.tip = Slerp(Normalize(a.tip), Normalize(b.tip), t);
    held.spine = Normalize(Lerp(a.spine, b.spine, Unit(t)), b.spine);
    return held;
}

BossBodyPlacement Place(const Held& held, float scale) {
    BossBodyPlacement placement;
    placement.position = held.grip;
    placement.rotation = AimZ(Normalize(held.tip), Perpendicular(held.spine, Normalize(held.tip)));
    placement.scale = scale;
    return placement;
}

Held HeldOf(const BossBodyPlacement& placement) {
    return {placement.position, Rotate(placement.rotation, {0.0f, 0.0f, 1.0f}),
            Rotate(placement.rotation, {0.0f, 1.0f, 0.0f})};
}

// 体の物体の変換で、体の局所（足元が原点）の点を世界へ（BossBodyRig の BodyFrame と同じ）。
Vec3 BodyToWorld(const BossComponent& boss, const HIKARI::GameProjectTransform& body, Vec3 local) {
    const Vec3 centered{local.x * body.scale.x, (local.y - boss.bodyHalfHeight) * body.scale.y, local.z * body.scale.z};
    return Add(body.position, Rotate(body.rotation, centered));
}

// 地面に刺さった刀。
Held StuckSword(const BossComponent& boss, const Ground& ground) {
    const BossIntroSettings& s = boss.intro;
    const float lean = s.swordLeanDegrees * kRadiansPerDegree;
    const Vec3 toward = ground.Direction(Forward(s.swordLeanYawDegrees * kRadiansPerDegree));
    // 柄の向き（地面の点から握りへ）。
    const Vec3 up = Normalize(Add(Vec3{0.0f, std::cos(lean), 0.0f}, Mul(toward, std::sin(lean))));
    const Vec3 enter = ground.Point(s.swordGround);
    const Vec3 edge = ground.Direction(Forward(s.swordEdgeYawDegrees * kRadiansPerDegree));
    Held stuck;
    stuck.grip = Add(enter, Mul(up, VisibleBladeReach(boss) - s.swordBuried));
    stuck.tip = Mul(up, -1.0f);
    stuck.spine = Mul(edge, -1.0f);
    return stuck;
}

// 抜刀と一振りの間の体の動き：胴の捻り (rad。正が左)、前への傾き (rad)、腰を落とす割合、左手を柄へ添える重み。
struct Motion {
    float twist = 0.0f;
    float lean = 0.0f;
    float sink = 0.0f;
    float twoHand = 0.0f;
};

Motion MixMotion(const Motion& a, const Motion& b, float t) {
    return {Lerp(a.twist, b.twist, t), Lerp(a.lean, b.lean, t), Lerp(a.sink, b.sink, t), Lerp(a.twoHand, b.twoHand, t)};
}

Motion MotionOf(const BossComponent& boss) {
    const BossIntroSettings& s = boss.intro;
    const BossIntroPhase phase = boss.introState.phase;
    const float deg = kRadiansPerDegree;
    const Motion strain{s.strainTwistDegrees * deg, s.strainLeanDegrees * deg, s.strainSink, 0.0f};
    const Motion high{s.highTwistDegrees * deg, s.highLeanDegrees * deg, 0.0f, 0.0f};
    if (phase == BossIntroPhase::Draw) {
        const float u = Progress(boss, BossIntroPhase::Draw);
        // 柄へ手を伸ばしながら刀の方へ傾き、握って力を込める間に沈む。斬り上げで体が起き、右へ捻って八相に収まる。
        if (u < s.drawHoldShare)
            return MixMotion({}, strain,
                             Smooth((u - s.drawTendrilShare) / std::max(kTiny, s.drawHoldShare - s.drawTendrilShare)));
        return MixMotion(strain, high,
                         Smooth((u - s.drawHoldShare) / std::max(kTiny, s.drawRipShare - s.drawHoldShare)));
    }
    if (phase != BossIntroPhase::Flourish)
        return {};
    const float u = Progress(boss, BossIntroPhase::Flourish);
    const float hold = Unit(s.slashHoldShare);
    const float strike = std::max(hold + kTiny, s.slashStrikeShare);
    const float rest = std::max(strike + kTiny, s.slashRestShare);
    Motion cock = high;
    cock.twist *= 1.0f + kCockTwist;
    cock.sink = kCockSink;
    cock.twoHand = 1.0f;
    const Motion cut{s.slashTwistDegrees * deg, s.slashLeanDegrees * deg, s.slashSink, 1.0f};
    if (u < hold) {
        // 八相で溜める：左手が柄へ添え、振り始める前に少し引いて沈む。
        Motion m = MixMotion(high, cock, Smooth((u - (hold - kSlashCockLead)) / kSlashCockLead));
        m.twoHand = Smooth(u / kTwoHandArrive);
        return m;
    }
    // 斬る：体は刀より少し先に回る。
    if (u < strike)
        return MixMotion(cock, cut, Smooth((u - hold) / (strike - hold) * 1.3f));
    // 残心：振り抜いた姿のまま、わずかに前へ沈み込む。
    if (u < rest) {
        Motion m = cut;
        m.lean += kZanshinLean * EaseOut((u - strike) / (rest - strike));
        return m;
    }
    // 見得：体を起こして胸を張り、刀を右の外へ振り払って（左手を離す）止まる。そのあと待機の構えへ戻る。
    const float pose = std::max(rest + kTiny, s.poseShare);
    const float release = std::max(pose, s.poseHoldShare);
    const Motion stance{s.poseTwistDegrees * deg, s.poseLeanDegrees * deg, 0.0f, 0.0f};
    if (u < release) {
        Motion end = cut;
        end.lean += kZanshinLean;
        Motion m = MixMotion(end, stance, Smooth((u - rest) / (pose - rest)));
        m.twoHand = 1.0f - Smooth((u - rest) / ((pose - rest) * kTwoHandRelease));
        return m;
    }
    return MixMotion(stance, {}, Smooth((u - release) / std::max(kTiny, 1.0f - release)));
}

// 斬り上げの進み（0〜1。動き出しで一気に動き、八相を少し行き過ぎて戻る）。
float RipAlong(float t, float overshoot) {
    t = Unit(t);
    const float rest = 1.0f - t;
    return 1.0f - rest * rest * rest + overshoot * std::sin(kPi * t) * t;
}

// v を、軸 axis（単位の向き）のまわりに angle (rad) 回す。
Vec3 RotateAbout(Vec3 v, Vec3 axis, float angle) {
    const float c = std::cos(angle), s = std::sin(angle);
    return Add(Add(Mul(v, c), Mul(Cross(axis, v), s)), Mul(axis, Dot(axis, v) * (1.0f - c)));
}

Quat AboutAxis(Vec3 axis, float angle) {
    const float s = std::sin(angle * 0.5f);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(angle * 0.5f)};
}

// 体の動きの、前への傾きの軸（捻った後の体の右の向き。正の回転で上が前へ倒れる）。
Vec3 LeanAxis(const BossIntroState& state, float twist) {
    return Normalize(Cross({0.0f, 1.0f, 0.0f}, RotateY(Forward(state.yaw), twist)), {1.0f, 0.0f, 0.0f});
}

// 体の部品の世界の点に、体の動き（足元のまわりの捻り → 腰のまわりの前への傾き → 腰を落とす）を掛ける。
// twistShare・leanShare：付いて回る割合。pivot：傾きの軸の、足元からの高さ (m)。
Vec3 BendPoint(const BossIntroState& state, const Motion& m, float pivot, Vec3 point, float twistShare,
               float leanShare) {
    const float twist = m.twist * twistShare;
    const Vec3 hip{0.0f, pivot, 0.0f};
    Vec3 rel = RotateY(Sub(point, state.origin), twist);
    rel = Add(hip, RotateAbout(Sub(rel, hip), LeanAxis(state, twist), m.lean * leanShare));
    rel.y *= 1.0f - m.sink;
    return Add(state.origin, rel);
}

Quat BendTurn(const BossIntroState& state, const Motion& m, float twistShare, float leanShare) {
    const float twist = m.twist * twistShare;
    return Multiply(AboutAxis(LeanAxis(state, twist), m.lean * leanShare), AroundY(twist));
}

// 両手で握る左手（BossBodyLeftHand の両手持ちと同じ置き方：右手の後ろの柄頭の側に、正握の拳。親指の側が剣先、手首が
// 左肩の側）。
BossBodyPlacement TwoHandLeft(const BossComponent& boss, const BossBodyPlacement& sword, Vec3 shoulder) {
    const BossBodyLeftHandSettings& s = boss.body.leftHand;
    const Vec3 x = Rotate(sword.rotation, {1.0f, 0.0f, 0.0f});
    const Vec3 y = Rotate(sword.rotation, {0.0f, 1.0f, 0.0f});
    const Vec3 z = Rotate(sword.rotation, {0.0f, 0.0f, 1.0f});
    const Vec3 handX = Mul(z, -1.0f);
    Vec3 back = Perpendicular(Add(Add(Mul(x, s.twoHandBack.x), Mul(y, s.twoHandBack.y)), Mul(z, s.twoHandBack.z)), z);
    const Vec3 toShoulder = ProjectOut(Sub(shoulder, sword.position), z);
    if (Length(toShoulder) > kTiny)
        back = Cross(Mul(Normalize(toShoulder), -1.0f), handX);
    const Vec3 fingers = Cross(handX, back);
    const float handScale = HandScale(boss);
    const Vec3 hilt = Sub(sword.position, Mul(z, s.twoHandBehind));
    const Vec3 fist = Add(Add(Mul(handX, s.fistCenter.x), Mul(back, s.fistCenter.y)), Mul(fingers, s.fistCenter.z));
    BossBodyPlacement hand;
    hand.position = Sub(hilt, Mul(fist, handScale));
    hand.rotation = AimZ(fingers, back);
    hand.scale = handScale;
    return hand;
}

// 一振りの道の上の姿勢：構え → 正面 → 振り抜きの 3 つを通る、折れ目の無い弧（e は 0〜1 の外でもよい）。
Held Sweep(const Held& wind, const Held& mid, const Held& end, float e) {
    const float a = 2.0f * (e - 0.5f) * (e - 1.0f), b = -4.0f * e * (e - 1.0f), c = 2.0f * e * (e - 0.5f);
    Held held = mid;
    held.grip = Add(Add(Mul(wind.grip, a), Mul(mid.grip, b)), Mul(end.grip, c));
    held.tip = Normalize(Add(Add(Mul(wind.tip, a), Mul(mid.tip, b)), Mul(end.tip, c)), mid.tip);
    return held;
}

// 登場の間の刀と右手（世界）。柄を握るまでは、右手は握る前の置き場所から柄へ向かう。
void SolveSword(const BossComponent& boss, const HIKARI::GameProjectTransform& body, const BossBodyPose& solved,
                Held& sword, Held& hand, Held& free) {
    const BossIntroSettings& s = boss.intro;
    const Ground ground{boss.introState};
    const Held stuck = StuckSword(boss, ground);
    free.grip = BodyToWorld(boss, body, BodyPoint(boss.body, s.rightRestWrist));
    free.tip = Normalize(Rotate(body.rotation, kFreeHandAxis));
    free.spine = Normalize(Rotate(body.rotation, kFreeHandSpine));
    const BossIntroPhase phase = boss.introState.phase;
    if (static_cast<int>(phase) < static_cast<int>(BossIntroPhase::Draw)) {
        sword = stuck;
        hand = free;
        return;
    }
    const float scale = boss.body.bodyScale;
    const float reach = VisibleBladeReach(boss);
    const auto local = [&](Vec3 grip, Vec3 tip) {
        Held held;
        held.grip = ground.Point(Mul(grip, scale));
        held.tip = Normalize(ground.Direction(tip));
        return held;
    };
    const Held ripMid = local(s.drawRipGrip, s.drawRipTip);
    const Held high = local(s.highGrip, s.highTip);
    const Held cutMid = local(s.slashMidGrip, s.slashMidTip);
    const Held cutEnd = local(s.slashEndGrip, s.slashEndTip);
    // 3 つの姿勢を通る道の上の姿勢。棟の向きは、剣先の進む向き（道の少し前後の差）から決める：lead が 1 なら棟が先
    // （振り上げる）、-1 なら刃が先（斬る）。
    const auto path = [&](const Held& a, const Held& b, const Held& c, float e, float lead) {
        Held held = Sweep(a, b, c, e);
        const Held before = Sweep(a, b, c, e - kPathStep);
        const Held after = Sweep(a, b, c, e + kPathStep);
        const Vec3 travel = Sub(Add(after.grip, Mul(after.tip, reach)), Add(before.grip, Mul(before.tip, reach)));
        held.spine = Mul(Perpendicular(travel, held.tip), lead);
        return held;
    };
    if (phase == BossIntroPhase::Draw) {
        const float u = Progress(boss, BossIntroPhase::Draw);
        const float start = Unit(s.drawTendrilShare);
        const float touch = std::max(start + kTiny, s.drawReachShare);
        const float hold = std::max(touch, s.drawHoldShare);
        const float rip = std::max(hold + kTiny, s.drawRipShare);
        sword = stuck;
        if (u < touch) {
            // 袖口の煙が柄へ流れ、手はその後を追って柄へ滑る（煙が先、手が後）。
            const float lag = (touch - start) * 0.3f;
            hand = Mix(free, stuck, Smooth((u - start - lag) / std::max(kTiny, touch - start - lag)));
            return;
        }
        if (u < hold) {
            // 握って力を込める：刀の震えが強まっていく。
            const float strain = kDrawStrain * (u - touch) / std::max(kTiny, hold - touch);
            const float t = boss.introState.coilClock * kTau * kStrainFrequency;
            sword.grip.x += strain * std::sin(t);
            sword.grip.z += strain * std::sin(t * 1.31f + 1.0f);
        } else {
            // 斬り上げ：地面を裂いて一気に抜き、そのまま体の右の外を通って右の肩の上へ斬り上げ、八相に構える（刃が先。
            // 八相に入る所で、刃を前へ返す）。
            // 動き出しは、刺さっていた時の棟の向きから移す（いきなり刃が返らない）。
            const float t = (u - hold) / (rip - hold);
            sword = path(stuck, ripMid, high, RipAlong(t, s.drawRipOvershoot), -1.0f);
            sword.spine = Normalize(Lerp(stuck.spine, sword.spine, Smooth(t / kRipSpineBlend)), sword.spine);
        }
        hand = sword;
        return;
    }
    // 一振り：八相で溜め（左手を柄へ添える）、少し引いてから、両手で袈裟に斬り下ろす（刃が先）。残心、待機の構えへ。
    const float u = Progress(boss, BossIntroPhase::Flourish);
    const float hold = Unit(s.slashHoldShare);
    const float strike = std::max(hold + kTiny, s.slashStrikeShare);
    const float rest = std::max(strike + kTiny, s.slashRestShare);
    float along = 0.0f;
    if (u < hold) {
        along = -kSlashCock * Smooth((u - (hold - kSlashCockLead)) / kSlashCockLead);
    } else if (u < strike) {
        const float t = (u - hold) / (strike - hold);
        along = Lerp(-kSlashCock, 1.0f, Lerp(t, t * t, kSlashAccel));
    } else {
        along = 1.0f + kSlashFollow * EaseOut((u - strike) / (rest - strike));
    }
    Held swing = path(high, cutMid, cutEnd, along, -1.0f);
    // 八相に入る所：抜いた道の終わりから、一振りの道へ移す。
    if (u < kHighBlend)
        swing = Mix(path(stuck, ripMid, high, 1.0f, -1.0f), swing, Smooth(u / kHighBlend));
    // 残心の後：刀を体の右の外へ一気に振り払う見得（血振り。刃は下。少し行き過ぎて止まる）、止まってから待機の構えへ戻る。
    const float pose = std::max(rest + kTiny, s.poseShare);
    const float release = std::max(pose, s.poseHoldShare);
    Held aim = local(s.poseGrip, s.poseTip);
    aim.spine = Perpendicular({0.0f, 1.0f, 0.0f}, aim.tip);
    const float t = Unit((u - rest) / (pose - rest));
    const float remain = 1.0f - t;
    const Held held = Mix(swing, aim, 1.0f - remain * remain * remain + kPoseSettle * std::sin(kPi * t) * t);
    sword = Mix(held, HeldOf(solved.sword), Smooth((u - release) / std::max(kTiny, 1.0f - release)));
    hand = sword;
}

// 草摺 k 枚目の、地面に置いてある所。
BossBodyPlacement TassetRest(const BossComponent& boss, const Ground& ground, int k, const BossBodyPlacement& target) {
    const BossIntroSettings& s = boss.intro;
    const int key = s.tassetSeed * 31 + k;
    // 向きは、枚数で割った扇の中で少しずらす（1 か所に固まらない）。
    const float angle =
        kTau * (static_cast<float>(k) + Hash(key, 1) * 0.8f) / static_cast<float>(std::max(1, boss.body.tassetCount));
    const float radius = Lerp(s.tassetRestRadiusMin, s.tassetRestRadiusMax, Hash(key, 2));
    const Vec3 position{std::sin(angle) * radius, s.tassetRestHeight, std::cos(angle) * radius};
    const Vec3 turn{-90.0f + (Hash(key, 3) - 0.5f) * 14.0f, Hash(key, 4) * 360.0f, (Hash(key, 5) - 0.5f) * 10.0f};
    return Rest(ground, position, turn, target);
}

// とぐろの煙の域へ煙を入れる。
void EmitSmoke(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id, Vec3 position,
               Vec3 velocity, float strength, float radius, float heat) {
    if (strength <= 0.0f)
        return;
    HIKARI::GameProjectFluidInteraction emit{};
    emit.kind = HIKARI::GameProjectFluidInteractionKind::Emit;
    emit.position = position;
    // extents.y は温度：煙の芯が暗い赤に光る（資産の域の発光）。
    emit.extents = {std::max(0.05f, radius), heat, 0.0f};
    emit.direction = velocity;
    emit.strength = std::min(10000.0f, strength);
    emit.channels = FluidChannels::kCoil;
    boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, emit).Accepted() && boss.fluidAccepted;
}

// 向きの無い吸い寄せ（中心へ流れ込む）。
void SubmitPull(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id, Vec3 position,
                float radius, float speed, uint32_t channels) {
    if (speed <= 0.0f)
        return;
    HIKARI::GameProjectFluidInteraction pull{};
    pull.kind = HIKARI::GameProjectFluidInteractionKind::Force;
    pull.position = position;
    pull.extents = {std::max(0.5f, radius), 0.0f, 0.0f};
    pull.strength = -speed;
    pull.channels = channels;
    boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, pull).Accepted() && boss.fluidAccepted;
}

// 煙の塊を 1 つ頼む（いちばん古い枠を使い回す）。
void QueuePuff(BossIntroState& state, Vec3 position, Vec3 velocity, float seconds, float radius, float density,
               float heat) {
    if (seconds <= 0.0f || density <= 0.0f)
        return;
    state.puffs[static_cast<size_t>(state.puffCursor)] = {position, velocity, seconds, radius, density, heat};
    state.puffCursor = (state.puffCursor + 1) % kBossIntroPuffCapacity;
}

// 立ち込める煙：登場が始まると湧き止み、中央へ流れて薄れる。登場が済んだら、消えるのを待って域を止める。
void RenderHaze(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float dt) {
    using namespace BossVfx;
    const BossIntroSettings& s = boss.intro;
    BossIntroState& state = boss.introState;
    if (s.hazeEffect.IsEmpty()) {
        StopHandle(world, state.hazeVfx);
        return;
    }
    if (state.phase == BossIntroPhase::Done) {
        if (!state.hazeVfx.IsValid())
            return;
        state.hazeIdleSeconds += dt;
        if (state.hazeIdleSeconds >= s.hazeFadeSeconds) {
            StopHandle(world, state.hazeVfx);
            return;
        }
    }
    // 一振りまで湧き続け（その間、中央へゆっくり流れる）、一振りで吹き払われて、湧き止む。
    const bool blown = state.fogBlown || static_cast<int>(state.phase) >= static_cast<int>(BossIntroPhase::Settle);
    const float gather = Progress(boss, BossIntroPhase::Gather);
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kAmountInput, blown ? 0.0f : 1.0f);
    StartOrSend(world, boss, state.hazeVfx, s.hazeEffect, Placed(state.origin, 1.0f), parameters);
    if (state.phase != BossIntroPhase::Scattered && !blown)
        SubmitPull(world, object, boss, kHazeDrainId, Add(state.origin, Vec3{0.0f, kHazeDrainHeight, 0.0f}),
                   s.hazeDrainRadius, s.hazeDrain * Smooth(gather * 4.0f) + s.chargeHazePull * Smooth(ChargeOf(boss)),
                   FluidChannels::kHaze);
    if (state.blastPending) {
        state.blastPending = false;
        for (int k = 0; k < kBlastCount && s.slashBlast > 0.0f; ++k) {
            const Vec3 out = Forward(kTau * static_cast<float>(k) / static_cast<float>(kBlastCount));
            HIKARI::GameProjectFluidInteraction push{};
            push.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
            push.position =
                Add(Add(state.origin, Mul(out, s.slashBlastRadius * 0.5f)), Vec3{0.0f, kHazeDrainHeight, 0.0f});
            push.extents = {s.slashBlastRadius, 0.0f, 0.0f};
            push.direction = out;
            push.strength = s.slashBlast;
            // 立ち込める煙と場の霧だけを払う（とぐろの域の煙を押すと、渦になって体の前へ巻き上がる）。
            push.channels = FluidChannels::kHaze | FluidChannels::kFog;
            boss.fluidAccepted =
                world.GetFluid().Submit(object.GetHandle(), kBlastBase + static_cast<uint64_t>(k), push).Accepted() &&
                boss.fluidAccepted;
        }
    }
    // 吹き払った後：輪が外へ広がるのを見せてから、吸収で消す（戦いの間に煙を残さない）。
    if (blown) {
        state.clearSeconds += dt;
        const float clear = Smooth((state.clearSeconds - kClearDelay) / kClearRamp);
        if (clear > 0.0f && s.hazeClearRate > 0.0f) {
            HIKARI::GameProjectFluidInteraction eat{};
            eat.kind = HIKARI::GameProjectFluidInteractionKind::Force;
            eat.position = Add(state.origin, Vec3{0.0f, kHazeDrainHeight, 0.0f});
            eat.extents = {s.hazeDrainRadius * kHazeClearReach, 0.0f, 0.0f};
            eat.absorption = s.hazeClearRate * clear;
            eat.channels = FluidChannels::kHaze;
            boss.fluidAccepted =
                world.GetFluid().Submit(object.GetHandle(), kHazeClearId, eat).Accepted() && boss.fluidAccepted;
        }
    }
}

} // namespace

void BossIntroVisuals::Advance(BossComponent& boss, float seconds) {
    BossIntroState& state = boss.introState;
    if (state.phase != state.seenPhase) {
        state.seenPhase = state.phase;
        state.phaseSeconds = 0.0f;
    }
    state.phaseSeconds += std::max(0.0f, seconds);
}

float BossIntroVisuals::SmokeScale(const BossComponent& boss) {
    const BossIntroSettings& s = boss.intro;
    const BossIntroPhase phase = boss.introState.phase;
    if (static_cast<int>(phase) < static_cast<int>(BossIntroPhase::Assemble))
        return 0.0f;
    if (phase != BossIntroPhase::Assemble)
        return 1.0f;
    const float u = Progress(boss, BossIntroPhase::Assemble);
    const float settle = std::max(0.05f, s.smokeSettleShare);
    // うずくまっている間は少なく、立ち上がる勢いで一気に満ちる（胴の煙は、立った高さの源から湧く）。
    const float span = std::max(kTiny, s.riseEndShare - s.riseStartShare);
    const float risen = Smooth((u - s.riseStartShare) / span);
    // 立ち上がった瞬間は、継ぎ目から噴き出して、すぐ収まる。
    const float spurt = risen * (1.0f - Smooth((u - s.riseEndShare) / kSurgeFade));
    return Smooth(u * 6.0f) * Lerp(s.smokeCrouchScale, Lerp(s.smokeColumnScale, 1.0f, Smooth(u / settle)), risen) *
           (1.0f + s.smokeBurstScale * spurt);
}

void BossIntroVisuals::ApplyPose(BossComponent& boss, const HIKARI::GameProjectTransform& body, BossBodyPose& pose,
                                 float seconds) {
    BossIntroState& state = boss.introState;
    state.trailWeight = 0.0f;
    state.sleeve = 0.0f;
    if (static_cast<int>(state.phase) >= static_cast<int>(BossIntroPhase::Settle)) {
        state.aura = 0.0f;
        state.bladeEdge = 0.0f;
        state.bladeSwing = false;
        for (BossIntroState::Part& part : state.parts)
            part.smoke = 0.0f;
        return;
    }
    const BossIntroSettings& s = boss.intro;
    const BossBodySettings& b = boss.body;
    const Ground ground{state};
    const BossBodyPose solved = pose;
    const float share = Progress(boss, BossIntroPhase::Assemble);
    const Shape shape = ShapeOf(boss);
    const float dt = std::max(0.0f, seconds);
    for (BossIntroState::Shock& shock : state.shocks)
        shock.delay -= dt;
    const Motion motion = MotionOf(boss);
    state.aura = 0.0f;
    state.bladeEdge = 0.0f;
    state.bladeSwing = false;

    // 刀と右手。
    Held sword, hand, free;
    SolveSword(boss, body, solved, sword, hand, free);
    pose.sword = Place(sword, SwordScale(boss));
    pose.rightHand = Place(hand, HandScale(boss));
    // 刺さっている間は、燃える刀身を使わない。
    pose.bladeHot = false;
    // 右の肩は、体の動き（捻り・傾き・腰を落とす）に付く。
    const float pivot = BodyPoint(b, {0.0f, kLeanPivot, 0.0f}).y;
    const Vec3 rightShoulder =
        BendPoint(state, motion, pivot, BodyToWorld(boss, body, BodyPoint(b, b.shoulder)), 1.0f, 1.0f);
    const float handScale = HandScale(boss);
    const Vec3 rightWrist = Add(pose.rightHand.position, Rotate(pose.rightHand.rotation, Mul(b.gripWrist, handScale)));
    const Vec3 gripElbow = Normalize(Rotate(pose.rightHand.rotation, b.gripElbow));
    const Vec3 rightElbow = Lerp(Normalize(Sub(rightShoulder, rightWrist), gripElbow), gripElbow, b.cuffGripBlend);
    const BossBodyPlacement rightCuff =
        Cuff(rightWrist, rightElbow, Rotate(pose.rightHand.rotation, b.gripBack), handScale, b, rightShoulder);

    // 鎧：煙に憑かれ、引きずられ、煙の中でゆるく形になる。
    int index = 0;
    const auto piece = [&](Vec3 position, Vec3 turn, const BossBodyPlacement& target, float order, Lift lift) {
        const BossBodyPlacement rest = Rest(ground, position, turn, target);
        return Possess(boss, index++, rest, target, order, lift, shape, dt);
    };
    pose.torso = piece(s.torsoRest, s.torsoRestTurn, solved.torso, kTorsoOrder, kTorsoLift);
    pose.sode[0] = piece(s.sodeRestRight, s.sodeRestRightTurn, solved.sode[0], kSodeOrders[0], kSodeLifts[0]);
    pose.sode[1] = piece(s.sodeRestLeft, s.sodeRestLeftTurn, solved.sode[1], kSodeOrders[1], kSodeLifts[1]);
    pose.rightCuff = piece(s.cuffRestRight, s.cuffRestRightTurn, rightCuff, kCuffOrders[0], kCuffLifts[0]);
    pose.leftCuff = piece(s.cuffRestLeft, s.cuffRestLeftTurn, solved.leftCuff, kCuffOrders[1], kCuffLifts[1]);
    for (int k = 0; k < pose.tassetCount && index < kBossIntroPartCapacity - 1; ++k) {
        const BossBodyPlacement& target = solved.tassets[static_cast<size_t>(k)];
        const BossBodyPlacement rest = TassetRest(boss, ground, k, target);
        const float order = Lerp(kTassetOrderFrom, kTassetOrderTo, Hash(s.tassetSeed * 17 + k, 21));
        const Lift lift{kTassetLiftFirst + kTassetLiftStep * static_cast<float>(k), kTassetLiftLength};
        pose.tassets[static_cast<size_t>(k)] = Possess(boss, index++, rest, target, order, lift, shape, dt);
    }
    {
        // 兜：立ち上がった後、左右を見回してから前を向く（目が灯るまで）。
        BossBodyPlacement target = solved.head;
        const float gaze = Unit((share - s.riseEndShare) / std::max(kTiny, kEyesShare - s.riseEndShare));
        const float look = s.headLookDegrees * kRadiansPerDegree * std::sin(gaze * kTau) * (1.0f - gaze);
        target.rotation = Multiply(AroundY(look), target.rotation);
        const BossBodyPlacement rest = Rest(ground, s.headRest, s.headRestTurn, target);
        pose.head = Possess(boss, index++, rest, target, kHeadOrder, kHeadLift, shape, dt);
    }
    // 手は、籠手が浮き上がった後に煙から現れる。うずくまっている間は、手も低い。描かない間も大きさは 0 にしない。
    const float hands = LiftProgress(share, kHandLift);
    pose.showHands = hands > 0.0f;
    if (state.phase == BossIntroPhase::Assemble) {
        pose.rightHand = Slump(boss, pose.rightHand, shape.height);
        pose.leftHand = Slump(boss, pose.leftHand, shape.height);
    }
    pose.rightHand.scale *= std::max(kHiddenHandScale, hands);
    pose.leftHand.scale *= std::max(kHiddenHandScale, hands);

    // 抜刀と一振りの間：胴を捻り、腰のまわりで傾け、腰を落とす（胴・大袖・左手は丸ごと、兜と草摺は少なめに付く）。
    if (motion.twist != 0.0f || motion.lean != 0.0f || motion.sink != 0.0f) {
        const auto bend = [&](BossBodyPlacement& placement, float twistShare, float leanShare) {
            placement.position = BendPoint(state, motion, pivot, placement.position, twistShare, leanShare);
            placement.rotation = Multiply(BendTurn(state, motion, twistShare, leanShare), placement.rotation);
        };
        bend(pose.torso, 1.0f, 1.0f);
        bend(pose.sode[0], 1.0f, 1.0f);
        bend(pose.sode[1], 1.0f, 1.0f);
        bend(pose.leftCuff, 1.0f, 1.0f);
        bend(pose.leftHand, 1.0f, 1.0f);
        bend(pose.head, kHeadTwistShare, 1.0f);
        for (int k = 0; k < pose.tassetCount; ++k)
            bend(pose.tassets[static_cast<size_t>(k)], kTassetTwistShare, kTassetLeanShare);
    }
    // 両手で握る：左手を右手の後ろ（柄頭の側）へ添える。籠手は左の肩の方へ伸ばす。
    if (motion.twoHand > 0.0f) {
        const Vec3 right = BodyPoint(b, b.shoulder);
        const Vec3 leftShoulder =
            BendPoint(state, motion, pivot, BodyToWorld(boss, body, {-right.x, right.y, right.z}), 1.0f, 1.0f);
        const BossBodyPlacement grip = TwoHandLeft(boss, pose.sword, leftShoulder);
        const float w = Smooth(motion.twoHand);
        pose.leftHand.position = Lerp(pose.leftHand.position, grip.position, w);
        pose.leftHand.rotation = Blend(pose.leftHand.rotation, grip.rotation, w);
        if (w > 0.5f)
            pose.leftShape = BossBodyHandShape::Fist;
        const Vec3 back = Rotate(pose.leftHand.rotation, {0.0f, 1.0f, 0.0f});
        pose.leftCuff = Cuff(pose.leftHand.position, Sub(leftShoulder, pose.leftHand.position), back, HandScale(boss),
                             b, leftShoulder);
    }
    // 目が灯る瞬間：弱い拍。
    if (state.phase == BossIntroPhase::Assemble && share >= kEyesShare && !state.eyesAccent) {
        state.eyesAccent = true;
        boss.bodyState.accents.flare = std::max(boss.bodyState.accents.flare, s.eyesFlare);
        state.accentPending = std::max(state.accentPending, s.accentSoftShare);
    }

    // 刀：袖口の煙が柄へ流れ、握って力を込め、抜ける瞬間に裂け目から煙が噴く（登場でただ 1 つの強い拍）。抜いてから
    // 弧が終わるまで刀身が煙を引き、弧の終わりに体が煙を吐いて、足元から輪が広がる。
    const float reach = VisibleBladeReach(boss);
    state.trailTip = Add(sword.grip, Mul(Normalize(sword.tip), reach));
    state.trailMiddle = Add(sword.grip, Mul(Normalize(sword.tip), reach * kTrailMiddleShare));
    if (state.phase == BossIntroPhase::Draw) {
        const float u = Progress(boss, BossIntroPhase::Draw);
        if (u >= s.drawTendrilShare && u < s.drawHoldShare) {
            // 煙は手より先に柄へ届き、握っている間は柄にまとわり付く。
            const float flow =
                Smooth((u - s.drawTendrilShare) / std::max(kTiny, (s.drawReachShare - s.drawTendrilShare) * 0.7f));
            const Held stuck = StuckSword(boss, ground);
            state.sleeveAt = Lerp(free.grip, stuck.grip, flow);
            state.sleeve = 1.0f;
        }
        // 斬り上げる間：刀光とゆがみ、刀身が煙の弧を引く。
        state.bladeSwing = u >= s.drawHoldShare && u < s.drawRipShare;
        state.trailWeight = state.bladeSwing && s.drawTrailScale > 0.0f ? 1.0f : 0.0f;
        state.trailScale = s.drawTrailScale;
        // 剣先が地面を離れた瞬間。
        if (u >= s.drawHoldShare && state.trailTip.y >= state.origin.y && !state.drawBurst) {
            state.drawBurst = true;
            QueuePuff(state, Add(ground.Point(s.swordGround), Vec3{0.0f, s.drawBurstRadius * 0.5f, 0.0f}),
                      {0.0f, s.drawBurstSpeed, 0.0f}, s.drawBurstSeconds, s.drawBurstRadius, s.drawBurstDensity,
                      s.bladeTrailHeat);
            // 抜く拍は、最後の一振りより弱く。
            state.accentPending = 0.6f;
            state.hitStopPending = s.drawHitStopFrames;
            state.shocks[0] = {ground.Point(s.swordGround), s.drawShock, 0.0f};
            state.glint = s.drawGlint;
        }
    } else if (state.phase == BossIntroPhase::Flourish) {
        const float u = Progress(boss, BossIntroPhase::Flourish);
        const float hold = Unit(s.slashHoldShare);
        const float strike = std::max(hold + kTiny, s.slashStrikeShare);
        const float rest = std::max(strike + kTiny, s.slashRestShare);
        // 八相で溜める間：刃口が明るくなり、体を包む気配が立ち上がる。斬る間：刀光とゆがみ、刀身が煙の弧を引く。
        const float charge = Smooth(u / std::max(kTiny, hold));
        const float spent = Smooth((u - strike) / (rest - strike));
        state.bladeEdge = s.slashEdge * charge * (1.0f - spent);
        state.aura = s.slashAura * charge * (1.0f - Smooth((u - strike) / kAuraFall));
        state.bladeSwing = u >= hold && u < strike + kSwingTail;
        state.trailWeight = state.bladeSwing ? 1.0f : 0.0f;
        state.trailScale = s.slashTrailScale;
        // 両手で握った瞬間：目が閃き、刀身を光が走る（見得）。
        if (u >= kTwoHandArrive && !state.slashCharge) {
            state.slashCharge = true;
            boss.bodyState.accents.flare = std::max(boss.bodyState.accents.flare, s.eyesFlare);
            state.glint = s.slashGlint;
            state.accentPending = std::max(state.accentPending, s.accentSoftShare);
        }
        // 斬り下ろす瞬間（登場でいちばん強い拍）：この一振りが、周りに立ち込めた煙を吹き払う。衝撃の輪を 2 つ、刀身の上の
        // 閃光、足元から広がる煙の輪、時間の停止、強い揺れ。
        const float fire = Lerp(hold, strike, kSlashFire);
        if (u >= fire && !state.flourishRing) {
            state.flourishRing = true;
            state.fogBlown = true;
            state.blastPending = true;
            state.fogPushPending = true;
            state.shocks[0] = {state.origin, s.slashShock, 0.0f};
            state.shocks[1] = {state.origin, s.slashShockSecond, kSecondShockDelay};
            state.flashPending = true;
            state.flashAt = state.trailMiddle;
            state.coilBlastPending = true;
            state.monoPending = true;
            state.glint = s.slashGlint;
            state.accentPending = 1.0f;
            state.hitStopPending = std::max(state.hitStopPending, s.slashHitStopFrames);
            const int count = std::clamp(s.flourishRingCount, 0, kBossIntroPuffCapacity);
            for (int k = 0; k < count; ++k) {
                const Vec3 out = Forward(kTau * static_cast<float>(k) / static_cast<float>(count));
                const Vec3 from = Add(state.origin, Mul(out, s.flourishRingStart));
                QueuePuff(state, Add(from, Vec3{0.0f, s.flourishRingRadius * 0.6f, 0.0f}),
                          Mul(out, s.flourishRingSpeed), s.flourishRingSeconds, s.flourishRingRadius,
                          s.flourishRingDensity, 0.0f);
            }
        }
    }
    // 見得に止まった瞬間：目が閃き、刀身を光が走る（弱い拍）。
    if (state.phase == BossIntroPhase::Flourish && Progress(boss, BossIntroPhase::Flourish) >= s.poseShare &&
        !state.poseAccent) {
        state.poseAccent = true;
        boss.bodyState.accents.flare = std::max(boss.bodyState.accents.flare, s.eyesFlare);
        state.glint = s.slashGlint;
        state.accentPending = std::max(state.accentPending, s.accentSoftShare);
    }
    // 確かめ用（Boss.bodyTraceMotion）：振っている間、刃の向きと剣先の動きの向きの合い方を記録に出す（刃が先なら +1、
    // 棟が先なら -1。斬り上げも斬り下ろしも刃が先に作ってある）。
    if (boss.body.motion.traceMotion) {
        if (state.bladeSwing && state.traceReady) {
            const Vec3 blade = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
            const Vec3 edge = Mul(Rotate(pose.sword.rotation, {0.0f, 1.0f, 0.0f}), -1.0f);
            const Vec3 travel = ProjectOut(Sub(state.trailTip, state.traceTip), blade);
            if (Length(travel) > kTraceMinStep) {
                char message[160]{};
                std::snprintf(message, sizeof(message), "IntroBlade tick=%llu phase=%d edgeLead=%.3f step=%.3f",
                              static_cast<unsigned long long>(boss.tick), static_cast<int>(state.phase),
                              static_cast<double>(Dot(edge, Normalize(travel))), static_cast<double>(Length(travel)));
                BossLog::Write(message);
            }
        }
        state.traceTip = state.trailTip;
        state.traceReady = true;
    }
    // 目は、形になる段の終わりに灯る。
    pose.eyesLit = solved.eyesLit && (static_cast<int>(state.phase) >= static_cast<int>(BossIntroPhase::Draw) ||
                                      (state.phase == BossIntroPhase::Assemble && share >= kEyesShare));
}

void BossIntroVisuals::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds) {
    using namespace BossVfx;
    const BossIntroSettings& s = boss.intro;
    BossIntroState& state = boss.introState;
    const float dt = std::max(0.0f, seconds);
    if (state.phase != BossIntroPhase::Done || state.coilVfx.IsValid())
        state.coilClock += dt;
    RenderHaze(world, object, boss, dt);
    const int streams = std::clamp(s.coilStreams, 0, kBossIntroCoilCapacity);
    // とぐろの煙を出す量：待つ間は 1。引き寄せる間に減らし（舞台が見えるように）、一振りで吹き払われるまで保つ。
    float amount = 0.0f;
    if (state.fogBlown || static_cast<int>(state.phase) >= static_cast<int>(BossIntroPhase::Settle))
        amount = 0.0f;
    else if (static_cast<int>(state.phase) < static_cast<int>(BossIntroPhase::Assemble))
        amount = Lerp(1.0f, s.gatherCoilShare, Smooth(Progress(boss, BossIntroPhase::Gather)));
    else
        amount = s.gatherCoilShare;
    if (s.coilEffect.IsEmpty()) {
        StopHandle(world, state.coilVfx);
        return;
    }
    // 登場の間（構えに収まるまで）は、鎧の部品・刀の煙もこの域へ入れるので、域を保つ。
    const bool busy = static_cast<int>(state.phase) < static_cast<int>(BossIntroPhase::Settle);
    if (!busy && amount <= 0.0f) {
        // 出し終えた後は、残った煙が消えるのを待って域を止める。
        if (state.coilVfx.IsValid()) {
            state.coilIdleSeconds += dt;
            if (state.coilIdleSeconds >= s.coilFadeSeconds)
                StopHandle(world, state.coilVfx);
        }
        if (!state.coilVfx.IsValid())
            return;
    } else {
        state.coilIdleSeconds = 0.0f;
    }
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kAmountInput, amount);
    // 輪の開いた側（資産の局所 +Z）を、プレイヤーのいる側へ向ける。
    StartOrSend(world, boss, state.coilVfx, s.coilEffect, Placed(state.origin, 1.0f, AroundY(state.yaw)), parameters);
    // 一振りで吹き払った後：とぐろの輪を吸収で消す（輪に並べる。舞台の中央の、刀の煙には掛けない）。構えた後は、
    // 域の全体を消す。
    if (state.fogBlown || static_cast<int>(state.phase) >= static_cast<int>(BossIntroPhase::Settle)) {
        const float clear = Smooth((state.clearSeconds - kClearDelay) / kClearRamp);
        HIKARI::GameProjectFluidInteraction eat{};
        eat.kind = HIKARI::GameProjectFluidInteractionKind::Force;
        eat.channels = FluidChannels::kCoil;
        eat.absorption = s.coilClearRate * clear;
        eat.extents = {kCoilClearRadius, 0.0f, 0.0f};
        for (int k = 0; k < kCoilClearCount && eat.absorption > 0.0f; ++k) {
            const Vec3 out = Forward(kTau * static_cast<float>(k) / static_cast<float>(kCoilClearCount));
            eat.position = Add(Add(state.origin, Mul(out, kCoilClearRing)), Vec3{0.0f, kCoilClearHeight, 0.0f});
            boss.fluidAccepted = world.GetFluid()
                                     .Submit(object.GetHandle(), kCoilClearBase + static_cast<uint64_t>(k), eat)
                                     .Accepted() &&
                                 boss.fluidAccepted;
        }
        // 刀の跡の煙の弧：入れた所に吸収を置いて消す（頭より高い所にも残るので、中央の吸収だけでは届かない）。
        eat.extents = {kTrailClearRadius, 0.0f, 0.0f};
        eat.absorption = s.coilClearRate * kTrailClearShare * clear;
        for (int k = 0; k < state.trailMarkCount && eat.absorption > 0.0f; ++k) {
            eat.position = state.trailMarks[static_cast<size_t>(k)];
            boss.fluidAccepted = world.GetFluid()
                                     .Submit(object.GetHandle(), kTrailClearBase + static_cast<uint64_t>(k), eat)
                                     .Accepted() &&
                                 boss.fluidAccepted;
        }
        // 舞台の中央（足元に残った煙の輪・刀の跡の煙の弧）も消す。構えた後は、域の全体を強く消す。
        if (clear > 0.0f && s.coilClearRate > 0.0f) {
            // 刀の跡の煙の弧は頭より高い所にも残るので、中央の吸収は高めに・広めに置く。
            eat.position = Add(state.origin, Vec3{0.0f, busy ? kCenterClearHeight : kCoilClearHeight, 0.0f});
            eat.extents = {kCoilClearRing * (busy ? kCenterClearReach : 2.0f), 0.0f, 0.0f};
            eat.absorption = s.coilClearRate * (busy ? kCenterClearShare : 2.0f) * clear;
            boss.fluidAccepted =
                world.GetFluid().Submit(object.GetHandle(), kCoilWipeId, eat).Accepted() && boss.fluidAccepted;
        }
    }
    // 斬る前の溜め：外から煙の筋が渦を巻いてボスへ巻き込み続け、とぐろの煙もボスの足元へ引き込まれる（だんだん強く）。
    const float charge = Smooth(ChargeOf(boss));
    if (charge > 0.0f) {
        state.chargeClock += dt;
        const int count = std::clamp(s.chargeStreams, 0, kChargeCapacity);
        const float spin = s.chargeSpinDegrees * kRadiansPerDegree;
        const auto at = [&](int k, float p) {
            const float radius = Lerp(s.chargeOuterRadius, s.chargeInnerRadius, p * p);
            const float angle = kTau * static_cast<float>(k) / static_cast<float>(std::max(1, count)) + spin * p;
            return Add(state.origin, Vec3{std::sin(angle) * radius, Lerp(s.chargeOuterHeight, s.chargeInnerHeight, p),
                                          std::cos(angle) * radius});
        };
        const float period = std::max(0.1f, s.chargeSeconds);
        for (int k = 0; k < count; ++k) {
            const float p = std::fmod(state.chargeClock / period + kChargeStagger * static_cast<float>(k), 1.0f);
            const Vec3 position = at(k, p);
            const Vec3 velocity = Mul(Sub(at(k, std::min(1.0f, p + 0.02f)), position), kChargeCarry / (0.02f * period));
            const float fade =
                Smooth(p / kChargeFadeIn) * (1.0f - Smooth((p - kChargeFadeOut) / (1.0f - kChargeFadeOut)));
            EmitSmoke(world, object, boss, kChargeBase + static_cast<uint64_t>(k), position, velocity,
                      s.chargeDensity * charge * fade, s.chargeRadius, 0.0f);
        }
        SubmitPull(world, object, boss, kChargePullId, Add(state.origin, Vec3{0.0f, kChargePullHeight, 0.0f}),
                   kChargePullRadius, s.chargePull * charge, FluidChannels::kCoil);
    }
    // 振り抜いた瞬間：ボスのまわりに集まった煙を斬り開く。まわりは輪に並べた外向きの押しで吹き飛ばし（正面へは弱く）、
    // 正面（プレイヤーとの間）の煙は左右へ割って、ボスとプレイヤーの間を開ける。
    if (state.coilBlastPending && s.slashCoilBlast > 0.0f) {
        state.coilBlastPending = false;
        const Vec3 front = Forward(state.yaw);
        const Vec3 side = Normalize(Cross({0.0f, 1.0f, 0.0f}, front), {1.0f, 0.0f, 0.0f});
        HIKARI::GameProjectFluidInteraction push{};
        push.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
        push.channels = FluidChannels::kCoil;
        const auto submit = [&](uint64_t id) {
            boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, push).Accepted() && boss.fluidAccepted;
        };
        for (int k = 0; k < kCoilBlastCount; ++k) {
            const Vec3 out =
                Forward(state.yaw + kTau * (static_cast<float>(k) + 0.5f) / static_cast<float>(kCoilBlastCount));
            push.position = Add(Add(state.origin, Mul(out, kCoilBlastRing)), Vec3{0.0f, kCoilBlastHeight, 0.0f});
            push.extents = {kCoilBlastReach, 0.0f, 0.0f};
            push.direction = Normalize(Add(out, Vec3{0.0f, kCoilBlastLift, 0.0f}));
            push.strength = s.slashCoilBlast * (1.0f - kCoilBlastFrontSoften * std::max(0.0f, Dot(out, front)));
            submit(kCoilBlastBase + static_cast<uint64_t>(k));
        }
        push.channels = FluidChannels::kCoil | FluidChannels::kHaze;
        push.extents = {kSplitReach, 0.0f, 0.0f};
        push.strength = s.slashCoilBlast;
        for (int k = 0; k < kSplitPairs; ++k) {
            const Vec3 line = Add(Add(state.origin, Mul(front, kSplitFirst + kSplitStep * static_cast<float>(k))),
                                  Vec3{0.0f, kCoilBlastHeight, 0.0f});
            for (int sign = 0; sign < 2; ++sign) {
                const Vec3 away = Mul(side, sign == 0 ? -1.0f : 1.0f);
                push.position = Add(line, Mul(away, kSplitOffset));
                push.direction = away;
                submit(kSplitBase + static_cast<uint64_t>(k * 2 + sign));
            }
        }
    }
    // 鎧の部品の煙（煙の筋・引きずった跡）と、袖口から柄へ流れる煙。
    for (int k = 0; k < kBossIntroPartCapacity; ++k) {
        const BossIntroState::Part& part = state.parts[static_cast<size_t>(k)];
        EmitSmoke(world, object, boss, kPartSmokeBase + static_cast<uint64_t>(k), part.smokeAt, {},
                  s.pieceSmokeDensity * part.smoke, s.pieceSmokeRadius, s.pieceSmokeHeat);
    }
    EmitSmoke(world, object, boss, kSleeveId, state.sleeveAt, {}, s.pieceSmokeDensity * state.sleeve,
              s.pieceSmokeRadius * 0.8f, s.pieceSmokeHeat);
    // 煙の塊（刀が抜ける瞬間・一振りの輪）。
    for (int k = 0; k < kBossIntroPuffCapacity; ++k) {
        BossIntroState::Puff& puff = state.puffs[static_cast<size_t>(k)];
        if (puff.seconds <= 0.0f)
            continue;
        puff.seconds -= dt;
        EmitSmoke(world, object, boss, kPuffBase + static_cast<uint64_t>(k), puff.position, puff.velocity, puff.density,
                  puff.radius, puff.heat);
    }
    // 刀身が引く煙の弧：前のフレームの位置との間を数点で埋める（速い一振りでも、弧が点に切れない）。振る間は、刀が
    // 動いた分だけ濃く入れ（止まった所に塊が溜まらない）、外へ流す（弧が波のように広がる）。
    if (state.trailWeight > 0.0f) {
        if (!state.trailLinked) {
            state.trailTipFrom = state.trailTip;
            state.trailMiddleFrom = state.trailMiddle;
            state.trailLinked = true;
        }
        const float radius = state.bladeSwing ? s.slashTrailRadius : s.bladeTrailRadius;
        const auto emit = [&](uint64_t base, Vec3 from, Vec3 to) {
            const float moved = state.bladeSwing ? Unit(Length(Sub(to, from)) / std::max(kTiny, radius)) : 1.0f;
            const float density = s.bladeTrailDensity * state.trailWeight *
                                  (state.bladeSwing ? state.trailScale * moved : 1.0f) /
                                  static_cast<float>(kTrailParts);
            for (int part = 0; part < kTrailParts; ++part) {
                const Vec3 at = Lerp(from, to, static_cast<float>(part + 1) / static_cast<float>(kTrailParts));
                const Vec3 out = FlatNormalize(Sub(at, state.origin), Forward(state.yaw));
                EmitSmoke(world, object, boss, base + static_cast<uint64_t>(part), at,
                          state.bladeSwing ? Mul(out, s.slashTrailSpeed) : Vec3{}, density, radius, s.bladeTrailHeat);
            }
        };
        emit(kTrailBase, state.trailTipFrom, state.trailTip);
        if (state.bladeSwing && state.trailMarkCount < kBossIntroTrailMarkCapacity &&
            (state.trailMarkCount == 0 ||
             Length(Sub(state.trailTip, state.trailMarks[static_cast<size_t>(state.trailMarkCount - 1)])) >=
                 kTrailMarkSpacing))
            state.trailMarks[static_cast<size_t>(state.trailMarkCount++)] = state.trailTip;
        emit(kTrailBase + static_cast<uint64_t>(kTrailParts), state.trailMiddleFrom, state.trailMiddle);
        state.trailTipFrom = state.trailTip;
        state.trailMiddleFrom = state.trailMiddle;
    } else {
        state.trailLinked = false;
    }
    // 一振り：足元の場の霧と残った煙を、輪に並べた向きのある押しで外へ（向きの無い膨張は霧を上へも持ち上げる）。
    if (state.fogPushPending) {
        state.fogPushPending = false;
        for (int k = 0; k < kFogPushCount && s.flourishFogPush > 0.0f; ++k) {
            const Vec3 out = Forward(kTau * static_cast<float>(k) / static_cast<float>(kFogPushCount));
            HIKARI::GameProjectFluidInteraction push{};
            push.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
            push.position =
                Add(Add(state.origin, Mul(out, s.flourishFogPushRadius * 0.5f)), Vec3{0.0f, kFogPushHeight, 0.0f});
            push.extents = {s.flourishFogPushRadius, 0.0f, 0.0f};
            push.direction = out;
            push.strength = s.flourishFogPush;
            push.channels = FluidChannels::kFog | FluidChannels::kCoil | FluidChannels::kHaze;
            boss.fluidAccepted =
                world.GetFluid().Submit(object.GetHandle(), kFogPushBase + static_cast<uint64_t>(k), push).Accepted() &&
                boss.fluidAccepted;
        }
    }
    // 引き寄せる間：とぐろの煙そのものを、中央へゆっくり吸い寄せる（既定は 0＝吸わない）。
    const float gather = Smooth(Progress(boss, BossIntroPhase::Gather));
    if (gather > 0.0f && amount > 0.0f)
        SubmitPull(world, object, boss, kGatherPullId, Add(state.origin, Vec3{0.0f, s.gatherPullHeight, 0.0f}),
                   s.gatherPullRadius, s.gatherPull * gather * amount, FluidChannels::kCoil);
    if (amount <= 0.0f || streams == 0)
        return;
    if (!state.coilReady) {
        for (int i = 0; i < kBossIntroCoilCapacity; ++i)
            state.coilAngle[static_cast<size_t>(i)] =
                kTau * static_cast<float>(i) / static_cast<float>(streams) + 0.7f * static_cast<float>(i);
        state.coilReady = true;
    }
    const float clock = state.coilClock;
    const Vec3 front = Forward(state.yaw);
    // 引き寄せる間、頭はボスの後ろへ寄る（体の前に煙が立たない。後ろに、形の無い煙の塊が育つ）。
    const Vec3 center = Add(state.origin, Mul(front, -s.mantleBack * gather));
    for (int i = 0; i < streams; ++i) {
        const float k = static_cast<float>(i);
        float& angle = state.coilAngle[static_cast<size_t>(i)];
        // 待つ間の頭：半径と高さがゆっくりうねり、ときどき 1 つが鎌首のように持ち上がる（後ろの側にいる間だけ）。
        float radius = std::max(0.3f, s.coilRadius + s.coilRadiusStep * k) +
                       s.coilRadiusWave * std::sin(clock * kRadiusWaveRate * (1.0f + 0.17f * k) + kStreamPhase * k);
        // 輪は頭ごとに違う向きへ傾く：巡りの角度に合わせて上下し、帯どうしが斜めに交わる。
        const float tiltPhase = kStreamPhase * k + s.coilTiltTurnDegrees * kRadiansPerDegree * clock;
        float height =
            s.coilHeight + s.coilHeightStep * k +
            radius * std::tan(s.coilTiltDegrees * kRadiansPerDegree) * std::sin(angle - tiltPhase) +
            s.coilHeightWave * std::sin(clock * kHeightWaveRate * (1.0f + 0.11f * k) + 1.3f * kStreamPhase * k);
        // プレイヤーの側にいる度合い（1＝真ん前、-1＝真後ろ）。
        const float facing = std::sin(angle) * front.x + std::cos(angle) * front.z;
        const float every = std::max(0.5f, s.coilRearEverySeconds);
        const float cycle = std::fmod(clock + every * k / static_cast<float>(streams), every);
        if (cycle < s.coilRearSeconds && state.phase == BossIntroPhase::Scattered && facing < 0.0f) {
            const float lift = std::sin(kPi * cycle / std::max(kTiny, s.coilRearSeconds)) * Smooth(-facing * 3.0f);
            height = Lerp(height, s.coilRearHeight, lift * lift);
        }
        // 引き寄せる間：頭が寄り、昇りながら少し速く巡る。
        const float climb = std::fmod(clock * kClimbRate + k / static_cast<float>(streams), 1.0f);
        radius = Lerp(radius, s.gatherRadius, gather);
        height = Lerp(height, s.gatherHeight * (0.15f + 0.85f * climb), gather);
        radius = std::max(0.15f, radius);
        const float speed = s.coilSpeed * Lerp(1.0f, s.gatherSpeedScale, gather);
        angle = std::fmod(angle + speed / radius * dt, kTau);
        const Vec3 position =
            Add(center, Vec3{std::sin(angle) * radius, std::max(kCoilFloor, height), std::cos(angle) * radius});
        const Vec3 forward{std::cos(angle), 0.0f, -std::sin(angle)};
        const Vec3 velocity = Mul(forward, speed * s.coilCarry);
        // 濃さのむら：帯の太さがゆっくり変わる。舞台の前を通る間は薄くする（寄せた後は、後ろにいるのでそのまま）。
        const float flicker = 1.0f + s.coilFlicker * std::sin(clock * kFlickerRate * (1.0f + 0.23f * k) + 1.9f * k);
        const float open = Lerp(Lerp(1.0f, s.coilFrontShare, Smooth((facing + 0.3f) / 0.8f)), 1.0f, gather);
        const float density = s.coilDensity * amount * open * std::max(0.15f, flicker);
        // 頭は、進む向きに少しずつずらした数点から入れる（域の解算が粗い刻みになった時も、帯と光が節に切れない）。
        for (int part = 0; part < kCoilHeadParts; ++part) {
            const float back = s.coilEmitRadius * kCoilPartSpacing * static_cast<float>(part);
            EmitSmoke(world, object, boss, kCoilHeadBase + static_cast<uint64_t>(i * kCoilHeadParts + part),
                      Sub(position, Mul(forward, back)), velocity, density / static_cast<float>(kCoilHeadParts),
                      s.coilEmitRadius, s.coilHeat);
        }
    }
}

void BossIntroVisuals::Feedback(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                                float unscaledSeconds) {
    const BossIntroSettings& s = boss.intro;
    BossIntroState& state = boss.introState;
    const uint64_t owner = object.GetHandle().value;
    if (state.accentPending > 0.0f) {
        Project_TD2_2::Feedback::CameraShakeParams shake{};
        shake.sourceId = kAccentCameraSource ^ owner;
        shake.priority = kAccentPriority;
        shake.frames = s.accentShakeFrames;
        shake.amplitude = s.accentShake * state.accentPending;
        shake.frequency = s.accentShakeFrequency;
        // 強い拍だけ、画角を一瞬広げる。
        shake.fovDegrees = state.accentPending >= 1.0f ? s.accentFovDegrees : 0.0f;
        Project_TD2_2::Feedback::StartCameraShake(state.shake, shake);
        if (state.accentPending >= 1.0f && s.accentRumble > 0.0f)
            Project_TD2_2::Feedback::Rumble(world, s.accentRumble, s.accentRumble * 0.6f, s.accentRumbleFrames);
        state.accentPending = 0.0f;
    }
    if (state.hitStopPending > 0) {
        Project_TD2_2::Feedback::RequestHitStop(world, kAccentHitStopSource ^ owner, kAccentPriority,
                                                state.hitStopPending, s.hitStopScale);
        state.hitStopPending = 0;
    }
    if (state.flashPending) {
        state.flashPending = false;
        if (s.slashFlashScale > 0.0f && !s.slashFlashEffect.IsEmpty()) {
            HIKARI::GameProjectVfxPlayRequest play{};
            play.asset = s.slashFlashEffect;
            play.transform = BossVfx::Placed(state.flashAt, s.slashFlashScale);
            play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
            BossVfx::Start(world, boss, play);
        }
    }
    // 白黒の閃き：反転した白黒 → 白黒 → 色へ戻る（実時間で進む。出す間は毎フレーム送る）。
    if (state.monoPending) {
        state.monoPending = false;
        state.monoSeconds = 0.0f;
    }
    if (state.monoSeconds >= 0.0f) {
        const float invert = std::max(0.0f, s.slashMonoInvertSeconds);
        const float hold = invert + std::max(0.0f, s.slashMonoHoldSeconds);
        const float end = hold + std::max(0.0f, s.slashMonoFadeSeconds);
        const float t = state.monoSeconds;
        if (t >= end || s.slashMonoStrength <= 0.0f || s.slashMonoProfile.empty()) {
            state.monoSeconds = -1.0f;
        } else {
            HIKARI::GameProjectActionPostFrameRequest request{};
            request.sourceId = kMonoSource ^ owner;
            request.priority = kMonoPriority;
            const float fade = t < hold ? 1.0f : 1.0f - Smooth((t - hold) / std::max(kTiny, end - hold));
            request.values[0] = {s.slashMonoStrength * fade, t < invert ? 1.0f : 0.0f, 0.0f, 0.0f};
            request.channelMasks[0] = 3u;
            world.GetPresentationVfx().SubmitActionPostProfile(s.slashMonoProfile, request);
            state.monoSeconds += std::max(0.0f, unscaledSeconds);
        }
    }
    Project_TD2_2::Feedback::SubmitCameraShake(world, state.shake, 0, unscaledSeconds);
}

void BossIntroVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    BossVfx::StopHandle(world, boss.introState.coilVfx);
    BossVfx::StopHandle(world, boss.introState.hazeVfx);
}

} // namespace Project_TD2_2
