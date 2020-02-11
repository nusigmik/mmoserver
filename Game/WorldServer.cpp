#include "stdafx.h"
#include "WorldServer.h"
#include "RemoteWorldClient.h"
#include "ManagerClient.h"
#include "Settings.h"
#include "DBSchema.h"
#include "CachedResources.h"
#include "protocol_ss_helper.h"
#include "protocol_cs_helper.h"
#include "World.h"
#include "Hero.h"

namespace fb = flatbuffers;
namespace db = db_schema;
namespace PSS = ProtocolSS;
namespace PCS = ProtocolCS;

WorldServer::WorldServer()
{
}

WorldServer::~WorldServer()
{
    Stop();
}

void WorldServer::Run()
{
    Settings& settings = Settings::GetInstance();

    SetName(settings.name);

    // Create io_service loop
    size_t thread_count = settings.thread_count;
    ev_loop_ = std::make_shared<net::EventLoop>(thread_count);
    // NetServer Config
    net::ServerConfig server_config;
    server_config.event_loop = ev_loop_;
    server_config.max_session_count = settings.max_session_count;
    server_config.max_receive_buffer_size = settings.max_receive_buffer_size;
    server_config.min_receive_size = settings.min_receive_size;
    server_config.no_delay = settings.no_delay;
    // Create NetServer
    net_server_ = net::NetServer::Create(server_config);
    net_server_->RegisterSessionOpenedHandler([this](auto& session) { HandleSessionOpened(session); });
    net_server_->RegisterSessionClosedHandler([this](auto& session, auto reason) { HandleSessionClosed(session, reason); });
    net_server_->RegisterMessageHandler([this](auto& session, auto* buf, auto bytes) { HandleMessage(session, buf, bytes); });

    // �޽��� �ڵ鷯 ���
    RegisterHandlers();

    // Create DB connection Pool
    db_conn_ = MySQLPool::Create(
        settings.db_host,
        settings.db_user,
        settings.db_password,
        settings.db_schema,
        settings.db_connection_pool);

    // �ʿ��� ������ �ε�.
    LoadResources();

    // ���� ������ ó���ϴ� World ����.
    world_ = std::make_shared<World>(ev_loop_);
    world_->Start();

    // Frame Update ����.
    strand_ = std::make_shared<strand>(ev_loop_->GetIoContext());
    update_timer_ = std::make_shared<timer_type>(ev_loop_->GetIoContext());
    ScheduleNextUpdate(clock_type::now(), TIME_STEP);

    // NetServer �� ���۽�Ų��.
    std::string bind_address = settings.bind_address;
    uint16_t bind_port = settings.bind_port;
    net_server_->Start(bind_address, bind_port);

    // Manager ������ ���� �ϴ� Ŭ���̾�Ʈ.
    net::ClientConfig client_config;
    client_config.event_loop = ev_loop_;
    client_config.min_receive_size = settings.min_receive_size;
    client_config.max_receive_buffer_size = settings.max_receive_buffer_size;
    client_config.no_delay = settings.no_delay;
    manager_client_ = std::make_shared<ManagerClient>(client_config, this,GetName(), ServerType::World_Server);
    // Ŭ���̾�Ʈ �� �޽��� �ڵ鷯 ���.
    RegisterManagerClientHandlers();
    // Manager ������ ���� ����.
    manager_client_->Connect(settings.manager_address, settings.manager_port);

    BOOST_LOG_TRIVIAL(info) << "Run " << GetName();
}

void WorldServer::Stop()
{
    // ���� �۾�.
    net_server_->Stop();
    ev_loop_->Stop();

    BOOST_LOG_TRIVIAL(info) << "Stop " << GetName();
}

// ������ ����� ������ ���
void WorldServer::Wait()
{
    if (ev_loop_)
        ev_loop_->Wait();
}

const Ptr<RemoteWorldClient> WorldServer::GetRemoteClient(int session_id)
{
    std::lock_guard<std::mutex> lock_guard(mutex_);
    auto iter = remote_clients_.find(session_id);

    return iter != remote_clients_.end() ? iter->second : nullptr;
}

const Ptr<RemoteWorldClient> WorldServer::GetRemoteClientByAccountUID(int account_uid)
{
    std::lock_guard<std::mutex> lock_guard(mutex_);
    auto iter = std::find_if(remote_clients_.begin(), remote_clients_.end(), [account_uid](auto& var)
    {
        return (var.second->GetAccount() && var.second->GetAccount()->uid == account_uid);
    });

    return iter != remote_clients_.end() ? iter->second : nullptr;
}

const Ptr<RemoteWorldClient> WorldServer::GetAuthedRemoteClient(int session_id)
{
    std::lock_guard<std::mutex> lock_guard(mutex_);
    auto iter = remote_clients_.find(session_id);

    return ((iter != remote_clients_.end()) && iter->second->IsAuthenticated()) ? iter->second : nullptr;
}

void WorldServer::AddRemoteClient(int session_id, Ptr<RemoteWorldClient> remote_client)
{
    std::lock_guard<std::mutex> lock_guard(mutex_);
    remote_clients_.emplace(session_id, remote_client);
}

void WorldServer::RemoveRemoteClient(int session_id)
{
    std::lock_guard<std::mutex> lock_guard(mutex_);
    remote_clients_.erase(session_id);
}

void WorldServer::RemoveRemoteClient(const Ptr<RemoteWorldClient>& remote_client)
{
    std::lock_guard<std::mutex> lock_guard(mutex_);
    remote_clients_.erase(remote_client->GetSessionID());
}

void WorldServer::ProcessRemoteClientDisconnected(const Ptr<RemoteWorldClient>& rc)
{
    // ��Ͽ��� ����
    RemoveRemoteClient(rc);
    rc->OnDisconnected();

    if (rc->IsAuthenticated() && rc->GetAccount())
    {
        // Manager ������ �α׾ƿ��� �˸���.
        manager_client_->NotifyUserLogout(rc->GetAccount()->uid);
        BOOST_LOG_TRIVIAL(info) << "Logout. account_uid: " << rc->GetAccount()->uid << " user_name: " << rc->GetAccount()->user_name;
    }
}

void WorldServer::ScheduleNextUpdate(const time_point& now, const duration& timestep)
{
    auto update_time = now + timestep;
    update_timer_->expires_at(update_time);
    update_timer_->async_wait(strand_->wrap([this, start_time = now, timestep](auto& error)
    {
        if (error) return;

        auto now = clock_type::now();
        double delta_time = double_seconds(now - start_time).count();
        ScheduleNextUpdate(now, timestep);
        DoUpdate(delta_time);
    }));
}

void WorldServer::NotifyUnauthedAccess(const Ptr<net::Session>& session)
{
    fb::FlatBufferBuilder fbb;
    auto notify = PCS::CreateNotify_UnauthedAccess(fbb);
    auto root = PCS::CreateMessageRoot(fbb, PCS::MessageType::Notify_UnauthedAccess, notify.Union());
    FinishMessageRootBuffer(fbb, root);

    session->Send(fbb.GetBufferPointer(), fbb.GetSize());
}

void WorldServer::LoadResources()
{
    HeroAttributeTable::Load(db_conn_);
    MapTable::Load(db_conn_);
    MapGateTable::Load(db_conn_);
    MonsterTable::Load(db_conn_);
    MonsterSpawnTable::Load(db_conn_);
    SkillTable::Load(db_conn_);
    HeroSpawnTable::Load(db_conn_);
}

// ������ ������Ʈ
void WorldServer::DoUpdate(double delta_time)
{
    GetWorld()->Dispatch([this, delta_time]() { GetWorld()->DoUpdate(delta_time); });
}

void WorldServer::HandleMessage(const Ptr<net::Session>& session, const uint8_t* buf, size_t bytes)
{
    // flatbuffer �޽����� ��ø��������
    const auto* message_root = PCS::GetMessageRoot(buf);
    if (message_root == nullptr)
    {
        BOOST_LOG_TRIVIAL(info) << "Invalid MessageRoot";
        return;
    }

    auto message_type = message_root->message_type();
    //BOOST_LOG_TRIVIAL(debug) << "On Recv message_type : " << PCS::EnumNameMessageType(message_type);

    auto iter = message_handlers_.find(message_type);
    if (iter == message_handlers_.end())
    {
        BOOST_LOG_TRIVIAL(info) << "Can not find the message handler. message_type : " << PCS::EnumNameMessageType(message_type);
        return;
    }

    try
    {
        // �޽��� �ڵ鷯�� ����
        iter->second(session, message_root);
    }
    catch (sql::SQLException& e)
    {
        BOOST_LOG_TRIVIAL(warning) << "SQL Exception: " << e.what()
            << ", (MySQL error code : " << e.getErrorCode()
            << ", SQLState: " << e.getSQLState() << " )";
    }
    catch (std::exception& e)
    {
        BOOST_LOG_TRIVIAL(warning) << "Exception: " << e.what();
    }
}

void WorldServer::HandleSessionOpened(const Ptr<net::Session>& session)
{
    BOOST_LOG_TRIVIAL(info) << "Connect session id : " << session->GetID();
}

void WorldServer::HandleSessionClosed(const Ptr<net::Session>& session, net::CloseReason reason)
{
    auto remote_client = GetRemoteClient(session->GetID());
    if (!remote_client)
        return;

    BOOST_LOG_TRIVIAL(info) << "Disconnected session id : " << session->GetID();
    ProcessRemoteClientDisconnected(remote_client);
}

// Login ================================================================================================================
void WorldServer::OnLogin(const Ptr<net::Session>& session, const PCS::World::Request_Login* message)
{
    if (message == nullptr) return;

    auto rc = GetRemoteClient(session->GetID());
    if (!rc)
    {
        // RemoteWorldClient ���� �� �߰�.
        rc = std::make_shared<RemoteWorldClient>(session, this);
        AddRemoteClient(session->GetID(), rc);
    }

    // ������ ���°� �ƴϸ� ����
    if (rc->GetState() != RemoteWorldClient::State::Connected) return;

    // �����Ϸ��� �ɸ��� uid �ӽ� ����.
    rc->selected_hero_uid_ = message->hero_uid();

    // Manager ������ ����Ű ���� ��û.
    manager_client_->RequestVerifyCredential(session->GetID(), boost::uuids::string_generator()(message->credential()->c_str()));
}

void WorldServer::OnLoadFinish(const Ptr<net::Session>& session, const PCS::World::Notify_LoadFinish * message)
{
    if (message == nullptr) return;

    auto rc = GetAuthedRemoteClient(session->GetID());
    if (!rc)
    {
        NotifyUnauthedAccess(session);
        return;
    }

    rc->EnterWorld();
}

void WorldServer::OnActionMove(const Ptr<net::Session>& session, const PCS::World::Request_ActionMove * message)
{
    if (message == nullptr) return;

    auto rc = GetAuthedRemoteClient(session->GetID());
    if (!rc)
    {
        NotifyUnauthedAccess(session);
        return;
    }
    // ���� ���°� �ƴϸ� ����
    if (rc->GetState() != RemoteWorldClient::State::WorldEntered)
    {
        return;
    }

    rc->ActionMove(message);
}

void WorldServer::OnActionSkill(const Ptr<net::Session>& session, const PCS::World::Request_ActionSkill * message)
{
    if (message == nullptr) return;

    auto rc = GetAuthedRemoteClient(session->GetID());
    if (!rc)
    {
        NotifyUnauthedAccess(session);
        return;
    }
    // ���� ���°� �ƴϸ� ����
    if (rc->GetState() != RemoteWorldClient::State::WorldEntered)
    {
        return;
    }

    rc->ActionSkill(message);
}

void WorldServer::OnRespawn(const Ptr<net::Session>& session, const PCS::World::Request_Respawn * message)
{
    if (message == nullptr) return;

    auto rc = GetAuthedRemoteClient(session->GetID());
    if (!rc)
    {
        NotifyUnauthedAccess(session);
        return;
    }
    // ���� ���°� �ƴϸ� ����
    if (rc->GetState() != RemoteWorldClient::State::WorldEntered)
    {
        return;
    }

    rc->RespawnImmediately();
}

void WorldServer::OnEnterGate(const Ptr<net::Session>& session, const PCS::World::Request_EnterGate * message)
{
    if (message == nullptr) return;

    auto rc = GetAuthedRemoteClient(session->GetID());
    if (!rc)
    {
        NotifyUnauthedAccess(session);
        return;
    }
    // ���� ���°� �ƴϸ� ����
    if (rc->GetState() != RemoteWorldClient::State::WorldEntered)
    {
        return;
    }

    rc->EnterGate(message);
}

void WorldServer::RegisterHandlers()
{
    RegisterMessageHandler<PCS::World::Request_Login>([this](auto& session, auto* msg) { OnLogin(session, msg); });
    RegisterMessageHandler<PCS::World::Notify_LoadFinish>([this](auto& session, auto* msg) { OnLoadFinish(session, msg); });
    RegisterMessageHandler<PCS::World::Request_ActionMove>([this](auto& session, auto* msg) { OnActionMove(session, msg); });
    RegisterMessageHandler<PCS::World::Request_ActionSkill>([this](auto& session, auto* msg) { OnActionSkill(session, msg); });
    RegisterMessageHandler<PCS::World::Request_Respawn>([this](auto& session, auto* msg) { OnRespawn(session, msg); });
    RegisterMessageHandler<PCS::World::Request_EnterGate>([this](auto& session, auto* msg) { OnEnterGate(session, msg); });
}

void WorldServer::RegisterManagerClientHandlers()
{
    manager_client_->OnConnected = [this](PSS::ErrorCode ec) {
        if (PSS::ErrorCode::OK == ec)
        {
            BOOST_LOG_TRIVIAL(info) << "Connection the Manager Server is successful.";
        }
        else
        {
            // Manager ������ ������ �����ϸ� �����Ѵ�.
            BOOST_LOG_TRIVIAL(info) << "Connection the Manager Server is failed!";
            Stop();
        }
    };

    manager_client_->OnDisconnected = [this]() {
        //  Manager ������ ������ ���� ���� ���� �Ѵ�.
        BOOST_LOG_TRIVIAL(info) << "Manager Server is disconnected.";
        Stop();
    };

    manager_client_->OnRelayMessage = [this](const PSS::RelayMessage* message) {
        // Test Code
        /*auto* msg = message->message_as<PSS::TestMessage>();
        if (msg != nullptr)
        {
            std::cout << msg->str_msg()->c_str() << std::endl;
        }*/
    };

    manager_client_->OnReplyVerifyCredential = [this](const PSS::Reply_VerifyCredential* message)
    {    
        auto error_code = message->error_code();
        int session_id = message->session_id();
        int account_uid = message->account_uid();
        uuid credential = boost::uuids::string_generator()(message->credential()->c_str());

        auto rc = GetRemoteClient(session_id);
        if (!rc) return;

        rc->Dispatch([=]() {
            // ����
            if (error_code != PSS::ErrorCode::OK || credential.is_nil())
            {
                PCS::World::Reply_LoginFailedT reply_msg;
                reply_msg.error_code = PCS::ErrorCode::WORLD_LOGIN_INVALID_CREDENTIAL;
                PCS::Send(*rc, reply_msg);
                return;
            }

            rc->Authenticate(credential);

            // ������ ���°� �ƴ�
            if (rc->GetState() != RemoteWorldClient::State::Connected)
            {
                PCS::World::Reply_LoginFailedT reply_msg;
                reply_msg.error_code = PCS::ErrorCode::WORLD_LOGIN_INVALID_STATE;
                PCS::Send(*rc, reply_msg);
                return;
            }

            // ���� ������ �ҷ��´�.
            auto db_account = db::Account::Get(db_conn_, account_uid);
            // ������ ����.
            if (!db_account)
            {
                PCS::Login::Reply_LoginFailedT reply;
                reply.error_code = PCS::ErrorCode::WORLD_LOGIN_INVALID_ACCOUNT;
                PCS::Send(*rc, reply);
                return;
            }
            rc->SetAccount(db_account);

            // ĳ���� ������ �ҷ��´�.
            auto db_hero = db::Hero::Get(GetDB(), rc->selected_hero_uid_, rc->GetAccount()->uid);
            // ĳ���� �ε� ����
            if (!db_hero)
            {
                PCS::Login::Reply_LoginFailedT reply;
                reply.error_code = PCS::ErrorCode::WORLD_CANNOT_LOAD_HERO;
                PCS::Send(*rc, reply);
                return;
            }
            rc->SetDBHero(db_hero);
            
            BOOST_LOG_TRIVIAL(info) << "World Login Success. user_name: " << rc->GetAccount()->user_name;

            // �ɸ��� ������ �����Ѵ�.
            fb::FlatBufferBuilder fbb;
            auto hero_offset = db_hero->Serialize(fbb);
            auto reply_offset = PCS::World::CreateReply_LoginSuccess(fbb, hero_offset);
            PCS::Send(*rc, fbb, reply_offset);
        });
    };
}
