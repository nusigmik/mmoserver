#pragma once
#include <boost/statechart/event.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/transition.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include "Common.h"

namespace sc = boost::statechart;

class Monster;

// States
struct NonCombat;
struct Idle;
struct Patrol;
struct Combat;
struct Chase;
struct Attack;
struct Dead;

// Events
struct EvUpdate : sc::event<EvUpdate>
{
public:
    EvUpdate(float delta_time) : delta_time_(delta_time) {}

    float DeltaTime() const { return delta_time_; }
private:
    float delta_time_;
};
struct EvDie : sc::event<EvDie> {};
struct EvNextPosition : sc::event<EvNextPosition> {};
struct EvPatrol : sc::event<EvPatrol> {};
struct EvIdle : sc::event<EvIdle> {};
struct EvChase : sc::event<EvChase> {};
struct EvAttack : sc::event<EvAttack> {};
struct EvCombat : sc::event<EvCombat>
{
public:
    EvCombat(const uuid& target_id) : target_id_(target_id) {}
    const uuid& TargetId() const { return target_id_; }
private:
    uuid target_id_;
};
struct EvNonCombat : sc::event<EvNonCombat> {};

// State Machine
struct MonsterAI : sc::state_machine<MonsterAI, NonCombat>
{
public:
    MonsterAI(Monster* monster);
    ~MonsterAI();

    Monster* GetMonster() { return monster_; }
    uuid& TargetId() { return target_id_; }

private:
    Monster* monster_;
    uuid target_id_;
};

// 비전투 상태
struct NonCombat : sc::simple_state<NonCombat, MonsterAI, Idle>
{
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<EvCombat>,
        sc::custom_reaction<EvUpdate>,
        sc::transition<EvDie, Dead>
    >;

    NonCombat();
    ~NonCombat();

    sc::result react(const EvUpdate & update)
    {
        return forward_event();
    }
    sc::result react(const EvCombat & evt);
};

// 쉼
struct Idle : sc::simple_state<Idle, NonCombat>
{
public:
    //using reactions = sc::transition<EvPatrol, Patrol>;
    using reactions = sc::custom_reaction<EvUpdate>;

    Idle();
    ~Idle();

    sc::result react(const EvUpdate & update)
    {
        if (clock_type::now() >= patrol_time_)
        {
            // post_event(update);
            post_event(EvNextPosition());
            return transit<Patrol>();
        }

        return discard_event();
    }

private:
    void NextPatrolTime();

    time_point patrol_time_;
};

// 순찰
struct Patrol : sc::simple_state<Patrol, NonCombat>
{
public:
    //using reactions = sc::transition<EvIdle, Idle>;
    using reactions = boost::mpl::list<
        sc::custom_reaction<EvUpdate>,
        sc::custom_reaction<EvNextPosition>
    >;

    Patrol();
    ~Patrol();

    sc::result react(const EvUpdate & update);
    sc::result react(const EvNextPosition & update);
private:

    Vector3 target_position_;
};

// 전투 상태
struct Combat : sc::simple_state<Combat, MonsterAI, Chase>
{
public:
    using reactions = boost::mpl::list<
        sc::custom_reaction<EvUpdate>,
        sc::transition<EvNonCombat, NonCombat>,
        sc::transition<EvDie, Dead>
    >;

    sc::result react(const EvUpdate & update)
    {
        return forward_event();
    }
};

// 추적
struct Chase : sc::simple_state<Chase, Combat>
{
public:
    using reactions = sc::custom_reaction<EvUpdate>;

    Chase();
    ~Chase();
    sc::result react(const EvUpdate & update);
};

// 공격
struct Attack : sc::simple_state<Attack, Combat>
{
public:
    using reactions = sc::custom_reaction<EvUpdate>;

    Attack();
    ~Attack();
    sc::result react(const EvUpdate & update);

private:
    time_point next_attack_time_;
};

// 죽음
struct Dead : sc::simple_state<Dead, MonsterAI>
{
public:
    Dead();

    sc::result react(const EvUpdate & update)
    {
        return discard_event();
    }
};