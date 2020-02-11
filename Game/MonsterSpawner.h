#pragma once
#include "Common.h"

class World;
class Zone;
class Monster;

// ���� ������
class MonsterSpawner
{
public:
    MonsterSpawner(Zone* zone);
    ~MonsterSpawner();
    // ���� ������ ����
    void Start();
    // ������ ������Ʈ
    void Update(double delta_time);

private:
    void Spawn(int spawn_uid);

    World* world_;
    Zone* zone_;
    std::unordered_map<int, Ptr<Monster>> spawn_monsters_;
};

