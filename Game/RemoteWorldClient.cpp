#include "stdafx.h"
#include "RemoteWorldClient.h"
#include "WorldServer.h"
#include "World.h"
#include "Hero.h"
#include "Monster.h"
#include "CachedResources.h"
#include "protocol_cs_helper.h"

RemoteWorldClient::RemoteWorldClient(const Ptr<net::Session>& net_session, WorldServer * owner)
    : RemoteClient(net_session)
    , owner_(owner)
    , state_(State::Connected)
    , disposed_(false)
{
    assert(owner != nullptr);
}

RemoteWorldClient::~RemoteWorldClient()
{
    Dispose();
}

World * RemoteWorldClient::GetWorld() { return owner_->GetWorld(); }

void RemoteWorldClient::UpdateToDB()
{
    // 케릭터 상태 DB Update.
    if (hero_)
        hero_->UpdateToDB(GetDB());
}

// 종료 처리. 상태 DB Update 등 을 한다.
void RemoteWorldClient::Dispose()
{
    bool exp = false;
    if (!disposed_.compare_exchange_strong(exp, true))
        return;

    ExitWorld();
}

void RemoteWorldClient::EnterWorld()
{
    // 입장전 상태가 아님
    if (GetState() != RemoteWorldClient::State::Connected)
    {
        PCS::World::Notify_EnterFailedT reply_msg;
        reply_msg.error_code = PCS::ErrorCode::WORLD_LOGIN_INVALID_STATE;
        PCS::Send(*this, reply_msg);
        return;
    }

    auto db_hero = GetDBHero();
    if (!db_hero)
    {
        PCS::World::Notify_EnterFailedT reply;
        reply.error_code = PCS::ErrorCode::WORLD_CANNOT_LOAD_HERO;
        PCS::Send(*this, reply);
        return;
    }
    // 캐릭터를 생성하고 처음 입장하는 경우
    if (db_hero->map_id == 0)
    {
        // 시작 지점을 찾는다.
        auto spawn_table = HeroSpawnTable::GetInstance().GetAll();
        auto iter = std::find_if(spawn_table.begin(), spawn_table.end(), [](auto& value) {
            return (value.second.map_id == 1001);
        });
        if (iter == spawn_table.end())
        {
            PCS::World::Notify_EnterFailedT reply;
            reply.error_code = PCS::ErrorCode::WORLD_CANNOT_ENTER_ZONE;
            PCS::Send(*this, reply);
            return;
        }
        // 맵, 좌표 셋
        db_hero->map_id = iter->second.map_id;
        db_hero->pos = iter->second.pos;
    }

    // 이미 캐릭터가 생성되있는경우
    if (hero_ != nullptr)
    {
        //최초에 한번 생성되므로 잘못된 접근
        PCS::World::Notify_EnterFailedT reply;
        reply.error_code = PCS::ErrorCode::WORLD_CANNOT_ENTER_ZONE;
        PCS::Send(*this, reply);
        return;
    }

    // 캐릭터 인스턴스 생성
    auto hero = std::make_shared<Hero>(random_generator()(), this);
    hero->Init(*db_hero);
    hero_ = hero;
    // 핸들러 등록
    hero->enter_zone_signal.connect([this](Zone* zone)
    {
        BOOST_LOG_TRIVIAL(info) << "On Hero " << hero_->GetName() << "Enter Zone.";

        // 관심 지역을 만든다
        interest_area_ = std::make_shared<ClientInterestArea>(this, zone);
        interest_area_->ViewDistance(Vector3(20.0f, 1.0f, 20.0f));

    });
    hero->poistion_update_signal.connect([this](const Vector3& position)
    {
        // 관심 지역을 업데이트
        if (interest_area_)
        {
            interest_area_->Position(position);
            interest_area_->UpdateInterest();
        }
    });
    hero->exit_zone_signal.connect([this]()
    {
        BOOST_LOG_TRIVIAL(info) << "On Hero " << hero_->GetName() << "Exit Zone .";

        // 관심지역 해제
        interest_area_ = nullptr;
    });
    hero->ConnectDeathSignal(std::bind(&RemoteWorldClient::OnHeroDeath, this));

    SetState(RemoteWorldClient::State::WorldEntering);

    BOOST_LOG_TRIVIAL(info) << "World Entring . account_uid : " << GetAccount()->uid << " hero_name: " << hero->GetName();
    
    // 월드 입장
    EnterZone(hero, hero->MapId(), hero->GetPosition(), [this](bool success)
    {
        if (success)
        {
            SetState(RemoteWorldClient::State::WorldEntered);
        }
        else
        {
            this->Disconnect();
        }
    });
}

// 메시지 처리
void RemoteWorldClient::ActionMove(const PCS::World::Request_ActionMove * message)
{
    assert(message);

    auto delta_time_duration = clock_type::now() - last_position_update_time_;
    if (delta_time_duration < 50ms) { return; }

    if (!hero_) return;
    if (!hero_->IsInZone()) return;

    last_position_update_time_ = clock_type::now();

    auto* pos = message->position();
    auto* vel = message->velocity();
    auto position = Vector3(pos->x(), pos->y(), pos->z());
    auto rotation = message->rotation();
    auto velocity = Vector3(pos->x(), pos->y(), pos->z());

    GetWorld()->Dispatch([this, hero = hero_, position, rotation, velocity]() {
        hero->ActionMove(position, rotation, velocity);
    });
}

void RemoteWorldClient::ActionSkill(const PCS::World::Request_ActionSkill * message)
{
    auto delta_time_duration = clock_type::now() - last_position_update_time_;
    if (delta_time_duration < 50ms) { return; }

    if (!hero_) return;
    if (!hero_->IsInZone()) return;

    last_position_update_time_ = clock_type::now();

    auto skill_id = message->skill();
    auto rotation = message->rotation();
    auto fb_targets = message->targets();
    std::vector<uuid> targets;
    targets.reserve(fb_targets->Length());
    for (const auto& e : (*fb_targets))
    {
        targets.emplace_back(boost::uuids::string_generator()(e->c_str()));
    }

    GetWorld()->Dispatch([this, hero = hero_, skill_id, rotation, targets = std::move(targets)]() {
        hero->ActionSkill(skill_id, rotation, targets);
    });
}

void RemoteWorldClient::RespawnImmediately()
{
    // 조건 검사
    if (!(hero_ && hero_->IsDead()))
        return;

    if (respawn_timer_)
    {
        // 리스폰 타이머 취소
        respawn_timer_->cancel();
        respawn_timer_.reset();
    }

    GetWorld()->Dispatch([this, hero = hero_]() {
        Respawn(hero);
    });
}

void RemoteWorldClient::EnterGate(const PCS::World::Request_EnterGate * message)
{
    auto hero = GetHero();
    if (!hero)
    {
        PCS::World::Reply_EnterGateFailedT reply;
        reply.error_code = PCS::ErrorCode::WORLD_INVALID_HERO;
        PCS::Send(*this, reply);
        return;
    }

    auto zone = hero->GetZone();
    if (!zone)
    {
        PCS::World::Reply_EnterGateFailedT reply;
        reply.error_code = PCS::ErrorCode::WORLD_CANNOT_FIND_ZONE;
        PCS::Send(*this, reply);
        return;
    }

    auto gate = zone->GetGate(message->gate_uid());
    if (!gate)
    {
        PCS::World::Reply_EnterGateFailedT reply;
        reply.error_code = PCS::ErrorCode::WORLD_CANNOT_FIND_GATE;
        PCS::Send(*this, reply);
        return;
    }

    auto dest_gate = MapGateTable::GetInstance().Get(gate->dest_uid);
    if (!dest_gate)
    {
        PCS::World::Reply_EnterGateFailedT reply;
        reply.error_code = PCS::ErrorCode::WORLD_CANNOT_FIND_GATE;
        PCS::Send(*this, reply);
        return;
    }

    int dest_map_id = dest_gate->map_id;

    // 랜덤 이동 좌표
    std::vector<float> b{ -3.0f, -2.0f, 2.0f, 3.0f };
    std::vector<float> w{ 10, 0, 10 };
    std::piecewise_constant_distribution<float> dist{ b.begin(), b.end(), w.begin() };
    std::random_device rd;
    std::default_random_engine rng{ rd() };
    Vector3 position = dest_gate->pos + Vector3(dist(rng), 0.0f, dist(rng));

    ExitZone(hero, [this, hero, dest_map_id, position]()
    {
        EnterZone(hero, dest_map_id, position);
    });
}

void RemoteWorldClient::ExitWorld()
{
    SetState(State::Disconnected);

    if (hero_)
    {
        ExitZone(hero_);
    }
    // DB 업데이트
    UpdateToDB();
}

void RemoteWorldClient::EnterZone(const Ptr<Hero>& hero, int map_id, const Vector3& position, std::function<void(bool)> handler)
{
    GetWorld()->Dispatch([this, hero = hero, map_id, position, handler = std::move(handler)]
    {
        auto map_data = MapTable::GetInstance().Get(map_id);
        if (!map_data)
        {
            // 실패 메시지
            PCS::World::Notify_EnterFailedT reply;
            reply.error_code = PCS::ErrorCode::WORLD_CANNOT_FIND_ZONE;
            PCS::Send(*this, reply);

            if (handler)
            {
                //this->Dispatch([handler = std::move(handler), result = false](){ handler(result); });
                this->Dispatch(std::bind(handler, false));
            }
            return;
        }

        bool result = false;
        Zone* zone = nullptr;
        Vector3 pos = position;
        // 인스턴스 던전
        if (map_data->type == MapType::Dungeon)
        {
            InstanceZone* ins_zone = nullptr;
            // 캐릭터가 있었던 인던을 찾는다.
            if (!std::get<0>(hero->instance_zone_).is_nil() && std::get<1>(hero->instance_zone_) == map_id)
            {
                ins_zone = GetWorld()->FindInstanceZone(std::get<0>(hero->instance_zone_));
            }
            // 없으면 새로 만든다.
            if (ins_zone == nullptr)
            {
                ins_zone = GetWorld()->CreateInstanceZone(map_id);

                // 출구를 찾는다.
                auto iter = ins_zone->GetGates().begin();
                if (iter != ins_zone->GetGates().end())
                {
                    // 랜덤 시작 좌표
                    std::vector<float> b{ -3.0f, -2.0f, 2.0f, 3.0f };
                    std::vector<float> w{ 10, 0, 10 };
                    std::piecewise_constant_distribution<float> dist{ b.begin(), b.end(), w.begin() };
                    std::random_device rd;
                    std::default_random_engine rng{ rd() };
                    pos = iter->second->pos + Vector3(dist(rng), 0.0f, dist(rng));
                }
            }

            zone = ins_zone;
        }
        // 필드 존
        else if (map_data->type == MapType::Field)
        {
            // 필드존을 찾는다.
            zone = GetWorld()->FindFieldZone(map_id);
        }

        if (zone)
        {
            // 입장 성공 메시지를 보낸다
            fb::FlatBufferBuilder fbb;
            auto offset_msg = PCS::World::CreateNotify_EnterSuccess(fbb,
                hero->SerializeAsHero(fbb),
                zone->Serialize(fbb));
            PCS::Send(*this, fbb, offset_msg);
            result = true;
            
            // 입장
            zone->Enter(hero, pos);
        }
        else
        {
            // 실패 메시지
            PCS::World::Notify_EnterFailedT reply;
            reply.error_code = PCS::ErrorCode::WORLD_CANNOT_FIND_ZONE;
            PCS::Send(*this, reply);
            result = false;
        }

        if (handler)
        {
            //this->Dispatch([handler = std::move(handler), result](){ handler(result); });
            this->Dispatch(std::bind(handler, result));
        }
    });
}

void RemoteWorldClient::ExitZone(const Ptr<Hero>& hero, std::function<void()> handler)
{
    // 나간다.
    GetWorld()->Dispatch([this, hero = hero, handler = std::move(handler)]
    {
        Zone* zone = hero->GetZone();
        if (zone)
        {
            zone->Exit(hero);
        }

        if (handler)
        {
            this->Dispatch(std::move(handler));
        }
    });
}

void RemoteWorldClient::Respawn(const Ptr<Hero> hero)
{
    if (!hero->IsDead()) return;
    if (!hero->IsInZone()) return;

    Vector3 pos;
    Zone* zone = hero->GetZone();
    if (zone->MapType() == MapType::Field)
    {
        // 시작 지점을 찾는다.
        auto spawn_table = HeroSpawnTable::GetInstance().GetAll();
        auto iter = std::find_if(spawn_table.begin(), spawn_table.end(), [&](auto& value) {
            return (value.second.map_id == hero->MapId());
        });
        if (iter == spawn_table.end())
            return;

        db::HeroSpawn& spawn_info = iter->second;
        pos = spawn_info.pos;
    }
    else if (zone->MapType() == MapType::Dungeon)
    {
        // 출구를 찾는다.
        auto iter = zone->GetGates().begin();
        if (iter == zone->GetGates().end())
            return;

        // 랜덤 시작 좌표
        std::vector<float> b{ -3.0f, -2.0f, 2.0f, 3.0f };
        std::vector<float> w{ 10, 0, 10 };
        std::piecewise_constant_distribution<float> dist{ b.begin(), b.end(), w.begin() };
        std::random_device rd;
        std::default_random_engine rng{ rd() };
        pos = iter->second->pos + Vector3(dist(rng), 0.0f, dist(rng));
    }

    hero->Hp(hero->MaxHp()); // HP 회복
    hero->Spawn(pos);

    BOOST_LOG_TRIVIAL(info) << "Spawn Hero : " << hero->GetName();

    // 주변에 통지
    PCS::World::ActorT actor_data;
    hero->SerializeT(actor_data);
    PCS::World::Notify_UpdateT update_msg;
    update_msg.entity_id = uuids::to_string(hero->GetEntityID());
    update_msg.update_data.Set(std::move(actor_data));
    hero->PublishActorUpdate(&update_msg);

    PCS::World::StateInfoT data;
    //data.entity_id = uuids::to_string(GetEntityID());
    data.state = PCS::World::StateType::Alive;
    update_msg.entity_id = uuids::to_string(hero->GetEntityID());
    update_msg.update_data.Set(std::move(data));
    hero->PublishActorUpdate(&update_msg);
}

void RemoteWorldClient::OnHeroDeath()
{
    if (!hero_)
        return;

    BOOST_LOG_TRIVIAL(info) << "On DeathSignal. " << hero_->GetName();

    // 10초후 리스폰
    respawn_timer_ = GetWorld()->RunAfter(10s, [this, hero = hero_](auto& timer) {
        Respawn(hero);
    });
}

const Ptr<MySQLPool>& RemoteWorldClient::GetDB()
{
    return owner_->GetDB();
}
