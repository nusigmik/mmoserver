#include "stdafx.h"
#include "MonsterSpawner.h"
#include "World.h"
#include "Monster.h"
#include "CachedResources.h"

MonsterSpawner::MonsterSpawner(Zone * zone)
    : world_(zone->GetWorld()), zone_(zone)
{
}

MonsterSpawner::~MonsterSpawner()
{
}

void MonsterSpawner::Start()
{
    const auto spawn_table = MonsterSpawnTable::GetInstance().GetAll();
    // 몬스터 인스턴스 생성하고 스폰.
    for (auto& e : spawn_table)
    {
        if (e.second.map_id == zone_->MapId())
        {
            Spawn(e.second.uid);
        }
    }
}

void MonsterSpawner::Update(double delta_time)
{

}

void MonsterSpawner::Spawn(int spawn_uid)
{
    if (spawn_monsters_[spawn_uid])
        return;
    // 스폰 정보
    const db::MonsterSpawn* db_spawn = MonsterSpawnTable::GetInstance().Get(spawn_uid);
    if (!db_spawn)
    {
        BOOST_LOG_TRIVIAL(info) << "Can not find MonsterSpawn. spawn_uid:" << spawn_uid;
        return;
    }
    // 몬스터 정보
    const db::Monster* db_monster = MonsterTable::GetInstance().Get(db_spawn->monster_uid);
    if (!db_monster)
    {
        BOOST_LOG_TRIVIAL(info) << "Can not find Monster. monster_uid:" << db_spawn->monster_uid;
        return;
    }
    // 몬스터 인스턴스 생성
    auto new_monster = std::make_shared<Monster>(boost::uuids::random_generator()());
    new_monster->Init(*db_monster);
    // 생성 목록에 추가
    spawn_monsters_[spawn_uid] = new_monster;
    // 랜덤 위치
    std::uniform_real_distribution<float> dist {-1.0f, 1.0f};
    std::random_device rd;
    std::default_random_engine rng {rd()};
    Vector3 position((db_spawn->pos).X + dist(rng), 0.0f, (db_spawn->pos).Z + dist(rng));
    // 몬스터 Zone 입장
    zone_->Enter(new_monster, position);
    new_monster->InitAI();
    BOOST_LOG_TRIVIAL(info) << "Spawn Monster. spawn_uid:" << spawn_uid << " monster_uid:" << db_monster->uid;
   
    new_monster->ConnectDeathSignal([this, db_spawn, spawn_uid](ILivingEntity* entity)
    {
        auto monster = spawn_monsters_[spawn_uid];
        if (monster)
        {
            spawn_monsters_[spawn_uid] = nullptr;
            // 5초후 퇴장
            world_->RunAfter(5s, [this, monster](auto timer) {
                zone_->Exit(monster->GetEntityID());
            });
            // 리스폰
            if (db_spawn->interval_s != 0s)
            {
                world_->RunAfter(db_spawn->interval_s, [this, spawn_uid](auto timer) {
                    Spawn(spawn_uid);
                });
            }
        }
    });
}
