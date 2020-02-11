#pragma once
#include <unordered_set>
#include <boost\signals2.hpp>
#include "Common.h"
#include "Grid.h"

using namespace boost;
namespace PCS = ProtocolCS;

class Actor;
class InterestArea;

class ZoneCell : public BasicCell
{
public:
    ZoneCell(int x, int y, int z)
        : BasicCell(x, y, z)
    {
        actor_cell_change_signal.connect(std::bind(&ZoneCell::OnActorCellChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    };

    signals2::signal<void(ZoneCell*, ZoneCell*, Actor*)> actor_cell_change_signal;
    signals2::signal<void(PCS::World::Notify_UpdateT*)> actor_update_signal;

    signals2::signal<void(InterestArea*)> interest_area_enter_signal;
    signals2::signal<void(InterestArea*)> interest_area_exit_signal;

private:
    void OnActorCellChange(ZoneCell* exit, ZoneCell* enter, Actor* actor)
    {
        if (exit == this)
        {
            actors_.erase(actor);
        }
        if (enter == this)
        {
            actors_.insert(actor);
        }
    }

    std::unordered_set<Actor*> actors_;
};