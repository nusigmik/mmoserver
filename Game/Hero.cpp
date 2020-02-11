#include "stdafx.h"
#include "Hero.h"
#include "Monster.h"
#include "Zone.h"
#include "CachedResources.h"

Hero::Hero(const uuid & entity_id, RemoteWorldClient * rc)
	: Actor(entity_id)
	, rc_(rc)
{
}

Hero::~Hero() {}

RemoteWorldClient * Hero::GetRemoteClient()
{
    return rc_;
}

void Hero::Send(const uint8_t * data, size_t size)
{
    rc_->Send(data, size);
}

void Hero::SetZone(Zone * zone)
{
	Actor::SetZone(zone);

	map_id_ = zone->MapId();
    //zone_entity_id_ = zone->EntityId();
}

void Hero::ActionMove(const Vector3 & position, float rotation, const Vector3 & velocity)
{
    if (IsDead())
        return;

    Vector3 pos(position);
    pos.Y = 0.0f; // Y는 0

    // 최대 속도보다 빠르게 움직일수 없다. 
    //if (distance(GetPosition(), position) > HERO_MOVE_SPEED * delta_time)
    //    return;
    if (GetZone() == nullptr) return;
    // 맵 경계 체크
    pos = GetZone()->CheckBoader(position);
   
    SetRotation(rotation);
    SetPosition(pos);
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

void Hero::ActionSkill(int skill_id, float rotation, const std::vector<uuid>& targets)
{
    if (IsDead())
        return;

    // 스킬 사용 조건 검사
    auto* skill = SkillTable::GetInstance().Get(skill_id);
    if (!(skill && skill->class_type == HeroClassType() && skill->cost <= Mp()))
        return;
    
    // mp 소비
    Mp(Mp() - skill->cost);
    SetRotation(rotation);

    // 통지 메시지
    PCS::World::SkillActionInfoT skill_info;
    skill_info.skill_id = skill_id;
    for (auto& e : targets)
    {
        skill_info.targets.emplace_back(uuids::to_string(e));
    }
    skill_info.rotation = rotation;

    PCS::World::Notify_UpdateT update_msg;
    update_msg.entity_id = uuids::to_string(GetEntityID());
    update_msg.update_data.Set(std::move(skill_info));
    // 통지
    PublishActorUpdate(&update_msg);

    Zone* zone = GetZone();
    if (zone == nullptr) return;
    // target을 찾아서 데미지를 준다.
    for (auto& entity_id : targets)
    {
        auto actor = zone->FindActor(entity_id);
        if (actor == nullptr)
            continue;

        ILivingEntity* entity = dynamic_cast<ILivingEntity*>(actor);
        if (entity)
        {
            entity->TakeDamage(GetEntityID(), skill->damage + Att());
        }
    }
}

void Hero::TakeDamage(const uuid& attacker, int damage)
{
    if (IsDead())
        return;

    // 방어도 만큼 데미지 감소
    damage = std::max(1, damage - Def());
    Hp(Hp() - damage);

    BOOST_LOG_TRIVIAL(info) << "Hero uid:" << Uid() << " Take damage. Amount:" << damage;

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

inline fb::Offset<PCS::World::Actor> Hero::Serialize(fb::FlatBufferBuilder & fbb) const
{
    auto hero_offset = SerializeAsHero(fbb);
    return PCS::World::CreateActor(fbb, PCS::World::ActorType::Hero, hero_offset.Union());
}

void Hero::SerializeT(PCS::World::ActorT& out) const
{
    PCS::World::HeroT hero_t;
    SerializeAsHeroT(hero_t);
    out.entity.Set(std::move(hero_t));
}

inline fb::Offset<PCS::World::Hero> Hero::SerializeAsHero(fb::FlatBufferBuilder & fbb) const
{
    auto hero_offset = PCS::World::CreateHeroDirect(fbb,
        boost::uuids::to_string(GetEntityID()).c_str(),
        uid_,
        GetName().c_str(),
        (PCS::ClassType)class_type_,
        exp_,
        level_,
        max_hp_,
        hp_,
        max_mp_,
        mp_,
        att_,
        def_,
        map_id_,
        //&pos,
        &PCS::Vec3(GetPosition().X, GetPosition().Y, GetPosition().Z),
        GetRotation()
    );

    return hero_offset;
}

void Hero::SerializeAsHeroT(PCS::World::HeroT & out) const
{
    out.entity_id     = uuids::to_string(GetEntityID());
    out.uid           = uid_;
    out.name          = GetName();
    out.class_type    = (PCS::ClassType)class_type_;
    out.exp           = exp_;
    out.level         = level_;
    out.max_hp        = max_hp_;
    out.hp            = hp_;
    out.max_mp        = max_mp_;
    out.mp            = mp_;
    out.att           = att_;
    out.def           = def_;
    out.map_id        = map_id_;
    out.pos.reset(new PCS::Vec3(GetPosition().X, GetPosition().Y, GetPosition().Z));
    out.rotation      = GetRotation();
}

inline void Hero::Update(double delta_time)
{
    if (!IsDead())
    {
        // Hp Mp 재생
        if (clock_type::now() >= restor_time_)
        {
            int prev_hp = Hp();
            int prev_mp = Mp();
            Hp(prev_hp + 5);
            Mp(prev_mp + 5);
            restor_time_ = clock_type::now() + 5s;

            if (prev_hp != Hp() || prev_mp != Mp())
            {
                // 메시지 통지
                PCS::World::AttributeInfoT data;
                //data.entity_id = uuids::to_string(GetEntityID());
                data.max_hp = MaxHp();
                data.hp = Hp();
                data.max_mp = MaxMp();
                data.mp = Mp();
                PCS::World::Notify_UpdateT update_msg;
                update_msg.entity_id = uuids::to_string(GetEntityID());
                update_msg.update_data.Set(std::move(data));
                PublishActorUpdate(&update_msg);
            }
        }
    }
}

void Hero::Init(const db::Hero & db_data)
{
    uid_ = db_data.uid;
    SetName(db_data.name);
    class_type_ = db_data.class_type;
    exp_ = db_data.exp;
    level_ = db_data.level;
    max_hp_ = db_data.max_hp;
    hp_ = db_data.hp;
    max_mp_ = db_data.max_mp;
    mp_ = db_data.mp;
    att_ = db_data.att;
    def_ = db_data.def;
    map_id_ = db_data.map_id;
    //zone_entity_id_ = db_data.zone_entity_id;
    SetPosition(db_data.pos);
    SetRotation(db_data.rotation);
}

bool Hero::IsDead() const
{
    return hp_ <= 0;
}

void Hero::Die()
{
    if (Hp() != 0)
        Hp(0);
        
    BOOST_LOG_TRIVIAL(info) << "Hero uid:" << Uid() <<" Die";

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

int Hero::MaxHp() const
{
    return max_hp_;
}

void Hero::MaxHp(int max_hp)
{
    max_hp_ = std::max(1, max_hp);
    Hp(std::min(Hp(), MaxHp()));
}

int Hero::Hp() const
{
    return hp_;
}

void Hero::Hp(int hp)
{
    hp_ = boost::algorithm::clamp(hp, 0, MaxHp());
}

signals2::connection Hero::ConnectDeathSignal(std::function<void(ILivingEntity*)> handler)
{
    return death_signal_.connect(handler);
}

void Hero::SetToDB(db::Hero & db_data)
{
    db_data.uid = uid_;
    db_data.exp = exp_;
    db_data.level = level_;
    db_data.max_hp = max_hp_;
    db_data.hp = hp_;
    db_data.max_mp = max_mp_;
    db_data.mp = mp_;
    db_data.att = att_;
    db_data.def = def_;
    db_data.map_id = map_id_;
    //db_data.zone_entity_id = zone_entity_id_;
    db_data.pos = GetPosition();
    db_data.rotation = GetRotation();
}

void Hero::UpdateToDB(const Ptr<MySQLPool>& db)
{
    db::Hero db_data;
    SetToDB(db_data);
    db_data.Update(db);
}

