#include "stdafx.h"
#include "Zone.h"
#include "RemoteClient.h"
#include "Hero.h"
#include "Monster.h"
#include "World.h"
#include "MonsterSpawner.h"
#include "CachedResources.h"
#include "protocol_cs_helper.h"


Zone::Zone(const uuid & entity_id, const Map & map_data, World * owner)
    : GridType(BoundingBox(Vector3(0.0f, 0.0f, 0.0f), Vector3(map_data.width, 10.0f, map_data.height)), Vector3(CELL_SIZE, CELL_SIZE, CELL_SIZE))
    , entity_id_(entity_id)
    , map_data_(map_data)
    , owner_(owner)
{
    auto& map_gate_table = MapGateTable::GetInstance().GetAll();
    std::for_each(map_gate_table.begin(), map_gate_table.end(), [this](const MapGate& value)
    {
        if (value.map_id == MapId())
        {
            map_gates_.emplace(value.uid, &value);
        }
    });

    mon_spawner_ = std::make_shared<MonsterSpawner>(this);
    mon_spawner_->Start();
}

Zone::~Zone()
{
    ExitAllActors();
}

void Zone::Enter(const Ptr<Actor>& actor, const Vector3& position)
{
    actors_.emplace(actor->GetEntityID(), actor);

    Hero* hero = dynamic_cast<Hero*>(actor.get());
    if (hero != nullptr)
    {
        heroes_.emplace(hero->GetEntityID(), hero);
    }

    actor->SetZone(this);
    actor->Spawn(CheckBoader(position));
}

void Zone::Exit(const Ptr<Actor>& actor)
{
    actor->ResetZone();

    actors_.erase(actor->GetEntityID());
    heroes_.erase(actor->GetEntityID());
}

void Zone::Exit(const uuid & entity_id)
{
    auto iter = actors_.find(entity_id);
    if (iter == actors_.end())
        return;

    auto actor = iter->second;
    Exit(actor);
}

void Zone::ExitAllActors()
{
    for (auto& e : actors_)
    {
        e.second->ResetZone();
    }
    actors_.clear();
    heroes_.clear();
}

void Zone::Update(float delta_time)
{
    for (auto& var : actors_)
    {
        var.second->Update(delta_time);
    }

    mon_spawner_->Update(delta_time);
}

const MapGate * Zone::GetGate(int uid)
{
    auto iter = map_gates_.find(uid);
    return iter != map_gates_.end() ? iter->second : nullptr;
}

Vector3 Zone::CheckBoader(const Vector3 & position)
{
    const auto& area = Area();
    Vector3 result = position;

    if (position.X < area.min.X)
    {
        result.X = area.min.X;
    }
    if (position.X > area.max.X)
    {
        result.X = area.max.X;
    }
    if (position.Y < area.min.Y)
    {
        result.Y = area.min.Y;
    }
    if (position.Y > area.max.Y)
    {
        result.Y = area.max.Y;
    }
    if (position.Z < area.min.Z)
    {
        result.Z = area.min.Z;
    }
    if (position.Z > area.max.Z)
    {
        result.Z = area.max.Z;
    }

    return result;
}

fb::Offset<PCS::World::MapData> Zone::Serialize(fb::FlatBufferBuilder & fbb) const
{
    std::vector<fb::Offset<PCS::World::GateInfo>> map_gates;
    for (auto& e : map_gates_)
    {
        auto gate = e.second;
        auto dest_map_type = MapTable::GetInstance().Get(MapGateTable::GetInstance().Get(gate->dest_uid)->map_id)->type;
        auto offset = PCS::World::CreateGateInfo(fbb,
            gate->uid,
            &PCS::Vec3(gate->pos.X, gate->pos.Y, gate->pos.Z), (PCS::MapType)dest_map_type);

        map_gates.emplace_back(offset);
    }

    return PCS::World::CreateMapDataDirect(fbb,
        boost::uuids::to_string(EntityId()).c_str(),
        MapId(),
        (PCS::MapType)map_data_.type,
        &map_gates);
}
