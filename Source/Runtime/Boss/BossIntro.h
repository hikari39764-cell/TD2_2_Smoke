#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "BossIntroComponent.h"

namespace Project_TD2_2 {

// 登場の規則（c-429242・役者が骨格の動画で演じる形は c-e0c500）：段を固定 tick ごとに進める。演技の段の始めに演出の
// 資産（Sequence）を流し、その資産が終わったら、戦いの体に替えて一息の段へ進む。登場の値と状態は、ボスと同じ物体の
// BossIntro（c-eb3b08）。問う関数は、その部品のポインタを取る：無ければ（nullptr）登場を切った時と同じ答え。
class BossIntroRules final {
public:
    // 始めの足元と向きを覚え、登場をする時は「散らばって待つ」から始める（ボスの Initialize が呼ぶ）。
    // seen：この Play でもう登場を始めた事がある（やり直しの後）。
    static void Begin(const Boss& boss, BossIntro& intro, bool seen);
    // 登場をやめる（役者がシーンにいない時。Begin の後に呼ぶ）：初めから構えて立つ。
    static void Cancel(BossIntro& intro);
    // 流している演出の資産 handle の事件段を、効果の拍として写す（流し始めた時・プレビューの手綱が変わった時に 1 回）。
    static void ReadCues(HIKARI::WorldView& world, HIKARI::GameProjectSequencePlaybackHandle handle,
                         BossIntroState& state);
    // 登場が始まっている（演技が始まった後）。
    static bool Started(const BossIntro* intro) { return intro != nullptr && intro->state.started; }
    // 散らばって待つ間に 1 度だけ、舞台の枠と体の向きを、プレイヤーのいる側へ向ける（プレイヤーが見えた tick。演技の
    // 前なので、演出の資産のカメラが付く物体は、演技の間はもう回らない）。
    static void Face(Boss& boss, BossIntro& intro, HIKARI::GameProjectFloat3 playerPosition);
    // 1 tick 進める。戦いが始まった（Flow が止めるのをやめた）tick に、演技が始まる。
    static void Step(HIKARI::WorldView& world, const Boss& boss, BossIntro& intro);
    // 登場が済むまで、歩かず技も出さない。
    static bool Holding(const BossIntro* intro) {
        return intro != nullptr && intro->state.phase != BossIntroPhase::Done;
    }
    // 構えに収まるまで、当たらない。
    static bool Shielded(const BossIntro* intro) {
        return intro != nullptr && intro->state.phase < BossIntroPhase::Settle;
    }
    // 演出の間（演技が始まってから、構えに収まるまで。飛ばした後は済み）：プレイヤーの入力を止めて無敵にする。
    static bool Cinematic(const BossIntro* intro) {
        if (intro == nullptr)
            return false;
        const BossIntroPhase phase = intro->state.phase;
        return phase != BossIntroPhase::Scattered && phase != BossIntroPhase::Done;
    }
    // 一息の段の初め、役者がまだ出ている間（カメラが引いてから、戦いの体に替える。飛ばした時は、すぐ替わる）。
    static bool ActorLingers(const BossIntro* intro) {
        if (intro == nullptr)
            return false;
        const BossIntroState& state = intro->state;
        return state.phase == BossIntroPhase::Settle && !state.skipped &&
               static_cast<float>(state.frame) < intro->introSettleSwapSeconds * 60.0f;
    }
    // 体の物体を舞台の枠に止めておく間（散らばって待つ・演技。演出の資産のカメラが物体に付くので動かさない）。
    static bool Pinned(const BossIntro* intro) {
        if (intro == nullptr)
            return false;
        const BossIntroPhase phase = intro->state.phase;
        return phase == BossIntroPhase::Scattered || phase == BossIntroPhase::Show;
    }
    // 止めておく間の、体の物体の変換（舞台の足元から体の高さの半分だけ上。向きは舞台の向き）。
    static HIKARI::GameProjectTransform StageTransform(const Boss& boss, const BossIntro& intro);
    // 舞台の足元の変換（役者の物体を置く所。向きは舞台の向き）。
    static HIKARI::GameProjectTransform GroundTransform(const BossIntro& intro);
    // ボスの HP の条を出すか（演技が済んでから出す）。
    static bool BarShown(const BossIntro* intro);
    // いまの段の長さ (F)。長さの無い段（待つ・済み）は 0。
    static int PhaseFrames(const BossIntro& intro, BossIntroPhase phase);
};

} // namespace Project_TD2_2
