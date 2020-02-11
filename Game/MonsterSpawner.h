#pragma once
#include "Common.h"

class World;
class Zone;
class Monster;

// 몬스터 스포너
class MonsterSpawner
{
public:
    MonsterSpawner(Zone* zone);
    ~MonsterSpawner();
    // 몬스터 생성을 시작
    void Start();
    // 프레임 업데이트
    void Update(double delta_time);

private:
    void Spawn(int spawn_uid);

    World* world_;
    Zone* zone_;
    std::unordered_map<int, Ptr<Monster>> spawn_monsters_;
};

