#include "stdafx.h"
#include "InstanceZone.h"
#include "World.h"
#include "Hero.h"

InstanceZone::InstanceZone(const uuid & entity_id, const Map & map_data, World * owner)
    : Zone(entity_id, map_data, owner)
{
    BOOST_LOG_TRIVIAL(info) << "Create Instance Zone. map_id: "<< map_data.id;
}

InstanceZone::~InstanceZone()
{
    BOOST_LOG_TRIVIAL(info) << "Destroy Instance Zone. map_id: " << MapId();
}

void InstanceZone::Enter(const Ptr<Actor>& actor, const Vector3 & position)
{
    Zone::Enter(actor, position);

    Hero* hero = dynamic_cast<Hero*>(actor.get());
    if (hero != nullptr)
    {
        hero->instance_zone_ = std::make_tuple(EntityId(), MapId());
    }
    // �δ��� �÷��̾ ������ ���� Ÿ�̸� ���
    if (heroes_.size() != 0 && destroy_timer_)
    {
        destroy_timer_->cancel();
        destroy_timer_.reset();
    }
}

void InstanceZone::Exit(const Ptr<Actor>& actor)
{
    Zone::Exit(actor);
    // �δ��� ��� �÷��̾ ������ 10���� ����
    if (heroes_.size() == 0 && !destroy_timer_)
    {
        destroy_timer_ = GetWorld()->RunAfter(10s, [this](auto& timer) { 
            GetWorld()->DeleteZone(EntityId());
        });
    }
}

void InstanceZone::Update(float delta_time)
{
    Zone::Update(delta_time);
}
