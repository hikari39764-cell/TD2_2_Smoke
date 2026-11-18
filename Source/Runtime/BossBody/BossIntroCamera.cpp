#include "BossIntroCamera.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "HIKARI/Project/ProjectDataView.h"
#include "BossBodyMath.h"
#include "../Boss/BossCsv.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossIntro.h"
#include "../Player/PlayerComponent.h"

namespace Project_TD2_2 {
namespace {

using namespace BossCsv;
using namespace BossBodyMath;

// 固定 tick の数（1 秒あたり）。段の長さ (F) と描画のフレームの秒を合わせる。
constexpr float kFramesPerSecond = 60.0f;

class TextSink final : public HIKARI::IProjectDataTextSink {
public:
    void Receive(const HIKARI::ProjectDataReadResult& result, std::string_view utf8Text) override {
        ok = result.Succeeded();
        text.assign(utf8Text.data(), utf8Text.size());
    }
    bool ok = false;
    std::string text;
};

// 表の列（見出しの行の次から）：カット・段・始め（段の中の割合）・目の位置（始め x y z・終わり x y z）・見る点
// （始め・終わり）・画角（始め・終わり 度）・ゲームのカメラへ戻す重み（始め・終わり）。
constexpr size_t kColumnCount = 19;

BossIntroPhase PhaseOf(std::string_view name) {
    if (name == "Gather" || name == "集める")
        return BossIntroPhase::Gather;
    if (name == "Assemble" || name == "形になる")
        return BossIntroPhase::Assemble;
    if (name == "Draw" || name == "抜刀")
        return BossIntroPhase::Draw;
    if (name == "Flourish" || name == "一振り")
        return BossIntroPhase::Flourish;
    if (name == "Settle" || name == "構え")
        return BossIntroPhase::Settle;
    return BossIntroPhase::Done;
}

float At(const Row& row, size_t column, float fallback) {
    if (column >= row.size())
        return fallback;
    const auto value = NumberOf(row[column]);
    return value ? static_cast<float>(*value) : fallback;
}

Vec3 Point(const Row& row, size_t column) {
    return {At(row, column, 0.0f), At(row, column + 1, 0.0f), At(row, column + 2, 0.0f)};
}

// 登場の始め（集める段の始め）から、その段のその割合までの F。
float FramesAt(const BossComponent& boss, BossIntroPhase phase, float share) {
    float frames = 0.0f;
    for (int p = static_cast<int>(BossIntroPhase::Gather); p < static_cast<int>(phase); ++p)
        frames += static_cast<float>(BossIntro::PhaseFrames(boss, static_cast<BossIntroPhase>(p)));
    return frames + share * static_cast<float>(BossIntro::PhaseFrames(boss, phase));
}

// いまの登場の進み（F。描画のフレームの段の秒から。なめらかに進む）。
float FramesNow(const BossComponent& boss) {
    const BossIntroState& state = boss.introState;
    const float frames = static_cast<float>(BossIntro::PhaseFrames(boss, state.phase));
    const float inside = std::clamp(state.phaseSeconds * kFramesPerSecond, 0.0f, frames);
    return FramesAt(boss, state.phase, 0.0f) + inside;
}

} // namespace

void BossIntroCamera::Load(HIKARI::WorldView& world, BossComponent& boss) {
    std::vector<BossIntroCut>& cuts = boss.introState.cuts;
    cuts.clear();
    const BossIntroSettings& s = boss.intro;
    if (!s.cameraEnabled || s.cameraPath.empty() || boss.introState.phase == BossIntroPhase::Done)
        return;
    TextSink sink;
    world.GetProjectData().ReadText(s.cameraPath, sink);
    if (!sink.ok) {
        char message[256]{};
        std::snprintf(message, sizeof(message), "登場のカメラの表が読めません（Data/%s）", s.cameraPath.c_str());
        BossLog::Write(message);
        return;
    }
    const auto table = ParseCsv(sink.text);
    for (size_t r = 1; r < table.size(); ++r) {
        const Row& row = table[r];
        if (row.size() < kColumnCount)
            continue;
        BossIntroCut cut;
        cut.phase = PhaseOf(Trim(row[1]));
        if (cut.phase == BossIntroPhase::Done)
            continue;
        cut.share = std::clamp(At(row, 2, 0.0f), 0.0f, 1.0f);
        cut.eyeFrom = Point(row, 3);
        cut.eyeTo = Point(row, 6);
        cut.lookFrom = Point(row, 9);
        cut.lookTo = Point(row, 12);
        cut.fovFrom = At(row, 15, 50.0f);
        cut.fovTo = At(row, 16, cut.fovFrom);
        cut.gameFrom = std::clamp(At(row, 17, 0.0f), 0.0f, 1.0f);
        cut.gameTo = std::clamp(At(row, 18, cut.gameFrom), 0.0f, 1.0f);
        cut.at = FramesAt(boss, cut.phase, cut.share);
        cuts.push_back(cut);
    }
    std::stable_sort(cuts.begin(), cuts.end(),
                     [](const BossIntroCut& a, const BossIntroCut& b) { return a.at < b.at; });
    char message[128]{};
    std::snprintf(message, sizeof(message), "IntroCamera cuts=%d", static_cast<int>(cuts.size()));
    BossLog::Write(message);
}

void BossIntroCamera::Update(const BossComponent& boss, PlayerComponent& player) {
    const BossIntroState& state = boss.introState;
    const std::vector<BossIntroCut>& cuts = state.cuts;
    if (cuts.empty() || state.skipped || !BossIntro::Cinematic(boss))
        return;
    const float now = FramesNow(boss);
    if (now < cuts.front().at)
        return;
    size_t index = 0;
    while (index + 1 < cuts.size() && cuts[index + 1].at <= now)
        ++index;
    const BossIntroCut& cut = cuts[index];
    // 最後のカットは、登場の終わり（構えの段の終わり）まで。
    const float end = index + 1 < cuts.size() ? cuts[index + 1].at : FramesAt(boss, BossIntroPhase::Done, 0.0f);
    const float t = Smooth((now - cut.at) / std::max(1.0f, end - cut.at));
    // 表は「ボスの始めの足元を原点、ボスの前（プレイヤーの側）が +Z、ボスの左が +X」の m。
    const auto world = [&](Vec3 local) { return Add(state.origin, RotateY(local, state.yaw)); };
    PlayerCinematicRequest request;
    request.eye = world(Lerp(cut.eyeFrom, cut.eyeTo, t));
    request.target = world(Lerp(cut.lookFrom, cut.lookTo, t));
    request.fovRadians = Lerp(cut.fovFrom, cut.fovTo, t) * kRadiansPerDegree;
    request.gameWeight = Lerp(cut.gameFrom, cut.gameTo, t);
    // 演出の間、ゲームのカメラはボスへ向けておく（戻った時にボスを見ている）。
    request.focus = Add(state.origin, Vec3{0.0f, boss.intro.cameraFocusHeight, 0.0f});
    player.cinematic = request;
    player.cinematicPending = true;
}

} // namespace Project_TD2_2
