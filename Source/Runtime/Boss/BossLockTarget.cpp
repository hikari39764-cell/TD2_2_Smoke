#include "BossLockTarget.h"

#include <cstdio>

#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {

void BossLockTarget::Step(HIKARI::ObjectView bossObject, const BossComponent& boss, Player& player) {
    const HIKARI::GameProjectObjectHandle self = bossObject.GetHandle();
    // ロック対象を変える入力（Q）：ボス以外をロックしていた時だけ、ボスへ戻す。
    const HIKARI::GameProjectObjectHandle current = PlayerRules::GetLockTarget(player);
    if (PlayerRules::TakeLockSwitch(player) && current.value != 0 && current.value != self.value) {
        PlayerRules::SetLockTarget(player, self);
        char message[160]{};
        std::snprintf(message, sizeof(message), "LockTarget tick=%llu to=boss index=0 targets=1 why=switch",
                      static_cast<unsigned long long>(boss.tick));
        BossLog::Write(message);
    }
    // ロックしていない時の照準補助へ、今 tick の候補を渡す（毎 tick 作り直す）。
    PlayerRules::SetLockCandidates(player, &self, 1);
    // カメラの見る点の高さ（c-880340）。
    PlayerRules::SetLockTargetShape(player, boss.bodyHalfHeight);
    // 倒れたボスはロックから外す。
    if (PlayerRules::GetLockTarget(player).value == self.value && boss.state == BossState::Dead)
        PlayerRules::ReleaseLock(player);
}

} // namespace Project_TD2_2
