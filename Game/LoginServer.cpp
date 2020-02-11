#include "stdafx.h"
#include "LoginServer.h"
#include "RemoteLoginClient.h"
#include "ManagerClient.h"
#include "Settings.h"
#include "DBSchema.h"
#include "CachedResources.h"
#include "protocol_ss_helper.h"
#include "protocol_cs_helper.h"

namespace fb = flatbuffers;
namespace db = db_schema;
namespace PSS = ProtocolSS;
namespace PCS = ProtocolCS;

LoginServer::LoginServer()
{
}

LoginServer::~LoginServer()
{
	Stop();
}

void LoginServer::Run()
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

	// 핸들러 등록
	RegisterHandlers();

	// Create DB connection Pool
	db_conn_ = MySQLPool::Create(
		settings.db_host,
		settings.db_user,
		settings.db_password,
		settings.db_schema,
		settings.db_connection_pool);

	// 필요한 데이터 로딩.
	HeroAttributeTable::Load(db_conn_);

	// Frame Update 시작.
	strand_ = std::make_shared<strand>(ev_loop_->GetIoContext());
	update_timer_ = std::make_shared<timer_type>(ev_loop_->GetIoContext());
	ScheduleNextUpdate(clock_type::now(), TIME_STEP);

	// NetServer 를 시작시킨다.
	std::string bind_address = settings.bind_address;
	uint16_t bind_port = settings.bind_port;
	net_server_->Start(bind_address, bind_port);

	// Manager 서버에 연결 하는 클라이언트.
	net::ClientConfig client_config;
	client_config.event_loop = ev_loop_;
	client_config.min_receive_size = settings.min_receive_size;
	client_config.max_receive_buffer_size = settings.max_receive_buffer_size;
	client_config.no_delay = settings.no_delay;
	manager_client_ = std::make_shared<ManagerClient>(client_config, this, GetName(), ServerType::Login_Server);
	// 클라이언트 에 메시지 핸들러 등록.
	RegisterManagerClientHandlers();
	// Manager 서버에 연결 시작.
	manager_client_->Connect(settings.manager_address, settings.manager_port);

	BOOST_LOG_TRIVIAL(info) << "Run " << GetName();
}

void LoginServer::Stop()
{
	// 종료 작업.
	net_server_->Stop();
	ev_loop_->Stop();

	BOOST_LOG_TRIVIAL(info) << "Stop " << GetName();
}

// 서버가 종료될 때가지 대기
void LoginServer::Wait()
{
	if (ev_loop_)
		ev_loop_->Wait();
}

const Ptr<RemoteLoginClient> LoginServer::GetRemoteClient(int session_id)
{
	std::lock_guard<std::mutex> lock_guard(mutex_);
	auto iter = remote_clients_.find(session_id);

	return iter != remote_clients_.end() ? iter->second : nullptr;
}

const Ptr<RemoteLoginClient> LoginServer::GetRemoteClientByAccountUID(int account_uid)
{
	std::lock_guard<std::mutex> lock_guard(mutex_);
	auto iter = std::find_if(remote_clients_.begin(), remote_clients_.end(), [account_uid](auto& var)
	{
		return (var.second->GetAccount() && var.second->GetAccount()->uid == account_uid);
	});

	return iter != remote_clients_.end() ? iter->second : nullptr;
}

const Ptr<RemoteLoginClient> LoginServer::GetAuthedRemoteClient(int session_id)
{
	std::lock_guard<std::mutex> lock_guard(mutex_);
	auto iter = remote_clients_.find(session_id);

	return ((iter != remote_clients_.end()) && iter->second->IsAuthenticated()) ? iter->second : nullptr;
}

void LoginServer::AddRemoteClient(int session_id, Ptr<RemoteLoginClient> remote_client)
{
	std::lock_guard<std::mutex> lock_guard(mutex_);
	remote_clients_.emplace(session_id, remote_client);
}

void LoginServer::RemoveRemoteClient(int session_id)
{
	std::lock_guard<std::mutex> lock_guard(mutex_);
	remote_clients_.erase(session_id);
}

void LoginServer::RemoveRemoteClient(const Ptr<RemoteLoginClient>& remote_client)
{
	std::lock_guard<std::mutex> lock_guard(mutex_);
	remote_clients_.erase(remote_client->GetSessionID());
}

void LoginServer::ProcessRemoteClientDisconnected(const Ptr<RemoteLoginClient>& rc)
{
	// 목록에서 제거
	RemoveRemoteClient(rc);
	rc->OnDisconnected();

    if (rc->IsAuthenticated() && rc->GetAccount())
    {
        // Manager 서버로 로그아웃을 알린다.
        manager_client_->NotifyUserLogout(rc->GetAccount()->uid);
        BOOST_LOG_TRIVIAL(info) << "Logout. account_uid: " << rc->GetAccount()->uid << " user_name: " << rc->GetAccount()->user_name;
    }
}

// 프레임 업데이트
inline void LoginServer::DoUpdate(double delta_time)
{
}

void LoginServer::ScheduleNextUpdate(const time_point& now, const duration& timestep)
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

void LoginServer::NotifyUnauthedAccess(const Ptr<net::Session>& session)
{
	fb::FlatBufferBuilder fbb;
	auto notify = PCS::CreateNotify_UnauthedAccess(fbb);
	auto root = PCS::CreateMessageRoot(fbb, PCS::MessageType::Notify_UnauthedAccess, notify.Union());
	FinishMessageRootBuffer(fbb, root);

	session->Send(fbb.GetBufferPointer(), fbb.GetSize());
}

void LoginServer::HandleMessage(const Ptr<net::Session>& session, const uint8_t* buf, size_t bytes)
{
	// flatbuffer 메시지로 디시리얼라이즈
	const auto* message_root = PCS::GetMessageRoot(buf);
	if (message_root == nullptr)
	{
		BOOST_LOG_TRIVIAL(info) << "Invalid MessageRoot";
		return;
	}

	auto message_type = message_root->message_type();
    //BOOST_LOG_TRIVIAL(info) << "On Recv message_type : " << PCS::EnumNameMessageType(message_type);

	auto iter = message_handlers_.find(message_type);
	if (iter == message_handlers_.end())
	{
		BOOST_LOG_TRIVIAL(info) << "Can not find the message handler. message_type : " << PCS::EnumNameMessageType(message_type);
		return;
	}

	try
	{
		// 메시지 핸들러를 실행
		iter->second(session, message_root);
	}
	catch (sql::SQLException& e)
	{
		BOOST_LOG_TRIVIAL(info) << "SQL Exception: " << e.what()
			<< ", (MySQL error code : " << e.getErrorCode()
			<< ", SQLState: " << e.getSQLState() << " )";
	}
	catch (std::exception& e)
	{
		BOOST_LOG_TRIVIAL(info) << "Exception: " << e.what();
	}
}

void LoginServer::HandleSessionOpened(const Ptr<net::Session>& session)
{
    BOOST_LOG_TRIVIAL(info) << "Connect session id : " << session->GetID();
}

void LoginServer::HandleSessionClosed(const Ptr<net::Session>& session, net::CloseReason reason)
{
	auto remote_client = GetRemoteClient(session->GetID());
	if (!remote_client)
		return;

    BOOST_LOG_TRIVIAL(info) << "Disconnected session id : " << session->GetID();
	ProcessRemoteClientDisconnected(remote_client);
}

// Join ================================================================================================================
void LoginServer::OnJoin(const Ptr<net::Session>& session, const PCS::Login::Request_Join * message)
{
	if (message == nullptr) return;

	const char* user_name = message->user_name()->c_str();
	const char* password = message->password()->c_str();

	// 문자열 검사
	std::regex pattern(R"([^A-Za-z0-9_]+)");
	std::cmatch m;
	if (std::regex_search(user_name, m, pattern))
	{
		PCS::Login::Reply_JoinFailedT reply;
		reply.error_code = PCS::ErrorCode::INVALID_STRING;
		PCS::Send(*session, reply);
		return;
	}

	if (db::Account::Get(db_conn_, user_name))
	{
		PCS::Login::Reply_JoinFailedT reply;
		reply.error_code = PCS::ErrorCode::JOIN_ACC_NAME_ALREADY;
		PCS::Send(*session, reply);
		return;
	}

	if (!db::Account::Create(db_conn_, user_name, password))
	{
		PCS::Login::Reply_JoinFailedT reply;
		reply.error_code = PCS::ErrorCode::JOIN_CANNOT_ACC_CREATE;
		PCS::Send(*session, reply);
		return;
	}

	BOOST_LOG_TRIVIAL(info) << "Join : " << user_name;

	PCS::Login::Reply_JoinSuccessT reply;
	PCS::Send(*session, reply);
}

// Login ================================================================================================================
void LoginServer::OnLogin(const Ptr<net::Session>& session, const PCS::Login::Request_Login* message)
{
	if (message == nullptr) return;

	const std::string user_name = message->user_name()->str();
	const std::string password = message->password()->str();

	auto db_account = db::Account::Get(db_conn_, user_name);
	// 계정이 없다.
	if (!db_account)
	{
		PCS::Login::Reply_LoginFailedT reply;
		reply.error_code = PCS::ErrorCode::LOGIN_INCORRECT_ACC_NAME;
		PCS::Send(*session, reply);
		return;
	}
	// 비번이 다르다.
	if (db_account->password != password)
	{
		PCS::Login::Reply_LoginFailedT reply;
		reply.error_code = PCS::ErrorCode::LOGIN_INCORRECT_ACC_PASSWORD;
		PCS::Send(*session, reply);
		return;
	}

	// TO DO : 분산 서버에서 중복 로그인 처리가?
	//// 중복 로그인 처리.
	//if (auto rc = GetRemoteClientByAccountID(db_account->id))
	//{
	//	PCS::Login::Reply_LoginFailedT reply;
	//	reply.error_code = PCS::ErrorCode::LOGIN_DUPLICATION;

	//	// RemoteClient 접속을 끊어준다.
	//	rc->Send(reply);
	//	rc->Disconnect();
	//	// 종료처리를 바로 해준다.
	//	ProcessRemoteClientDisconnected(rc);

	//	// 같은 세션이 아니었으면 신규 세션도 접속을 끊어준다.
	//	if (rc->GetSession() != session)
	//	{
	//		PCS::Send(session, reply);
	//		session->Close();
	//	}
	//	return;
	//}

	auto rc = GetRemoteClient(session->GetID());
	if (!rc)
	{
		// RemoteLoginClient 생성 및 추가.
		rc = std::make_shared<RemoteLoginClient>(session, db_account);
		AddRemoteClient(session->GetID(), rc);
	}

	BOOST_LOG_TRIVIAL(info) << "Login. account_uid: " << rc->GetAccount()->uid << " user_name: " << rc->GetAccount()->user_name;

	// Manager 서버에 인증키 요청.
	manager_client_->RequestGenerateCredential(session->GetID(), db_account->uid);
}

// Create Hero ================================================================================================================
void LoginServer::OnCreateHero(const Ptr<net::Session>& session, const PCS::Login::Request_CreateHero* message)
{
	if (message == nullptr) return;

	// 로그인 체크
	auto rc = GetAuthedRemoteClient(session->GetID());
	if (!rc)
	{
		NotifyUnauthedAccess(session);
		return;
	}

	const char* name = message->name()->c_str();
	const ClassType class_type = (ClassType)message->class_type();

	// 문자열 검사
	std::regex pattern(R"([^A-Za-z0-9_]+)");
	std::cmatch m;
	if (std::regex_search(name, m, pattern))
	{
		PCS::Login::Reply_CreateHeroFailedT reply;
		reply.error_code = PCS::ErrorCode::INVALID_STRING;
		PCS::Send(*session, reply);
		return;
	}

	if (db::Hero::Get(db_conn_, name))
	{
		// 이미 있는 이름.
		PCS::Login::Reply_CreateHeroFailedT reply;
		reply.error_code = PCS::ErrorCode::CREATE_HERO_NAME_ALREADY;
		PCS::Send(*session, reply);
		return;
	}

    // 렙1짜리 생성
	const int level = 1;
	auto db_hero = db::Hero::Create(db_conn_, rc->GetAccount()->uid, name, class_type, level);
	if (!db_hero)
	{
		// 생성 된게 없다.
		PCS::Login::Reply_CreateHeroFailedT reply;
		reply.error_code = PCS::ErrorCode::CREATE_HERO_CANNOT_CREATE;
		PCS::Send(*session, reply);
		return;
	}

	BOOST_LOG_TRIVIAL(info) << "Create Hero : " << db_hero->name;

	auto hero = std::make_unique<PCS::Login::HeroT>();
	hero->uid = db_hero->uid;
	hero->name = db_hero->name;
	hero->class_type = (PCS::ClassType)db_hero->class_type;
	hero->level = db_hero->level;

	PCS::Login::Reply_CreateHeroSuccessT reply;
	reply.hero = std::move(hero);
	PCS::Send(*session, reply);
}

// Hero List ================================================================================================================
void LoginServer::OnHeroList(const Ptr<net::Session>& session, const PCS::Login::Request_HeroList* message)
{
	if (message == nullptr) return;

	// 로그인 체크
	auto rc = GetAuthedRemoteClient(session->GetID());
	if (!rc)
	{
		NotifyUnauthedAccess(session);
		return;
	}

	auto db_char_list = db::Hero::GetList(db_conn_, rc->GetAccount()->uid);

	PCS::Login::Reply_HeroListT reply;
	for (auto& var : db_char_list)
	{
		auto hero = std::make_unique<PCS::Login::HeroT>();
		hero->uid = var->uid;
		hero->name = var->name;
		hero->class_type = (PCS::ClassType)var->class_type;
		hero->level = var->level;
		reply.list.emplace_back(std::move(hero));
	}

	PCS::Send(*session, reply);
}

// Delete Hero ==========================================================================================================
void LoginServer::OnDeleteHero(const Ptr<net::Session>& session, const PCS::Login::Request_DeleteHero* message)
{
	if (message == nullptr) return;

	// 로그인 체크
	auto rc = GetAuthedRemoteClient(session->GetID());
	if (!rc)
	{
		NotifyUnauthedAccess(session);
		return;
	}

	const int hero_uid = message->hero_uid();

	auto db_hero = db::Hero::Get(db_conn_, hero_uid, rc->GetAccount()->uid);
	if (!db_hero)
	{
		PCS::Login::Reply_DeleteHeroFailedT reply;
		reply.error_code = PCS::ErrorCode::DELETE_HERO_NOT_EXIST;
		PCS::Send(*session, reply);
		return;
	}

	// 삭제
	db_hero->Delete(GetDB());

	BOOST_LOG_TRIVIAL(info) << "Delete Hero : " << db_hero->name;

	PCS::Login::Reply_DeleteHeroSuccessT reply;
	reply.hero_uid = hero_uid;
	PCS::Send(*session, reply);
}

void LoginServer::RegisterHandlers()
{
	RegisterMessageHandler<PCS::Login::Request_Join>([this](auto& session, auto* msg) { OnJoin(session, msg); });
	RegisterMessageHandler<PCS::Login::Request_Login>([this](auto& session, auto* msg) { OnLogin(session, msg); });
	RegisterMessageHandler<PCS::Login::Request_CreateHero>([this](auto& session, auto* msg) { OnCreateHero(session, msg); });
	RegisterMessageHandler<PCS::Login::Request_HeroList>([this](auto& session, auto* msg) { OnHeroList(session, msg); });
	RegisterMessageHandler<PCS::Login::Request_DeleteHero>([this](auto& session, auto* msg) { OnDeleteHero(session, msg); });
}

void LoginServer::RegisterManagerClientHandlers()
{
	manager_client_->OnConnected = [this](PSS::ErrorCode ec) {
		if (PSS::ErrorCode::OK == ec)
		{
            BOOST_LOG_TRIVIAL(info) << "Connection the Manager Server is successful.";
		}
		else
		{
			// Manager 서버와 연결이 실패하면 종료한다.
			BOOST_LOG_TRIVIAL(info) << "Connection the Manager Server is failed!";
			Stop();
		}
	};

	manager_client_->OnDisconnected = [this]() {
		//  Manager 서버와 연결이 끊어 지면 종료 한다.
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

	manager_client_->OnReplyGenerateCredential = [this](const PSS::Reply_GenerateCredential* message)
    {
        int session_id = message->session_id();
        uuid credential = boost::uuids::string_generator()(message->credential()->c_str());
		
        auto rc = GetRemoteClient(session_id);
        if (!rc)
        {
            return;
        }

		rc->Dispatch([=]()
        {
			rc->Authenticate(credential);

			BOOST_LOG_TRIVIAL(info) << "Authenticate. account_uid: " << rc->GetAccount()->uid << " user_name: " << rc->GetAccount()->user_name;

			PCS::Login::Reply_LoginSuccessT reply_msg;
			reply_msg.credential = boost::uuids::to_string(credential);
			PCS::Send(*rc, reply_msg);
		});
	};
}
