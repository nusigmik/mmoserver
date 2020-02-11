#pragma once
#include "Common.h"
#include "DBSchema.h"
#include "Grid.h"
#include "ZoneCell.h"

using db_schema::Map;
using db_schema::MapGate;

class World;
class Actor;
class Hero;
class Monster;
class MonsterSpawner;

constexpr float CELL_SIZE = 10.0f;

// 게임 존.
// 캐릭터간의 상호 작용이 일어나는 지역.
class Zone : public Grid<ZoneCell>, std::enable_shared_from_this<Zone>
{
public:
    using GridType = Grid<ZoneCell>;

	Zone(const Zone&) = delete;
	Zone& operator=(const Zone&) = delete;

	Zone(const uuid& entity_id, const Map& map_data, World* owner);
	virtual ~Zone();

    // 고유 엔티티 id.
	const uuid& EntityId() const { return entity_id_; }
    // 맵 id.
	int MapId() const { return map_data_.id; }
    // 맵 데이터.
	const Map& MapData() const { return map_data_; }
    // 맵 타입.
    MapType MapType() const { return map_data_.type; }
    // 게임 월드 객체.
    World* GetWorld() { return owner_; }

    // 존 입장.
	virtual void Enter(const Ptr<Actor>& actor, const Vector3& position);
    // 존 퇴장.
    virtual void Exit(const Ptr<Actor>& actor);
    void Exit(const uuid& entity_id);
    // 존의 모든 캐릭터를 퇴장.
    virtual void ExitAllActors();

    // 프레임 업데이트.
	virtual void Update(float delta_time);

    // 존의 출입구 정보를 얻는다
    const MapGate* GetGate(int uid);
    const std::unordered_map<int, const MapGate*>& GetGates()
    {
        return map_gates_;
    }

    // 존의 캐릭터를 찾는다.
    Actor* FindActor(const uuid& entity_id)
    {
        auto iter = actors_.find(entity_id);
        return (iter != actors_.end()) ? (iter->second).get() : nullptr;
    }

    bool Contained(const Vector3& position)
    {
        return Area().Contains(position);
    }
    Vector3 CheckBoader(const Vector3& position);

    fb::Offset<PCS::World::MapData> Serialize(fb::FlatBufferBuilder& fbb) const;

protected:
	World* owner_;
	
	uuid entity_id_;
	Map map_data_;
    std::unordered_map<int, const MapGate*> map_gates_;
	
	// 지역에 속한 Actor
	std::unordered_map<uuid, Ptr<Actor>, std::hash<boost::uuids::uuid>> actors_;
    std::unordered_map<uuid, Hero*, std::hash<boost::uuids::uuid>> heroes_;
    Ptr<MonsterSpawner> mon_spawner_;
};
