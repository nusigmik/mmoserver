#pragma once
#include "Common.h"

using namespace boost;

class ILivingEntity
{
public:
    virtual bool IsDead() const = 0;
    virtual void Die() = 0;
    virtual void TakeDamage(const uuid& attacker, int damage) = 0;

    virtual signals2::connection ConnectDeathSignal(std::function<void(ILivingEntity*)> handler) = 0;
};