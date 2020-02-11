#include "stdafx.h"
#include "MonsterAI.h"
#include "Monster.h"
#include "Zone.h"

Idle::Idle()
    : patrol_time_(clock_type::now() + 5s)
{
    //std::cout << "Enter Idle" << "\n";
}

Idle::~Idle()
{
    //std::cout << "Exit Idle" << "\n";
}

void Idle::NextPatrolTime()
{
    std::uniform_real_distribution<float> dist{ 3.0f, 6.0f };
    std::random_device rd;
    std::default_random_engine rng{ rd() };

    patrol_time_ = clock_type::now() + std::chrono::duration_cast<clock_type::duration>(double_seconds(dist(rng)));
}

Patrol::Patrol()
{
    //std::cout << "Enter Patrol" << "\n";
}

Patrol::~Patrol()
{
    //std::cout << "Exit Patrol" << "\n";
    Monster* monster = context<MonsterAI>().GetMonster();
    monster->ActionMove(monster->GetPosition(), 0.0f);
}

sc::result Patrol::react(const EvUpdate & update)
{
    Monster* monster = context<MonsterAI>().GetMonster();
    Zone* zone = monster->GetZone();
    // �̵� ��ǥ ���
    Vector3 orgin_position = monster->GetPosition();
    Vector3 direction = target_position_ - orgin_position;
    toNormalized(direction);
    Vector3 dest_position = monster->GetPosition() + (direction * MON_MOVE_SPEED * update.DeltaTime());

    // �� ��� üũ
    if (!zone->Contained(dest_position))
    {
        return transit<Idle>();
    }
    // �̵� �������� ȸ��
    float rotation = atan2f(dest_position.X - orgin_position.X, dest_position.Z - orgin_position.Z) * (float)(rad2deg);
    monster->SetRotation(rotation);
    //�̵�
    monster->ActionMove(dest_position, update.DeltaTime());

    // �̵� ��ǥ������ �����ϸ� Idle�� ��ȯ
    if (distanceSquared(dest_position, target_position_) <= 0.1f)
    {
        return transit<Idle>();
    }

    return discard_event();
}

sc::result Patrol::react(const EvNextPosition & update)
{
    Monster* monster = context<MonsterAI>().GetMonster();
    auto& mapData = monster->GetZone()->MapData();

    // ���� �̵� ��ǥ
    std::uniform_real_distribution<float> dist{ -5.0f, 5.0f };
    std::random_device rd;
    std::default_random_engine rng{ rd() };
    Vector3 position = monster->GetPosition() + Vector3(dist(rng), 0.0f, dist(rng));
    // �� ��� üũ
    if (!((mapData.height > position.Z && 0 < position.Z) && (mapData.width > position.X && 0 < position.X)))
    {
        return transit<Idle>();
    }

    target_position_ = position;

    return discard_event();
}

Dead::Dead()
{
    //std::cout << "Enter Dead" << "\n";
}

MonsterAI::MonsterAI(Monster * monster)
    : monster_(monster)
{
    //std::cout << "Start MonsterAI" << "\n";
}

MonsterAI::~MonsterAI()
{
    //std::cout << "End MonsterAI" << "\n";
}

NonCombat::NonCombat()
{
    //std::cout << "Enter NonCombat" << "\n";
}

NonCombat::~NonCombat()
{
    //std::cout << "Exit NonCombat" << "\n";
}

sc::result NonCombat::react(const EvCombat & evt)
{
    context<MonsterAI>().TargetId() = evt.TargetId();
    return transit<Combat>();
}

Chase::Chase()
{
    //std::cout << "Enter Chase" << "\n";
}

Chase::~Chase()
{
    //std::cout << "Exit Chase" << "\n";
    Monster* monster = context<MonsterAI>().GetMonster();
    monster->ActionMove(monster->GetPosition(), 0.0f);
}

sc::result Chase::react(const EvUpdate & update)
{
    auto monster = context<MonsterAI>().GetMonster();
    auto zone = monster->GetZone();

    // Ÿ�� ��ü ����
    auto target_id = context<MonsterAI>().TargetId();
    auto target_actor = zone->FindActor(target_id);
    if (target_actor == nullptr)
    {
        return transit<NonCombat>();
    }
    
    ILivingEntity* entity = dynamic_cast<ILivingEntity*>(target_actor);
    if (entity != nullptr && entity->IsDead())
    {
        return transit<NonCombat>();
    }

    auto target_position = target_actor->GetPosition();
    // ���� ���� �Ÿ�
    float attack_range = MON_ATTACK_RANGE;
    if (distance(monster->GetPosition(), target_position) <= attack_range)
    {
        return transit<Attack>();
    }
    // �̵� ��ǥ ���
    Vector3 orgin_position = monster->GetPosition();
    Vector3 direction = target_position - orgin_position;
    toNormalized(direction);
    Vector3 dest_position = monster->GetPosition() + (direction * MON_MOVE_SPEED * update.DeltaTime());

    // �� ��� üũ
    if (!zone->Contained(dest_position))
    {
        return transit<NonCombat>();
    }
    // �̵� �������� ȸ��
    float rotation = atan2f(dest_position.X - orgin_position.X, dest_position.Z - orgin_position.Z) * (float)(rad2deg);
    monster->SetRotation(rotation);
    //�̵�
    monster->ActionMove(dest_position, update.DeltaTime());

    return discard_event();
}

Attack::Attack()
    : next_attack_time_(clock_type::now())
{
    //std::cout << "Enter Attack" << "\n";
}

Attack::~Attack()
{
    //std::cout << "Exit Attack" << "\n";
}

sc::result Attack::react(const EvUpdate & update)
{
    auto monster = context<MonsterAI>().GetMonster();
    auto zone = monster->GetZone();

    // Ÿ�� ��ü ����
    auto target_id = context<MonsterAI>().TargetId();
    auto target_actor = zone->FindActor(target_id);
    if (target_actor == nullptr)
    {
        return transit<NonCombat>();
    }
    
    ILivingEntity* entity = dynamic_cast<ILivingEntity*>(target_actor);
    if (entity != nullptr && entity->IsDead())
    {
        return transit<NonCombat>();
    }

    auto target_position = target_actor->GetPosition();
    // ���� ���� �Ÿ�
    float attack_range = MON_ATTACK_RANGE;
    if (distance(monster->GetPosition(), target_position) > attack_range)
    {
        return transit<Chase>();
    }

    Vector3 orgin_position = monster->GetPosition();
    // ���� �������� ȸ��
    float rotation = atan2f(target_position.X - orgin_position.X, target_position.Z - orgin_position.Z) * (float)(rad2deg);
    float prev_rot = monster->GetRotation();
    monster->SetRotation(rotation);

    // ���� ��Ÿ�� üũ
    if (clock_type::now() < next_attack_time_)
    {
        if (prev_rot != rotation)
        {
            monster->ActionMove(monster->GetPosition(), update.DeltaTime());
        }
        return discard_event();
    }

    // ����
    monster->ActionAttack(target_id);
    next_attack_time_ = clock_type::now() + MON_ATTACK_COOL;

    return discard_event();
}
