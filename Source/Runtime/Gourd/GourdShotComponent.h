#pragma once

#include <array>

#include "HIKARI/Extend/World.h"
#include "HIKARI/Project/VfxTypes.h"
#include "GourdFeedback.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kGourdShotType = "TD2_2.GourdShot";
inline constexpr std::string_view kGourdShotSystem = "Project_TD2_2.GourdShotSystem";
inline constexpr size_t kGourdProjectileCapacity = 8;
inline constexpr size_t kGourdResidueCapacity = 8;
inline constexpr size_t kGourdBurstCapacity = 8;
inline constexpr size_t kGourdFogDepositCapacity = 12;
inline constexpr size_t kGourdFireZoneCapacity = 12;
// 同時に残す一度きりの炎の域の枠。流体域は 1 フレームに 6 個まで（均衡画質）で、場の霧が 1 つ使う。
// 砲撃の域（砲口・命中・弾）は合わせて kGourdShotDomainBudget 個までにし、新しい域を出す前に古いものから止める。
// 止めた域も、そのフレームの間は数に入るので、上限より 1 つ少なくしておく。
inline constexpr size_t kGourdMuzzleLiveCapacity = 1;
inline constexpr size_t kGourdImpactLiveCapacity = 2;
inline constexpr size_t kGourdShotDomainBudget = 4;

enum class GourdShotPhase { Idle, Windup, Recovery };

struct GourdProjectile {
    bool active = false;
    bool finishTrailPending = false;
    uint64_t id = 0;
    uint64_t spawnedTick = 0;
    HIKARI::GameProjectFloat3 origin{};
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 direction{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectObjectHandle ignoredPlayer{};
    float distance = 0.0f;
    float speed = 0.0f;
    float radius = 0.0f;
    float densityScale = 1.0f;
    // 火弾（ヒョウタンが火弾の状態の間に撃った弾）。発射の瞬間に決め、飛んでいる間に状態が切れても火弾のまま。
    // 威力に GourdShot.fireShotDamageScale を掛ける。火弾の見た目（c-5d738d）はこの印を読む。
    bool fire = false;
    // 発射した高さ（地面から）。負は地面が見つからなかった。足元の霧を割る位置に使う。
    float groundClearance = -1.0f;
    HIKARI::GameProjectVfxPlaybackHandle trail{};
    // 炎の域。当たった後も閃きと冷却の数 F だけ残し、枠を再利用する時は古い域を先に止める。
    HIKARI::GameProjectVfxPlaybackHandle shell{};
    HIKARI::GameProjectVfxPlaybackHandle staleShell{};
    int flashFramesLeft = 0;
    int fadeFramesLeft = 0;
};

struct GourdShotResidue {
    bool active = false;
    bool burstPending = false;
    float burstScale = 0.0f;
    int framesLeft = 0;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectVfxPlaybackHandle smoke{};
};

// 一度だけ鳴らすエフェクトの種類。砲口と、火弾の状態の始まりの合図は砲口の枠を、命中は命中の枠を使う
// （合図のために流体域の枠を増やさない）。
enum class GourdBurstKind { Muzzle, Impact, FireStart };

// 一度だけ鳴らすエフェクトの予約。規則（固定 tick）が積み、表現（描画フレーム）が再生して消す。
struct GourdShotBurst {
    bool pending = false;
    GourdBurstKind kind = GourdBurstKind::Impact;
    // 火弾の発射・命中（火弾用の資産と倍率で出す）。
    bool fire = false;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 direction{0.0f, 1.0f, 0.0f};
    // 資産ごとの倍率（muzzleScale など）に掛ける割合（射程端の命中は小さい）。
    float scale = 1.0f;
};

// 撃った煙を場の霧へ戻す予約。近くに炎の域がある間は待ち、消えてから地面へ落とす。
struct GourdFogDeposit {
    bool active = false;
    HIKARI::GameProjectFloat3 ground{};
    // 落とし始めの高さ（地面から）。終点の煙は爆ぜた高さから地面へ降ろす。
    float dropHeight = 0.0f;
    float smokeLeft = 0.0f;
    float smokePerFrame = 0.0f;
    // この固定 tick の間に戻す速さ（煙／秒）。0 は待機中。描画側が読む。
    float rate = 0.0f;
    int delayFrames = 0;
    int age = 0;
};

// 炎の域（砲口・命中）が残っている場所。戻す煙はこの近くを避ける。
struct GourdFireZone {
    int framesLeft = 0;
    HIKARI::GameProjectFloat3 position{};
};

struct GourdShot : HIKARI::Component {
    HIKARI_COMPONENT(GourdShot)

    FIELD(bool, enabled, true, "有効", HIKARI::Group("砲撃の時間と消費"))
    // 撃ってすぐ動けるように、構えと後隙は短く、構えと後隙の後半は歩ける（c-a0a892。元の値は調整ガイド）。
    FIELD(int, windupFrames, 6, "構えの前隙 (F)", HIKARI::Range(0, 7200))
    // 長押し連射の最初の間隔。n 発目の後の間隔は
    // max(rapidMinFrames, round(repeatFrames / min(rapidMax, 1 + rapidAccel × (n − 1))))。
    FIELD(int, repeatFrames, 24, "長押し連射の最初の間隔 (F)", HIKARI::Range(1, 7200))
    FIELD(float, rapidAccel, 0.5f, "1 発ごとに連射が速くなる割合 (+倍)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, rapidMax, 3.0f, "連射の速さの上限 (倍)", HIKARI::Range(1.0f, 20.0f))
    FIELD(int, rapidMinFrames, 4, "連射の間隔の下限 (F)", HIKARI::Range(1, 7200),
          HIKARI::Tip("既定は 24・16・12・10・8・8… F。攻撃を離すと次の長押しは最初の間隔に戻ります。"))
    FIELD(int, recoveryFrames, 20, "最後の発射からの後隙 (F)", HIKARI::Range(0, 7200))
    // 最後の発射からこの F 以降は、ローリングで後隙を打ち切れる。それより前に押したローリングは、この F まで待つ。
    FIELD(int, rollCancelFrames, 6, "後隙をローリングで打ち切れる最初の F（発射から）", HIKARI::Range(0, 7200))
    // 最後の発射からこの F 以降は、後隙の間も shotMoveScale 倍で歩ける。構えの間も同じ倍率で歩ける。
    FIELD(int, moveCancelFrames, 12, "後隙の間に歩き出せる最初の F（発射から）", HIKARI::Range(0, 7200))
    // 歩きを 5.2 → 3.8 m/秒に下げた時に、撃ちながらの速さを保つよう 0.6 → 0.75 にした（c-ff3e98）。
    FIELD(float, shotMoveScale, 0.75f, "構えと後隙の後半の歩行倍率", HIKARI::Range(0.0f, 1.0f))
    // 最後の発射からこの F 以降は、攻撃を離して吸い込みを押すと後隙を取り消して吸い込みの構えへ移れる。
    FIELD(int, absorbCancelFrames, 6, "後隙を吸い込みで取り消せる最初の F", HIKARI::Range(0, 7200),
          HIKARI::Tip("構えの間と、打ち切れる F より前の後隙に押したローリングは消えずに、打ち切れる F に出ます。"
                      "押し続けている間は、後隙が明けても次の発射まで連射が続きます（ローリングでは打ち切れます）。"))
    FIELD(int, inputBufferFrames, 12, "先行入力 (F)", HIKARI::Range(1, 60))
    // 精力満タンで連射 4 発（c-ff3e98。元は 20 で 5 発、ちょうど気絶値が埋まっていた）。
    FIELD(float, staminaCost, 25.0f, "一発のスタミナ消費（0 は制限なし）", HIKARI::Range(0.0f, 100000.0f))
    FIELD(float, smokeCost, 20.0f, "一発の煙消費（0 は制限なし）", HIKARI::Range(0.0f, 100000.0f))
    FIELD(float, recoilDistance, 0.7f, "プレイヤー反動距離 (m)", HIKARI::Range(0.0f, 100.0f))
    FIELD(int, recoilFrames, 8, "プレイヤー反動時間 (F)", HIKARI::Range(1, 600))
    // 反動の進み方。2 で初め速くあとゆっくり、1 で等速。
    FIELD(float, recoilEasePower, 2.0f, "反動の出だしの速さ（1 は等速、2 は初め速くあとゆっくり）",
          HIKARI::Range(1.0f, 5.0f))
    // 構えの間と長押しの間に、撃つ向きへ振り向く速さ（度/秒。573 ≒ 10 rad/s）。
    FIELD(float, aimTurnSpeed, 573.0f, "構えの間に撃つ向きへ振り向く速さ (度/s)", HIKARI::Range(0.0f, 5000.0f))
    // ロックしていない時はカメラの正面へ撃つ。下向きはこの値（方向の y 成分）までにして、足元へ撃ち込まない。
    FIELD(float, aimMinVertical, -0.05f, "ロックなしで撃つ向きの下向きの限界（方向の y）", HIKARI::Range(-1.0f, 1.0f))
    // ロックしていない時の照準の補助。カメラの正面（水平）から左右この角度・この距離以内のロック候補を狙う。
    // 0 度で無効。
    FIELD(float, aimAssistDegrees, 35.0f, "ロックなしの照準補助：カメラ正面から左右の角度 (度、0 は無効)",
          HIKARI::Range(0.0f, 90.0f))
    FIELD(
        float, aimAssistRange, 25.0f, "ロックなしの照準補助：届く距離 (m)", HIKARI::Range(0.0f, 200.0f),
        HIKARI::Tip(
            "ロックオン中は対象へ撃ちます。ロックしていない時は、照準補助の範囲にロックできる相手がいればいちばん正面に近い相手へ、いなければカメラの正面へ撃ちます。"))
    FIELD(float, gourdRecoilDistance, 0.48f, "ヒョウタン反動距離 (m)", HIKARI::Range(0.0f, 10.0f))
    FIELD(int, gourdRecoilFrames, 8, "ヒョウタン反動時間 (F)", HIKARI::Range(1, 600))
    FIELD(float, gourdWindupPull, 0.14f, "構え中にヒョウタンを引く距離 (m、0 は無効)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, damage, 40.0f, "基礎威力", HIKARI::Range(0.0f, 100000.0f), HIKARI::Group("弾の数値"))
    // 火弾の威力の倍率（ヒョウタンの火弾の状態の間に撃った弾。c-ad360d）。
    FIELD(float, fireShotDamageScale, 1.5f, "火弾の威力の倍率（火を吸った後、ヒョウタンが火弾の間に撃った弾）",
          HIKARI::Range(0.0f, 100.0f))
    FIELD(float, initialSpeed, 60.0f, "初速 (m/s)", HIKARI::Range(0.1f, 1000.0f))
    FIELD(float, range, 15.0f, "霧散する距離 (m)", HIKARI::Range(0.1f, 1000.0f))
    FIELD(float, falloffStart, 10.0f, "威力・大きさ・濃さの減衰開始 (m)", HIKARI::Range(0.0f, 1000.0f))
    FIELD(float, finalDamageRatio, 0.2f, "射程端の威力倍率", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, finalSpeedRatio, 0.25f, "射程端の弾速倍率", HIKARI::Range(0.01f, 1.0f))
    FIELD(float, projectileRadius, 0.18f, "弾の当たり半径 (m)", HIKARI::Range(0.01f, 10.0f))
    FIELD(float, finalRadiusScale, 3.0f, "射程端の大きさ倍率", HIKARI::Range(1.0f, 10.0f))
    FIELD(float, finalDensityRatio, 0.15f, "射程端の煙の濃さ倍率", HIKARI::Range(0.0f, 1.0f))
    FIELD(int, pelletsPerShot, 1, "一発に含む弾の数", HIKARI::Range(1, 8))
    FIELD(float, spreadDegrees, 0.0f, "水平の散布角 (度)", HIKARI::Range(0.0f, 180.0f),
          HIKARI::Tip("既定は単発弾・散布 0。配列は同時 8 弾まで。空き不足なら消費せず発射を待ちます。"))
    FIELD(HIKARI::VfxAsset, shellEffect, (HIKARI::VfxAsset{"80ac20536e6b4989b654e6ba7b223c24"}),
          "飛んでいる火の弾（燃える流体の域）", HIKARI::Group("炎の弾・砲口・命中のエフェクト"))
    FIELD(float, shellScale, 1.0f, "火の弾の源の大きさ倍率（0 は無効）", HIKARI::Range(0.0f, 8.0f))
    FIELD(int, shellFlashFrames, 3, "命中の瞬間に弾の域が熱い閃きを弾く時間 (F)", HIKARI::Range(0, 30))
    FIELD(int, shellFadeFrames, 8, "命中・霧散の後に弾の域を消すまでの時間 (F)", HIKARI::Range(0, 120))
    FIELD(HIKARI::VfxAsset, muzzleEffect, (HIKARI::VfxAsset{"a464fd20f18f7a9f5426b1050bb22283"}),
          "砲口の発射エフェクト（火の輪）")
    FIELD(float, muzzleScale, 1.0f, "砲口のエフェクトの源の大きさ倍率（0 は無効）", HIKARI::Range(0.0f, 8.0f))
    FIELD(HIKARI::VfxAsset, impactEffect, (HIKARI::VfxAsset{"45c172ae00b625b1e245690a8a313e3d"}),
          "命中の爆発エフェクト")
    FIELD(float, impactScale, 1.0f, "命中の爆発の源の大きさ倍率（0 は無効）", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, impactFarScale, 0.6f, "射程端で命中した時の爆発の倍率", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, impactStandoff, 0.6f, "爆発を出す位置を命中点から撃った側へ戻す距離 (m)", HIKARI::Range(0.0f, 3.0f),
          HIKARI::Tip("流体の域は同時に 6 個までしか描けません。連射中は、"
                      "砲口の炎 1・命中の爆発 2・弾 2 を超える分を古いものから止めます（場の霧が消えないようにす"
                      "るため）。弾と砲口の資産は +X が前、爆発は世界の向き。倍率は源（噴き出し口）だけに効き、"
                      "流体の域の大きさは資産の値です。炎の色・明るさ・消える速さ・画面の波紋は各資産で調整します。"
                      "爆発を敵の体に重ねて出すと隠れて小さく暗く見えるので、少し手前で出します。霧散（射程切れ）"
                      "では爆発を出しません。"))
    // 火弾（GourdProjectile.fire）の見た目（c-5d738d）。ふだんの弾と同じ枠・同じ入力の、別の資産で出す
    // （流体域の数は増やさない）。資産を空にすると、その部分はふだんの弾の資産と倍率で出る。
    FIELD(HIKARI::VfxAsset, fireShellEffect, (HIKARI::VfxAsset{"7e5d6bba578986bc1203bc0bac526420"}),
          "火弾：飛んでいる弾（空ならふだんの弾の資産）", HIKARI::Group("火弾のエフェクトと手応え"))
    FIELD(float, fireShellScale, 1.0f, "火弾：弾の源の大きさ倍率（0 は無効）", HIKARI::Range(0.0f, 8.0f))
    FIELD(HIKARI::VfxAsset, fireMuzzleEffect, (HIKARI::VfxAsset{"342315cc01503eae1bc77ccdd5b16e50"}),
          "火弾：砲口の発射エフェクト（空ならふだんの資産）")
    FIELD(float, fireMuzzleScale, 1.0f, "火弾：砲口のエフェクトの源の大きさ倍率（0 は無効）", HIKARI::Range(0.0f, 8.0f))
    FIELD(HIKARI::VfxAsset, fireImpactEffect, (HIKARI::VfxAsset{"e3d2200c3c34e9fea2dfc0d23118a048"}),
          "火弾：命中の爆発エフェクト（空ならふだんの資産）")
    FIELD(float, fireImpactScale, 1.0f, "火弾：命中の爆発の源の大きさ倍率（0 は無効）", HIKARI::Range(0.0f, 8.0f))
    // 火弾の状態が始まった瞬間に、ヒョウタンの口から短く炎を噴く合図（1 回だけ。延長では出さない）。
    FIELD(HIKARI::VfxAsset, fireStartEffect, (HIKARI::VfxAsset{"58ea134da39dbbe24b7379ba0159bf66"}),
          "火弾の状態が始まった合図（ヒョウタンの口から短く噴く炎。空なら出さない）")
    FIELD(float, fireStartScale, 1.0f, "…合図の源の大きさ倍率（0 は無効）", HIKARI::Range(0.0f, 8.0f))
    // 火弾の発射でヒョウタンが跳ねる距離の倍率（プレイヤーの反動は変えない）。
    FIELD(float, fireGourdRecoilScale, 1.4f, "火弾の発射：ヒョウタンが跳ねる距離の倍率", HIKARI::Range(0.0f, 10.0f))
    // 火弾の命中の手応え：カメラの押しと視野角の広がりに掛ける倍率（揺れ幅には掛けない。連射で震え続けない）と、
    // 止めの F 数（相手の Damageable.hitStopFrames より長い時だけ効く）。
    FIELD(float, fireHitCameraScale, 1.5f, "火弾の命中：カメラの押しと視野角の広がりの倍率", HIKARI::Range(0.0f, 10.0f))
    FIELD(int, fireHitStopFrames, 3, "火弾の命中の止め (F。相手の止めより長い時だけ効く)", HIKARI::Range(0, 60),
          HIKARI::Tip("火弾（火を吸った後の弾）は、ふだんの弾と同じ枠で、"
                      "火弾用の資産に替えて出します（流体の域の数は増えません）。"
                      "色・明るさ・太さ・消える速さは各資産で調整します。始まりの合図は、"
                      "火弾の状態が始まった時に 1 回だけ出ます（吸い続けて残りを延ばした時は出ません）。"
                      "火弾の発射のカメラ・色ずれ・振動の倍率は「発射の手応え」にあります。命中の倍率は、"
                      "相手のDamageable の命中のカメラの値に掛けます（揺れ幅には掛けません）。"))
    FIELD(HIKARI::VfxAsset, trailEffect, {}, "弾道の流体注入エフェクト", HIKARI::Group("煙の演出"))
    FIELD(float, muzzleRadius, 0.65f, "砲口の押し広げ半径 (m)", HIKARI::Range(0.01f, 10.0f))
    FIELD(float, muzzleStrength, 7.0f, "砲口の押し広げ速度 (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, muzzleSmokeRate, 0.0f, "砲口の煙注入率 (/s)", HIKARI::Range(0.0f, 10000.0f))
    FIELD(float, impactRadius, 2.4f, "命中・霧散で足元の霧を押し広げる半径 (m)", HIKARI::Range(0.01f, 100.0f))
    FIELD(float, impactStrength, 16.0f, "命中・霧散で足元の霧を押し広げる速度 (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, impactGroundReach, 5.0f, "足元の霧に届く高さ (m。これより高い所で爆ぜると霧は動かない)",
          HIKARI::Range(0.1f, 100.0f))
    FIELD(float, impactGroundOffset, 0.3f, "霧を押す中心の地面からの高さ (m)", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, shellWakeStrength, 22.0f, "飛んでいる弾が足元の霧を左右へ割る加速度 (m/s2、0 は無効)",
          HIKARI::Range(0.0f, 200.0f))
    FIELD(float, shellWakeRadius, 0.8f, "弾が霧を割る力の半径 (m)", HIKARI::Range(0.05f, 10.0f))
    FIELD(
        float, shellWakeSpread, 0.5f, "弾道の左右へ力を置く幅 (m)", HIKARI::Range(0.0f, 5.0f),
        HIKARI::Tip(
            "霧を押す力は、重なっている流体の域すべてに効きます。爆発の炎の域を吹き飛ばさないように、爆ぜた点の真下の地面で押します（高い所ほど弱い）。"))
    FIELD(float, residueRadius, 0.75f, "残り煙の注入半径 (m)", HIKARI::Range(0.01f, 100.0f))
    FIELD(float, residueSmokeRate, 0.0f, "残り煙の注入率 (/s)", HIKARI::Range(0.0f, 10000.0f))
    FIELD(int, residueFrames, 24, "残り煙を補い続ける時間 (F)", HIKARI::Range(0, 7200))
    FIELD(HIKARI::Color, smokeColor, (HIKARI::Color{0.86f, 0.88f, 0.92f, 1.0f}), "尾煙・残り煙の色")
    FIELD(float, trailSmokeScale, 0.0f, "尾煙の注入量倍率", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, fallbackTrailRate, 6.0f, "注入エフェクト未設定時の尾煙率 (/s)", HIKARI::Range(0.0f, 10000.0f),
          HIKARI::Tip("尾煙は input.amount を濃さ倍率に使います。VFX 注入源は全砲撃合計 8 個、"
                      "Fluid 提出は合計 24 個／描画フレームまで。砲口の煙・残り煙・尾煙は既定で 0（切）"
                      "です。場の霧へ入れる煙は炎の域にも入り、炎を暗い煙で包んでしまうので、"
                      "煙は炎の域が出す煤に任せています。"))
    // 1 発の戻りは 1 発の消費（SmokeArena は 15）より少なくする：多いと、場の霧が無くても足元へ撃って吸い直すだけで
    // 煙が増える（2026-11-17 に 20 → 8）。通り道へは戻さず、当たった所だけに湧かせる（0.3 → 0）。
    FIELD(float, fogReturnSmoke, 8.0f, "1 発が場の霧へ戻す煙の量（0 は戻さない）", HIKARI::Range(0.0f, 1000.0f),
          HIKARI::Group("撃った煙を場の霧へ戻す"))
    FIELD(float, fogReturnTrailShare, 0.0f, "そのうち通り道に落とす割合（残りは終点）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, fogReturnTrailSpacing, 2.5f, "通り道に落とす間隔 (m)", HIKARI::Range(0.5f, 20.0f))
    FIELD(float, fogReturnRadius, 1.4f, "落とす煙の半径 (m)", HIKARI::Range(0.2f, 10.0f))
    FIELD(float, fogReturnHeight, 0.35f, "落とす煙の中心の地面からの高さ (m)", HIKARI::Range(0.0f, 5.0f))
    FIELD(int, fogReturnFrames, 40, "1 か所の煙を出し切る時間 (F)", HIKARI::Range(1, 600))
    FIELD(int, fogReturnDelayFrames, 6, "弾が終わってから落とし始めるまで (F)", HIKARI::Range(0, 600))
    FIELD(int, fogReturnFallFrames, 14, "終点の煙が爆ぜた高さから地面へ降りる時間 (F、0 は最初から地面)",
          HIKARI::Range(0, 600))
    FIELD(float, fogReturnSink, 1.5f, "落とす煙を下へ押す速さ (m/s)", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, fogReturnWander, 0.5f, "落とす中心を回す半径 (m、0 は丸い塊のまま)", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, fogReturnSwirl, 1.2f, "落とす煙を横へ流す速さ (m/s、渦を巻いて広がる)", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, fogReturnClearance, 3.4f, "炎の域からこの距離 (m) 以内には落とさず、炎が消えるまで待つ",
          HIKARI::Range(0.0f, 20.0f))
    FIELD(int, impactFireFrames, 48, "命中の爆発の域が残る時間 (F、資産の長さに合わせる)", HIKARI::Range(0, 600))
    FIELD(int, muzzleFireFrames, 18, "砲口の炎の域が残る時間 (F、資産の長さに合わせる)", HIKARI::Range(0, 600),
          HIKARI::Tip("吸って撃った煙は、弾の通り道と終点（命中・射程切れ）の地面へ冷たい煙として戻り、また吸えます。"
                      "戻す量は一発の煙消費とは別の値です（消費を 0 にして試し撃ちしても戻ります）。"
                      "戻す量は一発の消費より少なくしておきます（多いと、"
                      "場の霧が無くても足元へ撃って吸い直すだけで煙が増えます）。"
                      "場の霧へ入れる煙は重なっている炎の域にも入るので、炎が消えるまで待ってから落とします。"
                      "同じ場所へ撃ち続けると、撃ち終えた後にまとめて湧きます。"
                      "地面に届かない高さで終わった弾の煙は戻りません。"))
    FIELD(int, traceEveryFrames, 0, "弾道ログ間隔（0 はログ無効）", HIKARI::Range(0, 7200), HIKARI::Group("検証"))

    GourdFeedback feedback{};
    GourdShotPhase phase = GourdShotPhase::Idle;
    bool attackHeld = false;
    bool attackPressed = false;
    int attackBuffer = 0;
    uint64_t processedTick = 0;
    uint64_t beginTick = 0;
    uint64_t nextFireTick = 0;
    uint64_t recoveryEndTick = 0;
    uint64_t lastShotTick = 0;
    uint64_t shotsFired = 0;
    // 最後に撃った弾が火弾だったか（発射の手応えを強めるかを、手応えの側が読む）。
    bool lastShotFire = false;
    // 押し続けている間の何発目か。離すと 0 に戻り、次の長押しは最初の間隔から。
    int chainShots = 0;
    bool chainHeld = false;
    uint64_t dryFires = 0;
    uint64_t nextProjectileId = 1;
    int blockedReason = 0;
    bool muzzlePending = false;
    HIKARI::GameProjectFloat3 muzzlePosition{};
    HIKARI::GameProjectFloat3 muzzleDirection{0.0f, 0.0f, 1.0f};
    // 構えの間と押し続けている間の狙い（ヒョウタンの位置と撃つ向き）。ボスが「撃とうとしている」を読む（c-59bd08）。
    HIKARI::GameProjectFloat3 aimOrigin{};
    HIKARI::GameProjectFloat3 aimDirection{0.0f, 0.0f, 1.0f};
    bool vfxAccepted = true;
    bool fluidAccepted = true;
    // 他の表現（ボスの斬撃）が使う流体域の数。使う側が毎フレーム入れ、砲撃の枠をその分だけ減らす。
    size_t reservedDomains = 0;
    uint32_t fluidSubmissions = 0;
    uint32_t droppedVisuals = 0;
    uint32_t residueCursor = 0;
    uint32_t burstCursor = 0;
    uint64_t burstsPlayed = 0;
    uint32_t fireZoneCursor = 0;
    float fogReturned = 0.0f;
    float fogReturnQueued = 0.0f;
    std::array<GourdProjectile, kGourdProjectileCapacity> projectiles{};
    std::array<GourdShotResidue, kGourdResidueCapacity> residues{};
    std::array<GourdShotBurst, kGourdBurstCapacity> bursts{};
    std::array<GourdFogDeposit, kGourdFogDepositCapacity> fogDeposits{};
    std::array<HIKARI::GameProjectVfxPlaybackHandle, kGourdMuzzleLiveCapacity> muzzleLive{};
    std::array<HIKARI::GameProjectVfxPlaybackHandle, kGourdImpactLiveCapacity> impactLive{};
    uint32_t muzzleLiveCursor = 0;
    uint32_t impactLiveCursor = 0;
    std::array<GourdFireZone, kGourdFireZoneCapacity> fireZones{};
};

} // namespace Project_TD2_2
