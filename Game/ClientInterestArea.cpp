#include "stdafx.h"
#include "ClientInterestArea.h"
#include "RemoteClient.h"
#include "Zone.h"
#include "Actor.h"

ClientInterestArea::ClientInterestArea(RemoteClient * rc, GridType * grid)
    : InterestArea(grid)
    , rc_(rc)
{
}

ClientInterestArea::~ClientInterestArea()
{
    for (auto& e : update_connections_)
    {
        e.second.disconnect();
    }
}

// ���� ������ Cell�� ���ö�
void ClientInterestArea::OnCellEnter(CellType * cell)
{
    InterestArea::OnCellEnter(cell);

    signals2::connection conn = cell->actor_update_signal.connect(std::bind(&ClientInterestArea::OnActorUpdate, this, std::placeholders::_1));
    update_connections_.emplace(cell, std::move(conn));
}

// ���� ������ Cell�� ������
void ClientInterestArea::OnCellExit(CellType * cell)
{
    InterestArea::OnCellExit(cell);

    // ���� ���� ���� ���� �� ����
    auto iter = update_connections_.find(cell);
    if (iter != update_connections_.end())
    {
        iter->second.disconnect();
        update_connections_.erase(iter);
    }
}

// ���� ������ Actor�� ���ö�
void ClientInterestArea::OnActorEnter(Actor * actor)
{
    if (actor == nullptr) return;

    flatbuffers::FlatBufferBuilder fbb;
    auto actor_offset = actor->Serialize(fbb);
    auto notify = PCS::World::CreateNotify_Appear(fbb, actor_offset);
    PCS::Send(*rc_, fbb, notify);
}

// ���� ������ Actor�� ������
void ClientInterestArea::OnActorExit(Actor * actor)
{
    if (actor == nullptr) return;

    flatbuffers::FlatBufferBuilder fbb;
    auto notify = PCS::World::CreateNotify_DisappearDirect(fbb, uuids::to_string(actor->GetEntityID()).c_str());
    PCS::Send(*rc_, fbb, notify);
}

void ClientInterestArea::OnActorUpdate(PCS::World::Notify_UpdateT * message)
{
    if (message == nullptr) return;

    PCS::Send(*rc_, *message);
}
