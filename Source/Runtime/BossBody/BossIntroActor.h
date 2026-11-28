#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyRig.h"
#include "BossBodyComponent.h"
#include "../Boss/BossComponent.h"
#include "../Boss/BossIntroComponent.h"

namespace Project_TD2_2 {

// 登場の役者（c-e0c500）：動きを捕まえた演技を、骨格と動画を持つ模型（Assets/Models/BossIntroActor）がエンジンの動画の
// 機能で演じる。指も骨で動く。演出の資産（Sequence）の事件段が、どの片段をいつ流すか・部品がいつ骨へ飛ぶかを決める：
// - Anim.<片段>：その段の間、役者はその片段を流す（段の始め＝片段の始め、段の長さ＝流す長さ。段を伸ばすと遅くなる）。
//   次の Anim の段が始まるまでは、流し終えた最後の姿のまま。
// - Part.<部品>.Fly（部品＝HandR・HandL・Helm）：その段の間に、落ちている部品が骨の所まで寄り、段の終わりで体に付く
//   （独立した物体を隠し、役者の同じ部品を見せる）。段が無ければ、初めから付いている。
// - Part.<部品>.Twitch：その段の間、落ちている部品が自分の片段 Twitch（指の痙攣・跳ね）を流す。
// 落ちている部品は、役者の子の物体（模型と動画の部品を持つ）。置き場所は物体の変換（シーンか、演出の資産の物体の轨）で、
// コードは模型の節点へ偏移を足して骨へ寄せるだけ（物体の変換は書かない＝物体の轨とぶつからない）。
namespace BossIntroActor {

// 役者と部品の物体を探し、流せるか確かめる（Initialize と、編集器のプレビューの始め）。役者がいなければ false。
bool Find(HIKARI::WorldView& world, BossIntro& intro);
// 事件段を読み直した後に 1 回：役者が使う段（Anim・Part）を見分ける。
void ReadCues(BossIntro& intro);
// 描画のフレームごと（演技の前・演技の間・プレビュー）：役者を舞台 stage（足元と向き。nullptr なら置き直さない）に置き、
// 演出の時計 t (秒) の姿にする。started が false なら、最初の片段の最初の姿で、部品は落ちたまま。body は体の見た目の
// 設定（草摺の枚数）。
void Drive(HIKARI::WorldView& world, const BossBody& body, BossIntro& intro,
           const HIKARI::GameProjectTransform* stage, float t, bool started);
// 演出の灯を、演技の間に置かれていた所へ置き直す（演技の後、役者がまだ出ている間に毎フレーム呼ぶ）。
void HoldKeyLight(HIKARI::WorldView& world, BossIntro& intro);
// 役者と部品を隠す（演技が済んだ・飛ばした）。
void Hide(HIKARI::WorldView& world, BossIntro& intro);
// 編集器のプレビューをやめた：見せ方・偏移・姿の層を元へ戻す。
void Restore(HIKARI::WorldView& world, BossIntro& intro);
// 効果のための置き場所（兜・刀・胴。BossBodyPose の約束）を、役者の骨から入れる。骨が読めていなければ false。
bool Pose(const BossIntro& intro, BossBodyPose& pose);
// 面頬の隙間の中の光（役者の模型の中の自発光の面）：色と明るさ（画面基準。0 以下か、兜がまだ付いていなければ隠す）。
void SetHelmGlow(HIKARI::WorldView& world, BossIntro& intro, HIKARI::GameProjectFloat3 color, float strength);
// 落ちている兜（体に付く前の部品の物体）の、面頬の隙間の瞬き：色と明るさ（画面基準。0 以下か、兜がもう付いていれば
// 材質のまま＝黒）。
void SetHelmPartGlint(HIKARI::WorldView& world, BossIntro& intro, HIKARI::GameProjectFloat3 color,
                      float strength);
// 役者の刀の刃口の光（材質 BladeEdge の発光）：色と明るさ（画面基準。0 以下なら材質のまま）。
void SetBladeEdge(HIKARI::WorldView& world, BossIntro& intro, HIKARI::GameProjectFloat3 color, float strength);
// 体に付く効果のための体の変換（原点は体の中心：腰の真下の足元から体の高さの半分だけ上。向きは腰の前）。
bool BodyTransform(const Boss& boss, const BossIntro& intro, HIKARI::GameProjectTransform& body);
// 地に立つ刀が地面に入る所（世界）。演技の始めの刀の骨から。
HIKARI::GameProjectFloat3 SwordEntry(const BossIntro& intro);

} // namespace BossIntroActor
} // namespace Project_TD2_2
