#include "BossBodySword.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "../Boss/BossFrame.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossMoveTable.h"
#include "../Boss/BossPose.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;
using Transform = HIKARI::GameProjectTransform;

// ばねを 1 刻みで進める角の上限（角速度 × 刻み）。粗いと、陽に進めるばねが暴れる。刻みの数の上限。
constexpr float kMaxSpringStep = 0.5f;
constexpr int kMaxSubsteps = 64;
// 1 フレームの長さの上限 (秒)。止まった後の長いフレームでも一度に進めすぎない（その分ゆっくり見える）。
constexpr float kMaxFrameSeconds = 0.1f;
// 足元がこの速さ (m/秒) より速く動いたら瞬間移動と見なし、刀を目標へそろえる（突進の出現・霞隠れ）。
constexpr float kTeleportSpeed = 25.0f;
// 規則の剣が 1 tick でこの角度 (度) より大きく変わったら、振りでなく構えの切り替え（状態の変わり目）と見なし、
// 速さに数えない（数えると、刃が切り替えの向きへ一瞬で向いてしまう）。
constexpr float kRuleJumpDegrees = 40.0f;
// 溜めの刃：刀が「斬る先」とほぼ同じ向きの時は向ける先が決まらない。直交する成分がこれより短ければ前の向きを保つ。
constexpr float kEdgeGoalMinLength = 0.15f;
// 待機の構えの重みがこれより大きい時に、刃を待機の向きへ戻す（溜めの始めは待機の刃のまま）。
constexpr float kCalmEdgeShare = 0.5f;
// 剣先までの長さの下限 (m)。ずれの角度を出す時に 0 で割らない。
constexpr float kMinTipLength = 0.1f;
// 溜めの震えの 2 本目の揺れ：速さの比と位相（縦と横が同じ揺れに揃わないように）。
constexpr float kTremorRatio = 1.37f;
constexpr float kTremorPhase = 1.1f;
// 割る数・時間の下限（0 で割らない）。
constexpr float kTiny = 1e-4f;

Vec3 ProjectOut(Vec3 v, Vec3 axis) { return Sub(v, Mul(axis, Dot(v, axis))); }

float AngleBetween(Vec3 a, Vec3 b) { return std::atan2(Length(Cross(a, b)), Dot(a, b)); }

// a から b への大円の接ベクトル（長さ＝角度 rad）。真逆の時は向きが決まらないので 0。
Vec3 LogMap(Vec3 a, Vec3 b) {
    const Vec3 tangent = ProjectOut(b, a);
    const float s = Length(tangent);
    if (s < 1e-6f)
        return {};
    return Mul(tangent, std::atan2(s, std::clamp(Dot(a, b), -1.0f, 1.0f)) / s);
}

// a から接ベクトル v（長さ＝角度）だけ大円に沿って進んだ向き。
Vec3 ExpMap(Vec3 a, Vec3 v) {
    const float angle = Length(v);
    if (angle < 1e-6f)
        return a;
    return Normalize(Add(Mul(a, std::cos(angle)), Mul(v, std::sin(angle) / angle)), a);
}

Vec3 Slerp(Vec3 a, Vec3 b, float t) { return ExpMap(a, Mul(LogMap(a, b), t)); }

Vec3 ClampLength(Vec3 v, float maximum) {
    const float length = Length(v);
    return length > maximum && length > kTiny ? Mul(v, maximum / length) : v;
}

// 時間 seconds で追う時の、1 フレームで縮める割合。
float Follow(float dt, float seconds) { return 1.0f - std::exp(-dt / std::max(kTiny, seconds)); }

// 刀に直交する向き（刃・棟）。直交する成分が無ければ、刀に直交する何かの向きを返す。
Vec3 Perpendicular(Vec3 v, Vec3 axis) {
    const Vec3 fallback = std::abs(axis.y) < 0.9f ? Vec3{0.0f, -1.0f, 0.0f} : Vec3{0.0f, 0.0f, 1.0f};
    return Normalize(ProjectOut(v, axis), Normalize(ProjectOut(fallback, axis)));
}

// edge を axis まわりに goal の方へ、近い回り（180 度以内）で share の割合だけ回す。
Vec3 TurnAbout(Vec3 edge, Vec3 goal, Vec3 axis, float share) {
    const float angle = std::atan2(Dot(Cross(edge, goal), axis), Dot(edge, goal)) * share;
    return Normalize(Add(Mul(edge, std::cos(angle)), Mul(Cross(axis, edge), std::sin(angle))), edge);
}

// 局所 +Z を direction、+Y を spine（direction に直交する成分）へ向ける回転。
Quat AimZ(Vec3 direction, Vec3 spine) {
    const Vec3 side = Normalize(Cross(spine, direction), {1.0f, 0.0f, 0.0f});
    return BasisRotation(side, Cross(direction, side));
}

// 手首に付く籠手の筒：筒は手首から肘の向き（局所 -Z）へ伸び、腕の甲（局所 +Y）は手の甲の側を向く。
BossBodyPlacement Cuff(Vec3 wrist, Vec3 elbow, Vec3 back, float scale) {
    BossBodyPlacement cuff;
    cuff.position = wrist;
    cuff.rotation = AimZ(Mul(Normalize(elbow), -1.0f), back);
    cuff.scale = scale;
    return cuff;
}

const BossMove* CurrentMove(const BossComponent& boss) {
    const auto& moves = boss.table.GetMoves();
    if (boss.state != BossState::Act || boss.move < 0 || static_cast<size_t>(boss.move) >= moves.size())
        return nullptr;
    return &moves[static_cast<size_t>(boss.move)];
}

// 次の振り（命中の窓）が始まるまでの F。もう振らない時は -1。
int FramesToWindow(const BossComponent& boss, const BossMove& move) {
    size_t next = 0;
    int frames = 0;
    if (boss.phase == BossActPhase::Pre) {
        // 攻撃中の 0 F 目から振る。残りの前隙と、まだ止まる溜めの分だけ先。
        frames = move.pre - boss.stateFrame + (boss.stateFrame <= boss.windupHoldAt ? boss.windupHold : 0);
    } else if (boss.phase == BossActPhase::Act) {
        const BossSegmentProgress g = BossPose::Progress(boss, move);
        frames = move.segments[static_cast<size_t>(g.segment)].length - g.tIn;
        next = static_cast<size_t>(g.segment) + 1;
    } else {
        return -1;
    }
    for (size_t k = next; k < move.segments.size(); ++k) {
        if (move.segments[k].hit)
            return std::max(0, frames);
        frames += move.segments[k].length;
    }
    return -1;
}

// 1 フレームの目標（向き・位置は体の向きの局所。足元が原点）。
struct MotionTargets {
    Vec3 ruleDirection{};
    Vec3 ruleRoot{};
    Vec3 direction{};
    Vec3 grip{};
    // 規則の線へ寄せる重み（振りの直前 0→1、窓の間 1）・溜めの誇張の重み・待機の構えの重み・窓の間の腕の伸び (m)。
    float lead = 0.0f;
    float charge = 0.0f;
    float calm = 0.0f;
    float reach = 0.0f;
    // 剣先までの長さ (m。規則の付け根から)。
    float tipLength = 1.0f;
    bool window = false;
    // 溜めている（前隙・連撃の間の構え）。
    bool charging = false;
};

// 待機の構え：下段で、息に合わせて握りと剣先が少し上下する。
void Kamae(const BossComponent& boss, Vec3& direction, Vec3& grip) {
    const BossBodyMotionSettings& s = boss.body.motion;
    const float breath = std::sin(2.0f * kPi * s.breathFrequency * boss.bodyState.clock);
    const Vec3 base = Normalize(s.idleDirection);
    const Vec3 up = Perpendicular({0.0f, 1.0f, 0.0f}, base);
    direction = ExpMap(base, Mul(up, s.breathDegrees * kRadiansPerDegree * breath));
    grip = Add(s.idleGrip, Vec3{0.0f, s.breathMeters * breath, 0.0f});
}

// 待機の構えを混ぜる重み。待機・溜めの始めは「規則の剣が下ろした構えに近いほど」、後隙は後半で待機の構えへ戻す。
float CalmWeight(const BossComponent& boss, const BossMove* move, bool swing, float rest) {
    if (boss.state == BossState::Idle)
        return rest;
    if (move == nullptr)
        return 0.0f;
    if (boss.phase == BossActPhase::Pre)
        return rest;
    if (boss.phase == BossActPhase::Act)
        return swing ? 0.0f : rest;
    const float hold = std::clamp(boss.body.motion.postHoldShare, 0.0f, 1.0f);
    return Smooth((Share(boss.stateFrame, move->post) - hold) / std::max(kTiny, 1.0f - hold));
}

MotionTargets Targets(const BossComponent& boss) {
    const BossBodyMotionSettings& s = boss.body.motion;
    MotionTargets t;
    t.ruleDirection = RotateY(BossPose::SwordLine(boss, BossPose::Target(boss)).direction, -boss.yaw);
    // BossSwordPose の既定値は、規則の「下ろした構え」（BossPose の kRest）と同じ。
    const Vec3 rest = RotateY(BossPose::SwordLine(boss, BossSwordPose{}).direction, -boss.yaw);
    t.ruleRoot = boss.swordShoulder;
    t.window = boss.swordSmoke;
    const float restWeight =
        1.0f - Smooth(AngleBetween(t.ruleDirection, rest) / std::max(kTiny, s.restBlendDegrees * kRadiansPerDegree));
    const BossMove* move = CurrentMove(boss);
    const bool swing =
        move != nullptr && BossMoveTable::IsSwingType(move->type) && !BossMoveTable::IsMoveType(move->type);
    BossSegmentProgress g{};
    if (move != nullptr && boss.phase == BossActPhase::Act)
        g = BossPose::Progress(boss, *move);
    t.calm = CalmWeight(boss, move, swing, restWeight);
    if (swing) {
        const int frames = t.window ? 0 : FramesToWindow(boss, *move);
        t.lead = frames < 0
                     ? 0.0f
                     : std::clamp(1.0f - static_cast<float>(frames) / static_cast<float>(std::max(1, s.lockLeadFrames)),
                                  0.0f, 1.0f);
        t.charging = !t.window && (boss.phase == BossActPhase::Pre || (boss.phase == BossActPhase::Act && !g.hit));
        if (t.charging)
            t.charge = (1.0f - restWeight) * (1.0f - t.lead);
        if (t.window)
            t.reach = s.armReach * Smooth(g.progress / std::max(0.05f, s.armReachPeak));
    }
    t.tipLength = std::max(kMinTipLength, boss.swordLength + t.reach);
    Vec3 kamaeDirection{}, kamaeGrip{};
    Kamae(boss, kamaeDirection, kamaeGrip);
    t.direction = Slerp(t.ruleDirection, kamaeDirection, t.calm);
    const Vec3 ruleGrip = Add(t.ruleRoot, Mul(t.ruleDirection, boss.body.swordGripAlong));
    // 溜め：規則の振り上げの上で、握りを剣の向きへさらに送り出し、持ち上げる（手が高く・遠く、輪郭が大きく見える）。
    const Vec3 charged = Add(Mul(t.ruleDirection, s.chargeReach), Vec3{0.0f, s.chargeLift, 0.0f});
    t.grip = Add(Lerp(ruleGrip, kamaeGrip, t.calm), Mul(charged, t.charge));
    return t;
}

// 規則の剣の向きの速さを、固定 tick が進んだ時に測る（描画フレームの間は前の速さのまま）。
void MeasureRule(const BossComponent& boss, Vec3 rule, BossBodyMotionState& m) {
    if (m.ready && boss.tick != m.ruleTick) {
        const float elapsed = boss.clockSeconds - m.ruleClock;
        const float ticks = static_cast<float>(boss.tick - m.ruleTick);
        const Vec3 step = LogMap(m.ruleDirection, rule);
        const bool jump = Length(step) > kRuleJumpDegrees * kRadiansPerDegree * ticks;
        m.ruleVelocity = jump || elapsed <= 0.0f ? Vec3{} : ProjectOut(Mul(step, 1.0f / elapsed), rule);
    } else if (!m.ready) {
        m.ruleVelocity = {};
    }
    m.ruleTick = boss.tick;
    m.ruleClock = boss.clockSeconds;
    m.ruleDirection = rule;
}

// 単位ベクトルの向きのばね（速さは接ベクトル）。短い刻みに分けて進める。
void SpringOnSphere(Vec3& direction, Vec3& velocity, Vec3 target, float frequency, float damping, float dt) {
    const float omega = 2.0f * kPi * frequency;
    const int steps = std::clamp(static_cast<int>(std::ceil(dt * omega / kMaxSpringStep)), 1, kMaxSubsteps);
    const float h = dt / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        const Vec3 pull = Sub(Mul(LogMap(direction, target), omega * omega), Mul(velocity, 2.0f * damping * omega));
        velocity = Add(velocity, Mul(pull, h));
        const Vec3 next = ExpMap(direction, Mul(velocity, h));
        velocity = ProjectOut(velocity, next);
        direction = next;
    }
}

// 位置のばね（StepSway と同じ形）。
void SpringPoint(Vec3& position, Vec3& velocity, Vec3 target, float frequency, float damping, float dt) {
    const float omega = 2.0f * kPi * frequency;
    const int steps = std::clamp(static_cast<int>(std::ceil(dt * omega / kMaxSpringStep)), 1, kMaxSubsteps);
    const float h = dt / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        const Vec3 pull = Sub(Mul(Sub(target, position), omega * omega), Mul(velocity, 2.0f * damping * omega));
        velocity = Add(velocity, Mul(pull, h));
        position = Add(position, Mul(velocity, h));
    }
}

void StepDirection(const BossBodyMotionSettings& s, const MotionTargets& t, BossBodyMotionState& m, float dt) {
    if (t.window) {
        // 振っている間：規則の剣の速さで進め、規則の線へ引き寄せ、許すずれの内へ収める。速さも規則の剣に揃える
        // （窓が閉じた瞬間、その勢いのまま行き過ぎて戻る）。
        m.velocity = ProjectOut(m.ruleVelocity, m.direction);
        m.direction = ExpMap(m.direction, Mul(m.velocity, dt));
        m.direction = Slerp(m.direction, t.ruleDirection, Follow(dt, 1.0f / (2.0f * kPi * s.swordHitFrequency)));
        const float maxAngle = std::max(0.0f, s.hitMaxTipGap) / t.tipLength;
        const Vec3 gap = LogMap(t.ruleDirection, m.direction);
        if (Length(gap) > maxAngle)
            m.direction = ExpMap(t.ruleDirection, Mul(Normalize(gap), maxAngle));
        m.velocity = ProjectOut(m.velocity, m.direction);
        return;
    }
    if (m.wasWindow)
        m.velocity = Mul(m.velocity, s.overshootCarry);
    SpringOnSphere(m.direction, m.velocity, t.direction, Lerp(s.swordFrequency, s.swordHitFrequency, t.lead),
                   Lerp(s.swordDamping, s.swordHitDamping, t.lead), dt);
}

// 刃の向き：振る間は剣先の動きの先、溜めの間はこれから斬る向き、待機では待機の向き。ほかは前の向きを保つ
// （振り抜いて行き過ぎる間も、振り終わりの刃の向きのまま。裏返らない）。刀身の軸まわりに、近い回りで回す。
void StepEdge(const BossBodyMotionSettings& s, const MotionTargets& t, BossBodyMotionState& m, float dt) {
    m.edge = Perpendicular(m.edge, m.direction);
    float seconds = s.edgeTurnSeconds;
    if (t.window) {
        if (Length(m.velocity) > s.edgeMinDegreesPerSecond * kRadiansPerDegree)
            m.edgeGoal = Normalize(m.velocity);
    } else if (t.charging && t.calm < kCalmEdgeShare) {
        const Vec3 toward = ProjectOut(Normalize(s.chargeEdgeToward), m.direction);
        if (Length(toward) > kEdgeGoalMinLength)
            m.edgeGoal = Normalize(toward);
    } else if (t.calm > kCalmEdgeShare) {
        m.edgeGoal = Perpendicular(s.idleEdge, m.direction);
        seconds = s.edgeIdleSeconds;
    }
    m.edge = TurnAbout(m.edge, Perpendicular(m.edgeGoal, m.direction), m.direction, Follow(dt, seconds));
}

void StepGrip(const BossComponent& boss, const MotionTargets& t, BossBodyMotionState& m, float dt) {
    const BossBodyMotionSettings& s = boss.body.motion;
    const float along = boss.body.swordGripAlong;
    if (t.window) {
        // 振っている間：握りは規則の剣の線の上（腕を伸ばす分だけ線に沿って外へ）。
        const Vec3 grip = Add(t.ruleRoot, Mul(m.direction, along + t.reach));
        m.gripVelocity = Mul(Sub(grip, m.grip), 1.0f / dt);
        m.grip = grip;
        return;
    }
    if (m.wasWindow)
        m.gripVelocity = Mul(m.gripVelocity, s.overshootCarry);
    SpringPoint(m.grip, m.gripVelocity, t.grip, s.gripFrequency, s.gripDamping, dt);
    // 振りの直前：握りを規則の線の上へ寄せる（窓が開く時に跳ばない）。
    if (t.lead > 0.0f)
        m.grip = Lerp(m.grip, Add(t.ruleRoot, Mul(m.direction, along)), t.lead);
}

float TwistTarget(const BossBodyMotionSettings& s, Vec3 direction) {
    const float limit = s.twistMaxDegrees * kRadiansPerDegree;
    return std::clamp(direction.x * s.twistDegrees * kRadiansPerDegree, -limit, limit);
}

// 兜の向き（体の向きからの rad）：プレイヤーの方へ、上限の内で。気絶・撃破・プレイヤー無しは正面。
float HeadTarget(const BossComponent& boss) {
    const BossBodyState& state = boss.bodyState;
    if (!state.lookTargetSeen || boss.state == BossState::Stunned || boss.state == BossState::Dead)
        return 0.0f;
    const Vec3 offset = Sub(state.lookTarget, boss.ground);
    if (FlatLength(offset) < kTiny)
        return 0.0f;
    const float limit = boss.body.motion.headTurnMaxDegrees * kRadiansPerDegree;
    return std::clamp(WrapPi(YawOf(offset) - (boss.yaw + boss.spin)), -limit, limit);
}

Vec3 LeftTarget(const BossBodyMotionSettings& s, Vec3 direction) {
    return ClampLength(Mul(Sub(direction, Normalize(s.idleDirection)), -s.leftBalanceMeters), s.leftBalanceMax);
}

void StepBody(const BossComponent& boss, const MotionTargets& t, BossBodyMotionState& m, float dt) {
    const BossBodyMotionSettings& s = boss.body.motion;
    m.lock = t.window ? 1.0f : std::max(t.lead, m.lock - dt / std::max(kTiny, s.bodyBlendSeconds));
    m.charge = t.charge;
    m.twist = Lerp(m.twist, TwistTarget(s, m.direction), Follow(dt, s.twistSeconds));
    m.headYaw = Lerp(m.headYaw, HeadTarget(boss), Follow(dt, s.headTurnSeconds));
    m.leftOffset = Lerp(m.leftOffset, LeftTarget(s, m.direction), Follow(dt, s.leftBalanceSeconds));
}

// 最初のフレーム・隠れている間・瞬間移動：ばねを進めず、目標へそろえる。
void Snap(const BossComponent& boss, const MotionTargets& t, BossBodyMotionState& m) {
    const BossBodyMotionSettings& s = boss.body.motion;
    m.direction = t.window ? t.ruleDirection : t.direction;
    m.velocity = {};
    m.edgeGoal = Perpendicular(s.idleEdge, m.direction);
    m.edge = m.edgeGoal;
    m.grip = t.window ? Add(t.ruleRoot, Mul(m.direction, boss.body.swordGripAlong + t.reach)) : t.grip;
    m.gripVelocity = {};
    m.lock = t.window ? 1.0f : t.lead;
    m.charge = t.charge;
    m.twist = TwistTarget(s, m.direction);
    m.headYaw = HeadTarget(boss);
    m.leftOffset = LeftTarget(s, m.direction);
    m.wasWindow = t.window;
    m.ready = true;
}

// 局所（体の向きの枠。足元が原点）の点・向きを世界へ移す。lock で「体の物体の変換（傾き・屈み込み）」と
// 「規則の線の枠（足元と体の向きだけ）」を混ぜる。
struct MotionFrame {
    const BossComponent& boss;
    const Transform& body;
    float lock;

    Vec3 Point(Vec3 local) const {
        const Vec3 root{boss.ground.x, boss.ground.y + BossPose::RootHeight(boss), boss.ground.z};
        const Vec3 rule = Add(root, RotateY(local, boss.yaw));
        const Vec3 centered{local.x * body.scale.x, (local.y - boss.bodyHalfHeight) * body.scale.y,
                            local.z * body.scale.z};
        return Lerp(Add(body.position, Rotate(body.rotation, centered)), rule, lock);
    }
    Vec3 Direction(Vec3 local) const {
        return Normalize(Lerp(Rotate(body.rotation, local), RotateY(local, boss.yaw), lock), local);
    }
};

// 左手：待機の置き場所から、刀と反対へ少し動く。手の甲は待機の向き（外・上）のまま。体の傾きに付く。
void SolveLeft(const BossComponent& boss, const Transform& body, float handScale, BossBodyPose& pose) {
    const BossBodySettings& settings = boss.body;
    const BossBodyMotionState& m = boss.bodyState.motion;
    const MotionFrame frame{boss, body, 0.0f};
    const Vec3 shoulder = RotateY({-settings.shoulder.x, settings.shoulder.y, settings.shoulder.z}, m.twist);
    const Vec3 back = frame.Direction(RotateY(settings.leftRestBack, m.twist));
    pose.leftHand.position = frame.Point(RotateY(Add(settings.leftRestWrist, m.leftOffset), m.twist));
    pose.leftHand.rotation = AimZ(frame.Direction(RotateY(settings.leftRestFingers, m.twist)), back);
    pose.leftHand.scale = handScale;
    pose.leftCuff = Cuff(pose.leftHand.position, Sub(frame.Point(shoulder), pose.leftHand.position), back, handScale);
}

} // namespace

void BossBodySword::Step(BossComponent& boss, float seconds) {
    BossBodyMotionState& m = boss.bodyState.motion;
    const MotionTargets t = Targets(boss);
    MeasureRule(boss, t.ruleDirection, m);
    const bool teleported = m.ready && seconds > 0.0f && Length(Sub(boss.ground, m.ground)) > kTeleportSpeed * seconds;
    m.ground = boss.ground;
    if (!m.ready || boss.hidden || teleported) {
        Snap(boss, t, m);
        return;
    }
    if (seconds <= 0.0f)
        return;
    const float dt = std::min(seconds, kMaxFrameSeconds);
    StepDirection(boss.body.motion, t, m, dt);
    StepEdge(boss.body.motion, t, m, dt);
    StepGrip(boss, t, m, dt);
    StepBody(boss, t, m, dt);
    m.wasWindow = t.window;
}

void BossBodySword::Solve(const BossComponent& boss, const Transform& body, BossBodyPose& pose) {
    const BossBodySettings& settings = boss.body;
    const BossBodyMotionSettings& s = settings.motion;
    const BossBodyMotionState& m = boss.bodyState.motion;
    const MotionFrame frame{boss, body, m.lock};
    // 溜めの震え：ばねを通すと消える速さなので、置く時に足す（見た目だけ）。
    const float wave = 2.0f * kPi * s.chargeTremorFrequency * boss.bodyState.clock;
    const Vec3 tremor{std::sin(wave), std::sin(wave * kTremorRatio + kTremorPhase), 0.0f};
    const Vec3 direction =
        ExpMap(m.direction, ProjectOut(Mul(tremor, s.chargeTremorDegrees * kRadiansPerDegree * m.charge), m.direction));
    const Vec3 grip = Add(m.grip, Mul(tremor, s.chargeTremorMeters * m.charge));
    const Vec3 worldDirection = frame.Direction(direction);
    const Vec3 worldEdge = frame.Direction(Perpendicular(m.edge, direction));
    const float scale =
        std::max(0.01f, boss.swordLength - settings.swordGripAlong) / std::max(0.01f, settings.swordModelLength);
    const float handScale = scale * settings.handScale;
    // 刀と握りの右手は同じ置き場所（原点・向き・刃の向きが同じ）。模型の +Z＝剣先、+Y＝棟＝刃の反対。
    pose.sword.position = frame.Point(grip);
    pose.sword.rotation = AimZ(worldDirection, Mul(worldEdge, -1.0f));
    pose.sword.scale = scale;
    pose.rightHand = pose.sword;
    pose.rightHand.scale = handScale;
    // 右の籠手の筒：握りの手の模型の手首に付け、肩（胴の捻りに付く）の方へ伸ばす。手の模型の肘の向きを少し混ぜて、
    // 手首が不自然に折れないようにする。
    const Vec3 rightShoulder = frame.Point(RotateY(settings.shoulder, m.twist));
    const Vec3 rightWrist = Add(pose.sword.position, Rotate(pose.sword.rotation, Mul(settings.gripWrist, handScale)));
    const Vec3 gripElbow = Normalize(Rotate(pose.sword.rotation, settings.gripElbow));
    const Vec3 rightElbow =
        Lerp(Normalize(Sub(rightShoulder, rightWrist), gripElbow), gripElbow, settings.cuffGripBlend);
    pose.rightCuff = Cuff(rightWrist, rightElbow, Rotate(pose.sword.rotation, settings.gripBack), handScale);
    SolveLeft(boss, body, handScale, pose);
}

void BossBodySword::Trace(const BossComponent& boss, const BossBodyPose& pose) {
    if (!boss.body.motion.traceMotion || !boss.swordSmoke)
        return;
    const BossBodyMotionState& m = boss.bodyState.motion;
    const Vec3 direction = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const Vec3 edge = Rotate(pose.sword.rotation, {0.0f, -1.0f, 0.0f});
    // 窓の間は規則の線の枠（体の向きだけ）なので、局所の速さを体の向きで回せば世界の剣先の動きの向き。
    const Vec3 motion = RotateY(m.velocity, boss.yaw);
    const float speed = Length(motion);
    const BossSwordLine line = BossPose::SwordLine(boss, BossPose::Target(boss));
    const Vec3 tip = Add(pose.sword.position, Mul(direction, boss.swordLength - boss.body.swordGripAlong));
    const Vec3 far =
        Add(line.root, Mul(line.direction, boss.swordLength + boss.swordHazeLength + boss.body.motion.armReach));
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "[BossBody] MotionTrace tick=%llu move=%d stateFrame=%d dot=%.3f speed=%.1f angle=%.2f "
                  "tipGap=%.4f",
                  static_cast<unsigned long long>(boss.tick), boss.move, boss.stateFrame,
                  static_cast<double>(speed > kTiny ? Dot(edge, Mul(motion, 1.0f / speed)) : 0.0f),
                  static_cast<double>(speed / kRadiansPerDegree),
                  static_cast<double>(AngleBetween(direction, line.direction) / kRadiansPerDegree),
                  static_cast<double>(SegmentDistance(tip, tip, line.root, far)));
    BossLog::Write(message);
}

} // namespace Project_TD2_2
