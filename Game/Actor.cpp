#include "stdafx.h"
#include "Actor.h"
#include "Zone.h"
#include "InterestArea.h"

Actor::Actor(const uuid & entity_id)
    : GameObject(entity_id)
    , zone_(nullptr)
    , current_cell_(nullptr)
{}

Actor::~Actor()
{
    ResetZone();
}

void Actor::UpdateInterest()
{
    // 관찰자에게 이동변경 알림
    poistion_update_signal(GetPosition());

    if (zone_ == nullptr) return;

    ZoneCell* prev_cell = current_cell_;
    ZoneCell* new_cell = zone_->GetCell(GetPosition());

    if (new_cell == current_cell_)
        return;

    // 다른 구역으로 넘어갔으면
    current_cell_ = new_cell;
    // 구독 해제
    for (auto& conn : cell_connections_)
    {
        conn.disconnect();
    }
    cell_connections_.clear();

    if (prev_cell != nullptr)
    {
        // 지역이 바뀜을 알린다.
        prev_cell->actor_cell_change_signal(prev_cell, new_cell, this);
    }
    if (new_cell != nullptr)
    {
        // 지역이 바뀜을 알린다.
        new_cell->actor_cell_change_signal(prev_cell, new_cell, this);

        cell_connections_.emplace_back(new_cell->interest_area_enter_signal.connect([this](InterestArea* var) { var->OnActorEnter(this); }));
        cell_connections_.emplace_back(new_cell->interest_area_exit_signal.connect([this](InterestArea* var) { var->OnActorExit(this); }));
    }
}

void Actor::ResetInterest()
{
    // 구독 해제
    for (auto& conn : cell_connections_)
    {
        conn.disconnect();
    }
    cell_connections_.clear();

    if (current_cell_ != nullptr)
    {
        // 나감을 알린다.
        current_cell_->actor_cell_change_signal(current_cell_, nullptr, this);
    }

    current_cell_ = nullptr;
}

void Actor::PublishActorUpdate(PCS::World::Notify_UpdateT * message)
{
    if (current_cell_ == nullptr)
        return;

    current_cell_->actor_update_signal(message);
}

void Actor::Spawn(const Vector3 & position)
{
    SetPosition(position);
    UpdateInterest();
}
