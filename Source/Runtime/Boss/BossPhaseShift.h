#pragma once

#include "BossComponent.h"
#include "BossPhaseShiftComponent.h"

namespace Project_TD2_2 {

struct Player;

// フェーズの変わり目の演出の規則（c-8dd296）：拍を固定 tick ごとに進め、閃光の tick（フェーズ 2 の見た目の切り替え・
// プレイヤーを押し出す）と、HP の条が満ち始める時を決める。変わり目そのもの（無敵・技を出さない・2 本目を満たす）は
// BossHealthBars（c-3e0ffa）のまま。見た目は BossPhaseShiftVisuals が読む。
// 演出の設定と状態は同じ物体の BossPhaseShiftComponent（c-3a0f87）。shift が nullptr（部品が無い）の時は、演出を
// 切った時と同じ（変わり目は Boss.phaseShiftFrames の間、待機のまま。フェーズ 2 の見た目はすぐ出る）。
class BossPhaseShift final {
public:
    // 変わり目の長さ (F)：演出をする時は拍の長さの合計、しない時は Boss.phaseShiftFrames。
    static int TotalFrames(const BossComponent& boss, const BossPhaseShiftComponent* shift);
    // 拍の長さ (F。長さの無い拍は 0) と、変わり目の始めからその拍の始めまでの F。
    static int BeatFrames(const BossPhaseShiftComponent& shift, BossShiftBeat beat);
    static int FramesBefore(const BossPhaseShiftComponent& shift, BossShiftBeat beat);
    // 1 本目が空になった tick（BossHealthBars::CheckEmptied の後）に呼ぶ：演出を始める。rootBefore は打ち切る前の
    // 体の高さ（記録に書く）。プレイヤーがいれば、その側を演出の前にする。
    static void Begin(BossComponent& boss, BossPhaseShiftComponent* shift, HIKARI::GameProjectFloat3 playerCenter,
                      bool hasPlayer, float rootBefore);
    // 固定 tick の始め（BossHealthBars::Step の後）：拍を 1 F 進める。演出の後の、技を選ばない間も数える。
    static void Step(BossComponent& boss, BossPhaseShiftComponent* shift);
    // 演出の間（崩れてから構えに収まるまで）：プレイヤーの入力を止めて無敵にし、演出のカメラを出す。
    static bool Cinematic(const BossPhaseShiftComponent* shift) {
        if (shift == nullptr)
            return false;
        const BossShiftBeat beat = shift->state.beat;
        return beat != BossShiftBeat::None && beat != BossShiftBeat::Done;
    }
    // 演出の後の、技を選ばない間。
    static bool Calm(const BossPhaseShiftComponent* shift) { return shift != nullptr && shift->state.calmLeft > 0; }
    // 閃光の後の最初の tick：プレイヤーを押し出す（ボスから pushToDistance まで。場の縁の内側で止める）。
    static void Push(BossComponent& boss, BossPhaseShiftComponent* shift, Player& player,
                     HIKARI::GameProjectFloat3 playerCenter);
};

} // namespace Project_TD2_2
