#include "BossPose.h"

#include <algorithm>
#include <cmath>

#include "BossMath.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 剣の姿勢（度）。試作の POSE と同じ値。剣のモデルを入れるまでは、剣の当たりと霞の位置だけに効く。
constexpr BossSwordPose kRest{70.0f, -10.0f};
constexpr BossSwordPose kUp{-110.0f, 0.0f};
constexpr BossSwordPose kUpLeft{-55.0f, 45.0f};
constexpr BossSwordPose kUpRight{-55.0f, -45.0f};
constexpr BossSwordPose kDownLeft{55.0f, 50.0f};
constexpr BossSwordPose kDownRight{55.0f, -50.0f};
constexpr BossSwordPose kBack{30.0f, -150.0f};
constexpr BossSwordPose kSide{30.0f, 85.0f};
constexpr BossSwordPose kVerticalEnd{80.0f, 0.0f};
constexpr BossSwordPose kStunned{105.0f, -25.0f};
constexpr BossSwordPose kKnifeStart{-30.0f, -120.0f};
constexpr BossSwordPose kKnifeEnd{30.0f, 40.0f};
// 回し蹴りの間、剣を横に構えたまま回る。分身を出す間は剣をまっすぐ前へ（試作の held と [0, 0]）。
constexpr BossSwordPose kHeld{20.0f, -80.0f};
constexpr BossSwordPose kForward{0.0f, 0.0f};
// 天降り斬りで落ちる・着地した剣：切っ先を真下へ向けて突き立てる。落ちる進みのこの倍で向け終える（落ちる前半で下を向く）。
constexpr BossSwordPose kPlunge{90.0f, 0.0f};
constexpr float kPlungeTurnSpeed = 2.0f;
// 炎走り：前隙は剣を右後ろへ引きずる（切っ先が地面すれすれ）。前線を放つたびに前の高い所へ斬り上げ、放つ間隔のこの割合で
// 上げ切って、残りで引きずる構えへ戻す（次の 1 本も低い所から斬り上げる）。
constexpr BossSwordPose kDrag{60.0f, -160.0f};
constexpr BossSwordPose kFlameRunUp{-65.0f, 0.0f};
constexpr float kFlameRunUpShare = 0.6f;
// 振り上げた構えの引き（反対へ 22% 引く）、振り出しの割合、フェイントで引き戻す長さ (F)。
constexpr float kCockedPull = 0.22f;
constexpr float kSwingInShare = 0.16f;
constexpr int kFeintPullbackFrames = 7;
// 剣の根元（剣の長さに対する割合）と、霞の内側の始まり。
constexpr float kSwordBaseShare = 0.14f;
constexpr float kSwordInnerShare = 0.65f;

BossSwordPose Sweep(BossSwordPose a, BossSwordPose c, float k) {
    const float e = Smooth(k);
    return {Lerp(a.x, c.x, e), Lerp(a.y, c.y, e)};
}

// 振り上げた構え：体の後ろへは回さず、プレイヤーから剣が見える高さ・横に掲げる。
BossSwordPose Cocked(BossSwordPose a, BossSwordPose c) {
    float x = a.x + (a.x - c.x) * kCockedPull;
    const float y = a.y + (a.y - c.y) * kCockedPull;
    if (std::abs(a.y) > 60.0f)
        x = std::min(x, a.x - 25.0f);
    return {std::clamp(x, -100.0f, 60.0f), std::clamp(y, -105.0f, 105.0f)};
}

// 振り：最初の振り出しはゆっくり動き始め（振り出しが見える）、残りで一気に振り抜く。
BossSwordPose Swing(const BossComponent& boss, BossSwordPose start, BossSwordPose end, int tIn, int length) {
    const BossSwordPose cocked = Cocked(start, end);
    const int swingIn = std::min(boss.swingInFrames, std::max(0, length - 2));
    float e = 0.0f;
    if (tIn <= swingIn) {
        const float u = static_cast<float>(tIn) / static_cast<float>(std::max(1, swingIn));
        e = kSwingInShare * u * u;
    } else {
        const float u = std::clamp(
            static_cast<float>(tIn - swingIn) / static_cast<float>(std::max(1, length - swingIn)), 0.0f, 1.0f);
        e = std::min(1.0f, kSwingInShare + (1.0f - kSwingInShare) *
                                               (2.0f * kSwingInShare * u + (1.0f - 2.0f * kSwingInShare) * u * u));
    }
    return {Lerp(cocked.x, end.x, e), Lerp(cocked.y, end.y, e)};
}

// 振り上げ→構え。剣を振る方向と反対へ大きく引いて、構えの間は止める。
// フェイント（遅）は、いつものテンポで「振り出し」まで見せてから引き戻し、溜めて（小さく震えて）待つ。
BossSwordPose Windup(const BossComponent& boss, BossSwordPose previous, BossSwordPose start, BossSwordPose end, int tIn,
                     int length, bool feint) {
    const BossSwordPose cocked = Cocked(start, end);
    const int base = feint ? std::max(1, length - boss.feintFrames) : length;
    const int kamae = std::min(boss.kamaeFrames, base - 1);
    const int raiseEnd = std::max(1, base - kamae);
    const int raiseStart = std::max(0, raiseEnd - boss.raiseFrames);
    if (tIn < base || !feint) {
        if (tIn < raiseStart)
            return previous;
        if (tIn < raiseEnd)
            return Sweep(previous, cocked,
                         static_cast<float>(tIn - raiseStart) / static_cast<float>(std::max(1, raiseEnd - raiseStart)));
        return cocked;
    }
    const int t = tIn - base;
    const int swingIn = boss.swingInFrames;
    const BossSwordPose fake = Swing(boss, start, end, swingIn, swingIn + boss.swingFrames);
    if (t <= swingIn)
        return Swing(boss, start, end, t, swingIn + boss.swingFrames);
    if (t <= swingIn + kFeintPullbackFrames)
        return Sweep(fake, cocked, static_cast<float>(t - swingIn) / static_cast<float>(kFeintPullbackFrames));
    return {cocked.x + std::sin(static_cast<float>(t) * 0.8f) * 2.0f, cocked.y};
}

struct StrikePose {
    BossSwordPose start;
    BossSwordPose end;
};

// 連撃の index 番目（0 から）の一撃の始点・終点。横の一撃は左右を往復するので、手前の横の数で向きが決まる。
// 一撃の無い行は、横の一撃 1 つとして扱う。呼ぶたびに配列を作らない（毎 tick 呼ばれる）。
StrikePose StrikePoseAt(const BossMove& move, size_t index) {
    bool side = false;
    for (size_t i = 0; i < move.strikes.size(); ++i) {
        StrikePose pose{};
        switch (move.strikes[i].direction) {
        case BossStrikeDirection::Horizontal:
            pose = side ? StrikePose{kSide, kBack} : StrikePose{kBack, kSide};
            side = !side;
            break;
        case BossStrikeDirection::Vertical: pose = {kUp, kVerticalEnd}; break;
        case BossStrikeDirection::Kesa: pose = {kUpLeft, kDownRight}; break;
        case BossStrikeDirection::ReverseKesa: pose = {kUpRight, kDownLeft}; break;
        }
        if (i == index)
            return pose;
    }
    return {kBack, kSide};
}

int SegmentLength(const BossComponent& boss, const BossMove& move, size_t index) {
    if (BossMoveTable::IsMoveType(move.type) && index == 0)
        return std::max(1, boss.actFrames);
    return move.segments[index].length;
}

} // namespace

BossSegmentProgress BossPose::Progress(const BossComponent& boss, const BossMove& move) {
    BossSegmentProgress result;
    int accumulated = 0, hitIndex = 0;
    for (size_t k = 0; k < move.segments.size(); ++k) {
        const BossSegment& segment = move.segments[k];
        const int length = SegmentLength(boss, move, k);
        if (boss.stateFrame <= accumulated + length) {
            result.segment = static_cast<int>(k);
            result.hitIndex = hitIndex;
            result.tIn = boss.stateFrame - accumulated;
            result.swingLength = std::max(1, std::min(length, boss.swingInFrames + boss.swingFrames));
            result.hit = segment.hit;
            result.progress = segment.hit ? static_cast<float>(result.tIn) / static_cast<float>(result.swingLength)
                                          : static_cast<float>(result.tIn) / static_cast<float>(length);
            return result;
        }
        if (segment.hit)
            ++hitIndex;
        accumulated += length;
    }
    result.segment = std::max(0, static_cast<int>(move.segments.size()) - 1);
    result.hitIndex = hitIndex;
    result.progress = 1.0f;
    return result;
}

namespace {

// 後隙・待機の戻り：from から下ろした構え（kRest）へ、frame F 目（全体 frames F）までに戻す。
BossSwordPose BackToRest(BossSwordPose from, int frame, int frames) { return Sweep(from, kRest, Share(frame, frames)); }

// 待機：技・気絶の終わりの構え（lastEndPose）を保ったまま入った時、poseRecoverFrames F かけて下ろした構えへ戻る。
// 0 は待機の最初の tick で切り替える（これまでの動き）。
BossSwordPose IdlePose(const BossComponent& boss) {
    return boss.poseRecoverFrames > 0 ? BackToRest(boss.lastEndPose, boss.idleFrames, boss.poseRecoverFrames) : kRest;
}

// 気絶：技を打ち切った時の構え（lastEndPose）から、poseRecoverFrames F かけて崩れた構え（kStunned）へ。0 は一気に
// 切り替える（これまでの動き）。
BossSwordPose StunPose(const BossComponent& boss) {
    return boss.poseRecoverFrames > 0
               ? Sweep(boss.lastEndPose, kStunned, Share(boss.stateFrame, boss.poseRecoverFrames))
               : kStunned;
}

// 剣を決まった向きに保つ技（回し蹴り・分身）：前隙で、前の技の構えからその向きへ移す。
BossSwordPose HeldPose(const BossComponent& boss, const BossMove& move, BossSwordPose held) {
    return boss.phase == BossActPhase::Pre ? Sweep(boss.moveStartPose, held, Share(boss.stateFrame, move.pre)) : held;
}

// 1 撃目は左上→右下、2 撃目は右上→左下（X 字）。間の短い区間で 2 撃目の構えへ引く。
BossSwordPose JumpPose(const BossComponent& boss, const BossMove& move) {
    if (boss.phase == BossActPhase::Pre)
        return Windup(boss, boss.moveStartPose, kUpLeft, kDownRight, boss.stateFrame, move.pre, false);
    if (boss.phase == BossActPhase::Post)
        return BackToRest(boss.lastEndPose, boss.stateFrame, move.post);
    const auto g = BossPose::Progress(boss, move);
    if (g.hitIndex % 2 == 0)
        return g.hit ? Swing(boss, kUpLeft, kDownRight, g.tIn, g.swingLength)
                     : Sweep(kDownRight, Cocked(kUpRight, kDownLeft), g.progress);
    return Swing(boss, kUpRight, kDownLeft, g.tIn, g.swingLength);
}

// 剣を頭上へ掲げ、切っ先を下へ向けて地面へ突き立て、溜めの間・爆ぜる間は突き立てたまま。後隙で抜いて戻す。
BossSwordPose DetonatePose(const BossComponent& boss, const BossMove& move) {
    const int t = boss.stateFrame;
    if (boss.phase == BossActPhase::Post)
        return Sweep(kPlunge, kRest, static_cast<float>(t) / static_cast<float>(std::max(1, move.post)));
    if (boss.phase != BossActPhase::Pre)
        return kPlunge;
    const float progress = static_cast<float>(t) / static_cast<float>(std::max(1, move.pre));
    const float raise = std::clamp(boss.detonateRaiseShare, 0.05f, 0.9f);
    const float plant = std::max(raise + 0.01f, boss.detonatePlantShare);
    return progress < raise ? Sweep(boss.moveStartPose, kUp, progress / raise)
                            : Sweep(kUp, kPlunge, (progress - raise) / (plant - raise));
}

BossSwordPose FlameRunPose(const BossComponent& boss, const BossMove& move) {
    const int t = boss.stateFrame;
    if (boss.phase == BossActPhase::Pre)
        return Sweep(boss.moveStartPose, kDrag,
                     static_cast<float>(t) / static_cast<float>(std::max(1, boss.raiseFrames)));
    if (boss.phase == BossActPhase::Post)
        return Sweep(kFlameRunUp, kRest, static_cast<float>(t) / static_cast<float>(std::max(1, move.post)));
    // 攻撃中：いちばん新しく放った前線を放った tick（stateFrame 1＋間隔×番号）からの F で斬り上げる。
    // 最後の 1 本は斬り上げたまま後隙へ。
    const int interval = std::max(1, boss.flameRunIntervalFrames);
    const int since = std::max(0, t - 1 - interval * std::max(0, boss.flameRun.released - 1));
    const int up = std::max(1, static_cast<int>(static_cast<float>(interval) * kFlameRunUpShare));
    if (since <= up)
        return Sweep(kDrag, kFlameRunUp, static_cast<float>(since) / static_cast<float>(up));
    if (boss.flameRun.released >= boss.flameRun.count)
        return kFlameRunUp;
    return Sweep(kFlameRunUp, kDrag, static_cast<float>(since - up) / static_cast<float>(std::max(1, interval - up)));
}

// 屈みながら剣を頭上へ掲げ、跳ぶ・空中で溜める間は掲げたまま。落ちる間に切っ先を下へ向け、着地で突き立てる。
BossSwordPose SkyfallPose(const BossComponent& boss, const BossMove& move) {
    switch (boss.skyfall.phase) {
    case BossSkyfallPhase::Crouch: return Sweep(boss.moveStartPose, kUp, boss.crouch);
    case BossSkyfallPhase::Rise:
    case BossSkyfallPhase::Hang: return kUp;
    case BossSkyfallPhase::Dive: return Sweep(kUp, kPlunge, boss.skyfall.dive * kPlungeTurnSpeed);
    case BossSkyfallPhase::Landed:
        return boss.phase == BossActPhase::Post
                   ? Sweep(kPlunge, kRest,
                           static_cast<float>(boss.stateFrame) / static_cast<float>(std::max(1, move.post)))
                   : kPlunge;
    default: return kRest;
    }
}

// 霞隠れ：前隙で前の技の構えから下ろした構えへ。隠れている間は下ろした構えで、現れる時の振り上げもそこから始める
// （隠れている間は剣が見えないので、前の技の構えへは戻さない）。
BossSwordPose AmbushPose(const BossComponent& boss, const BossMove& move) {
    const int t = boss.stateFrame;
    if (boss.phase == BossActPhase::Pre)
        return Sweep(boss.moveStartPose, kRest, Share(t, move.pre));
    if (boss.phase == BossActPhase::Post)
        return BackToRest(boss.lastEndPose, t, move.post);
    const auto g = BossPose::Progress(boss, move);
    const BossSegment& segment = move.segments[static_cast<size_t>(g.segment)];
    if (g.hit)
        return Swing(boss, kBack, kSide, g.tIn, g.swingLength);
    if (segment.kind != BossSegmentKind::Appear)
        return kRest;
    // 現れて振り上げ→構え（最後の 1 回はフェイント）。
    return Windup(boss, kRest, kBack, kSide, g.tIn, segment.length, segment.feint);
}

// 投げナイフ：前隙で前の技の構えから投げの構えへ、攻撃中で投げ切り、後隙で下ろした構えへ戻る。
BossSwordPose KnifePose(const BossComponent& boss, const BossMove& move) {
    if (boss.phase == BossActPhase::Pre)
        return Sweep(boss.moveStartPose, kKnifeStart, Share(boss.stateFrame, move.pre));
    if (boss.phase == BossActPhase::Act)
        return Sweep(kKnifeStart, kKnifeEnd, BossPose::Progress(boss, move).progress * 2.0f);
    return BackToRest(boss.lastEndPose, boss.stateFrame, move.post);
}

// 連撃：一撃ごとの構えと振りは StrikePoseAt。後隙は最後の一撃の振り終わりを保つ。
BossSwordPose StringPose(const BossComponent& boss, const BossMove& move) {
    const size_t last = std::max<size_t>(1, move.strikes.size()) - 1;
    if (boss.phase == BossActPhase::Pre) {
        const StrikePose first = StrikePoseAt(move, 0);
        return Windup(boss, boss.moveStartPose, first.start, first.end, boss.stateFrame, move.pre,
                      !move.strikes.empty() && move.strikes[0].feint);
    }
    if (boss.phase == BossActPhase::Post)
        return StrikePoseAt(move, last).end;
    const auto g = BossPose::Progress(boss, move);
    const BossSegment& segment = move.segments[static_cast<size_t>(g.segment)];
    const size_t k = std::min(static_cast<size_t>(std::max(0, segment.strike)), last);
    const StrikePose now = StrikePoseAt(move, k);
    if (g.hit)
        return Swing(boss, now.start, now.end, g.tIn, g.swingLength);
    return Windup(boss, StrikePoseAt(move, k > 0 ? k - 1 : 0).end, now.start, now.end, g.tIn, segment.length,
                  segment.feint);
}

} // namespace

BossSwordPose BossPose::Target(const BossComponent& boss) {
    if (boss.state == BossState::Dead)
        return kStunned;
    if (boss.state == BossState::Stunned)
        return StunPose(boss);
    if (boss.state == BossState::Idle)
        return IdlePose(boss);
    const auto& moves = boss.table.GetMoves();
    if (boss.state != BossState::Act || boss.move < 0 || static_cast<size_t>(boss.move) >= moves.size())
        return kRest;
    const BossMove& move = moves[static_cast<size_t>(boss.move)];
    const BossActPhase phase = boss.phase;
    const int t = boss.stateFrame;
    auto sw = [&](BossSwordPose a, BossSwordPose c) {
        if (phase == BossActPhase::Pre)
            return Windup(boss, boss.moveStartPose, a, c, t, move.pre, false);
        if (phase == BossActPhase::Post)
            return c;
        const auto g = Progress(boss, move);
        return g.hit ? Swing(boss, a, c, g.tIn, g.swingLength) : c;
    };
    switch (move.type) {
    case BossMoveType::Dash:
        return phase == BossActPhase::Post ? BackToRest(boss.lastEndPose, t, move.post) : sw(kBack, kSide);
    case BossMoveType::SlashR: return sw(kUpLeft, kDownRight);
    case BossMoveType::SlashL: return sw(kUpRight, kDownLeft);
    case BossMoveType::SlashH: return sw(kBack, kSide);
    case BossMoveType::Overhead:
    case BossMoveType::SlashV: return sw(kUp, kVerticalEnd);
    case BossMoveType::Retreat:
        // 退く間は剣を下げたまま。降りたら振り上げて、溜めの間ずっと振りかぶったまま構える（長い溜めを剣で見せる）。
        if (phase == BossActPhase::Pre) {
            if (!boss.retreat.charging)
                return Sweep(boss.moveStartPose, kRest,
                             Share(t, std::clamp(boss.retreatStepFrames, 1, std::max(1, move.pre - 1))));
            return Sweep(kRest, Cocked(kUp, kVerticalEnd),
                         static_cast<float>(boss.retreat.chargeElapsed) /
                             static_cast<float>(std::max(1, boss.raiseFrames)));
        }
        return sw(kUp, kVerticalEnd);
    case BossMoveType::Jump: return JumpPose(boss, move);
    case BossMoveType::Kick: return HeldPose(boss, move, kHeld);
    case BossMoveType::Detonate: return DetonatePose(boss, move);
    case BossMoveType::Clone: return HeldPose(boss, move, kForward);
    case BossMoveType::FlameRun: return FlameRunPose(boss, move);
    case BossMoveType::Skyfall: return SkyfallPose(boss, move);
    case BossMoveType::Ambush: return AmbushPose(boss, move);
    case BossMoveType::Knife: return KnifePose(boss, move);
    case BossMoveType::String: return StringPose(boss, move);
    default:
        if (phase == BossActPhase::Pre)
            return Sweep(boss.moveStartPose, kRest, Share(t, move.pre));
        return phase == BossActPhase::Post ? BackToRest(boss.lastEndPose, t, move.post) : kRest;
    }
}

float BossPose::RootHeight(const BossComponent& boss) { return boss.height + boss.floatHeight - boss.sink; }

BossSwordLine BossPose::SwordLine(const BossComponent& boss, BossSwordPose pose) {
    const float px = pose.x * kRadiansPerDegree, py = pose.y * kRadiansPerDegree;
    const Vec3 local{std::cos(px) * std::sin(py), -std::sin(px), std::cos(px) * std::cos(py)};
    const Vec3 root{boss.ground.x, boss.ground.y + RootHeight(boss), boss.ground.z};
    return {Add(root, RotateY(boss.swordShoulder, boss.yaw)), RotateY(local, boss.yaw)};
}

BossSwordPoints BossPose::SwordPoints(const BossComponent& boss, BossSwordPose pose) {
    const BossSwordLine line = SwordLine(boss, pose);
    BossSwordPoints points;
    points.base = Add(line.root, Mul(line.direction, boss.swordLength * kSwordBaseShare));
    points.inner = Add(line.root, Mul(line.direction, boss.swordLength * kSwordInnerShare));
    points.outer = Add(line.root, Mul(line.direction, boss.swordLength + boss.swordHazeLength));
    return points;
}

} // namespace Project_TD2_2
