#pragma once

#include "HIKARI/Project/FrameContext.h"
#include "HIKARI/Project/WorldView.h"
#include "BossBrain.h"
#include "BossTreeBrain.h"

namespace Project_TD2_2 {

// どの技を出すかを聞く相手：ふだんは行動ツリー、確かめ用の項目が入っているボスだけ確かめ用。世界につき 1 組
// （GameLoop.cpp の GameLoop が持つ）。
struct BossBrains {
    BossTestBrain test;
    BossTreeBrain tree;
    // 行動ツリーの節を登録したか（ボスの居る場面の最初の tick に 1 度だけ）。
    bool attached = false;
};

} // namespace Project_TD2_2

namespace Project_TD2_2::BossSystem {

// ボスの本体の規則：表の読み込み、HP・フェーズ・気絶、技の実行、斬撃、体の姿勢、場の霧への煙と霞のエフェクト。
// 旧 IGameSystem の各段の中身。段の名前は旧いまま。HIKARI_SYSTEM（GameLoop.cpp の GameLoop）が、旧い実行順
// （Flow 80・PlayerDebug 90・Player 100・CloudMount 102・GourdShot 105・Gourd 110・GourdFeedback 115・物理 120・Boss 125・
// BurnField 130）と同じ並びで呼ぶ。場面にボスの部品が無い時は、どの段も何もしない（旧い System はボスの部品に結んであった）。
void FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame, BossBrains& brains);
void Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame);
void LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame);
void PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame);
void OnWorldDetached(HIKARI::WorldView& world);

} // namespace Project_TD2_2::BossSystem
