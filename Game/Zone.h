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

// ���� ��.
// ĳ���Ͱ��� ��ȣ �ۿ��� �Ͼ�� ����.
class Zone : public Grid<ZoneCell>, std::enable_shared_from_this<Zone>
{
public:
    using GridType = Grid<ZoneCell>;

	Zone(const Zone&) = delete;
	Zone& operator=(const Zone&) = delete;

	Zone(const uuid& entity_id, const Map& map_data, World* owner);
	virtual ~Zone();

    // ���� ��ƼƼ id.
	const uuid& EntityId() const { return entity_id_; }
    // �� id.
	int MapId() const { return map_data_.id; }
    // �� ������.
	const Map& MapData() const { return map_data_; }
    // �� Ÿ��.
    MapType MapType() const { return map_data_.type; }
    // ���� ���� ��ü.
    World* GetWorld() { return owner_; }

    // �� ����.
	virtual void Enter(const Ptr<Actor>& actor, const Vector3& position);
    // �� ����.
    virtual void Exit(const Ptr<Actor>& actor);
    void Exit(const uuid& entity_id);
    // ���� ��� ĳ���͸� ����.
    virtual void ExitAllActors();

    // ������ ������Ʈ.
	virtual void Update(float delta_time);

    // ���� ���Ա� ������ ��´�
    const MapGate* GetGate(int uid);
    const std::unordered_map<int, const MapGate*>& GetGates()
    {
        return map_gates_;
    }

    // ���� ĳ���͸� ã�´�.
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
	
	// ������ ���� Actor
	std::unordered_map<uuid, Ptr<Actor>, std::hash<boost::uuids::uuid>> actors_;
    std::unordered_map<uuid, Hero*, std::hash<boost::uuids::uuid>> heroes_;
    Ptr<MonsterSpawner> mon_spawner_;
};
