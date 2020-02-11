#include "stdafx.h"
#include "World.h"
#include "CachedResources.h"
#include "InstanceZone.h"


World::World(Ptr<EventLoop>& loop)
	: ev_loop_(loop)
    , strand_(ev_loop_->GetIoContext())
{
}

World::~World()
{
	Stop();
}

void World::Start()
{
	CreateFieldZones();
}

void World::Stop()
{

}

Zone * World::FindFieldZone(int map_id)
{
	auto& indexer = zone_set_.get<zone_tags::map_id>();
	auto iter = indexer.find(map_id);
	return iter != indexer.end() ? iter->get() : nullptr;
}

InstanceZone * World::FindInstanceZone(const uuid & entity_id)
{
    auto& indexer = zone_set_.get<zone_tags::entity_id>();
    auto iter = indexer.find(entity_id);
    if (iter == indexer.end())
        return nullptr;

    return dynamic_cast<InstanceZone*>(iter->get());
}

void World::DoUpdate(float delta_time)
{
	for(auto& var : zone_set_)
	{
		var->Update(delta_time);
	}
}

void World::CreateFieldZones()
{
	// 필드존 생성
	auto& map_table = MapTable::GetInstance().GetAll();
	for (auto& map_data : map_table)
	{
        if (map_data.type == MapType::Field)
        {
            auto zone = std::make_shared<Zone>(random_generator()(), map_data, this);
            zone_set_.insert(zone);
        }
	}
}

InstanceZone* World::CreateInstanceZone(int map_id)
{
    auto map_data = MapTable::GetInstance().Get(map_id);
    if (map_data && map_data->type == MapType::Dungeon)
    {
        auto zone = std::make_shared<InstanceZone>(random_generator()(), *map_data, this);
        zone_set_.insert(zone);
        return zone.get();
    }
    
    return nullptr;
}




