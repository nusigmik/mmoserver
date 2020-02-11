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
    // 관심 지역에 Cell이 들어올때
    virtual void OnCellEnter(CellType* cell) override;
    // 관심 지역에 Cell이 나갈때
    virtual void OnCellExit(CellType* cell) override;

    // 관심 지역에 Actor가 들어올때
    virtual void OnActorEnter(Actor* actor) override;
    // 관심 지역에 Actor가 나갈때
    virtual void OnActorExit(Actor* actor) override;

    void OnActorUpdate(PCS::World::Notify_UpdateT* message);

private:
    RemoteClient* rc_;
    std::map<CellType*, signals2::connection> update_connections_;
};
