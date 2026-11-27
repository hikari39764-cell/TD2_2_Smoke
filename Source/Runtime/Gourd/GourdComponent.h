#pragma once

#include <array>

#include "HIKARI/Extend/World.h"
#include "HIKARI/Project/VfxTypes.h"
#include "../Player/Feedback.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kGourdType = "TD2_2.Gourd";
inline constexpr std::string_view kGourdSystem = "Project_TD2_2.GourdSystem";
// 物体変数の ID。ProjectSettings/runtime_bindings.json の宣言と同じ名前・型（Float）にする。
inline constexpr std::string_view kGourdSmokeVariable = "Gourd.Smoke";
inline constexpr std::string_view kGourdCapacityVariable = "Gourd.Capacity";
// 火弾の残り秒数（火弾でなければ 0）。HUD が読む。
inline constexpr std::string_view kGourdFireShotVariable = "Gourd.FireShot";
// 満杯で吸えないまま吸い込みを続けている秒数（吸えている・吸っていない間は 0）。HUD が「入らない」の合図に読む。
inline constexpr std::string_view kGourdBlockedVariable = "Gourd.Blocked";

// 吸い込みの段階：構え（吸う力はまだ出ない）→ 吸う → 後隙。どの段階でもゆっくり歩けて、向きも変えられる。
// ローリングはどの段階でも出せて、吸い込みを後隙なしで打ち切る。
enum class GourdAbsorbPhase { Idle, Windup, Absorbing, Recovery };

struct Gourd : HIKARI::Component {
    HIKARI_COMPONENT(Gourd)

    FIELD(bool, enabled, true, "有効", HIKARI::Group("追従と外形"))
    FIELD(HIKARI::Entity, player, {}, "追従するプレイヤー")
    FIELD(HIKARI::Entity, camera, {}, "照準用カメラ（制御視点が無い時）")
    FIELD(HIKARI::Vec3, shoulderOffset, (HIKARI::Vec3{0.65f, 0.45f, -0.1f}), "肩からの位置（右・上・前 m）")
    FIELD(
        float, followFrequency, 14.0f, "ばね追従の速さ (/s)", HIKARI::Range(0.1f, 100.0f),
        HIKARI::Tip("砲撃の反動から戻る速さも、このばね追従の速さを使います。反動の距離と F 数は砲撃側で調整します。"))
    FIELD(float, teleportDistance, 10.0f, "瞬間移動とみなす距離 (m)", HIKARI::Range(1.0f, 1000.0f))
    FIELD(float, height, 0.5f, "高さ (m)", HIKARI::Range(0.02f, 10.0f))
    FIELD(float, radius, 0.18f, "半径 (m)", HIKARI::Range(0.01f, 5.0f),
          HIKARI::Tip("カプセルの +Y 側が口。基準メッシュは高さ 0.5 m・半径 0.18 m、物理の当たりは付けません。"))
    FIELD(float, absorbRadius, 1.0f, "口元で煙を回収する半径 (m)", HIKARI::Range(0.01f, 10.0f),
          HIKARI::Group("霧の吸い込み"))
    FIELD(float, absorbStrength, 28.0f, "口元の吸入速度 (m/s。周りの霧を口へ引き寄せる主役)",
          HIKARI::Range(0.0f, 100.0f))
    FIELD(float, absorption, 60.0f, "口元で煙を消す率 (/s)", HIKARI::Range(0.0f, 1000.0f))
    FIELD(float, gatherRange, 7.5f, "霧を吸い寄せる前方の距離 (m)", HIKARI::Range(0.5f, 20.0f))
    FIELD(float, gatherFanDegrees, 60.0f, "霧を集める扇の片側の角度 (度)", HIKARI::Range(0.0f, 80.0f))
    FIELD(float, gatherNearRadius, 0.0f, "足元の全周から霧を寄せる半径 (m、0 は無効)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, gatherRadius, 1.8f, "霧を口へ運ぶ流れ 1 本の半径 (m)", HIKARI::Range(0.01f, 10.0f))
    FIELD(float, gatherStrength, 12.0f, "霧を口へ運ぶ加速度 (m/s2・0 は停止)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, gatherFarScale, 1.0f, "いちばん奥の流れの強さの倍率（手前は 1）", HIKARI::Range(0.0f, 2.0f))
    FIELD(int, gatherRings, 3, "霧を運ぶ流れの輪の数（多いほど奥まで途切れずに運ぶ）", HIKARI::Range(1, 4))
    FIELD(float, gatherSwirl, 0.6f, "渦の強さ（内向きに対する横向きの割合、0 はまっすぐ）", HIKARI::Range(-2.0f, 2.0f))
    FIELD(float, gatherPulse, 0.6f, "奥から手前へ進む脈の強さ（0 は一定）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, gatherPulseFrequency, 1.4f, "脈の速さ (Hz)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, gatherLift, 0.3f, "足元に集まった霧を口へ持ち上げる力の倍率", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, gatherAbsorption, 2.0f, "扇の中の霧が、口へ運ばれながら消える率 (/s、0 は消さない)",
          HIKARI::Range(0.0f, 100.0f))
    FIELD(float, gatherSmokeShare, 0.25f, "扇の中で消えた霧のうち、煙に数える割合", HIKARI::Range(0.0f, 1.0f),
          HIKARI::Tip("gatherAbsorption を上げるほど、吸った所の霧が早く薄くなって穴が見えます。"
                      "ヒョウタンが満ちる速さは gatherSmokeShare で合わせます（口元まで届いた霧は全部"
                      "数えます）。穴が戻る速さは、場の霧（ArenaGroundFog）の補充と消散で決まります。"))
    FIELD(float, absorbForwardDistance, 1.3f, "霧を集める中心の前方距離 (m)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, absorbHeightOffset, -0.45f, "霧を集める中心の高さ（プレイヤー原点から m）",
          HIKARI::Range(-10.0f, 10.0f),
          HIKARI::Tip("近くの霧は口元の吸入が引き寄せます（流れが口で終わるので行き過ぎない）。"
                      "扇の流れは 3 m より奥の霧を軽く押すだけにします。"
                      "扇の流れ・持ち上げ・足元の全周の力を強くすると、霧がプレイヤーを通り過ぎて空へ噴き上がります。"))
    FIELD(float, absorbCatchRadius, 1.1f, "口の上へ抜けた霧を回収する半径 (m、0 は無効)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, absorbCatchHeight, 0.7f, "回収する中心の、口からの高さ (m)", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, absorbCatchRate, 30.0f, "口の上で霧を消す率 (/s)", HIKARI::Range(0.0f, 1000.0f),
          HIKARI::Tip("持ち上げた霧が口を通り過ぎて空へ噴き上がらないように、口の少し上で消して煙に数えます。"
                      "霧が口の上まで噴き上がる時は、この半径か率を上げるか、持ち上げる力の倍率を下げます。"))
    // 満杯の間は吸えない（霧・火・斬撃・ナイフのどれも。c-d07c33）。入にすると、満杯でも吸える前の扱いに戻る。
    FIELD(bool, absorbWhenFull, false, "満杯でも吸える（既定は切：満杯の間は霧・火・斬撃・ナイフのどれも吸えない）",
          HIKARI::Tip("切（既定）：満杯の間は、吸い込みを構えても場の霧・場の火を消さず、"
                      "ボスの斬撃・ナイフは当たります（火弾にもなりません）。空きが少しでもあれば吸え、"
                      "入りきらない煙は捨てます。斬撃・ナイフは、口へ引き込み中の分の煙も先に数えるので、"
                      "1 本で満杯になる時は、続けて来た次の 1 本が当たります（引き込み中の分は最後まで入ります）。"
                      "入にすると、満杯でも吸えて煙だけが増えない前の扱いに戻ります。"
                      "砲撃の煙のコスト（GourdShot.smokeCost）が 0 のシーンでは、"
                      "切だとずっと満杯で何も吸えません。口元の吸入が周り全体の霧を口へ引き寄せ、"
                      "前方の扇の流れが遠くの霧を渦を巻かせて運びます。吸入速度と半径を上げると吸う速さ（煙／秒）"
                      "も上がります。既定は濃い霧の中で約 60／秒です。プレイヤーの向きの前方の扇から、"
                      "場の霧を輪から輪へ口の前まで運び、口元だけで回収します。運ぶ力だけでは煙量は増えません。"
                      "奥の輪は扇の両端まで 5 本、いちばん手前の輪は 3 本の流れです。"))
    // 吸ってすぐ次の行動へ移れるように、構えと後隙は短く、その間も歩ける（c-28900b。元の値は調整ガイド）。
    FIELD(int, absorbWindupFrames, 5, "構えの前隙 (F。吸う力はまだ出ない)", HIKARI::Range(0, 600),
          HIKARI::Group("吸い込みの操作"))
    FIELD(int, absorbMinFrames, 10, "1 回押したら少なくとも吸う長さ (F。構えの後から。0 は離した所で止める)",
          HIKARI::Range(0, 600))
    FIELD(int, absorbRecoveryFrames, 8, "吸い終わった後の後隙 (F)", HIKARI::Range(0, 600))
    FIELD(int, absorbBufferFrames, 12, "先行入力を覚えておく長さ (押した F の後の F 数。0 は無し)",
          HIKARI::Range(0, 120))
    // 歩きを 5.2 → 3.8 m/秒に下げた時に、吸いながらの速さ（約 2.6 m/秒）を保つよう 0.5 → 0.7 にした（c-ff3e98）。
    FIELD(float, absorbMoveScale, 0.7f, "構え・吸い込み・後隙の間の歩行倍率（歩く速さに掛ける。0.7 で約 2.7 m/秒）",
          HIKARI::Range(0.0f, 1.0f))
    FIELD(float, absorbTurnSpeed, 720.0f, "構え・吸い込み・後隙の間に向きを変えられる速さ (度/秒)",
          HIKARI::Range(0.0f, 3600.0f),
          HIKARI::Tip("構え・吸い込み・後隙の間は歩けて、入力した方向へ向きを変えられます（ロックオン中でも）。"
                      "吸う向きはプレイヤーの向きです。ローリングはいつでも出せて、吸い込みを後隙なしで打ち切ります。"
                      "構えと吸っている間は回復と砲撃を始められません（先行入力は残り、後隙に入った所で出ます）。"
                      "後隙は砲撃と回復でも打ち切れます。ローリング・砲撃の後隙・起き上がりなどの間に押した吸い込みは、"
                      "先行入力の長さだけ覚えておき、動けるようになった最初の F に構えます。軽く押しただけでも、"
                      "最低の長さは吸います。"))
    FIELD(float, capacity, 100.0f, "煙の容量", HIKARI::Range(0.0f, 100000.0f), HIKARI::Group("煙の残量"))
    FIELD(float, initialSmoke, 100.0f, "開始時の煙量", HIKARI::Range(0.0f, 100000.0f))
    FIELD(
        float, smokePerVolume, 4.0f, "吸った密度・体積から煙量への倍率（小さいほど満杯までに多くの霧を使う）",
        HIKARI::Range(0.0f, 100000.0f),
        HIKARI::Tip(
            "回読は 1〜3 フレーム遅れます。満杯の間は霧を消しません（「満杯でも吸える」を入れると、吸い込みは続き、超過分は捨てます）。"))
    // 火弾：場の火・火の刃を吸うと、しばらく撃つ弾が火弾になる（c-ad360d。威力は GourdShot.fireShotDamageScale）。
    // 吸い込みで消した場の火の面積が fireShotTriggerArea に届くたびと、火の刃を吸い取るたびに、残りを fireShotDuration へ戻す。
    FIELD(float, fireShotDuration, 8.0f, "火弾の長さ (秒。火を吸うたびに残りをこの長さへ戻す。0 は火弾にしない)",
          HIKARI::Range(0.0f, 600.0f), HIKARI::Group("火弾（火を吸った後）"))
    FIELD(float, fireShotTriggerArea, 0.5f, "場の火をこれだけ吸うたびに火弾の残りを戻す (m²)",
          HIKARI::Range(0.0f, 1000.0f),
          HIKARI::Tip("吸い込みで場の火を消すか、ボスの火の刃（火を残す斬撃・ナイフ）を吸い取ると、"
                      "しばらく撃つ弾が火弾になります（威力の倍率は砲撃の「火弾の威力の倍率」）。"
                      "霞の刃では火弾になりません。吸い続けている間は残りが長さへ戻り続け、長さを超えては積まれません。"
                      "満杯の間は火も火の刃も吸えないので、火弾になりません（「満杯でも吸える」が入っている時だけ、"
                      "満杯でも火を吸えて火弾になります）。残りは固定 tick で減り、"
                      "ヒットストップの間は減りません。"))
    FIELD(HIKARI::VfxAsset, absorbEffect, (HIKARI::VfxAsset{"5f8903664eb0a5691bcc566b125bc81e"}),
          "口の流体エフェクト（吸い込みの霧の筋）", HIKARI::Group("吸い込みの演出と手応え"))
    FIELD(float, absorbEffectScale, 1.0f, "口のエフェクトの源の大きさ倍率（0 は無効）", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, absorbFullIntake, 40.0f, "口の演出が最大になる吸う速さ (煙／秒)", HIKARI::Range(0.1f, 1000.0f))
    FIELD(float, absorbIdleEffect, 0.35f, "吸える霧が無い時の口の演出の強さ (0〜1)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, absorbFullEffect, 0.2f, "満杯で吸えない時の口の演出の強さ (0〜1、0 は何も出さない)",
          HIKARI::Range(0.0f, 1.0f),
          HIKARI::Tip("既定では霧を光らせません（抑えた見た目）。光らせる時は、下の温度を上げ、"
                      "場の霧（ArenaGroundFog）と口のエフェクトの域の emissionIntensity"
                      " も 0 より大きくします。"))
    FIELD(float, absorbGlowHeat, 0.0f, "吸われていく霧を温める温度（高いほど霧が明るく光る。0 は無効）",
          HIKARI::Range(0.0f, 20.0f))
    FIELD(float, absorbGlowRadius, 1.3f, "口元で煙を温める半径 (m)", HIKARI::Range(0.1f, 10.0f))
    FIELD(float, absorbGlowReach, 1.4f, "地面に沿って霧を温める場所の間隔 (m。3 か所、奥ほど広くぬるい)",
          HIKARI::Range(0.0f, 10.0f))
    FIELD(int, absorbRetireFrames, 12, "吸い込みをやめてから口の域を消すまでの時間 (F)", HIKARI::Range(0, 600))
    FIELD(float, absorbFovDegrees, 2.0f, "吸い込み中に狭める視野角 (度、0 は無効)", HIKARI::Range(0.0f, 20.0f))
    FIELD(int, absorbFovEaseFrames, 10, "視野角が寄る・戻る時間 (F)", HIKARI::Range(1, 120))
    FIELD(float, absorbShakeAmplitude, 0.004f, "吸い込み中のカメラの細かい震え (m、0 は無効)",
          HIKARI::Range(0.0f, 0.2f))
    FIELD(float, absorbShakeFrequency, 17.0f, "カメラの震えの周波数 (Hz)", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, absorbPulseScale, 0.12f, "吸い込み中にヒョウタンが膨らむ割合（0 は無効）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, absorbPulseFrequency, 5.0f, "ヒョウタンの脈動の速さ (Hz)", HIKARI::Range(0.0f, 30.0f))
    FIELD(float, absorbRumble, 0.16f, "吸い込み中の振動の強さ (0〜1、0 は無効)", HIKARI::Range(0.0f, 1.0f))
    FIELD(int, fullRumbleFrames, 8, "満杯になった時の振動時間 (F、0 は無効)", HIKARI::Range(0, 120))
    FIELD(float, fullRumble, 0.7f, "満杯になった時の振動の強さ (0〜1)", HIKARI::Range(0.0f, 1.0f))
    // 満杯で吸えない時の「入らない」合図（c-d07c33）：吸い込みを始めた時か、吸っている間に満杯になった時に 1 回、
    // ヒョウタンが口の向きと逆へ小さく跳ね、口の前の霧を押し戻す短い噴きを出す（振動は fullRumble）。
    FIELD(float, blockedKickDistance, 0.06f, "満杯で吸えない時、ヒョウタンが口と逆へ跳ねる距離 (m、0 は無効)",
          HIKARI::Range(0.0f, 1.0f))
    FIELD(int, blockedKickFrames, 3, "…跳ねる時間 (F)", HIKARI::Range(1, 60))
    // 噴きは口の域の霧の筋を blockedPuffEffect まで濃くして、その霧と口の前の場の霧を押し出す（短く・0.3 秒以内）。
    FIELD(int, blockedPuffFrames, 16, "…口の前の霧を押し戻す噴きの長さ (F、0 は無効)", HIKARI::Range(0, 120))
    FIELD(float, blockedPuffStrength, 300.0f, "…噴きの強さ (m/s²。始めの強さで、長さの間に 0 へ弱まる)",
          HIKARI::Range(0.0f, 2000.0f))
    FIELD(float, blockedPuffRadius, 1.8f, "…噴きの半径 (m)", HIKARI::Range(0.05f, 10.0f))
    FIELD(float, blockedPuffReach, 0.6f, "…噴きの中心の、口からの前方距離 (m)", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, blockedPuffEffect, 1.0f, "…噴きの間の口のエフェクトの濃さ（input.on。噴きの残りに比例して戻る）",
          HIKARI::Range(0.0f, 4.0f),
          HIKARI::Tip("エフェクト資産の +X は口の向く先。入力 input.on は吸い込みの強さ（0〜1）で、"
                      "実際に吸えている速さに合わせて上がります。霧の無い所で吸うと弱く、"
                      "満杯で吸えない間は「満杯で吸えない時の口の演出の強さ」です。使っていない間は域を消して、"
                      "同時に描ける流体の域の枠を空けます。満杯で吸えない時（吸い込みを始めた時か、"
                      "吸っている間に満杯になった時）は 1 回、ヒョウタンが跳ね、口のエフェクトの霧が濃くなって、"
                      "口の前の霧（場の霧と口のエフェクトの域）と一緒に口から押し戻され、満杯の振動が出ます。"
                      "満杯で吸えない間は、吸い込みの視野角の寄り・カメラの震え・ヒョウタンの脈・吸い込みの振動は出しま"
                      "せん。HUD の煙の条は、物体変数 Gourd.Blocked（吸えないまま吸い込んでいる秒数）"
                      "を読んで短く光ります（Flow の項目）。地面や画面に予告は出しません。"))
    // ボスの斬撃・ナイフを吸い取った瞬間の手応え（c-d330c0）。止め・手柄の振動・視野角の引き。
    FIELD(int, catchHitStopFrames, 7, "ボスの斬撃を吸い取った時の止め (F、0 は無効)", HIKARI::Range(0, 30),
          HIKARI::Group("吸い取りの手応え"))
    FIELD(float, catchHitStopScale, 0.03f, "…止め中の時間倍率", HIKARI::Range(0.0f, 1.0f))
    // 続けて吸い取った時（X 字の 2 本・ナイフの扇が順に入る）、止めを重ねない間隔。この間の吸い取りは、止め以外の
    // 手応え（エフェクト・振動・画角・膨らみ）だけ出す。
    FIELD(int, catchHitStopCooldownFrames, 20, "…続けて吸い取った時に、止めを重ねない間隔 (F)", HIKARI::Range(0, 240))
    FIELD(float, catchRumbleLow, 0.9f, "…振動：重いモーター (0〜1)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, catchRumbleHigh, 0.6f, "…振動：軽いモーター (0〜1)", HIKARI::Range(0.0f, 1.0f))
    FIELD(int, catchRumbleFrames, 12, "…振動時間 (F、0 は無効)", HIKARI::Range(0, 120))
    FIELD(float, catchFovDegrees, 3.5f, "…視野角の引き (度。正で広がり、0 は無効)", HIKARI::Range(-10.0f, 10.0f))
    FIELD(int, catchFovFrames, 14, "…視野角が戻るまで (F)", HIKARI::Range(0, 120))
    FIELD(float, catchKickDistance, 0.11f, "…カメラが口の向きと逆へ押される距離 (m、0 は無効)",
          HIKARI::Range(0.0f, 1.0f))
    FIELD(float, catchShakeAmplitude, 0.022f, "…カメラの揺れ幅 (m、0 は無効)", HIKARI::Range(0.0f, 0.2f))
    // 呑み込んだ瞬間の動きのぼかしの一瞬（カメラの押しと揺れが、画の流れになって見える。カメラの元の設定へ戻る）。
    FIELD(float, catchBlurShutter, 300.0f, "…動きのぼかしの一瞬：シャッター角 (度、0 は無効)",
          HIKARI::Range(0.0f, 360.0f))
    FIELD(float, catchBlurPixels, 40.0f, "…ぼかしの長さの上限 (1080p の画素)", HIKARI::Range(0.0f, 64.0f))
    FIELD(int, catchBlurFrames, 12, "…ぼかしが戻るまで (F)", HIKARI::Range(0, 120))
    // 吸い取った瞬間に口の位置で一度だけ出すエフェクト（内へ絞る輪 → 閃き → 外へ弾ける輪と火花 → 口元の余韻。色は
    // 入力 input.color：霞の刃・ナイフは catchMistColor、火の刃は catchFireColor）。空なら何も出さない。
    FIELD(HIKARI::VfxAsset, catchEffect, (HIKARI::VfxAsset{"b1f5a11c3053118f0c418f38f3a29ec0"}),
          "吸い取った瞬間に口で出すエフェクト（空なら出さない）")
    FIELD(HIKARI::Color, catchMistColor, (HIKARI::Color{0.62f, 0.86f, 1.0f, 1.0f}), "…霞の刃・ナイフを吸い取った時の色")
    FIELD(HIKARI::Color, catchFireColor, (HIKARI::Color{1.0f, 0.5f, 0.16f, 1.0f}), "…火の刃を吸い取った時の色")
    FIELD(float, catchEffectGlow, 2.2f, "…エフェクトの明るさ（色に掛ける倍率）", HIKARI::Range(0.0f, 16.0f))
    FIELD(float, catchSmallScale, 0.5f, "…ナイフを吸い取った時の、口のエフェクトと膨らみの大きさ（斬撃を 1 として）",
          HIKARI::Range(0.05f, 2.0f))
    // 吸い取った時、飲み込んだようにヒョウタンが一度膨らんで戻る（膨らむ割合と、戻るまでの秒）。
    FIELD(float, catchPulseScale, 0.7f, "吸い取った時にヒョウタンが膨らむ割合（0 は無効）", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, catchPulseSeconds, 0.36f, "…膨らみが戻るまで (秒)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, catchPulseBounce, 0.35f, "…戻る時の弾み（0 は弾まずに戻る。1 に近いほど、逆へ縮んでから落ち着く）",
          HIKARI::Range(0.0f, 1.0f))
    // 噛む（c-e0a9c3）：斬撃・ナイフが吸い込みに捕まった瞬間。短い止め・鋭い振動・口から捕まえた所へ走る火花の線。
    // 続けて捕まった分は、止めを重ねない（catchHitStopCooldownFrames と同じ間隔）。
    FIELD(int, biteHitStopFrames, 4, "捕まえた瞬間（噛む）の止め (F、0 は無効)", HIKARI::Range(0, 30))
    FIELD(float, biteHitStopScale, 0.04f, "…止め中の時間倍率", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, biteRumbleLow, 0.35f, "…振動：重いモーター (0〜1)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, biteRumbleHigh, 1.0f, "…振動：軽いモーター (0〜1)", HIKARI::Range(0.0f, 1.0f))
    FIELD(int, biteRumbleFrames, 5, "…振動時間 (F、0 は無効)", HIKARI::Range(0, 120))
    // カメラの打ち込み：一瞬だけ視野角を狭め（負で狭まる）、小さく揺らす。
    FIELD(float, biteFovDegrees, -1.6f, "…視野角の打ち込み (度。負で狭まり、0 は無効)", HIKARI::Range(-10.0f, 10.0f))
    FIELD(float, biteShakeAmplitude, 0.009f, "…カメラの揺れ幅 (m、0 は無効)", HIKARI::Range(0.0f, 0.2f))
    FIELD(int, biteCameraFrames, 6, "…カメラが戻るまで (F)", HIKARI::Range(0, 120))
    FIELD(HIKARI::VfxAsset, biteEffect, (HIKARI::VfxAsset{"03934a60d81b537ce65c61d1e3d5c3a7"}),
          "…口から捕まえた所へ走る線のエフェクト（+X が捕まえた所の向き。空なら出さない）")
    // 引き込む（c-e0a9c3）：捕まえた斬撃が口へ入るまで、視野角をさらに狭め、震えと振動を強めていく（入り切る直前が最大）。
    FIELD(float, pullFovDegrees, 2.5f, "引き込みの間に、さらに狭める視野角 (度、0 は無効)", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, pullShakeAmplitude, 0.01f, "…カメラの細かい震え (m、0 は無効)", HIKARI::Range(0.0f, 0.2f))
    FIELD(
        float, pullRumble, 0.6f, "…振動の強さ（入り切る直前。0〜1、0 は無効）", HIKARI::Range(0.0f, 1.0f),
        HIKARI::Tip("吸い取りは 3 拍です。噛む：斬撃・ナイフが吸い込みに捕まった瞬間に、短い止めと鋭い振動が 1 つ出て、"
                    "口から捕まえた所へ火花の線が走り、刃の光が一度膨らみます（膨らみはボスの項目 absorbBiteGlow）。"
                    "引き込む：口へ入るまで、刃の通った所に光の尾が残り（ボスの項目 absorbTrailEffect）、"
                    "視野角が狭まり、震えと振動が強まります。呑み込む：入り切った瞬間に、止め・重い振動・"
                    "カメラの押し・口のエフェクト・ヒョウタンの膨らみと弾みが出ます。"))
    // 捕まえた斬撃を飲み込む口の力（c-3341a7）：斬撃のエフェクトの域（Channel3）だけに届き、口へ寄せて消す。
    // 消した霧は煙に数えない（斬撃の煙は、ボスの項目 waveAbsorbSmoke の決まった量で入る）。
    // 口元の吸い口：半径の面から swallowStrength m/s で吸い込む（外の霧は、距離の 2 乗に反比例する速さで口へ流れる。
    // 半径 1.5 m・80 m/s なら、5 m 先の霧が約 0.2 秒で届く）。半径の中の霧は swallowAbsorption で消える。
    FIELD(float, swallowRadius, 1.5f, "斬撃を飲み込む口の吸い口の半径 (m)", HIKARI::Range(0.1f, 10.0f))
    FIELD(float, swallowStrength, 80.0f, "…吸い込む速さ (m/s。半径の面を通る速さ。外の霧は距離の 2 乗で遅くなる)",
          HIKARI::Range(0.0f, 2000.0f))
    FIELD(float, swallowAbsorption, 40.0f, "…半径の中の霧を消す率 (/s)", HIKARI::Range(0.0f, 400.0f))
    // 最後の斬撃が入りきった後も、口が飲み込み続ける長さ (F。残った霧を消し、口の見た目を保つ)。
    FIELD(int, swallowTailFrames, 24, "…最後の斬撃が入った後も飲み込み続ける長さ (F)", HIKARI::Range(0, 240),
          HIKARI::Tip("捕まえたボスの斬撃は、口へ引き込まれて入りきった瞬間に、"
                      "止め・振動・画角・膨らみ・エフェクトが出ます（引き込みの速さと動きはボスの項目 absorbPu"
                      "ll…）。飲み込む口の力は、斬撃の霧だけに届き、場の霧には触りません。消した霧は煙に数えません。"))
    FIELD(int, traceEveryFrames, 0, "状態ログ間隔（0 は無効）", HIKARI::Range(0, 7200), HIKARI::Group("検証"))

    void Update(float dt) override;

    // 累積回読値と所持量は別々に持ち、消費や容量超過を次の回読で戻さない。
    bool initialized = false;
    bool poseInitialized = false;
    bool absorbHeld = false;
    // 動けない間に押された吸い込みを覚えておく残り F（押した F を含む。0 で無し）。
    // 動けるようになった最初の F に構える。
    int absorbBuffer = 0;
    bool absorbing = false;
    GourdAbsorbPhase absorbPhase = GourdAbsorbPhase::Idle;
    int absorbPhaseFrames = 0;
    bool playerValid = false;
    bool transformAccepted = true;
    bool fluidAccepted = true;
    bool readAccepted = true;
    uint32_t fluidSubmissions = 0;
    float storedSmoke = 0.0f;
    // 描画のフレームで読み戻した霧の増え（煙の量。c-164cab）。次の固定 tick の始めに storedSmoke へ入れる（規則の量を
    // 描画のフレームで変えない）。
    double pendingSmoke = 0.0;
    double lastRemovedTotal = 0.0;
    double removedDelta = 0.0;
    uint64_t processedTick = 0;
    uint64_t renderFrameCount = 0;
    // 規則のヒョウタンの置き場所（c-164cab）：固定 tick の終わり（PostFixedUpdate）に、プレイヤーのその tick の位置・
    // 向き・吸い込みの段階・ロック対象と、Update で覚えたカメラの正面から決める。砲撃の口・ボスの斬撃を捕まえる口・
    // 満杯で跳ねる向きはこれを読む（描画のフレームの値に頼らない）。
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 aimDirection{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 mouthPosition{};
    HIKARI::GameProjectFloat3 sinkPosition{};
    bool rulePoseReady = false;
    // Update で覚えたカメラの正面（水平。プレイヤーの入力の基準）と、覚えられたか。
    HIKARI::GameProjectFloat3 inputCameraForward{0.0f, 0.0f, 1.0f};
    bool inputCameraForwardValid = false;
    // 描画のヒョウタン（LateUpdate が描画のフレームごとに、補間した体を追うばねと反動で決める。見た目と流体の注入だけが
    // 読む）。
    HIKARI::GameProjectFloat3 drawPosition{};
    HIKARI::GameProjectFloat3 drawAimDirection{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 drawMouthPosition{};
    HIKARI::GameProjectFloat3 drawSinkPosition{};
    HIKARI::GameProjectFloat3 followPosition{};
    HIKARI::GameProjectFloat3 followVelocity{};
    // 肩への追従と砲撃の反動を分離し、反動後も元の追従位置へ戻す。
    HIKARI::GameProjectFloat3 recoilOffset{};
    HIKARI::GameProjectFloat3 recoilVelocity{};
    HIKARI::GameProjectFloat3 recoilPerFrame{};
    int recoilFramesLeft = 0;
    // 撃っている間の描画のヒョウタンの向き（砲撃が LateUpdate で渡す。見た目だけ）。
    bool aimOverridePending = false;
    HIKARI::GameProjectFloat3 aimOverrideDirection{0.0f, 0.0f, 1.0f};
    // 見た目と手応えだけの状態。吸い込み・残量の規則はここを読まない。
    HIKARI::GameProjectVfxPlaybackHandle absorbVfx{};
    bool vfxAccepted = true;
    bool wasFull = false;
    // 吸い取った数の累計（ボスの規則が増やす）と、手応えを出し終えた分。差が出たら次の描画フレームで見せる。
    uint64_t catchCount = 0;
    uint64_t catchObservedCount = 0;
    // 次の止めを出せるまでの残り（実時間の秒）。
    float catchStopCooldown = 0.0f;
    // 口が斬撃を飲み込んでいる残りの tick（ボスの規則が、引き込みの間は毎 tick 入れ直す）。
    int swallowFrames = 0;
    // 膨らみを出し終えた分と、膨らみの残り秒。
    uint64_t catchPulseObserved = 0;
    float catchPulseLeft = 0.0f;
    Feedback::CameraShake catchShake{};
    Feedback::CameraShake biteShake{};
    // 最後に吸い取ったのが火の刃か（呑み込みの色）、ナイフだけか（呑み込みを小さく見せる）。
    bool catchFire = false;
    bool catchSmall = false;
    // 噛んだ数の累計（ボスの規則が増やす）と、手応えを出し終えた分、最後に捕まえた所、それが火の刃か、次の止めを
    // 出せるまでの残り（実時間の秒）。
    uint64_t biteCount = 0;
    uint64_t biteObserved = 0;
    HIKARI::GameProjectFloat3 bitePosition{};
    bool biteFire = false;
    float biteStopCooldown = 0.0f;
    // 引き込みの進み（0〜1。規則が毎 tick 入れる）と、見た目がならした値。
    float pullProgress = 0.0f;
    float pullBlend = 0.0f;
    float absorbBlend = 0.0f;
    // 実際に吸えている速さ（煙／秒、ならした値）。口の演出の強さに使う。
    float intakeRate = 0.0f;
    float absorbSeconds = 0.0f;
    float mouthIdleSeconds = 0.0f;
    // 霧を消す力ごとの、前に読んだ累計（0 は口元、1 は口の上、2 からは扇の力）と、増えた分を足し続けた合計。
    // 力ごとに差を取るので、力の数が変わっても（輪の数を実行中に変えた時など）古い累計を二重に数えない。
    std::array<double, 24> removedByForce{};
    double removedSum = 0.0;
    // 火弾の残り秒数（0 より大きい間が火弾の状態。固定 tick で減る）と、まだ fireShotTriggerArea に届いていない、
    // 吸い込みで消した場の火の面積 (m²)。
    float fireShotSeconds = 0.0f;
    float fireAreaPending = 0.0f;
    // 満杯で吸えないまま吸い込みを続けている秒数（固定 tick で進む。吸えている・吸っていない間は 0）と、
    // 「入らない」になった回数の累計（見た目が、手応えを出し終えた分と比べて 1 回ずつ見せる）。
    float blockedSeconds = 0.0f;
    uint64_t blockedCount = 0;
    uint64_t blockedObserved = 0;
    // 押し戻す噴きの残りの固定 tick。
    int blockedPuffLeft = 0;
};

} // namespace Project_TD2_2
