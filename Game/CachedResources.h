#pragma once
#include <algorithm>
#include <unordered_map>
#include "Common.h"
#include "Singleton.h"
#include "Settings.h"
#include "DBSchema.h"
#include "MySQL.h"

namespace db = db_schema;

// DB나 파일에서 미리 읽어놓은 게임에 필요한 데이터 테이블들

// 맵 테이블
class MapTable : public Singleton<MapTable>
{
public:
	const std::vector<db::Map>& GetAll()
	{
		return data_;
	}

	const db::Map* Get(int id)
	{
		auto iter = std::find_if(data_.begin(), data_.end(),
			[id](const db::Map& map)
			{
				return map.id == id;
			});

		return (iter != data_.end()) ? &(*iter) : nullptr;
	}

	static bool Load(Ptr<MySQLPool> db)
	{
		auto& instance = GetInstance();

		try
		{
			instance.data_.clear();

			auto result_set = db->Excute("SELECT * FROM map_tb");
			while (result_set->next())
			{
				db::Map map;
				map.id = result_set->getInt("id");
				map.name = result_set->getString("name").c_str();
				map.width = result_set->getInt("width");
				map.height = result_set->getInt("height");
				map.type = (MapType)result_set->getInt("type");

				instance.data_.push_back(map);
			}
		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << std::endl;
			return false;
		}

		return true;
	}

private:
	std::vector<db::Map> data_;
};

// 맵 출입구 테이블
class MapGateTable : public Singleton<MapGateTable>
{
public:
    const std::vector<db::MapGate>& GetAll()
    {
        return data_;
    }

    const db::MapGate* Get(int uid)
    {
        auto iter = std::find_if(data_.begin(), data_.end(), [uid](const db::MapGate& map)
        {
            return map.uid == uid;
        });

        return (iter != data_.end()) ? &(*iter) : nullptr;
    }

    static bool Load(Ptr<MySQLPool> db)
    {
        auto& instance = GetInstance();

        try
        {
            instance.data_.clear();

            auto result_set = db->Excute("SELECT * FROM map_gate_tb");
            while (result_set->next())
            {
                db::MapGate row;
                row.uid = result_set->getInt("uid");
                row.map_id = result_set->getInt("map_id");
                row.pos = Vector3((float)result_set->getDouble("pos_x"), (float)result_set->getDouble("pos_y"), (float)result_set->getDouble("pos_z"));
                row.dest_uid = result_set->getInt("dest_uid");

                instance.data_.push_back(row);
            }
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            return false;
        }

        return true;
    }

private:
    std::vector<db::MapGate> data_;
};

// 영웅 능력치 테이블
class HeroAttributeTable : public Singleton<HeroAttributeTable>
{
public:
	const std::vector<db::HeroAttribute>& GetAll()
	{
		return data_;
	}

	const db::HeroAttribute* Get(ClassType type, int level)
	{
		auto iter = std::find_if(data_.begin(), data_.end(),
			[&](const db::HeroAttribute& value)
			{
				return (value.class_type == type) && (value.level == level);
			});

		return (iter != data_.end()) ? &(*iter) : nullptr;
	}

	static bool Load(Ptr<MySQLPool> db)
	{
		auto& instance = GetInstance();

		try
		{
			instance.data_.clear();

			auto result_set = db->Excute("SELECT * FROM hero_attribute_tb");
			while (result_set->next())
			{
				db::HeroAttribute attribute;
				attribute.class_type = (ClassType)result_set->getInt("class_type");
				attribute.level = result_set->getInt("level");
				attribute.hp = result_set->getInt("hp");
				attribute.mp = result_set->getInt("mp");
				attribute.att = result_set->getInt("att");
				attribute.def = result_set->getInt("def");
					
				instance.data_.push_back(attribute);
			}

		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << std::endl;
			return false;
		}

		return true;
	}

private:
	std::vector<db::HeroAttribute> data_;
};

// 몬스터 테이블
class MonsterTable : public Singleton<MonsterTable>
{
public:
    const std::unordered_map<int, db::Monster>& GetAll()
    {
        return data_;
    }

    const db::Monster* Get(int uid)
    {
        auto iter = data_.find(uid);

        return (iter != data_.end()) ? &(iter->second) : nullptr;
    }

    static bool Load(Ptr<MySQLPool> db)
    {
        auto& instance = GetInstance();

        try
        {
            instance.data_.clear();

            auto result_set = db->Excute("SELECT * FROM monster_tb");
            while (result_set->next())
            {
                db::Monster row;
                row.uid     = result_set->getInt("uid");
                row.type_id = result_set->getInt("type_id");
                row.name    = result_set->getString("name").c_str();
                row.level   = result_set->getInt("level");
                row.max_hp  = result_set->getInt("max_hp");
                row.max_mp  = result_set->getInt("max_mp");
                row.att     = result_set->getInt("att");
                row.def     = result_set->getInt("def");

                instance.data_.emplace(row.uid, row);
            }
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            return false;
        }

        return true;
    }

private:
    std::unordered_map<int, db::Monster> data_;

};

// 몬스터 스폰 테이블
class MonsterSpawnTable : public Singleton<MonsterSpawnTable>
{
public:
    std::unordered_map<int, db::MonsterSpawn>& GetAll()
    {
        return data_;
    }

    const db::MonsterSpawn* Get(int uid)
    {
        auto iter = data_.find(uid);

        return (iter != data_.end()) ? &(iter->second) : nullptr;
    }

    static bool Load(Ptr<MySQLPool> db)
    {
        auto& instance = GetInstance();

        try
        {
            instance.data_.clear();

            auto result_set = db->Excute("SELECT * FROM monster_spawn_tb");
            while (result_set->next())
            {
                db::MonsterSpawn row;
                row.uid = result_set->getInt("uid");
                row.map_id = result_set->getInt("map_id");
                row.monster_uid = result_set->getInt("monster_uid");
                row.pos = Vector3((float)result_set->getDouble("pos_x"), (float)result_set->getDouble("pos_y"), (float)result_set->getDouble("pos_z"));
                row.interval_s = std::chrono::duration_cast<duration>(std::chrono::seconds(result_set->getInt("interval_s")));

                instance.data_.emplace(row.uid, row);
            }
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            return false;
        }

        return true;
    }

private:
    std::unordered_map<int, db::MonsterSpawn> data_;
};

// 스킬 테이블
class SkillTable : public Singleton<SkillTable>
{
public:
    std::unordered_map<int, db::Skill>& GetAll()
    {
        return data_;
    }

    const db::Skill* Get(int uid)
    {
        auto iter = data_.find(uid);

        return (iter != data_.end()) ? &(iter->second) : nullptr;
    }

    static bool Load(Ptr<MySQLPool> db)
    {
        auto& instance = GetInstance();

        try
        {
            instance.data_.clear();

            auto result_set = db->Excute("SELECT * FROM skill_tb");
            while (result_set->next())
            {
                db::Skill row;
                row.skill_id = result_set->getInt("skill_id");
                row.class_type = (ClassType)result_set->getInt("class_type");
                row.targeting_type = (TargetingType)result_set->getInt("targeting_type");
                row.range = (float)result_set->getDouble("range");
                row.radius = (float)result_set->getDouble("radius");
                row.angle = (float)result_set->getDouble("angle");
                row.cost = result_set->getInt("cost");
                row.cast_time = (float)result_set->getDouble("cast_time");
                row.cool_down = (float)result_set->getDouble("cool_down");
                row.damage = result_set->getInt("damage");

                instance.data_.emplace(row.skill_id, row);
            }
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            return false;
        }

        return true;
    }

private:
    std::unordered_map<int, db::Skill> data_;
};

// 플레이어 영웅 캐릭터 부활지점 테이블
class HeroSpawnTable : public Singleton<HeroSpawnTable>
{
public:
    std::unordered_map<int, db::HeroSpawn>& GetAll()
    {
        return data_;
    }

    const db::HeroSpawn* Get(int uid)
    {
        auto iter = data_.find(uid);

        return (iter != data_.end()) ? &(iter->second) : nullptr;
    }

    static bool Load(Ptr<MySQLPool> db)
    {
        auto& instance = GetInstance();

        try
        {
            instance.data_.clear();

            auto result_set = db->Excute("SELECT * FROM hero_spawn_tb");
            while (result_set->next())
            {
                db::HeroSpawn row;
                row.uid = result_set->getInt("uid");
                row.map_id = result_set->getInt("map_id");
                row.pos = Vector3((float)result_set->getDouble("pos_x"), (float)result_set->getDouble("pos_y"), (float)result_set->getDouble("pos_z"));

                instance.data_.emplace(row.uid, row);
            }
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            return false;
        }

        return true;
    }

private:
    std::unordered_map<int, db::HeroSpawn> data_;
};

