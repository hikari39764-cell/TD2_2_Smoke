#pragma once

#include "BossComponent.h"

namespace Project_TD2_2 {

// 登場の規則（c-429242）：段を固定 tick ごとに進める。エンジンへは何も頼まない。
class BossIntro final {
public:
    // 始めの足元と向きを覚え、登場をする時は「散らばって待つ」から始める（ボスの Initialize が呼ぶ）。
    // seen：この Play でもう登場を始めた事がある（やり直しの後）。
    static void Begin(BossComponent& boss, bool seen);
    // 登場が始まっている（煙が集まり始めた後）。
    static bool Started(const BossComponent& boss) { return boss.introState.started; }
    // 散らばって待つ間に 1 度だけ、散らばりの基準と体の向きを、プレイヤーのいる側へ向ける（プレイヤーが見えた tick）。
    static void Face(BossComponent& boss, HIKARI::GameProjectFloat3 playerPosition);
    // 1 tick 進める。戦いが始まった（Flow が止めるのをやめた）tick に、煙が集まり始める。
    static void Step(BossComponent& boss);
    // 登場が済むまで、歩かず技も出さない。
    static bool Holding(const BossComponent& boss) { return boss.introState.phase != BossIntroPhase::Done; }
    // 構えに収まるまで、当たらない。
    static bool Shielded(const BossComponent& boss) { return boss.introState.phase < BossIntroPhase::Settle; }
    // 演出の間（煙が集まり始めてから、構えに収まるまで。飛ばした後は済み）：プレイヤーの入力を止めて無敵にする。
    static bool Cinematic(const BossComponent& boss) {
        const BossIntroPhase phase = boss.introState.phase;
        return phase != BossIntroPhase::Scattered && phase != BossIntroPhase::Done;
    }
    // ボスの HP の条を出すか（演出の間は見得まで出さない）。
    static bool BarShown(const BossComponent& boss);
    // いまの段の長さ (F)。長さの無い段（待つ・済み）は 0。
    static int PhaseFrames(const BossComponent& boss, BossIntroPhase phase);
};

} // namespace Project_TD2_2
