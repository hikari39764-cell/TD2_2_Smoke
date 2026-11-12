#include "GourdComponent.h"

namespace Project_TD2_2 {

void GourdComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("追従と外形", true)) {
        fields.Bool("enabled", "有効", enabled);
        fields.SceneObject("player", "追従するプレイヤー", playerId);
        fields.SceneObject("camera", "照準用カメラ（制御視点が無い時）", cameraId);
        fields.Float3("shoulderOffset", "肩からの位置（右・上・前 m）", shoulderOffset);
        fields.Float("followFrequency", "ばね追従の速さ (/s)", followFrequency, 0.1f, 100.0f);
        fields.Note("砲撃の反動から戻る速さも、このばね追従の速さを使います。反動の距離と F 数は砲撃側で調整します。");
        fields.Float("teleportDistance", "瞬間移動とみなす距離 (m)", teleportDistance, 1.0f, 1000.0f);
        fields.Float("height", "高さ (m)", height, 0.02f, 10.0f);
        fields.Float("radius", "半径 (m)", radius, 0.01f, 5.0f);
        fields.Note("カプセルの +Y 側が口。基準メッシュは高さ 0.5 m・半径 0.18 m、物理の当たりは付けません。");
    }
    if (fields.Section("霧の吸い込み", true)) {
        fields.Float("absorbRadius", "口元で煙を回収する半径 (m)", absorbRadius, 0.01f, 10.0f);
        fields.Float("absorbStrength", "口元の吸入速度 (m/s。周りの霧を口へ引き寄せる主役)", absorbStrength, 0.0f,
                     100.0f);
        fields.Float("absorption", "口元で煙を消す率 (/s)", absorption, 0.0f, 1000.0f);
        fields.Float("gatherRange", "霧を吸い寄せる前方の距離 (m。口の渦の円錐もこの先の足元へ向く)", gatherRange, 0.5f,
                     20.0f);
        fields.Float("gatherFanDegrees", "霧を集める扇の片側の角度 (度)", gatherFanDegrees, 0.0f, 80.0f);
        fields.Float("gatherNearRadius", "足元の全周から霧を寄せる半径 (m、0 は無効)", gatherNearRadius, 0.0f, 10.0f);
        fields.Float("gatherRadius", "霧を口へ運ぶ流れ 1 本の半径 (m)", gatherRadius, 0.01f, 10.0f);
        fields.Float("gatherStrength", "霧を口へ運ぶ加速度 (m/s2・0 は停止)", gatherStrength, 0.0f, 100.0f);
        fields.Float("gatherFarScale", "いちばん奥の流れの強さの倍率（手前は 1）", gatherFarScale, 0.0f, 2.0f);
        fields.Int("gatherRings", "霧を運ぶ流れの輪の数（多いほど奥まで途切れずに運ぶ）", gatherRings, 1, 4);
        fields.Float("gatherSwirl", "渦の強さ（内向きに対する横向きの割合、0 はまっすぐ）", gatherSwirl, -2.0f, 2.0f);
        fields.Float("gatherPulse", "奥から手前へ進む脈の強さ（0 は一定）", gatherPulse, 0.0f, 1.0f);
        fields.Float("gatherPulseFrequency", "脈の速さ (Hz)", gatherPulseFrequency, 0.0f, 10.0f);
        fields.Float("gatherLift", "足元に集まった霧を口へ持ち上げる力の倍率", gatherLift, 0.0f, 5.0f);
        fields.Float("absorbForwardDistance", "霧を集める中心の前方距離 (m)", absorbForwardDistance, 0.0f, 10.0f);
        fields.Float("absorbHeightOffset", "霧を集める中心の高さ（プレイヤー原点から m）", absorbHeightOffset, -10.0f,
                     10.0f);
        fields.Bool("absorbWhenFull", "満杯でも霧を吸い続ける（切ると満杯の間は霧を消さない）", absorbWhenFull);
        fields.Note("口元の吸入が周り全体の霧を口へ引き寄せ、前方の扇の流れが遠"
                    "くの霧を渦を巻かせて運びます。"
                    "吸入速度と半径を上げると吸う速さ（煙／秒）も上がります。既"
                    "定は濃い霧の中で約 60／秒です。");
        fields.Note("プレイヤーの向きの前方の扇から、場の霧を輪から輪へ口の前ま"
                    "で運び、口元だけで回収します。"
                    "運ぶ力だけでは煙量は増えません。奥の輪は扇の両端まで 5 "
                    "本、いちばん手前の輪は 3 本の流れです。");
    }
    if (fields.Section("吸い込みの操作", true)) {
        fields.Int("absorbWindupFrames", "構えの前隙 (F。動けない)", absorbWindupFrames, 0, 600);
        fields.Int("absorbRecoveryFrames", "離した後の後隙 (F。動けない)", absorbRecoveryFrames, 0, 600);
        fields.Float("absorbMoveScale", "吸い込み中の歩行倍率（0.06 で約 0.3 m/秒）", absorbMoveScale, 0.0f, 1.0f);
        fields.Float("absorbTurnSpeed", "吸い込み中に向きを変えられる速さ (度/秒)", absorbTurnSpeed, 0.0f, 3600.0f);
        fields.Note("吸っている間は入力した方向へ向きを変えられます（ロックオン中でも）。吸う向きはプレイヤーの向きです。"
                    "構え・吸い込み・後隙の間は、ローリング・回復・砲撃を始められません。");
    }
    if (fields.Section("煙の残量", true)) {
        fields.Float("capacity", "煙の容量", capacity, 0.0f, 100000.0f);
        fields.Float("initialSmoke", "開始時の煙量", initialSmoke, 0.0f, 100000.0f);
        fields.Float("smokePerVolume",
                     "吸った密度・体積から煙量への倍率（小さいほど満杯までに多"
                     "くの霧を使う）",
                     smokePerVolume, 0.0f, 100000.0f);
        fields.Note("回読は 1〜3 "
                    "フレーム遅れます。満杯の間は霧を消しません（「満杯でも霧を"
                    "吸い続ける」を入れると、"
                    "吸い込みは続き、超過分は捨てます）。");
    }
    if (fields.Section("吸い込みの演出と手応え")) {
        fields.Vfx("absorbEffect", "口の流体エフェクト（吸い込みの霧の筋）", absorbEffect);
        fields.Float("absorbEffectScale", "口のエフェクトの源の大きさ倍率（0 は無効）", absorbEffectScale, 0.0f, 8.0f);
        fields.Float("absorbFullIntake", "口の演出が最大になる吸う速さ (煙／秒)", absorbFullIntake, 0.1f, 1000.0f);
        fields.Float("absorbIdleEffect", "吸える霧が無い時の口の演出の強さ (0〜1)", absorbIdleEffect, 0.0f, 1.0f);
        fields.Float("absorbFullEffect", "満杯で吸えない時の口の演出の強さ (0〜1、0 は何も出さない)", absorbFullEffect,
                     0.0f, 1.0f);
        fields.Float("absorbGlowHeat", "吸われていく霧を温める温度（高いほど霧が明るく光る。0 は無効）", absorbGlowHeat,
                     0.0f, 20.0f);
        fields.Float("absorbGlowRadius", "口元で煙を温める半径 (m)", absorbGlowRadius, 0.1f, 10.0f);
        fields.Float("absorbGlowReach", "地面に沿って霧を温める場所の間隔 (m。3 か所、奥ほど広くぬるい)",
                     absorbGlowReach, 0.0f, 10.0f);
        fields.Int("absorbRetireFrames", "吸い込みをやめてから口の域を消すまでの時間 (F)", absorbRetireFrames, 0, 600);
        fields.Float("absorbFovDegrees", "吸い込み中に狭める視野角 (度、0 は無効)", absorbFovDegrees, 0.0f, 20.0f);
        fields.Int("absorbFovEaseFrames", "視野角が寄る・戻る時間 (F)", absorbFovEaseFrames, 1, 120);
        fields.Float("absorbShakeAmplitude", "吸い込み中のカメラの細かい震え (m、0 は無効)", absorbShakeAmplitude, 0.0f,
                     0.2f);
        fields.Float("absorbShakeFrequency", "カメラの震えの周波数 (Hz)", absorbShakeFrequency, 0.0f, 60.0f);
        fields.Float("absorbPulseScale", "吸い込み中にヒョウタンが膨らむ割合（0 は無効）", absorbPulseScale, 0.0f, 1.0f);
        fields.Float("absorbPulseFrequency", "ヒョウタンの脈動の速さ (Hz)", absorbPulseFrequency, 0.0f, 30.0f);
        fields.Float("absorbRumble", "吸い込み中の振動の強さ (0〜1、0 は無効)", absorbRumble, 0.0f, 1.0f);
        fields.Int("fullRumbleFrames", "満杯になった時の振動時間 (F、0 は無効)", fullRumbleFrames, 0, 120);
        fields.Float("fullRumble", "満杯になった時の振動の強さ (0〜1)", fullRumble, 0.0f, 1.0f);
        fields.Note("エフェクト資産の +X は口の向く先。入力 input.on "
                    "は吸い込みの強さ（0〜1）で、実際に吸えている"
                    "速さに合わせて上がります。霧の無い所で吸うと弱く、満杯で吸"
                    "えない間は 0 です。使っていない間は"
                    "域を消して、同時に描ける流体の域の枠を空けます。");
    }
    if (fields.Section("検証")) {
        fields.Int("traceEveryFrames", "状態ログ間隔（0 は無効）", traceEveryFrames, 0, 7200);
    }
}

} // namespace Project_TD2_2
