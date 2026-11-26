#include "BossTableLoader.h"

#include <cstdio>
#include <string>
#include <vector>

#include "HIKARI/Project/ProjectDataView.h"
#include "BossFrame.h"

namespace Project_TD2_2 {
namespace {

class TextSink final : public HIKARI::IProjectDataTextSink {
public:
    void Receive(const HIKARI::ProjectDataReadResult& result, std::string_view utf8Text) override {
        ok = result.Succeeded();
        text.assign(utf8Text.data(), utf8Text.size());
    }
    bool ok = false;
    std::string text;
};

BossTiming TimingOf(const BossComponent& boss) {
    BossTiming timing;
    timing.swingFrames = boss.swingFrames;
    timing.gapSmallFrames = boss.gapSmallFrames;
    timing.gapMediumFrames = boss.gapMediumFrames;
    timing.gapLargeFrames = boss.gapLargeFrames;
    timing.followFrames = boss.followFrames;
    timing.feintFrames = boss.feintFrames;
    timing.kamaeFrames = boss.kamaeFrames;
    timing.raiseFrames = boss.raiseFrames;
    timing.swingInFrames = boss.swingInFrames;
    timing.damageBase = boss.damageBase;
    timing.damagePerFrame = boss.damagePerFrame;
    return timing;
}

void LogMoves(const BossComponent& boss) {
    const auto& moves = boss.table.GetMoves();
    for (size_t i = 0; i < moves.size(); ++i) {
        const BossMove& move = moves[i];
        std::string segments, strikes;
        for (const auto& segment : move.segments)
            segments += std::to_string(segment.length) + (segment.hit ? "h" : "") + (segment.feint ? "f" : "") + "/";
        for (const auto& strike : move.strikes) {
            char text[96]{};
            std::snprintf(text, sizeof(text), "[d%d s%.2f v%.2f h%.0f st%.2f %s r%d]", strike.damage,
                          static_cast<double>(strike.size), static_cast<double>(strike.speed),
                          static_cast<double>(strike.homing), static_cast<double>(strike.step),
                          strike.blow ? "blow" : "flinch", strike.reference);
            strikes += text;
        }
        std::string extras;
        for (const int extra : move.windupExtras)
            extras += (extras.empty() ? "" : "/") + std::to_string(extra);
        // 残す火：形[:一撃]〔@フェーズ〕/幅/秒 を , で並べる（1 つで一撃・フェーズの限りが無ければ以前と同じ line/1.50/25.00）。
        std::string burns;
        for (int k = 0; k < move.burnMarkCount; ++k) {
            const BossBurnMark& mark = move.burnMarks[static_cast<size_t>(k)];
            char values[48]{};
            std::snprintf(values, sizeof(values), "/%.2f/%.2f", static_cast<double>(mark.width),
                          static_cast<double>(mark.seconds));
            burns += std::string(k > 0 ? "," : "") + BossMoveTable::BurnKey(mark.shape) +
                     (mark.strike > 0 ? ":" + std::to_string(mark.strike) : "") +
                     (mark.minPhase > 1 ? "@" + std::to_string(mark.minPhase) : "") + values;
        }
        char message[1024]{};
        std::snprintf(message, sizeof(message),
                      "Move index=%zu name=%s type=%s executable=%d pre=%d act=%d post=%d segments=%s damage=%d "
                      "auto=%d size=%.2f speed=%.2f homing=%.1f step=%.2f burn=%s trackStop=%d windup=%s "
                      "breakHits=%d strikes=%s",
                      i, move.name.c_str(), BossMoveTable::TypeKey(move.type), static_cast<int>(move.executable),
                      move.pre, move.act, move.post, segments.c_str(), move.damage, static_cast<int>(move.damageAuto),
                      static_cast<double>(move.size), static_cast<double>(move.speed), static_cast<double>(move.homing),
                      static_cast<double>(move.step), burns.empty() ? "none" : burns.c_str(), move.trackStopFrames,
                      extras.empty() ? "-" : extras.c_str(), move.breakHits, strikes.c_str());
        BossLog::Write(message);
    }
}

} // namespace

void BossTableLoader::Load(HIKARI::WorldView& world, BossComponent& boss) {
    TextSink types, moves, clips, segments;
    world.GetProjectData().ReadText(boss.typesPath, types);
    world.GetProjectData().ReadText(boss.movesPath, moves);
    if (!boss.clipsPath.empty())
        world.GetProjectData().ReadText(boss.clipsPath, clips);
    if (!boss.segmentsPath.empty())
        world.GetProjectData().ReadText(boss.segmentsPath, segments);
    if (!types.ok || !moves.ok) {
        if (!boss.tableLoaded) {
            char message[256]{};
            std::snprintf(message, sizeof(message), "技の表が読めません（Data/%s、Data/%s）", boss.movesPath.c_str(),
                          boss.typesPath.c_str());
            BossLog::Warn(message);
            boss.tableLoaded = true;
        }
        return;
    }
    if (types.text == boss.loadedTypesText && moves.text == boss.loadedMovesText &&
        (!clips.ok || clips.text == boss.loadedClipsText) && (!segments.ok || segments.text == boss.loadedSegmentsText))
        return;
    std::vector<std::string> warnings;
    BossMoveTable table;
    table.LoadTypes(types.text, warnings);
    table.LoadMoves(moves.text, TimingOf(boss), warnings);
    // モーションの技の区切り（c-7b1a08）：アニメーションの列がある技の長さを、クリップの長さと区間の表で決め直す。
    table.ApplyClips(clips.ok ? std::string_view(clips.text) : std::string_view{},
                     segments.ok ? std::string_view(segments.text) : std::string_view{}, warnings);
    boss.table = std::move(table);
    boss.loadedTypesText = types.text;
    boss.loadedMovesText = moves.text;
    boss.loadedClipsText = clips.ok ? clips.text : std::string{};
    boss.loadedSegmentsText = segments.ok ? segments.text : std::string{};
    boss.tableLoaded = true;
    // 行が変わったので、行動ツリーの節と表の突き合わせ（節の無い始動技・表に無い名前）をやり直す。
    boss.brain.scoredRows = 0;
    boss.brain.coverageChecked = false;
    boss.brain.unknownCount = 0;
    // 咎められた回数は行の番号で数えているので、行が変わったら数え直す。
    boss.watch.punished = {};
    for (const auto& warning : warnings)
        BossLog::Warn(warning.c_str());
    char message[160]{};
    std::snprintf(message, sizeof(message), "TableLoaded tick=%llu moves=%zu warnings=%zu",
                  static_cast<unsigned long long>(boss.tick), boss.table.GetMoves().size(), warnings.size());
    BossLog::Write(message);
    LogMoves(boss);
}

void BossTableLoader::Describe(HIKARI::PropertyFields& fields, BossComponent& boss) {
    if (fields.Section("技の表", true)) {
        fields.Text("movesPath", "技の表（Data/ からの場所）", boss.movesPath);
        fields.Text("clipsPath",
                    "クリップの長さの表（Data/ からの場所。Tools/BossAnim/export_clip_lengths.ps1 が書き出す）",
                    boss.clipsPath);
        fields.Text("segmentsPath", "区間の表（Data/ からの場所。モーションの技の当たりの窓。空なら使わない）",
                    boss.segmentsPath);
        fields.Text("typesPath", "動きの型の既定値の表（Data/ からの場所）", boss.typesPath);
        fields.Note("表は技を出すたびに読み直すので、Play 中に CSV を保存すると次に出す技から変わります。"
                    "HP は「フェーズと気絶」の 2 本の条です（同じ物体の「被弾と HP」の最大 HP は使いません）。");
    }
}

} // namespace Project_TD2_2
