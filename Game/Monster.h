#pragma once
#include "Common.h"
#include "Actor.h"
#include "ILivingEntity.h"
#include "protocol_cs_helper.h"

using namespace boost;
namespace PCS = ProtocolCS;

constexpr float MON_MOVE_SPEED = 2.0f;
constexpr float MON_ATTACK_RANGE = 1.5f;
constexpr duration MON_ATTACK_COOL = 3s;

struct MonsterAI;

// 몬스터 게임 캐릭터.
class Monster : public Actor, public ILivingEntity
{
public:
	Monster(const uuid& entity_id);
	virtual ~Monster();

    void Init(const db::Monster& db_data);
    void InitAI();

    // Inherited via Actor
    virtual fb::Offset<PCS::World::Actor> Serialize(fb::FlatBufferBuilder& fbb) const override;
    virtual void SerializeT(PCS::World::ActorT & out) const override;
    
    fb::Offset<PCS::World::Monster> SerializeAsMonster(fb::FlatBufferBuilder& fbb) const;
    void SerializeAsMonsterT(PCS::World::MonsterT & out) const;

    void ActionMove(const Vector3 & position, float delta_time);
    void ActionAttack(const uuid& target);

    virtual void Update(double delta_time) override;

    int Uid();
    int TypeId();
    int Level();
    int MaxMp();
    int Mp();
    int Att();
    int Def();

    int MaxHp() const;
    void MaxHp(int max_hp);
    int Hp() const;
    void Hp(int hp);

    // Inherited via ILivingEntity
    virtual bool IsDead() const override;
    virtual void Die() override;
    virtual void TakeDamage(const uuid& attacker, int damage) override;
    virtual signals2::connection ConnectDeathSignal(std::function<void(ILivingEntity*)> handler) override;

private:
    signals2::signal<void(ILivingEntity*)> death_signal_;

    int            uid_;
	int			   type_id_;
	int            level_;
	int            max_hp_;
	int            hp_;
	int            max_mp_;
	int            mp_;
	int            att_;
	int            def_;
	int            map_id_;

    UPtr<MonsterAI> ai_;
};
