#pragma once
#include "Zone.h"

// �ν��Ͻ� ��.
// �������� ������.
// ��� �÷��̾ ������ �˾Ƽ� �����ȴ�.
class InstanceZone : public Zone
{
public:
    InstanceZone(const uuid& entity_id, const Map& map_data, World* owner);
    virtual ~InstanceZone();

    virtual void Enter(const Ptr<Actor>& actor, const Vector3& position) override;
    virtual void Exit(const Ptr<Actor>& actor) override;
    virtual void Update(float delta_time) override;

private:
    Ptr<timer_type> destroy_timer_;
};