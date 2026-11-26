#include "GourdComponent.h"

#include "GourdRules.h"

namespace Project_TD2_2 {

void Gourd::Update(float) {
    // HUD などが読めるよう、煙の量と容量をゲーム全体の値（Params）へ毎フレーム書く。値は GourdRules の問い合わせと
    // 同じ物を使い、ほかのコードが読む値と食い違わないようにする。
    HIKARI::Params::Set(kGourdSmokeVariable, GourdRules::GetSmoke(*this));
    HIKARI::Params::Set(kGourdCapacityVariable, GourdRules::GetCapacity(*this));
    HIKARI::Params::Set(kGourdFireShotVariable, GourdRules::GetFireShotSeconds(*this));
    HIKARI::Params::Set(kGourdBlockedVariable, blockedSeconds);
}

} // namespace Project_TD2_2
