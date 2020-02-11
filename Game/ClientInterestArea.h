#pragma once
#include "InterestArea.h"
#include "protocol_cs_helper.h"

using namespace boost;
namespace PCS = ProtocolCS;

class RemoteClient;

class ClientInterestArea : public InterestArea
{
public:
    ClientInterestArea(RemoteClient* rc, GridType* grid);
    ~ClientInterestArea();

protected:
    // ���� ������ Cell�� ���ö�
    virtual void OnCellEnter(CellType* cell) override;
    // ���� ������ Cell�� ������
    virtual void OnCellExit(CellType* cell) override;

    // ���� ������ Actor�� ���ö�
    virtual void OnActorEnter(Actor* actor) override;
    // ���� ������ Actor�� ������
    virtual void OnActorExit(Actor* actor) override;

    void OnActorUpdate(PCS::World::Notify_UpdateT* message);

private:
    RemoteClient* rc_;
    std::map<CellType*, signals2::connection> update_connections_;
};
