#include "BossBodySword.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossBodyLeftHand.h"
#include "BossBodyMath.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossMoveTable.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;
using Transform = HIKARI::GameProjectTransform;

// 1 フレームの長さの上限 (秒)。止まった後の長いフレームでも一度に進めすぎない（その分ゆっくり見える）。
constexpr float kMaxFrameSeconds = 0.1f;
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
// 霞の大剣の横薙ぎの刃：剣が立っている（薙ぐ向きが決まらない）ほど、刃を前（これから薙ぐ所）へ向ける割合。
constexpr float kUprightEdgeForward = 0.6f;
// 大剣の刃を向けたい先がこれより短い時は、前の向きを保つ。
constexpr float kGreatEdgeMinLength = 0.1f;
// 確かめ用：大剣の向きが 1 tick にこの角度 (rad) より動いた時だけ、刃と動きの向きを比べる。
constexpr float kGreatTraceMinStep = 1e-3f;

// edge を axis まわりに goal の方へ、近い回り（180 度以内）で share の割合だけ回す。
Vec3 TurnAbout(Vec3 edge, Vec3 goal, Vec3 axis, float share) {
    const float angle = std::atan2(Dot(Cross(edge, goal), axis), Dot(edge, goal)) * share;
    return Normalize(Add(Mul(edge, std::cos(angle)), Mul(Cross(axis, edge), std::sin(angle))), edge);
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
    // 規則の構えが決めた斬る向き（両手の型の間。0 は決まっていない）と、両手の型の重み（0〜1）。
    Vec3 ruleEdge{};
    float twoHand = 0.0f;
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
    // 霞の大剣が出ている（phase が None でない）。その間に重ねる刀の向き・握り・刃を向けたい先（局所）。
    bool great = false;
    Vec3 greatDirection{0.0f, 1.0f, 0.0f};
    Vec3 greatGrip{};
    Vec3 greatEdge{};
};

// 霞の大剣：見せる刀を大剣の柄と向きに重ねる（大剣は刀を包む霧になり、両手で握るのも同じ刀）。刃は、横薙ぎでは
// 薙ぐ向き（ボスの上下の軸まわりに角度が増える向き）、縦割りでは振り下ろす向き（縦割りの面の中で、上→前→下）。
// 構え・振りかぶりの間も同じ向き＝これから斬る向き。大剣の当たりは規則の角度で決まり、ここは読むだけ。
void GreatTargets(const BossComponent& boss, MotionTargets& t) {
    const BossGreatswordState& sword = boss.greatsword;
    if (sword.phase == BossGreatswordPhase::None)
        return;
    const Vec3 root{boss.ground.x, boss.ground.y + BossPose::RootHeight(boss), boss.ground.z};
    const Vec3 up{0.0f, 1.0f, 0.0f};
    t.great = true;
    // 体の枠は体ごとの回転（回転薙ぎ）も含む（c-9143b5）。
    const float facing = boss.yaw + boss.spin;
    t.greatDirection = Normalize(RotateY(sword.direction, -facing), up);
    t.greatGrip =
        Add(RotateY(Sub(sword.hilt, root), -facing), Mul(t.greatDirection, boss.body.motion.greatswordGripAlong));
    if (sword.cleave) {
        const Vec3 forward = RotateY(Forward(sword.cleaveYaw), -facing);
        t.greatEdge = Cross(Cross(up, forward), t.greatDirection);
        return;
    }
    const Vec3 sweep = Cross(up, t.greatDirection);
    t.greatEdge = Add(sweep, Vec3{0.0f, 0.0f, kUprightEdgeForward * std::max(0.0f, 1.0f - Length(sweep))});
}

// 待機の構え：下段で、息に合わせて握りと剣先が少し上下する。
void Kamae(const BossComponent& boss, Vec3& direction, Vec3& grip) {
    IdleKamae(boss.body.motion, boss.bodyState.clock, direction, grip);
    grip = BodyPoint(boss.body, grip);
}

// 待機の構えを混ぜる重み。待機・溜めの始めは「規則の剣が下ろした構えに近いほど」、後隙は始めから終わりまで少しずつ
// 待機の構えへ寄せる（振り終わりの形で止めない。c-999264）。
float CalmWeight(const BossComponent& boss, const BossMove* move, bool swing, float rest) {
    if (boss.state == BossState::Idle)
        return rest;
    if (move == nullptr)
        return 0.0f;
    if (boss.phase == BossActPhase::Pre)
        return rest;
    if (boss.phase == BossActPhase::Act)
        return swing ? 0.0f : rest;
    return Smooth(Share(boss.stateFrame, move->post));
}

MotionTargets Targets(const BossComponent& boss) {
    const BossBodyMotionSettings& s = boss.body.motion;
    MotionTargets t;
    t.ruleDirection = RotateY(BossPose::SwordLine(boss, boss.swordPose).direction, -boss.yaw);
    const Vec3 rest = RotateY(BossPose::SwordLine(boss, BossPose::Rest(boss)).direction, -boss.yaw);
    t.ruleRoot = boss.swordPose.grip;
    t.ruleEdge = boss.swordPose.edge;
    t.twoHand = std::clamp(boss.swordPose.twoHand, 0.0f, 1.0f);
    t.window = boss.swordSmoke;
    const float restWeight =
        1.0f - Smooth(AngleBetween(t.ruleDirection, rest) / std::max(kTiny, s.restBlendDegrees * kRadiansPerDegree));
    const BossMove* move = CurrentMove(boss);
    const bool swing =
        move != nullptr && BossMoveTable::IsSwingType(move->type) && !BossMoveTable::IsMoveType(move->type);
    BossSegmentProgress g{};
    if (move != nullptr && boss.phase == BossActPhase::Act)
        g = BossPose::Progress(boss, *move);
    // 振り抜き（振りの後、振り終わりの行き過ぎから戻る swingSettleFrames F）も規則の刀に付ける：当たって霞の窓が閉じても、
    // 刀は規則の振り（行き過ぎ→戻り）をそのままたどる（ばねに任せると規則の振りより遅れ、30 度ほどずれた。c-4a8ea1）。
    if (swing && boss.phase == BossActPhase::Act && g.hit && g.tIn <= g.swingLength + boss.swingSettleFrames)
        t.window = true;
    // 両手の型の間は、待機の構えへ混ぜない（体の屈み・前傾は規則の構えが戻すので、刀だけ先に戻すと手が胴に入る）。
    t.calm = CalmWeight(boss, move, swing, restWeight) * (1.0f - t.twoHand);
    if (swing) {
        const int frames = t.window ? 0 : FramesToWindow(boss, *move);
        t.lead = frames < 0
                     ? 0.0f
                     : std::clamp(1.0f - static_cast<float>(frames) / static_cast<float>(std::max(1, s.lockLeadFrames)),
                                  0.0f, 1.0f);
        t.charging = !t.window && (boss.phase == BossActPhase::Pre || (boss.phase == BossActPhase::Act && !g.hit));
        if (t.charging)
            t.charge = (1.0f - restWeight) * (1.0f - t.lead);
        // 腕を伸ばす・溜めで握りを送り出すのは片手の振りだけ（両手の型は、型の握りの通り道が腕の動きそのもの）。
        if (t.window)
            t.reach = s.armReach * Smooth(g.progress / std::max(0.05f, s.armReachPeak)) * (1.0f - t.twoHand);
    }
    t.tipLength = std::max(kMinTipLength, VisibleSwordLength(boss) + t.reach);
    Vec3 kamaeDirection{}, kamaeGrip{};
    Kamae(boss, kamaeDirection, kamaeGrip);
    t.direction = Slerp(t.ruleDirection, kamaeDirection, t.calm);
    const Vec3 ruleGrip = Add(t.ruleRoot, Mul(t.ruleDirection, boss.body.swordGripAlong));
    // 溜め：規則の振り上げの上で、握りを剣の向きへさらに送り出し、持ち上げる（手が高く・遠く、輪郭が大きく見える）。
    const Vec3 charged = Add(Mul(t.ruleDirection, s.chargeReach), Vec3{0.0f, s.chargeLift, 0.0f});
    t.grip = Add(LerpAroundY(ruleGrip, kamaeGrip, t.calm), Mul(charged, t.charge * (1.0f - t.twoHand)));
    GreatTargets(boss, t);
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
        m.velocity = Mul(m.velocity, s.overshootCarry * (1.0f - t.twoHand));
    // 両手の型の間は、規則の構えを行き過ぎずに（収まりの強さ 1 で）追う。片手の構えは前のばね。
    const float frequency = Lerp(s.swordFrequency, s.swordHitFrequency, t.lead);
    const float damping = Lerp(s.swordDamping, s.swordHitDamping, t.lead);
    SpringOnSphere(m.direction, m.velocity, t.direction, Lerp(frequency, s.followFrequency, t.twoHand),
                   Lerp(damping, 1.0f, t.twoHand), dt);
}

// 刃の向き（c-999264）：両手の型の技は、その一振りの斬る向き（規則の型の振りの面が決める）。振り上げで斬る向きへ回し、
// 構え・振り・戻りの終わりまで同じ向き（振りの後に刀が戻っても、棟が先に戻るだけで裏返らない）。片手の振りは剣先の
// 動きの先、片手の溜めは決まった向き。待機では待機の向きへ。ほかは前の向きを保つ。刀身の軸まわりに、近い回りで回す。
void StepEdge(const BossComponent& boss, const MotionTargets& t, BossBodyMotionState& m, float dt) {
    const BossBodyMotionSettings& s = boss.body.motion;
    m.edge = Perpendicular(m.edge, m.direction);
    float seconds = s.edgeTurnSeconds;
    const Vec3 cut = ProjectOut(t.ruleEdge, m.direction);
    if (t.great) {
        if (Length(t.greatEdge) > kGreatEdgeMinLength)
            m.edgeGoal = Normalize(t.greatEdge);
    } else if (Length(t.ruleEdge) > kEdgeGoalMinLength) {
        if (Length(cut) > kEdgeGoalMinLength)
            m.edgeGoal = Normalize(cut);
        // 振っている間（命中の窓）は斬る向きにぴったり合わせる（追うと、速い振りの間に 40 度ほど遅れた。c-2dd9a2）。
        if (t.window && Length(cut) > kEdgeGoalMinLength) {
            m.edge = m.edgeGoal;
            return;
        }
        seconds = s.edgeStanceSeconds;
    } else if (t.window) {
        if (Length(m.velocity) > s.edgeMinDegreesPerSecond * kRadiansPerDegree)
            m.edgeGoal = Normalize(m.velocity);
    } else if (t.charging && t.calm < kCalmEdgeShare) {
        const Vec3 toward = ProjectOut(Normalize(s.chargeEdgeToward), m.direction);
        if (Length(toward) > kEdgeGoalMinLength)
            m.edgeGoal = Normalize(toward);
    } else if (boss.state != BossState::Act && t.calm > kCalmEdgeShare) {
        // 待機の刃へは技の外でだけ回す（戻りの終わりまでは斬る向きのまま）。
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
        m.gripVelocity = Mul(m.gripVelocity, s.overshootCarry * (1.0f - t.twoHand));
    SpringPoint(m.grip, m.gripVelocity, t.grip, Lerp(s.gripFrequency, s.followFrequency, t.twoHand),
                Lerp(s.gripDamping, 1.0f, t.twoHand), dt);
    // 振りの直前：握りを規則の線の上へ寄せる（窓が開く時に跳ばない）。
    if (t.lead > 0.0f)
        m.grip = Lerp(m.grip, Add(t.ruleRoot, Mul(m.direction, along)), t.lead);
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

// 胴の捻りの目標 (rad)：両手の型は型の捻り、片手の振りは刀の横の向きから。
float TwistGoal(const BossComponent& boss, const MotionTargets& t, Vec3 direction) {
    return Lerp(TwistTarget(boss.body.motion, direction), boss.swordPose.twist * kRadiansPerDegree, t.twoHand);
}

void StepBody(const BossComponent& boss, const MotionTargets& t, BossBodyMotionState& m, float dt) {
    const BossBodyMotionSettings& s = boss.body.motion;
    // 霞の大剣に重ねている間も、規則の線の枠（足元と体の向きだけ）で置く（大剣の柄・向きはその枠の値）。
    // 両手の型の握りは規則の線の枠で書いてあるので、型の重みの分も規則の線の枠で置く（体の屈みで二重に下げない）。
    m.lock =
        t.window ? 1.0f : std::max({t.lead, m.great, t.twoHand, m.lock - dt / std::max(kTiny, s.bodyBlendSeconds)});
    m.charge = t.charge;
    m.twist = Lerp(m.twist, TwistGoal(boss, t, m.direction), Follow(dt, s.twistSeconds));
    m.headYaw = Lerp(m.headYaw, HeadTarget(boss), Follow(dt, s.headTurnSeconds));
}

// 霞の大剣に重ねる：重みを時間で上げ下げし、ばねで動いた刀を重みの分だけ大剣の柄・向きへそろえる（入る・出る時に
// 跳ばない）。重ねている間は、ばねの勢いを捨てる（離した時に溜まった勢いで暴れない）。
void StepGreat(const BossBodyMotionSettings& s, const MotionTargets& t, BossBodyMotionState& m, float dt) {
    m.great = std::clamp(m.great + (t.great ? dt : -dt) / std::max(kTiny, s.greatswordBlendSeconds), 0.0f, 1.0f);
    if (!t.great || m.great <= 0.0f)
        return;
    m.direction = Slerp(m.direction, t.greatDirection, m.great);
    m.grip = Lerp(m.grip, t.greatGrip, m.great);
    m.velocity = ProjectOut(Mul(m.velocity, 1.0f - m.great), m.direction);
    m.gripVelocity = Mul(m.gripVelocity, 1.0f - m.great);
}

// 確かめ用（bodyTraceMotion）：大剣に重ねている間、tick ごとに、見せる刀と大剣の向きの差（度）・握りのずれ (m)・
// 刃と大剣の剣先の動きの向きの内積を記録に出す。
void TraceGreat(const BossComponent& boss, const MotionTargets& t, BossBodyMotionState& m) {
    if (!t.great || boss.tick == m.greatTick) {
        m.greatPrevious = t.greatDirection;
        return;
    }
    const Vec3 motion = ProjectOut(Sub(t.greatDirection, m.greatPrevious), t.greatDirection);
    const bool moving = Length(motion) > kGreatTraceMinStep;
    m.greatPrevious = t.greatDirection;
    m.greatTick = boss.tick;
    if (!boss.body.motion.traceMotion)
        return;
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "[BossBody] GreatTrace tick=%llu phase=%d cleave=%d weight=%.2f angle=%.2f gripGap=%.4f dot=%.3f "
                  "moving=%d",
                  static_cast<unsigned long long>(boss.tick), static_cast<int>(boss.greatsword.phase),
                  static_cast<int>(boss.greatsword.cleave), static_cast<double>(m.great),
                  static_cast<double>(AngleBetween(m.direction, t.greatDirection) / kRadiansPerDegree),
                  static_cast<double>(Length(Sub(m.grip, t.greatGrip))),
                  static_cast<double>(moving ? Dot(Perpendicular(m.edge, m.direction), Normalize(motion)) : 0.0f),
                  moving ? 1 : 0);
    BossLog::Write(message);
}

// 確かめ用の鎧の近似（半径 m）：胴（胴の中心から上下 kTorsoHalf の線）、草摺（裾へ開くので上下 2 段：胴の中心の
// kTassetTop から kTassetMiddle 下までと、そこから kTassetBottom 下まで）、兜（球）、大袖（吊る所から垂れる向きへ
// kSodeLength の線）、握りの拳。数値は模型の大きさの目安。
constexpr float kTorsoHalf = 0.5f;
constexpr float kTorsoRadius = 0.70f;
constexpr float kTassetTop = 0.5f;
constexpr float kTassetMiddle = 0.9f;
constexpr float kTassetBottom = 1.25f;
constexpr float kTassetUpperRadius = 0.64f;
constexpr float kTassetLowerRadius = 0.74f;
constexpr float kHeadRadius = 0.55f;
constexpr float kSodeLength = 0.65f;
constexpr float kSodeRadius = 0.26f;
constexpr float kFistRadius = 0.12f;

// 線分 a〜b と、鎧の近似のいちばん近い隙間 (m)。who にどの部品かを入れる。
float ArmorGap(const BossBodyPose& pose, Vec3 a, Vec3 b, float pad, const char*& who) {
    const Vec3 up = Rotate(pose.torso.rotation, {0.0f, 1.0f, 0.0f});
    const Vec3 torso = pose.torso.position;
    // 近似の数値は 1 倍の体の値。鎧の大きさ（胴の拡大＝Boss.bodyScale）を掛ける。
    const float k = std::max(0.01f, pose.torso.scale);
    struct Part {
        const char* name;
        Vec3 from, to;
        float radius;
    };
    const auto hang = [k](const BossBodyPlacement& p) {
        return Add(p.position, Rotate(p.rotation, {0.0f, -kSodeLength * k, 0.0f}));
    };
    const Part parts[] = {
        {"torso", Add(torso, Mul(up, -kTorsoHalf * k)), Add(torso, Mul(up, kTorsoHalf * k)), kTorsoRadius * k},
        {"tassetUp", Add(torso, Mul(up, -kTassetMiddle * k)), Add(torso, Mul(up, -kTassetTop * k)),
         kTassetUpperRadius * k},
        {"tassetLow", Add(torso, Mul(up, -kTassetBottom * k)), Add(torso, Mul(up, -kTassetMiddle * k)),
         kTassetLowerRadius * k},
        {"head", pose.head.position, pose.head.position, kHeadRadius * k},
        {"sodeR", pose.sode[0].position, hang(pose.sode[0]), kSodeRadius * k},
        {"sodeL", pose.sode[1].position, hang(pose.sode[1]), kSodeRadius * k},
    };
    float best = 1000.0f;
    for (const Part& part : parts) {
        const float gap = SegmentDistance(a, b, part.from, part.to) - part.radius - pad;
        if (gap < best) {
            best = gap;
            who = part.name;
        }
    }
    return best;
}

// 確かめ用（bodyTraceMotion）：技の間、毎フレーム、刀身（鍔〜剣先）と握りの拳が鎧の近似からどれだけ離れているか、
// 手首から肩（胴の捻りに付く）までの距離、剣先の地面からの高さ、両手の重みを記録に出す。
void TraceClearance(const BossComponent& boss, const Transform& body, const BossBodyPose& pose) {
    const BossMove* move = CurrentMove(boss);
    if (!boss.body.motion.traceMotion || move == nullptr)
        return;
    const BossBodySettings& settings = boss.body;
    const Vec3 direction = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const Vec3 guard = Add(pose.sword.position, Mul(direction, settings.leftHand.bladeStart));
    const Vec3 tip = Add(pose.sword.position, Mul(direction, VisibleBladeReach(boss)));
    const char* bladeWho = "";
    const char* handWho = "";
    const float blade = ArmorGap(pose, guard, tip, 0.0f, bladeWho);
    const float hand = ArmorGap(pose, pose.sword.position, pose.sword.position, kFistRadius, handWho);
    const MotionFrame frame{boss, body, 0.0f};
    const float twist = boss.bodyState.motion.twist;
    const Vec3 shoulder = BodyPoint(settings, settings.shoulder);
    const Vec3 rightShoulder = frame.Point(RotateY(shoulder, twist));
    const Vec3 leftShoulder = frame.Point(RotateY({-shoulder.x, shoulder.y, shoulder.z}, twist));
    char message[320]{};
    std::snprintf(
        message, sizeof(message),
        "[BossBody] Clearance tick=%llu move=%d phase=%d stateFrame=%d window=%d blade=%.3f(%s) hand=%.3f(%s) "
        "armR=%.3f armL=%.3f tipY=%.3f twoHand=%.2f twist=%.1f",
        static_cast<unsigned long long>(boss.tick), boss.move, static_cast<int>(boss.phase), boss.stateFrame,
        boss.swordSmoke ? 1 : 0, static_cast<double>(blade), bladeWho, static_cast<double>(hand), handWho,
        static_cast<double>(Length(Sub(pose.rightHand.position, rightShoulder))),
        static_cast<double>(Length(Sub(pose.leftHand.position, leftShoulder))),
        static_cast<double>(tip.y - boss.ground.y), static_cast<double>(boss.swordPose.twoHand),
        static_cast<double>(twist / kRadiansPerDegree));
    BossLog::Write(message);
}

// 確かめ用（bodyTraceMotion）：ばねを進めず目標へそろえたフレームと、その理由（first・hidden・teleport）・
// 足元の速さ (m/秒)・技の相を記録に出す。
void TraceSnap(const BossComponent& boss, const char* reason, float speed) {
    if (!boss.body.motion.traceMotion)
        return;
    char message[192]{};
    std::snprintf(message, sizeof(message),
                  "[BossBody] Snap tick=%llu reason=%s speed=%.1f move=%d state=%d phase=%d stateFrame=%d",
                  static_cast<unsigned long long>(boss.tick), reason, static_cast<double>(speed), boss.move,
                  static_cast<int>(boss.state), static_cast<int>(boss.phase), boss.stateFrame);
    BossLog::Write(message);
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
    m.twist = TwistGoal(boss, t, m.direction);
    m.headYaw = HeadTarget(boss);
    m.great = t.great ? 1.0f : 0.0f;
    if (t.great) {
        m.direction = t.greatDirection;
        m.grip = t.greatGrip;
        m.edgeGoal = m.edge = Perpendicular(t.greatEdge, m.direction);
        m.lock = 1.0f;
    }
    m.wasWindow = t.window;
    m.ready = true;
}

} // namespace

void BossBodySword::Step(BossComponent& boss, float seconds) {
    BossBodyMotionState& m = boss.bodyState.motion;
    const MotionTargets t = Targets(boss);
    MeasureRule(boss, t.ruleDirection, m);
    // 規則の層が位置を跳ばした（teleportCount が増えた）フレームだけ、刀を目標へそろえる。速いが連続の動き
    // （突進・落下）はばねで追う。足元の速さは確かめ用の記録にだけ使う。
    const bool teleported = boss.teleportCount != m.teleportSeen;
    m.teleportSeen = boss.teleportCount;
    const float speed = m.ready && seconds > 0.0f ? Length(Sub(boss.ground, m.ground)) / seconds : 0.0f;
    m.ground = boss.ground;
    if (!m.ready || boss.hidden || teleported) {
        TraceSnap(boss, !m.ready ? "first" : boss.hidden ? "hidden" : "teleport", speed);
        Snap(boss, t, m);
        // 左手は刀の後で決める（両手持ち・刀身をなぞる手は、見せる刀に付く）。
        BossBodyLeftHand::Step(boss, 0.0f, true);
        return;
    }
    if (seconds <= 0.0f)
        return;
    const float dt = std::min(seconds, kMaxFrameSeconds);
    StepDirection(boss.body.motion, t, m, dt);
    StepGrip(boss, t, m, dt);
    StepGreat(boss.body.motion, t, m, dt);
    StepEdge(boss, t, m, dt);
    StepBody(boss, t, m, dt);
    TraceGreat(boss, t, m);
    BossBodyLeftHand::Step(boss, dt, false);
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
    // 剣先を地面の下へ入れない：突き抜ける分だけ、剣の向きを水平の側へ起こす（握りは動かさない。縦に振り下ろした
    // 後、規則の剣の線は地面の下へ入るが、見える刀は地面に当たって止まる）。霧へ沈んでいる間は掛けない。
    const Vec3 gripWorld = frame.Point(grip);
    const float floorHeight = boss.ground.y + boss.bodyState.presentShift.y + s.tipGroundClearance;
    const bool sunk = BossPose::RootHeight(boss) < -kTiny;
    const Vec3 freeDirection = frame.Direction(direction);
    const Vec3 worldDirection = s.tipGroundClearance < 0.0f || sunk
                                    ? freeDirection
                                    : KeepTipAbove(gripWorld, freeDirection, VisibleBladeReach(boss), floorHeight,
                                                   RotateY({0.0f, 0.0f, 1.0f}, boss.yaw + boss.spin));
    pose.swordTipClearance =
        sunk ? 1000.0f : gripWorld.y + freeDirection.y * VisibleBladeReach(boss) - (floorHeight - s.tipGroundClearance);
    const Vec3 worldEdge = Perpendicular(frame.Direction(Perpendicular(m.edge, direction)), worldDirection);
    const float scale = SwordScale(boss);
    const float handScale = HandScale(boss);
    // 刀と握りの右手は同じ置き場所（原点・向き・刃の向きが同じ）。模型の +Z＝剣先、+Y＝棟＝刃の反対。
    pose.sword.position = gripWorld;
    pose.sword.rotation = AimZ(worldDirection, Mul(worldEdge, -1.0f));
    pose.sword.scale = scale;
    pose.rightHand = pose.sword;
    pose.rightHand.scale = handScale;
    // 右の籠手の筒：握りの手の模型の手首に付け、肩（胴の捻りに付く）の方へ伸ばす。手の模型の肘の向きを少し混ぜて、
    // 手首が不自然に折れないようにする。
    const Vec3 rightShoulder = frame.Point(RotateY(BodyPoint(settings, settings.shoulder), m.twist));
    const Vec3 rightWrist = Add(pose.sword.position, Rotate(pose.sword.rotation, Mul(settings.gripWrist, handScale)));
    const Vec3 gripElbow = Normalize(Rotate(pose.sword.rotation, settings.gripElbow));
    const Vec3 rightElbow =
        Lerp(Normalize(Sub(rightShoulder, rightWrist), gripElbow), gripElbow, settings.cuffGripBlend);
    pose.rightCuff =
        Cuff(rightWrist, rightElbow, Rotate(pose.sword.rotation, settings.gripBack), handScale, settings, rightShoulder);
    BossBodyLeftHand::Solve(boss, body, pose);
}

namespace {

// 確かめ用（bodyTraceMotion）：技の間、毎フレーム、見せる刃の向きと「斬る向き」（規則の型が決めた刃の向き）の内積
// rule・剣先の動きの向きとの内積 motion（遅い時は 9）・刀の向きの角速度（度/秒）を記録に出す。規則の刃の向きが
// 決まっていない時（片手の構え・下ろした構え）の rule は 9。
void TraceEdge(const BossComponent& boss, const Transform& body, const BossBodyPose& pose) {
    const BossMove* move = CurrentMove(boss);
    if (!boss.body.motion.traceMotion || move == nullptr)
        return;
    const BossBodyMotionState& m = boss.bodyState.motion;
    const MotionFrame frame{boss, body, m.lock};
    const Vec3 direction = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const Vec3 edge = Rotate(pose.sword.rotation, {0.0f, -1.0f, 0.0f});
    const Vec3 ruleEdge = boss.swordPose.edge;
    const float rule =
        Length(ruleEdge) > kEdgeGoalMinLength ? Dot(edge, Perpendicular(frame.Direction(ruleEdge), direction)) : 9.0f;
    const float speed = Length(m.velocity);
    const float motion = speed > boss.body.motion.edgeMinDegreesPerSecond * kRadiansPerDegree
                             ? Dot(edge, Perpendicular(frame.Direction(m.velocity), direction))
                             : 9.0f;
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "[BossBody] EdgeTrace tick=%llu move=%d phase=%d stateFrame=%d twoHand=%.2f window=%d rule=%.3f "
                  "motion=%.3f speed=%.0f",
                  static_cast<unsigned long long>(boss.tick), boss.move, static_cast<int>(boss.phase), boss.stateFrame,
                  static_cast<double>(boss.swordPose.twoHand), boss.swordSmoke ? 1 : 0, static_cast<double>(rule),
                  static_cast<double>(motion), static_cast<double>(speed / kRadiansPerDegree));
    BossLog::Write(message);
}

} // namespace

void BossBodySword::Trace(const BossComponent& boss, const Transform& body, const BossBodyPose& pose) {
    TraceClearance(boss, body, pose);
    TraceEdge(boss, body, pose);
    if (!boss.body.motion.traceMotion || !boss.swordSmoke)
        return;
    const BossBodyMotionState& m = boss.bodyState.motion;
    const Vec3 direction = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const Vec3 edge = Rotate(pose.sword.rotation, {0.0f, -1.0f, 0.0f});
    // 窓の間は規則の線の枠（体の向きだけ）なので、局所の速さを体の向きで回せば世界の剣先の動きの向き。
    const Vec3 motion = RotateY(m.velocity, boss.yaw);
    const float speed = Length(motion);
    const BossSwordLine line = BossPose::SwordLine(boss, boss.swordPose);
    // 見える刀は補間でずらしてあるので、規則の線と比べる時はずらす前へ戻す。
    const Vec3 tip =
        Sub(Add(pose.sword.position, Mul(direction, VisibleBladeReach(boss))), boss.bodyState.presentShift);
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
