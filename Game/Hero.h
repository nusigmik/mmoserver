#pragma once
#include "Common.h"
#include "Actor.h"
#include "ILivingEntity.h"
#include "RemoteWorldClient.h"
#include "protocol_cs_helper.h"

using namespace boost;
namespace PCS = ProtocolCS;

constexpr float HERO_MOVE_SPEED = 3.0f;

// 플레이어 게임 캐릭터
class Hero : public Actor, public ILivingEntity
{
public:
    Hero(const uuid& entity_id, RemoteWorldClient* rc);
    virtual ~Hero();

    void Init(const db::Hero& db_data);

    RemoteWorldClient* GetRemoteClient();

    void Send(const uint8_t * data, size_t size);
    template <typename BufferT>
    void Send(BufferT&& data)
    {
        rc_->Send(std::forward<BufferT>(data));
    }

    void SetToDB(db::Hero& db_data);
    void UpdateToDB(const Ptr<MySQLPool>& db);

    void ActionMove(const Vector3& position, float rotation, const Vector3& velocity);

    void ActionSkill(int skill_id, float rotation, const std::vector<uuid>& targets);

    // Inherited via Actor
    virtual fb::Offset<PCS::World::Actor> Serialize(fb::FlatBufferBuilder& fbb) const override;
    virtual void SerializeT(PCS::World::ActorT& out) const override;

    fb::Offset<PCS::World::Hero> SerializeAsHero(fb::FlatBufferBuilder& fbb) const;
    void SerializeAsHeroT(PCS::World::HeroT& out) const;

    virtual void Update(double delta_time) override;

    int Uid() { return uid_; }
    ClassType HeroClassType() { return class_type_; }
    int Exp() { return exp_; }
    int Level() { return level_; }
    int MaxMp() { return max_mp_; }
    int Mp() { return mp_; }
    void Mp(int mp) { mp_ = boost::algorithm::clamp(mp, 0, MaxMp()); }
    int Att() { return att_; }
    int Def() { return def_; }
    int MapId() { return map_id_; }
    //uuid ZoneEntityId() { return zone_entity_id_; }

    int MaxHp() const;
    void MaxHp(int max_hp);
    int Hp() const;
    void Hp(int hp);

    // Inherited via Actor
    virtual void SetZone(Zone* zone) override;

    // Inherited via ILivingEntity
    virtual bool IsDead() const override;
    virtual void Die() override;
    virtual void TakeDamage(const uuid& attacker, int damage) override;
    virtual signals2::connection ConnectDeathSignal(std::function<void(ILivingEntity*)> handler) override;

    std::tuple<uuid, int> instance_zone_;
private:
    RemoteWorldClient* rc_;
    signals2::signal<void(ILivingEntity*)> death_signal_;
    time_point restor_time_;

    int            uid_;
    ClassType      class_type_;
    int            exp_;
    int            level_;
    int            max_hp_;
    int            hp_;
    int            max_mp_;
    int            mp_;
    int            att_;
    int            def_;
    int            map_id_;
    //uuid           zone_entity_id_;
};