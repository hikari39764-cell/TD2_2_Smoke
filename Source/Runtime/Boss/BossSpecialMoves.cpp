#include "BossSpecialMoves.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossClones.h"
#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossWaves.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 屈みを戻す割合（1 tick あたり）。試作の回し蹴りの値。
constexpr float kKickCrouchBlend = 0.15f;
// 跳躍の後隙：最初の 1/3 で降りきる（試作 3.6 × (1 − 後隙の進み × 3)）。
constexpr float kJumpLandSpeed = 3.0f;

void Log(const BossComponent& boss, const char* event, const char* extra) {
    char message[384]{};
    std::snprintf(message, sizeof(message), "%s tick=%llu stateFrame=%d %s", event,
                  static_cast<unsigned long long>(boss.tick), boss.stateFrame, extra);
    BossLog::Write(message);
}

Vec3 PlayerGround(const BossFrame& frame) { return {frame.playerCenter.x, frame.boss.ground.y, frame.playerCenter.z}; }

float Share(int tIn, int frames) {
    return std::clamp(static_cast<float>(tIn) / static_cast<float>(std::max(1, frames)), 0.0f, 1.0f);
}

// 霞隠れの「現れる」：プレイヤーの向きの反対（背後）から左右へ散らした所に回り込み、プレイヤーを向く。
void Appear(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    if (frame.player == nullptr)
        return;
    const Vec3 behind = Mul(PlayerRules::GetFacing(*frame.player), -1.0f);
    const float spread = boss.ambushAppearSpreadDegrees * kRadiansPerDegree;
    const float angle = (BossRandom(boss) * 2.0f - 1.0f) * spread;
    Vec3 to = Add(PlayerGround(frame), Mul(FlatNormalize(RotateY(behind, angle)), boss.ambushAppearDistance));
    BossMoveExecutor::ClampToArena(boss, to);
    boss.ground = to;
    boss.lastBodyPosition = to;
    boss.yaw = YawOf(Sub(PlayerGround(frame), boss.ground));
    boss.hidden = false;
    boss.sink = boss.hideSinkDepth;
    BossSpecialMoves::Puff(boss, boss.ground);
    // 背後からの角度：プレイヤーの真後ろが 0、真横が 90。
    const Vec3 fromPlayer = FlatNormalize(Sub(boss.ground, PlayerGround(frame)));
    const float behindDegrees =
        std::acos(std::clamp(Dot(fromPlayer, FlatNormalize(behind)), -1.0f, 1.0f)) / kRadiansPerDegree;
    char extra[192]{};
    std::snprintf(extra, sizeof(extra), "distance=%.3f behindDegrees=%.1f rolled=%.1f",
                  static_cast<double>(FlatLength(Sub(boss.ground, PlayerGround(frame)))),
                  static_cast<double>(behindDegrees), static_cast<double>(angle / kRadiansPerDegree));
    Log(boss, "AmbushAppear", extra);
}

void AmbushAct(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g, bool entered) {
    BossComponent& boss = frame.boss;
    const BossSegment& segment = move.segments[static_cast<size_t>(g.segment)];
    if (segment.kind == BossSegmentKind::Hide) {
        if (entered) {
            BossSpecialMoves::Puff(boss, boss.ground);
            boss.hidden = true;
            char extra[64]{};
            std::snprintf(extra, sizeof(extra), "cycle=%d", segment.strike + 1);
            Log(boss, "AmbushHide", extra);
        }
        boss.sink = Lerp(boss.hidePreSinkDepth, boss.hideSinkDepth, Share(g.tIn, boss.hideSinkFrames));
        return;
    }
    if (segment.kind == BossSegmentKind::Appear) {
        if (entered)
            Appear(frame);
        boss.sink = boss.hideSinkDepth * (1.0f - Share(g.tIn, boss.appearRiseFrames));
        BossMoveExecutor::FaceRate(frame, boss.ambushAppearTurnRate);
        boss.crouch = Share(g.tIn, segment.length);
        return;
    }
    boss.sink = 0.0f;
    if (g.hit && g.tIn <= g.swingLength) {
        boss.ground =
            Add(boss.ground, Mul(Forward(boss.yaw), boss.ambushLungeDistance / static_cast<float>(g.swingLength)));
        boss.crouch = 0.0f;
    }
}

} // namespace

void BossSpecialMoves::Pre(BossFrame& frame, const BossMove& move, float progress) {
    BossComponent& boss = frame.boss;
    switch (move.type) {
    case BossMoveType::Jump: {
        const float share = std::clamp(boss.jumpCrouchShare, 0.0f, 0.95f);
        const float lift = progress < share ? 0.0f : (progress - share) / (1.0f - share);
        boss.crouch = progress < share && share > 0.0f ? progress / share : 0.0f;
        boss.height = boss.jumpHeight * (1.0f - (1.0f - lift) * (1.0f - lift));
        break;
    }
    case BossMoveType::Kick: boss.crouch = std::clamp(progress, 0.0f, 1.0f); break;
    // 霞隠れ・分身：霧の中へ少しずつ沈む（試作では体が薄れていく）。
    case BossMoveType::Ambush:
    case BossMoveType::Clone: boss.sink = boss.hidePreSinkDepth * std::clamp(progress, 0.0f, 1.0f); break;
    default: break;
    }
}

void BossSpecialMoves::Act(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g, bool entered,
                           float progress) {
    BossComponent& boss = frame.boss;
    switch (move.type) {
    case BossMoveType::Kick:
        boss.spin = progress * 2.0f * kPi;
        boss.crouch = Lerp(boss.crouch, 0.0f, kKickCrouchBlend);
        break;
    case BossMoveType::Ambush: AmbushAct(frame, move, g, entered); break;
    case BossMoveType::Clone:
        // 新しい場所で、霧から浮かび上がる。
        boss.sink = boss.hidePreSinkDepth * (1.0f - Share(boss.stateFrame, boss.appearRiseFrames));
        break;
    default: break;
    }
}

void BossSpecialMoves::Trigger(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    if (move.type == BossMoveType::Kick) {
        BossWaves::SpawnRing(frame, boss.move);
        BossBlast& blast = boss.blasts[boss.blastCursor++ % boss.blasts.size()];
        blast = {};
        blast.effectPending = true;
        blast.scale = boss.ringBurstScale;
        blast.position = {boss.ground.x, boss.ground.y + boss.ringHeight, boss.ground.z};
    } else if (move.type == BossMoveType::Clone) {
        BossClones::Place(frame, move);
    }
}

void BossSpecialMoves::Post(BossComponent& boss, const BossMove& move, float progress) {
    boss.hidden = false;
    boss.spin = 0.0f;
    switch (move.type) {
    case BossMoveType::Jump: boss.height = boss.jumpHeight * std::max(0.0f, 1.0f - progress * kJumpLandSpeed); break;
    case BossMoveType::Ambush:
    case BossMoveType::Clone: boss.sink = 0.0f; break;
    default: break;
    }
}

void BossSpecialMoves::Puff(BossComponent& boss, Vec3 ground, bool residue) {
    if (residue)
        BossWaves::AddDeposit(boss, {ground.x, ground.y + boss.depositHeight, ground.z}, boss.puffResidue,
                              boss.puffRadius);
    if (!(boss.puffEffectScale > 0.0f))
        return;
    BossBlast& blast = boss.blasts[boss.blastCursor++ % boss.blasts.size()];
    blast = {};
    blast.effectPending = true;
    blast.scale = boss.puffEffectScale;
    blast.position = {ground.x, ground.y + boss.bodyHalfHeight, ground.z};
}

} // namespace Project_TD2_2
