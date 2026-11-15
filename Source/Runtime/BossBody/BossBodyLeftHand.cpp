#include "BossBodyLeftHand.h"

#include <cstdio>

#include "BossBodyMath.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossMoveTable.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;
using Gesture = BossBodyLeftGesture;
using Shape = BossBodyHandShape;

// 技の型 → 左手の手振り。載っていない型（ふつうの斬り・突進・連撃・歩き）は釣り合い（c-a08a28 の動き）。
// 各手振りが技のどの間に出るかは、下の手振りごとの関数が決める（出ていない間は釣り合い）。
struct GestureRow {
    BossMoveType type;
    Gesture gesture;
};
constexpr GestureRow kGestureTable[] = {
    {BossMoveType::Detonate, Gesture::Detonate},  {BossMoveType::Knife, Gesture::Knife},
    {BossMoveType::Clone, Gesture::Seal},         {BossMoveType::Ambush, Gesture::Seal},
    {BossMoveType::FlameRun, Gesture::FlameWipe}, {BossMoveType::Retreat, Gesture::Aim},
    {BossMoveType::Greatsword, Gesture::TwoHand}, {BossMoveType::GreatswordCleave, Gesture::TwoHand},
    {BossMoveType::Skyfall, Gesture::TwoHand},    {BossMoveType::Kick, Gesture::Kick},
};

// 体の中心の高さ（局所 m）。刀身をなぞる手を、刀身のどちらの面に付けるか（体の側の面）を決めるのに使う。
constexpr float kBodyCenterHeight = 2.5f;

// 1 フレームの手の目標（局所）。attach の時は、ばねの遅れを残さず手を刀に付ける。
struct LeftTarget {
    Gesture gesture = Gesture::Balance;
    Shape shape = Shape::Open;
    Vec3 wrist{};
    Vec3 fingers{0.0f, -1.0f, 0.0f};
    Vec3 back{1.0f, 0.0f, 0.0f};
    // ばねの速さ（0 ならふつうの速さ）。
    float frequency = 0.0f;
    bool attach = false;
};

LeftTarget Pose(Gesture gesture, Shape shape, Vec3 wrist, Vec3 fingers, Vec3 back, float frequency = 0.0f) {
    LeftTarget t;
    t.gesture = gesture;
    t.shape = shape;
    t.wrist = wrist;
    t.fingers = fingers;
    t.back = back;
    t.frequency = frequency;
    return t;
}

// 釣り合い：待機の置き場所から、刀と反対へ少し動く。手の甲は待機の向き（外・上）のまま。胴の捻りに付く。
LeftTarget Balance(const BossComponent& boss) {
    const BossBodySettings& settings = boss.body;
    const BossBodyMotionState& m = boss.bodyState.motion;
    return Pose(Gesture::Balance, Shape::Open, RotateY(Add(settings.leftRestWrist, m.leftOffset), m.twist),
                RotateY(settings.leftRestFingers, m.twist), RotateY(settings.leftRestBack, m.twist));
}

// 手の模型の座標（x・y・z の軸＝親指の反対・手の甲・指先）で書いた点を、局所へ移す。
Vec3 HandPoint(Vec3 x, Vec3 y, Vec3 z, Vec3 local) {
    return Add(Add(Mul(x, local.x), Mul(y, local.y)), Mul(z, local.z));
}

// 見せる刀の座標の軸（局所）：+X＝刀の左、+Y＝棟、+Z＝剣先。
struct SwordAxes {
    Vec3 x, y, z;
};
SwordAxes Axes(const BossBodyMotionState& m) {
    const Vec3 z = m.direction;
    const Vec3 y = Mul(Perpendicular(m.edge, z), -1.0f);
    return {Cross(y, z), y, z};
}

// 引爆：前隙の初めに掲げ、溜めの間に胸へ寄せる。爆ぜ始めた tick（BossDetonate::Begin）に拳を握って下へ落とす。
bool Detonate(const BossComponent& boss, const BossMove& move, LeftTarget& t) {
    const BossBodyLeftHandSettings& s = boss.body.leftHand;
    if (boss.phase == BossActPhase::Pre) {
        const float p = Share(boss.stateFrame, move.pre);
        const float raise = std::clamp(s.detonateRaiseShare, 0.01f, 1.0f);
        const Vec3 wrist =
            p < raise ? Lerp(Balance(boss).wrist, s.detonateRaise, Smooth(p / raise))
                      : Lerp(s.detonateRaise, s.detonateChest, Smooth((p - raise) / std::max(kTiny, 1.0f - raise)));
        t = Pose(Gesture::Detonate, Shape::Open, wrist, s.detonateFingers, s.detonateBack);
        return true;
    }
    // 爆ぜ始めるのは攻撃中の 1 F 目（BossSpecialMoves::Trigger）。燃えている所が無いと同じ tick に後隙へ入るので、
    // 後隙の初めの detonateFistHoldFrames F も拳のまま（爆ぜ始めの tick に拳が必ず見える）。その後は釣り合いへ戻す。
    const bool fist = boss.phase == BossActPhase::Act ? boss.detonation.active || boss.stateFrame >= 1
                                                      : boss.stateFrame < s.detonateFistHoldFrames;
    if (fist)
        t = Pose(Gesture::Detonate, Shape::Fist, s.detonateFist, s.detonateFingers, s.detonateBack, s.snapFrequency);
    else if (boss.phase == BossActPhase::Act)
        t = Pose(Gesture::Detonate, Shape::Open, s.detonateChest, s.detonateFingers, s.detonateBack);
    return fist || boss.phase == BossActPhase::Act;
}

// 投げナイフ：前隙は刀印で肩の後ろへ引き、攻撃中に前へ素早く振り出して開く。
bool Knife(const BossComponent& boss, LeftTarget& t) {
    const BossBodyLeftHandSettings& s = boss.body.leftHand;
    if (boss.phase == BossActPhase::Pre)
        t = Pose(Gesture::Knife, Shape::Seal, s.knifePull, s.knifePullFingers, s.knifePullBack);
    else if (boss.phase == BossActPhase::Act)
        t = Pose(Gesture::Knife, Shape::Open, s.knifeThrow, s.knifeThrowFingers, s.knifeThrowBack, s.snapFrequency);
    return boss.phase != BossActPhase::Post;
}

// 霞分身・霞隠れ：前隙の間、顔の前で刀印を立てる。
bool Seal(const BossComponent& boss, LeftTarget& t) {
    const BossBodyLeftHandSettings& s = boss.body.leftHand;
    if (boss.phase != BossActPhase::Pre)
        return false;
    t = Pose(Gesture::Seal, Shape::Seal, s.sealWrist, s.sealFingers, s.sealBack);
    return true;
}

// 炎走り：前隙の決めた間、開いた手で刀身を鍔の側から剣先へなぞる。掌は刀身の平らな面（体の側）に付け、指先は剣先へ。
bool FlameWipe(const BossComponent& boss, const BossMove& move, LeftTarget& t) {
    const BossBodyLeftHandSettings& s = boss.body.leftHand;
    const BossBodyMotionState& m = boss.bodyState.motion;
    if (boss.phase != BossActPhase::Pre)
        return false;
    const float p = Share(boss.stateFrame, move.pre);
    if (p < s.wipeStartShare || p > s.wipeEndShare)
        return false;
    const float q = Smooth((p - s.wipeStartShare) / std::max(kTiny, s.wipeEndShare - s.wipeStartShare));
    const float blade = std::max(0.0f, boss.swordLength - boss.body.swordGripAlong - s.bladeStart);
    const SwordAxes sword = Axes(m);
    const Vec3 point = Add(m.grip, Mul(sword.z, s.bladeStart + blade * Lerp(s.wipeFrom, s.wipeTo, q)));
    // 手の甲は刀身から離れる向き（掌が刀身へ向く）。体の側の面を選ぶ。
    const Vec3 toBody = Sub(Vec3{0.0f, kBodyCenterHeight, 0.0f}, point);
    const Vec3 back = Dot(sword.x, toBody) >= 0.0f ? sword.x : Mul(sword.x, -1.0f);
    const Vec3 handX = Cross(back, sword.z);
    const Vec3 wrist = Sub(point, Mul(HandPoint(handX, back, sword.z, s.palmCenter), HandScale(boss)));
    t = Pose(Gesture::FlameWipe, Shape::Open, wrist, sword.z, back);
    t.attach = true;
    return true;
}

// 両手持ち：拳を右手の後ろ（柄頭の側）に握る。親指の側（模型の -X）を剣先へ、掌を柄へ、手の甲を外へ。
LeftTarget TwoHand(const BossComponent& boss) {
    const BossBodyLeftHandSettings& s = boss.body.leftHand;
    const BossBodyMotionState& m = boss.bodyState.motion;
    const SwordAxes sword = Axes(m);
    const Vec3 handX = Mul(sword.z, -1.0f);
    const Vec3 back = Perpendicular(HandPoint(sword.x, sword.y, sword.z, s.twoHandBack), sword.z);
    const Vec3 fingers = Cross(handX, back);
    const Vec3 hilt = Sub(m.grip, Mul(sword.z, s.twoHandBehind));
    const Vec3 wrist = Sub(hilt, Mul(HandPoint(handX, back, fingers, s.fistCenter), HandScale(boss)));
    LeftTarget t = Pose(Gesture::TwoHand, Shape::Fist, wrist, fingers, back);
    t.attach = true;
    return t;
}

bool HoldingGreatSword(const BossComponent& boss, const BossMove& move) {
    if (move.type == BossMoveType::Skyfall) {
        const BossSkyfallPhase phase = boss.skyfall.phase;
        return phase == BossSkyfallPhase::Rise || phase == BossSkyfallPhase::Hang || phase == BossSkyfallPhase::Dive ||
               phase == BossSkyfallPhase::Landed;
    }
    return boss.greatsword.phase != BossGreatswordPhase::None;
}

Gesture GestureOf(BossMoveType type) {
    for (const GestureRow& row : kGestureTable)
        if (row.type == type)
            return row.gesture;
    return Gesture::Balance;
}

// いまの手振りの目標。表の手振りが出ていない間は釣り合い。
LeftTarget Target(const BossComponent& boss) {
    const BossBodyLeftHandSettings& s = boss.body.leftHand;
    if (boss.state == BossState::Stunned || boss.state == BossState::Dead)
        return Pose(Gesture::Limp, Shape::Open, s.limpWrist, s.limpFingers, s.limpBack);
    const BossMove* move = CurrentMove(boss);
    LeftTarget t;
    bool active = false;
    switch (move != nullptr ? GestureOf(move->type) : Gesture::Balance) {
    case Gesture::Detonate: active = Detonate(boss, *move, t); break;
    case Gesture::Knife: active = Knife(boss, t); break;
    case Gesture::Seal: active = Seal(boss, t); break;
    case Gesture::FlameWipe: active = FlameWipe(boss, *move, t); break;
    case Gesture::Aim:
        active = boss.retreat.charging;
        t = Pose(Gesture::Aim, Shape::Open, s.aimWrist, s.aimFingers, s.aimBack);
        break;
    case Gesture::TwoHand:
        active = HoldingGreatSword(boss, *move);
        t = TwoHand(boss);
        break;
    case Gesture::Kick:
        active = boss.phase != BossActPhase::Post;
        t = Pose(Gesture::Kick, Shape::Open, s.kickWrist, s.kickFingers, s.kickBack);
        break;
    default: break;
    }
    return active ? t : Balance(boss);
}

Vec3 BalanceOffset(const BossComponent& boss) {
    const BossBodyMotionSettings& s = boss.body.motion;
    return ClampLength(Mul(Sub(boss.bodyState.motion.direction, Normalize(s.idleDirection)), -s.leftBalanceMeters),
                       s.leftBalanceMax);
}

const char* ShapeName(Shape shape) {
    switch (shape) {
    case Shape::Fist: return "Fist";
    case Shape::Seal: return "Seal";
    default: return "Open";
    }
}

// 確かめ用（bodyTraceMotion）：手振り・形が変わった tick と、その時の指先・手の甲の向き（局所）を記録に出す。
void TraceChange(const BossComponent& boss, const LeftTarget& t) {
    const BossBodyMotionState& m = boss.bodyState.motion;
    if (!boss.body.motion.traceMotion || (t.gesture == m.leftGesture && t.shape == m.leftShape))
        return;
    const Vec3 f = Normalize(t.fingers), b = Normalize(t.back);
    char message[320]{};
    std::snprintf(message, sizeof(message),
                  "[BossBody] LeftHand tick=%llu move=%d phase=%d stateFrame=%d gesture=%d shape=%s "
                  "fingers=(%.2f,%.2f,%.2f) back=(%.2f,%.2f,%.2f) detonating=%d",
                  static_cast<unsigned long long>(boss.tick), boss.move, static_cast<int>(boss.phase), boss.stateFrame,
                  static_cast<int>(t.gesture), ShapeName(t.shape), static_cast<double>(f.x), static_cast<double>(f.y),
                  static_cast<double>(f.z), static_cast<double>(b.x), static_cast<double>(b.y),
                  static_cast<double>(b.z), boss.detonation.active ? 1 : 0);
    BossLog::Write(message);
}

} // namespace

void BossBodyLeftHand::Step(BossComponent& boss, float dt, bool snap) {
    const BossBodyLeftHandSettings& s = boss.body.leftHand;
    BossBodyMotionState& m = boss.bodyState.motion;
    m.leftOffset = snap ? BalanceOffset(boss)
                        : Lerp(m.leftOffset, BalanceOffset(boss), Follow(dt, boss.body.motion.leftBalanceSeconds));
    const LeftTarget t = Target(boss);
    TraceChange(boss, t);
    m.leftShape = t.shape;
    m.leftGesture = t.gesture;
    if (snap) {
        m.leftWrist = t.wrist;
        m.leftVelocity = {};
        m.leftFingers = Normalize(t.fingers);
        m.leftBack = Normalize(t.back);
        m.leftAttach = t.attach ? 1.0f : 0.0f;
        return;
    }
    SpringPoint(m.leftWrist, m.leftVelocity, t.wrist, t.frequency > 0.0f ? t.frequency : s.frequency, s.damping, dt);
    const float turn = Follow(dt, s.turnSeconds);
    m.leftFingers = Slerp(m.leftFingers, Normalize(t.fingers), turn);
    m.leftBack = Slerp(m.leftBack, Normalize(t.back), turn);
    m.leftAttach = std::clamp(m.leftAttach + (t.attach ? dt : -dt) / std::max(kTiny, s.attachSeconds), 0.0f, 1.0f);
    if (!t.attach)
        return;
    // 刀に付ける手振り：ばねの遅れを残さず、重みの分だけ目標（刀の上）へそろえる。離す時はばねに任せる。
    m.leftWrist = Lerp(m.leftWrist, t.wrist, m.leftAttach);
    m.leftFingers = Slerp(m.leftFingers, Normalize(t.fingers), m.leftAttach);
    m.leftBack = Slerp(m.leftBack, Normalize(t.back), m.leftAttach);
}

void BossBodyLeftHand::Solve(const BossComponent& boss, const HIKARI::GameProjectTransform& body, BossBodyPose& pose) {
    const BossBodySettings& settings = boss.body;
    const BossBodyMotionState& m = boss.bodyState.motion;
    // 刀に付けている間は、刀と同じ枠で置く（刀は振る間、規則の線の枠に寄る）。ほかは体の傾きに付く。
    const MotionFrame frame{boss, body, m.lock * m.leftAttach};
    const MotionFrame bodyFrame{boss, body, 0.0f};
    const float handScale = HandScale(boss);
    const Vec3 back = frame.Direction(m.leftBack);
    pose.leftHand.position = frame.Point(m.leftWrist);
    pose.leftHand.rotation = AimZ(frame.Direction(m.leftFingers), back);
    pose.leftHand.scale = handScale;
    pose.leftShape = m.leftShape;
    // 籠手の筒は肩（胴の捻りに付く）の方へ伸ばす。
    const Vec3 shoulder = RotateY({-settings.shoulder.x, settings.shoulder.y, settings.shoulder.z}, m.twist);
    pose.leftCuff =
        Cuff(pose.leftHand.position, Sub(bodyFrame.Point(shoulder), pose.leftHand.position), back, handScale);
}

} // namespace Project_TD2_2
