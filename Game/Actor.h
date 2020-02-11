#pragma once
#include "Common.h"
#include "GameObject.h"
#include "DBSchema.h"

namespace db = db_schema;
namespace fb = flatbuffers;
namespace PCS = ProtocolCS;

class Zone;
class ZoneCell;

// 개임상에서 서로 상호 작용하는 최상위 클래스
class Actor : public GameObject
{
public:
    Actor(const uuid& entity_id);
    ~Actor();

	const std::string& GetName() const { return name_; }

    virtual void SetZone(Zone* zone)
    {
        assert(zone != nullptr);
        zone_ = zone;
        enter_zone_signal(zone);
    }

    virtual void ResetZone()
    {
        ResetInterest();
        exit_zone_signal();
        zone_ = nullptr;
    }

    bool IsInZone() { return zone_ != nullptr; }

    Zone* GetZone()
    {
        return zone_;
    }

    ZoneCell* GetCurrentCell()
    {
        return current_cell_;
    }

    void UpdateInterest();

    void ResetInterest();

    void PublishActorUpdate(PCS::World::Notify_UpdateT* message);

    void Spawn(const Vector3& position);

    virtual fb::Offset<PCS::World::Actor> Serialize(fb::FlatBufferBuilder& fbb) const = 0;
    virtual void SerializeT(PCS::World::ActorT& out) const = 0;

    signals2::signal<void(const Vector3&)> poistion_update_signal;
    signals2::signal<void(Zone* zone)> enter_zone_signal;
    signals2::signal<void()> exit_zone_signal;

protected:
	void SetName(const std::string& name) { name_ = name; }

private:
	std::string name_;
	Zone* zone_;
    ZoneCell* current_cell_;
    std::vector<signals2::connection> cell_connections_;
};
