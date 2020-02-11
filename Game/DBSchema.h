#pragma once
#include "Common.h"
#include "TypeDef.h"
#include "MySQL.h"

namespace fb = flatbuffers;
namespace PCS = ProtocolCS;

// DB 테이블을 정의
namespace db_schema {

// 계정 정보
class Account
{
public:
	int         uid;
	std::string user_name;
	std::string password;

	static Ptr<Account> Create(Ptr<MySQLPool> db, const std::string& user_name, const std::string& password)
	{
		ConnectionPtr conn = db->GetConnection();
		PstmtPtr pstmt(conn->prepareStatement(
			"INSERT INTO account_tb(user_name, password) VALUES(?,?)"));
		pstmt->setString(1, user_name.c_str());
		pstmt->setString(2, password.c_str());

		if (pstmt->executeUpdate() == 0)
			return nullptr;

		return Get(db, user_name);
	}

	static Ptr<Account> Get(Ptr<MySQLPool> db, const std::string& user_name)
	{
		ConnectionPtr conn = db->GetConnection();
		PstmtPtr pstmt(conn->prepareStatement("SELECT uid, user_name, password FROM account_tb WHERE user_name=?"));
		pstmt->setString(1, user_name);
		
		ResultSetPtr result_set(pstmt->executeQuery());
		if (!result_set->next())
			return nullptr;

		auto account = std::make_shared<Account>();
		account->uid = result_set->getInt("uid");
		account->user_name = result_set->getString("user_name").c_str();
		account->password = result_set->getString("password").c_str();
		return account;
	}

	static Ptr<Account> Get(Ptr<MySQLPool> db, int uid)
	{
		ConnectionPtr conn = db->GetConnection();
		PstmtPtr pstmt(conn->prepareStatement("SELECT uid, user_name, password FROM account_tb WHERE uid=?"));
		pstmt->setInt(1, uid);

		ResultSetPtr result_set(pstmt->executeQuery());
		if (!result_set->next())
			return nullptr;

		auto account = std::make_shared<Account>();
		account->uid = result_set->getInt("uid");
		account->user_name = result_set->getString("user_name").c_str();
		account->password = result_set->getString("password").c_str();
		return account;
	}
};

// 맵정보
class Map
{
public:
	int         id;
	std::string name;
	int         width;
	int         height;
	MapType	    type;
};

// 맵 출입구
class MapGate
{
public:
    int uid;
    int map_id;
    Vector3 pos;
    int dest_uid;
};

// 영웅 능력치
class HeroAttribute
{
public:
	ClassType class_type;
	int		  level;
	int       hp;
	int       mp;
	int       att;
	int       def;
};

// 영웅 정보
class Hero
{
public:
	int            uid;
	int            account_uid;
	std::string    name;
	ClassType      class_type;
	int            exp;
	int            level;
	int            max_hp;
	int            hp;
	int            max_mp;
	int            mp;
	int            att;
	int            def;
	int            map_id;
    //uuid           zone_entity_id;
	Vector3        pos;
	float          rotation;

    static Ptr<Hero> Create(Ptr<MySQLPool> db, int account_uid, const std::string& name, ClassType class_type, int level)
	{
        ConnectionPtr conn = db->GetConnection();
        // 저장 프로시져
        PstmtPtr pstmt(conn->prepareStatement("CALL create_hero(?,?,?,?,@pop)"));
        pstmt->setInt(1, account_uid);
        pstmt->setString(2, name.c_str());
        pstmt->setInt(3, (int)class_type);
        pstmt->setInt(4, level);
        pstmt->execute();

        StmtPtr stmt(conn->createStatement());
        ResultSetPtr result_set(stmt->executeQuery("SELECT @pop AS hero_uid"));
        if (!result_set->next())
            return nullptr;

        int hero_uid = result_set->getInt("hero_uid");
        if (hero_uid == 0)
            return nullptr;

        return Get(db, hero_uid, account_uid);
	}

	static std::vector<Ptr<Hero>> GetList(Ptr<MySQLPool> db, int account_uid)
	{
		ConnectionPtr conn = db->GetConnection();
		PstmtPtr pstmt(conn->prepareStatement(
			"SELECT * FROM hero_tb WHERE account_uid=?"));
		pstmt->setInt(1, account_uid);

		ResultSetPtr result_set(pstmt->executeQuery());
		std::vector<Ptr<Hero>> hero_vec;
		while (result_set->next())
		{
			auto c = std::make_shared<Hero>();
			Init(c, result_set);
			hero_vec.push_back(std::move(c));
		}

		return hero_vec;
	}

	static Ptr<Hero> Get(Ptr<MySQLPool> db, int account_uid, const std::string& name)
	{
		ConnectionPtr conn = db->GetConnection();
		PstmtPtr pstmt(conn->prepareStatement(
			"SELECT * FROM hero_tb WHERE account_uid=? AND name=?"));
		pstmt->setInt(1, account_uid);
		pstmt->setString(2, name.c_str());

		ResultSetPtr result_set(pstmt->executeQuery());
		if (!result_set->next())
			return nullptr;

		auto c = std::make_shared<Hero>();
		Init(c, result_set);
		return c;
	}

	static Ptr<Hero> Get(Ptr<MySQLPool> db, int uid, int account_uid)
	{
		ConnectionPtr conn = db->GetConnection();
		PstmtPtr pstmt(conn->prepareStatement(
			"SELECT * FROM hero_tb WHERE uid=? AND account_uid=?"));
		pstmt->setInt(1, uid);
		pstmt->setInt(2, account_uid);

		ResultSetPtr result_set(pstmt->executeQuery());
		if (!result_set->next())
			return nullptr;

		auto c = std::make_shared<Hero>();
		Init(c, result_set);
		return c;
	}

	static Ptr<Hero> Get(Ptr<MySQLPool> db, const std::string& name)
	{
		ConnectionPtr conn = db->GetConnection();
		PstmtPtr pstmt(conn->prepareStatement(
			"SELECT * FROM hero_tb WHERE name=?"));
		pstmt->setString(1, name.c_str());

		ResultSetPtr result_set(pstmt->executeQuery());
		if (!result_set->next())
			return nullptr;

		auto c = std::make_shared<Hero>();
		Init(c, result_set);
		return c;
	}

	bool Update(Ptr<MySQLPool> db)
	{
		if (!db) return false;

		ConnectionPtr conn = db->GetConnection();
		StmtPtr stmt(conn->createStatement());
		
		std::stringstream ss;
        ss << "UPDATE hero_tb SET"
            << " exp=" << exp
            << ",level=" << level
            << ",max_hp=" << max_hp
            << ",hp=" << hp
            << ",max_mp=" << max_mp
            << ",mp=" << mp
            << ",att=" << att
            << ",def=" << def
            << ",map_id=" << map_id
            //<< ",zone_entity_id=" << boost::uuids::to_string(zone_entity_id)
			<< ",pos_x=" << pos.X
			<< ",pos_y=" << pos.Y
			<< ",pos_z=" << pos.Z
			<< ",rotation=" << rotation
			<< " WHERE uid=" << uid;

		return stmt->execute(ss.str().c_str());
	}

	bool Delete(Ptr<MySQLPool> db)
	{
		if (!db) return false;

		ConnectionPtr conn = db->GetConnection();
		StmtPtr stmt(conn->createStatement());

		std::stringstream ss;
		ss << "DELETE FROM hero_tb WHERE uid=" << uid;

		return stmt->execute(ss.str().c_str());
	}

    fb::Offset<PCS::World::Hero> Serialize(fb::FlatBufferBuilder & fbb) const
    {
        return PCS::World::CreateHeroDirect(fbb,
            nullptr,
            uid,
            name.c_str(),
            (PCS::ClassType)class_type,
            exp,
            level,
            max_hp,
            hp,
            max_mp,
            mp,
            att,
            def,
            map_id,
            //&pos,
            &PCS::Vec3(pos.X, pos.Y, pos.Z),
            rotation
        );
    }

private:
	static void Init(Ptr<Hero>& c, ResultSetPtr& result_set)
	{
		c->uid = result_set->getInt("uid");
		c->account_uid = result_set->getInt("account_uid");
		c->name = result_set->getString("name").c_str();
		c->class_type = (ClassType)result_set->getInt("class_type");
		c->exp = result_set->getInt("exp");
		c->level = result_set->getInt("level");
		c->max_hp = result_set->getInt("max_hp");
		c->hp = result_set->getInt("hp");
		c->max_mp = result_set->getInt("max_mp");
		c->mp = result_set->getInt("mp");
		c->att = result_set->getInt("att");
		c->def = result_set->getInt("def");
		c->map_id = result_set->getInt("map_id");
        //c->zone_entity_id = boost::uuids::string_generator()(result_set->getString("zone_entity_id").c_str());
		c->pos.X = (float)result_set->getDouble("pos_x");
		c->pos.Y = (float)result_set->getDouble("pos_y");
		c->pos.Z = (float)result_set->getDouble("pos_z");
		c->rotation = (float)result_set->getDouble("rotation");
	}
};

// 몬스터 정보
class Monster
{
public:
    int            uid;
    int            type_id;
    std::string    name;
    int            level;
    int            max_hp;
    int            max_mp;
    int            att;
    int            def;
};

// 몬스터 스폰 정보
class MonsterSpawn
{
public:
    int      uid;
    int      map_id;
    int      monster_uid;
    Vector3  pos;
    duration interval_s;
};

// 스킬 정보
class Skill
{
public:
    int           skill_id;
    ClassType     class_type;
    TargetingType targeting_type;
    float         range;
    float         radius;
    float         angle;
    int           cost;
    float         cast_time;
    float         cool_down;
    int           damage;
};

// 영웅 부활 지점 정보
class HeroSpawn
{
public:
    int      uid;
    int      map_id;
    Vector3  pos;
};

}