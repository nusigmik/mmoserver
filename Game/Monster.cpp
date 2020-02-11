#include "stdafx.h"
#include "Monster.h"
#include "Zone.h"
#include "MonsterAI.h"

Monster::Monster(const uuid & entity_id)
	: Actor(entity_id)
{}

Monster::~Monster()
{
    ai_->terminate();

    Zone* zone = GetZone();
    if (zone != nullptr)
    {
        zone->Exit(GetEntityID());
    }
}

fb::Offset<PCS::World::Actor> Monster::Serialize(fb::FlatBufferBuilder & fbb) const
{
    auto mon_offset = SerializeAsMonster(fbb);
    return PCS::World::CreateActor(fbb, PCS::World::ActorType::Monster, mon_offset.Union());
}

fb::Offset<PCS::World::Monster> Monster::SerializeAsMonster(fb::FlatBufferBuilder & fbb) const
{
    auto mon_offset = PCS::World::CreateMonsterDirect(
        fbb,
        boost::uuids::to_string(GetEntityID()).c_str(),
        uid_,
        type_id_,
        GetName().c_str(),
        level_,
        max_hp_,
        hp_,
        max_mp_,
        mp_,
        //&pos,
        &PCS::Vec3(GetPosition().X, GetPosition().Y, GetPosition().Z),
        GetRotation()
    );

    return mon_offset;
}

int Monster::Uid()
{
    return uid_;
}

int Monster::TypeId()
{
    return type_id_;
}

int Monster::Level()
{
    return level_;
}

int Monster::MaxMp()
{
    return max_mp_;
}

int Monster::Mp()
{
    return mp_;
}

int Monster::Att()
{
    return att_;
}

int Monster::Def()
{
    return def_;
}

bool Monster::IsDead() const
{
    return hp_ <= 0;
}

void Monster::Die()
{
    if (Hp() != 0)
        Hp(0);

    ai_->process_event(EvDie());
    BOOST_LOG_TRIVIAL(info) << "Monster " << this->Uid() << " Die.";

    // 죽음을 통지
    PCS::World::StateInfoT data;
    //data.entity_id = uuids::to_string(GetEntityID());
    data.state = PCS::World::StateType::Dead;
    PCS::World::Notify_UpdateT update_msg;
    update_msg.entity_id = uuids::to_string(GetEntityID());
    update_msg.update_data.Set(std::move(data));
    PublishActorUpdate(&update_msg);

    death_signal_(this);
}

int Monster::MaxHp() const
{
    return max_hp_;
}

void Monster::MaxHp(int max_hp)
{
    max_hp_ = std::max(1, max_hp);
    Hp(std::min(Hp(), MaxHp()));
}

int Monster::Hp() const
{
    return hp_;
}

void Monster::Hp(int hp)
{
    hp_ = boost::algorithm::clamp(hp, 0, MaxHp());
}

void Monster::TakeDamage(const uuid& attacker, int damage)
{
    if (IsDead())
        return;

    ai_->process_event(EvCombat(attacker));

    // 방어도 만큼 데미지 감소
    damage = std::max(1, damage - Def());
    Hp(Hp() - damage);

    BOOST_LOG_TRIVIAL(info) << "Monster " << this->Uid() << " Take damage. Amount : " << damage;

    // 데미지를 통지
    PCS::World::DamageInfoT data;
    //data.entity_id = uuids::to_string(GetEntityID());
    data.damage = damage;
    PCS::World::Notify_UpdateT update_msg;
    update_msg.entity_id = uuids::to_string(GetEntityID());
    update_msg.update_data.Set(std::move(data));
    PublishActorUpdate(&update_msg);

    if (Hp() <= 0)
    {
        Die();
    }
}

signals2::connection Monster::ConnectDeathSignal(std::function<void(ILivingEntity*)> handler)
{
    return death_signal_.connect(handler);
}

void Monster::SerializeAsMonsterT(PCS::World::MonsterT & out) const
{
    out.entity_id     = uuids::to_string(GetEntityID());
    out.uid           = uid_;
    out.name          = GetName();
    out.type_id       = type_id_;
    out.level         = level_;
    out.max_hp        = max_hp_;
    out.hp            = hp_;
    out.max_mp        = max_mp_;
    out.mp            = mp_;
    out.pos.reset(new PCS::Vec3(GetPosition().X, GetPosition().Y, GetPosition().Z));
    out.rotation      = GetRotation();
}

void Monster::ActionMove(const Vector3 & position, float delta_time)
{
    if (IsDead())
        return;

    Vector3 target_pos(position);
    target_pos.Y = 0.0f; // Y는 0
    if (GetZone() == nullptr) return;

    Vector3 delta = target_pos - GetPosition();
    Vector3 velocity = delta_time != 0.0f ? (delta / delta_time) : Vector3::Zero;
    
    SetPosition(target_pos);
    UpdateInterest();

    PCS::World::MoveActionInfoT move_info;
    //move_info.entity_id = uuids::to_string(GetEntityID());
    move_info.position = std::make_unique<PCS::Vec3>(GetPosition().X, GetPosition().Y, GetPosition().Z);
    move_info.rotation = GetRotation();
    move_info.velocity = std::make_unique<PCS::Vec3>(velocity.X, velocity.Y, velocity.Z);

    PCS::World::Notify_UpdateT update_msg;
    update_msg.entity_id = uuids::to_string(GetEntityID());
    update_msg.update_data.Set(std::move(move_info));
    // 통지
    PublishActorUpdate(&update_msg);
}

void Monster::ActionAttack(const uuid & target)
{
    if (IsDead())
        return;

    Zone* zone = GetZone();
    if (zone == nullptr) return;

    // target을 찾는다.
    auto actor = zone->FindActor(target);
    if (actor == nullptr)
        return;
    
    // 통지 메시지
    PCS::World::SkillActionInfoT skill_info;
    skill_info.skill_id = 0;
    skill_info.targets.emplace_back(uuids::to_string(target));
    skill_info.rotation = GetRotation();

    PCS::World::Notify_UpdateT update_msg;
    update_msg.entity_id = uuids::to_string(GetEntityID());
    update_msg.update_data.Set(std::move(skill_info));
    // 통지
    PublishActorUpdate(&update_msg);
 
    // 데미지를 준다.
    ILivingEntity* entity = dynamic_cast<ILivingEntity*>(actor);
    if (entity)
    {
        entity->TakeDamage(GetEntityID(), Att());
    }
}

void Monster::Update(double delta_time)
{
    if (ai_)
    {
        ai_->process_event(EvUpdate(delta_time));
    }
}

void Monster::SerializeT(PCS::World::ActorT & out) const
{
    PCS::World::MonsterT monster_t;
    SerializeAsMonsterT(monster_t);
    out.entity.Set(std::move(monster_t));
}

void Monster::Init(const db::Monster & db_data)
{
    uid_ = db_data.uid;
    SetName(db_data.name);
    type_id_ = db_data.type_id;
    level_ = db_data.level;
    MaxHp(db_data.max_hp);
    Hp(db_data.max_hp);
    max_mp_ = db_data.max_mp;
    mp_ = db_data.max_mp;
    att_ = db_data.att;
    def_ = db_data.def;
}

void Monster::InitAI()
{
    ai_ = std::make_unique<MonsterAI>(this);
    ai_->initiate();
}

