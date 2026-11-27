#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "BossPhaseShiftComponent.h"

namespace Project_TD2_2 {

// フェーズの変わり目の演出の規則（c-271cba で動捕の演出に作り直し。元は c-8dd296）：1 本目が空になった tick に始め、
// 次の tick に舞台を決めて演出の資産（Sequence）を流す。資産の時刻が時計：一閃の事件段でフェーズ 2 の見た目へ切り替えて
// HP の条を満たし始め、Settle の事件段（無ければ資産の終わり）でカメラをゲームのカメラへ渡す段へ、渡し終えたら済み。
// 変わり目そのもの（無敵・技を出さない・2 本目を満たす）は BossHealthBars（c-3e0ffa）のまま。見た目は
// BossPhaseShiftVisuals、カメラの渡しは BossPhaseShiftCamera。設定と状態は同じ物体の BossPhaseShiftComponent。
// shift が nullptr（部品が無い）の時は、演出を切った時と同じ（変わり目は Boss.phaseShiftFrames の間、待機のまま）。
class BossPhaseShift final {
public:
    // 変わり目の長さの上限 (F)：演出をする時は資産の長さ＋渡し＋余り（済んだ時に打ち切る）、しない時は phaseShiftFrames。
    static int TotalFrames(const BossComponent& boss, const BossPhaseShiftComponent* shift);
    // 1 本目が空になった tick（BossHealthBars::CheckEmptied の後）に呼ぶ：演出を始める頼みを立てる（資産は次の Step で流す）。
    // rootBefore は打ち切る前の体の高さ（記録に書く）。
    static void Begin(BossComponent& boss, BossPhaseShiftComponent* shift, HIKARI::GameProjectFloat3 playerCenter,
                      bool hasPlayer, float rootBefore);
    // 固定 tick の始め（BossHealthBars::Step の後）：段を進める。演出の後の、技を選ばない間も数える。
    static void Step(HIKARI::WorldView& world, BossComponent& boss, BossPhaseShiftComponent* shift);
    // 演出の間（始めの頼みから、カメラを渡し終えるまで）：プレイヤーの入力を止めて無敵にする。
    static bool Cinematic(const BossPhaseShiftComponent* shift) {
        if (shift == nullptr)
            return false;
        const BossShiftBeat beat = shift->state.beat;
        return beat != BossShiftBeat::None && beat != BossShiftBeat::Done;
    }
    // 体の物体を舞台に止めておく間（資産のカメラは骨格の体の物体に付くので、浮遊のゆれを入れない）。
    static bool Pinned(const BossPhaseShiftComponent* shift) {
        return shift != nullptr &&
               (shift->state.beat == BossShiftBeat::Show || shift->state.beat == BossShiftBeat::Settle);
    }
    // 止めておく間の、体の物体の変換（舞台の足元から体の高さの半分だけ上。向きは舞台の向き）。
    static HIKARI::GameProjectTransform StageTransform(const BossComponent& boss, const BossPhaseShiftComponent& shift);
    // 演出の後の、技を選ばない間。
    static bool Calm(const BossPhaseShiftComponent* shift) { return shift != nullptr && shift->state.calmLeft > 0; }
    // プレイヤーを置き直す頼み（舞台を決めた tick に立つ）を取る。取ったら true（at：置く所、facing：向く向き）。
    static bool TakePlacement(BossPhaseShiftComponent* shift, HIKARI::GameProjectFloat3& at,
                              HIKARI::GameProjectFloat3& facing);
    // 事件段の時刻（無ければ start が負）。
    static const BossShiftCueTime& Cue(const BossPhaseShiftComponent& shift, BossShiftCue cue) {
        return shift.state.cues[static_cast<size_t>(cue)];
    }
    // 事件段の中の進み（0〜1。始まる前は 0、終わった後は 1。段が無ければ 0）。長さ 0 の段は始まったら 1。
    static float CueProgress(const BossPhaseShiftComponent& shift, BossShiftCue cue, float seconds);
    // 事件段が始まったか（段が無ければ false）。
    static bool CueStarted(const BossPhaseShiftComponent& shift, BossShiftCue cue, float seconds);
    // 流している資産の事件段を、拍ごとに写す（流し始めた時・プレビューの手綱が変わった時）。
    static void ReadCues(HIKARI::WorldView& world, HIKARI::GameProjectSequencePlaybackHandle handle,
                         BossPhaseShiftComponent& shift);
};

} // namespace Project_TD2_2
