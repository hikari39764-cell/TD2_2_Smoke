#include "Player.h"

#include "PlayerRules.h"

namespace Project_TD2_2 {

void Player::Update(float) {
    // 値は PlayerRules の問い合わせと同じ物を使い、ほかのコードが読む値と食い違わないようにする。
    HIKARI::Params::Set(kPlayerHealthVariable, PlayerRules::GetHealth(*this));
    HIKARI::Params::Set(kPlayerMaxHealthVariable, maxHealth);
    HIKARI::Params::Set(kPlayerStaminaVariable, PlayerRules::GetStamina(*this));
    HIKARI::Params::Set(kPlayerMaxStaminaVariable, maxStamina);
    HIKARI::Params::Set(kPlayerHealChargesVariable, PlayerRules::GetHealCharges(*this));
    HIKARI::Params::Set(kPlayerStateVariable, static_cast<int>(state));
    HIKARI::Params::Set(kPlayerBurnVariable, burnRemaining);
}

} // namespace Project_TD2_2
