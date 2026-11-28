#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossSwordPose.h"

namespace Project_TD2_2 {

struct Boss;
struct BossBody;
struct BossMove;
struct BossBodyPose;


// 技のクリップの、規則の時計の F（60 Hz）ごとの刀の骨と左手の骨の置き場所（模型の空間：足元が原点、+X 左・+Z 前、
// 骨格の体の大きさの倍率は掛けていない）。初めて要った時に、クリップをその時刻で 1 度だけ読む（c-7b1a09）。
struct BossClipSwordTrack {
    std::string clip;
    std::vector<HIKARI::GameProjectFloat3> swordPosition;
    std::vector<HIKARI::GameProjectQuaternion> swordRotation;
    std::vector<HIKARI::GameProjectFloat3> leftHand;
    std::vector<HIKARI::GameProjectQuaternion> leftHandRotation;
    // 刀の骨の動きの指紋（c-884095。狙いのずれの表が、今のクリップで計算されたかを見る）。SwordFingerprint で作る。
    uint32_t fingerprint = 0;
};

// 技のクリップの刀（規則の側。c-bd1a7a でボスの部品の骨格の体の状態から分けた）：クリップごとの刀の骨・左手の骨と、
// 読めなかったクリップ（何度も試さない）、待機のクリップの始めの刀。規則の tick が読む（PrepareSword が固定 tick の中で
// 読んでおく。描画のフレームの値ではない）。ボスの部品（Boss::clipSwords）が持つ。
struct BossClipSwords {
    std::vector<BossClipSwordTrack> tracks;
    std::vector<std::string> failed;
    // 待機のクリップの始め（時刻 0）の刀の骨（模型の空間。c-164cab）：クリップの刀で当たる技でない間の、規則の剣の構え。
    // PrepareSword が固定 tick の中で 1 度だけ読む。読めたか・読めなかったか（何度も試さない）。
    HIKARI::GameProjectFloat3 idlePosition{};
    HIKARI::GameProjectQuaternion idleRotation{0.0f, 0.0f, 0.0f, 1.0f};
    bool idleReady = false;
    bool idleFailed = false;
    // 狙いのずれの表の指紋を、読んだ刀と比べたか（表を読み直すと BossTableLoader が戻す）。
    bool aimChecked = false;
};

// 骨格の体の、描画のフレームごとの状態（シーンに保存しない。描画のフレームだけが書き、読む。体の見た目の部品
// BossBody::anim が持つ）。
struct BossAnimBodyState {
    // 物体の模型を出したか（-1 まだ聞いていない・0 隠した・1 出した）。
    int8_t shown = -1;
    // いま流しているクリップ（空は何も流していない）。
    std::string playing;
    // いま重ねている技のクリップ（空は重ねていない）。
    std::string layerClip;
    // 骨格の体が体を演じているか。
    bool active = false;
    // 流せなかったクリップ（同じクリップで何度も知らせない）。
    std::string failedClip;
    // 物体が無い・動画の機能が無い時に 1 度だけ知らせたか。
    bool warned = false;
    // 最後に骨格の体を置いた足元・向き・大きさ（描画のフレーム。刀光を見える刀と同じ所に出す）。
    HIKARI::GameProjectTransform placed{};
    // 見えているクリップの刀の剣先の速さ (m/s。描画のフレーム。刀光・振りのゆがみを出す目安)。
    float tipSpeed = 0.0f;
    // 見えている左手の骨の所（世界。描画のフレーム。1 フレーム前に描いた姿）と、それを読めたか。
    HIKARI::GameProjectFloat3 leftHandWorld{};
    bool leftHandSeen = false;
    // 技のクリップの層の重み（0〜1。c-bc5908：待機の姿から重ね始める時は、混ぜる長さで 0 から上げる。いきなり 1 に
    // すると待機の姿から技の始めの姿へ跳ぶ）、待機の姿から重ね始めたか、層を外してからの秒。
    float layerWeight = 0.0f;
    bool layerFromIdle = false;
    float layerOffSeconds = 1000.0f;
    // 見えている刀の骨（世界。描画のフレーム。剣先の速さを出す。規則の tick は読まない）と、それを読めたか。
    HIKARI::GameProjectFloat3 swordGripWorld{};
    HIKARI::GameProjectFloat3 swordDirectionWorld{};
    bool swordSeen = false;
    // 前のフレームの剣先（剣先の速さを出す）。
    HIKARI::GameProjectFloat3 lastTip{};
    bool lastTipValid = false;
    // 骨の読み方を 1 度だけ記録に書いたか。
    bool bonesLogged = false;
    // 演出のクリップ（変わり目の演出が毎フレーム頼む。c-271cba）：流すクリップ（空は頼んでいない）、その時刻（0〜1）、
    // 外す時に待機の姿へ混ぜる長さ (秒)。いま重ねているか。
    std::string cinematicClip;
    float cinematicNormalized = 0.0f;
    float cinematicBlendOut = 0.35f;
    bool cinematicLayered = false;
    // 見えている腰の骨（世界。描画のフレーム。1 フレーム前に描いた姿）と、それを読めたか（演出の間、胴の煙の域を
    // 体に付いて行かせる）。
    HIKARI::GameProjectFloat3 hipsWorld{};
    bool hipsSeen = false;
    // 見えている胸の骨（Spine2）の所（世界。描画のフレーム。1 フレーム前に描いた姿。hipsSeen の時だけ意味がある）。
    HIKARI::GameProjectFloat3 chestWorld{};
    // 見えている頭の骨の所（同じ。戦いの間、胴の煙の域を倒す向きを 腰 → 頭 で決める）。
    HIKARI::GameProjectFloat3 headWorld{};
    // 胴の煙の域を倒す向き（世界。腰 → 胸をなめらかに追った向き。長さ 1）と、倒す重み（0 倒さない 〜 1。変わり目の
    // 演出で体を据えている間は 1、戦いの間は smokeTiltCombat）。
    HIKARI::GameProjectFloat3 smokeSpine{0.0f, 1.0f, 0.0f};
    float smokeTiltWeight = 0.0f;
    // 胴の煙の域の軸（世界。腰をなめらかに追った所。細かい揺れは追わない）と、それを持っているか。
    HIKARI::GameProjectFloat3 smokePivot{};
    bool smokePivotValid = false;
    // 戦いの間、域の軸と向きを体（根）の動きと一緒に運ぶための、前のフレームの体の置き場所と、それを持っているか。
    HIKARI::GameProjectFloat3 smokeBodyPosition{};
    HIKARI::GameProjectQuaternion smokeBodyRotation{0.0f, 0.0f, 0.0f, 1.0f};
    bool smokeBodyValid = false;
};

namespace BossAnimBody {

// 描画のフレームごと：流すクリップを決め、骨格の体をボスの足元 body（体の中心の変換）と向きに置いて流す。alpha は
// 固定 tick の間の補間の割合（技のクリップの時刻を tick の間でも滑らかにする）。
// 骨格の体が体を演じる時（流すクリップがある）は true。
// look は同じ物体の体の見た目の部品（見た目の状態を持つ）。
bool Update(HIKARI::WorldView& world, const Boss& boss, BossBody& look,
            const HIKARI::GameProjectTransform& body, float alpha, float seconds);
// 骨格の体を隠して止める（ボスが消える・Play が止まる）。
void Stop(HIKARI::WorldView& world, const Boss& boss, BossBody& look);
// いま骨格の体が体を演じているか（体の見た目の部品が無ければ false）。
bool Showing(const BossBody* look);
// 規則の tick ごと（技の規則より前）：表のクリップの技の刀の骨・左手の骨と、待機のクリップの始めの刀の骨を、まだ
// 読んでいなければ読んでおく。
void PrepareSword(HIKARI::WorldView& world, Boss& boss);
// move がクリップの刀で当たる技か（クリップがあり、その刀の骨を読めた）。当たりの窓は区間の表どおりになる。
bool ClipMove(const Boss& boss, const BossMove& move);
// 刀の骨の動きの指紋（c-884095）：位置を mm、回転を 1/10000 に丸めてから混ぜる（パソコンの小数の差で変わらない）。
// 狙いのずれの表（verify.py aim）にも、計測の時に同じ値を書く。
uint32_t SwordFingerprint(const BossClipSwordTrack& track);
// clip の刀の指紋（その刀を読めていなければ 0）。
uint32_t SwordFingerprint(const Boss& boss, const std::string& clip);
// クリップの刀を読んだ後に 1 度（表を読み直したらまた）：狙いのずれの表の指紋が今の刀と違う斬りを警告する。
void CheckAimFingerprints(Boss& boss);
// いまの技がクリップの刀で当たる技なら、規則の時計のいまの時刻のクリップの刀を、規則の剣の構え（足元が原点・体の向きの
// 局所）にして true（BossPose::Target が使う。当たり・斬撃の面・刀の煙がクリップの刀に付いて行く）。
bool ClipSwordPose(const Boss& boss, BossSwordPose& out);
// 左手の骨の所（世界。規則の tick）。いまの技がクリップの刀で当たる技でなければ false（左手の技の弾を出す所）。
bool LeftHandPoint(const Boss& boss, HIKARI::GameProjectFloat3& out);
// 両手で握っている度合い（0〜1。規則の tick のいまの時刻。クリップの刀で当たる技の間だけ。c-40f6c1）。
float TwoHand(const Boss& boss);
// クリップの刀で当たる技でない間（待機・移動・気絶・演出）の規則の剣の構え：待機のクリップの始めの刀の骨を、規則の
// 剣の構え（足元が原点・体の向きの局所）にして true（BossPose::Target が使う。c-164cab：描画のフレームの骨は読まない。
// 同じ入力なら毎回同じ構えになる）。待機のクリップの刀の骨を読めていなければ false。
bool IdleSwordPose(const Boss& boss, BossSwordPose& out);
// 描画のフレーム（Update の後）：骨格の体が出ている間、体に付く効果の置き場所（刀・両手・袖口・肩・胴・兜・草摺）を、
// 見えている骨格の骨にする（c-bc5908）。クリップの刀で当たる技の間は、刀を
// 規則の時計の時刻のクリップの刀にする（alpha は tick の間の補間）。剣先の速さ（tipSpeed）も求める。
void ApplyPose(HIKARI::WorldView& world, const Boss& boss, BossBody& look, float alpha, float seconds,
               BossBodyPose& pose);

} // namespace BossAnimBody

} // namespace Project_TD2_2
