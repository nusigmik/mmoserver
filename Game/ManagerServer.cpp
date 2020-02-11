#include "stdafx.h"
#include "ManagerServer.h"
#include "Settings.h"
#include "DBSchema.h"
#include "protocol_ss_helper.h"

namespace fb = flatbuffers;
namespace db = db_schema;
namespace PSS = ProtocolSS;

ManagerServer::ManagerServer()
{
}

ManagerServer::~ManagerServer()
{
	Stop();
}

void ManagerServer::Run()
{
	Settings& settings = Settings::GetInstance();

	SetName(settings.name);

	// Create io_service loop
	size_t thread_count = settings.thread_count;
	ev_loop_ = std::make_shared<net::EventLoop>(thread_count);
	
	// NetServer Config
	net::ServerConfig net_config;
	net_config.event_loop = ev_loop_;
	net_config.max_session_count = settings.max_session_count;
	net_config.max_receive_buffer_size = settings.max_receive_buffer_size;
	net_config.min_receive_size = settings.min_receive_size;
	net_config.no_delay = settings.no_delay;
	// Create NetServer
	net_server_ = net::NetServer::Create(net_config);
	net_server_->RegisterSessionOpenedHandler([this](auto& session) { HandleSessionOpened(session); });
	net_server_->RegisterSessionClosedHandler([this](auto& session, auto reason) { HandleSessionClosed(session, reason); });
	net_server_->RegisterMessageHandler([this](auto& session, auto buf, auto bytes) { HandleMessage(session, buf, bytes); });
	
	// 메시지 핸들러 등록
	RegisterHandlers();

	// Create DB connection Pool
	db_conn_ = MySQLPool::Create(
		settings.db_host,
		settings.db_user,
		settings.db_password,
		settings.db_schema,
		settings.db_connection_pool);

	// Frame Update 시작.
	strand_ = std::make_shared<strand>(ev_loop_->GetIoContext());
    update_timer_ = std::make_shared<timer_type>(ev_loop_->GetIoContext());
	ScheduleNextUpdate(clock_type::now(), TIME_STEP);

	// NetServer 를 시작시킨다.
	std::string bind_address = settings.bind_address;
	uint16_t bind_port = settings.bind_port;
	net_server_->Start(bind_address, bind_port);

	BOOST_LOG_TRIVIAL(info) << "Run " << GetName();
}

void ManagerServer::Stop()
{
	// 종료 작업.
    if (ev_loop_)
        ev_loop_->Stop();

	BOOST_LOG_TRIVIAL(info) << "Stop " << GetName();
}

// 서버가 종료될 때가지 대기
void ManagerServer::Wait()
{
	if (ev_loop_)
		ev_loop_->Wait();
}

const Ptr<RemoteManagerClient> ManagerServer::GetRemoteClient(int session_id)
{
	auto iter = remote_clients_.find(session_id);

	return iter == remote_clients_.end() ? nullptr : iter->second;
}

const Ptr<RemoteManagerClient> ManagerServer::GetRemoteClientByName(const std::string & name)
{
	auto iter = std::find_if(remote_clients_.begin(), remote_clients_.end(), [name](auto& var)
	{
		return (var.second->GetServerName() == name);
	});

	return iter == remote_clients_.end() ? nullptr : iter->second;
}


// 프레임 업데이트
void ManagerServer::DoUpdate(double delta_time)
{
	// 삭제 유예 시간.
	constexpr duration wait_time(60000ms);

	std::lock_guard<std::mutex> lock_guard(mutex_);

	// 로그아웃후 시간이 지나면 지운다.
	// TO DO : 무식하게 전체를 돌려버림. 정렬하는 방법이 있을듯.
	auto iter = user_session_set_.begin();
	for (; iter != user_session_set_.end(); )
	{
		if (!(iter->IsLogin()) && iter->logout_time_ + wait_time < clock_type::now())
		{
			BOOST_LOG_TRIVIAL(info) << "Delete user session. account_uid : " << iter->account_uid_ << " credential: " << iter->credential_;
			iter = user_session_set_.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

void ManagerServer::AddRemoteClient(int session_id, Ptr<RemoteManagerClient> remote_client)
{
	remote_clients_.emplace(session_id, remote_client);
}

void ManagerServer::RemoveRemoteClient(int session_id)
{
	remote_clients_.erase(session_id);
}

void ManagerServer::RemoveRemoteClient(const Ptr<RemoteManagerClient>& remote_client)
{
	remote_clients_.erase(remote_client->GetSessionID());
}

void ManagerServer::ProcessRemoteClientDisconnected(const Ptr<RemoteManagerClient>& rc)
{
	// 목록에서 제거
	RemoveRemoteClient(rc);
	rc->OnDisconnected();

	BOOST_LOG_TRIVIAL(info) << "Logout. Server name: " << rc->GetServerName();
    
    // 모든 클라이언트에게 서버 리스트를 통지
    for (auto& e : remote_clients_)
    {
        NotifyServerList(e.second->GetSession());
    }
}

void ManagerServer::ScheduleNextUpdate(const time_point& now, const duration& timestep)
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

void ManagerServer::NotifyUnauthedAccess(const Ptr<net::Session>& session)
{
	fb::FlatBufferBuilder fbb;
	auto notify = PSS::CreateNotify_UnauthedAccess(fbb);
	auto msg_root = PSS::CreateMessageRoot(fbb, PSS::MessageType::Notify_UnauthedAccess, notify.Union());
	PSS::FinishMessageRootBuffer(fbb, msg_root);

	session->Send(fbb.GetBufferPointer(), fbb.GetSize());
}

void ManagerServer::NotifyServerList(const Ptr<net::Session>& session)
{
    fb::FlatBufferBuilder fbb;
    std::vector<fb::Offset<PSS::ServerInfo>> vec_server_info;
    for (auto& e : remote_clients_)
    {
        auto client = e.second;
        auto server_info = PSS::CreateServerInfoDirect(fbb, client->GetSessionID(), client->GetServerName().c_str(), client->GetServerType());
        vec_server_info.emplace_back(server_info);
    }
    
    auto notify = PSS::CreateNotify_ServerListDirect(fbb, &vec_server_info);
    PSS::Send(*session, fbb, notify);
}

void ManagerServer::HandleMessage(const Ptr<net::Session>& session, const uint8_t* buf, size_t bytes)
{
	// flatbuffer 메시지로 디시리얼라이즈
	const auto* message_root = PSS::GetMessageRoot(buf);
	if (message_root == nullptr)
	{
		BOOST_LOG_TRIVIAL(info) << "Invalid MessageRoot";
		return;
	}

	auto message_type = message_root->message_type();
    //BOOST_LOG_TRIVIAL(debug) << "On Recv message_type : " << PSS::EnumNameMessageType(message_type);

	auto iter = message_handlers_.find(message_type);
	if (iter == message_handlers_.end())
	{
		BOOST_LOG_TRIVIAL(info) << "Can not find the message handler. message_type : " << PSS::EnumNameMessageType(message_type);
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

void ManagerServer::HandleSessionOpened(const Ptr<net::Session>& session)
{

}

void ManagerServer::HandleSessionClosed(const Ptr<net::Session>& session, net::CloseReason reason)
{
	std::lock_guard<std::mutex> lock_guard(mutex_);
	auto remote_client = GetRemoteClient(session->GetID());
	if (!remote_client)
		return;

	ProcessRemoteClientDisconnected(remote_client);
}

void ManagerServer::OnRelayMessage(const Ptr<net::Session>& session, const PSS::RelayMessage * message)
{
    fb::FlatBufferBuilder fbb;
    auto relay_offset = PSS::RelayMessage::Pack(fbb, message->UnPack());
    auto offset_root = PSS::CreateMessageRoot(fbb, PSS::MessageTypeTraits<PSS::RelayMessage>::enum_value, relay_offset.Union());
    FinishMessageRootBuffer(fbb, offset_root);

    auto* destinations_id = message->destinations_id();
    for (size_t i = 0; i < destinations_id->Length(); i++)
    {
        int id = destinations_id->Get(i);
        auto dest_client = GetRemoteClient(id);
        if (dest_client)
        {
            dest_client->Send(fbb.GetBufferPointer(), fbb.GetSize());
        }
    }
}

// Login ================================================================================================================
void ManagerServer::OnLogin(const Ptr<net::Session>& session, const PSS::Request_Login* message)
{
	if (message == nullptr) return;

	std::lock_guard<std::mutex> lock_guard(mutex_);
	const std::string name = message->client_name()->str();
	const ServerType type = (ServerType)message->client_type();

	// 이미 인증됐거나 같은 이름이 있으면 거절
	if( GetRemoteClient(session->GetID()) || GetRemoteClientByName(name))
	{
		PSS::Reply_LoginT reply;
		reply.error_code = PSS::ErrorCode::LOGIN_ALREADY_CONNECTED;
		PSS::Send(*session, reply);
		return;
	}

	// 로그인 성공. RemoteClient 생성 및 추가.
	auto new_rc = std::make_shared<RemoteManagerClient>(session, name, type);
	AddRemoteClient(new_rc->GetSessionID(), new_rc);

	BOOST_LOG_TRIVIAL(info) << "Login. Server name: " << new_rc->GetServerName();

	PSS::Reply_LoginT reply;
	reply.error_code = PSS::ErrorCode::OK;
    reply.session_id = session->GetID();
	PSS::Send(*session, reply);

    // 모든 클라이언트에게 서버 리스트를 통지
    for (auto& e : remote_clients_)
    {
        NotifyServerList(e.second->GetSession());
    }
}

void ManagerServer::OnGenerateCredential(const Ptr<net::Session>& session, const PSS::Request_GenerateCredential * message)
{
	if (message == nullptr) return;

	std::lock_guard<std::mutex> lock_guard(mutex_);
	auto rc = GetRemoteClient(session->GetID());
	if (!rc)
	{
		NotifyUnauthedAccess(session);
		return;
	}

	const int account_uid = message->account_uid();
	const uuid new_credential = boost::uuids::random_generator()();

	auto& indexer = user_session_set_.get<tags::account_uid>();
	auto iter = indexer.find(account_uid);
	if (iter != indexer.end())
	{
		// credential 을 새로 발급 하고 로그인 상태로 돌린다.
		indexer.modify(iter, [&new_credential, &rc](UserSession& var) { var.credential_ = new_credential; var.login_servers_[rc->GetServerName()] = true; });
	}
	else
	{
		// 새 삽입
        UserSession user(account_uid, new_credential);
        user.login_servers_[rc->GetServerName()] = true;
		user_session_set_.emplace(std::move(user));
	}
	// 다시 찾는다.
	iter = indexer.find(account_uid);
	if (iter == indexer.end())
		return;

	BOOST_LOG_TRIVIAL(info) << "Generate credential. account_uid: " << iter->account_uid_ << " credential: " << iter->credential_;

	PSS::Reply_GenerateCredentialT reply;
	reply.session_id = message->session_id();
	reply.credential = boost::uuids::to_string(new_credential);
	PSS::Send(*session, reply);
}

void ManagerServer::OnVerifyCredential(const Ptr<net::Session>& session, const PSS::Request_VerifyCredential * message)
{
	if (message == nullptr) return;

	std::lock_guard<std::mutex> lock_guard(mutex_);
	auto rc = GetRemoteClient(session->GetID());
	if (!rc)
	{
		NotifyUnauthedAccess(session);
		return;
	}

    std::string str_credential = message->credential()->c_str();
	const uuid credential = boost::uuids::string_generator()(str_credential);
	auto error_code = PSS::ErrorCode::OK;

	auto& indexer = user_session_set_.get<tags::credential>();
	auto iter = indexer.find(credential);
	if (iter == indexer.end())
	{
        BOOST_LOG_TRIVIAL(info) << "Failed to verify credential : " << credential;

		// 없으면 실패
		PSS::Reply_VerifyCredentialT reply;
        reply.error_code = PSS::ErrorCode::VERIFY_CREDENTIAL_FAILED;
		reply.session_id = message->session_id();
        reply.credential = str_credential;
        reply.account_uid = iter->account_uid_;
		PSS::Send(*session, reply);
		return;
	}

	// 다시 로그인 상태로 돌린다.
	indexer.modify(iter, [&rc](UserSession& user) { user.login_servers_[rc->GetServerName()] = true; });

	BOOST_LOG_TRIVIAL(info) << "Verify credential. account_uid: " << iter->account_uid_ << " credential: " << iter->credential_;

	PSS::Reply_VerifyCredentialT reply;
	reply.error_code = PSS::ErrorCode::OK;
	reply.session_id = message->session_id();
	reply.credential = str_credential;
	reply.account_uid = iter->account_uid_;
	PSS::Send(*session, reply);
}

void ManagerServer::OnUserLogout(const Ptr<net::Session>& session, const PSS::Notify_UserLogout * message)
{
	if (message == nullptr) return;

	std::lock_guard<std::mutex> lock_guard(mutex_);
	auto rc = GetRemoteClient(session->GetID());
	if (!rc)
	{
		NotifyUnauthedAccess(session);
		return;
	}

	// 바로 지우지 않고 로그아웃 상태로 변경.
	const int account_uid = message->account_uid();
	auto& indexer = user_session_set_.get<tags::account_uid>();
	auto iter = indexer.find(account_uid);
	if (iter != indexer.end())
	{
		indexer.modify(iter, [&rc](UserSession& user) { user.login_servers_[rc->GetServerName()] = false; user.logout_time_ = clock_type::now(); });
		BOOST_LOG_TRIVIAL(info) << "User logout. account_uid: " << iter->account_uid_ << " credential: " << iter->credential_;
	}
}

void ManagerServer::RegisterHandlers()
{
    RegisterMessageHandler<PSS::RelayMessage>([this](auto& session, auto* msg) { OnRelayMessage(session, msg); });
	RegisterMessageHandler<PSS::Request_Login>([this](auto& session, auto* msg) { OnLogin(session, msg); });
	RegisterMessageHandler<PSS::Request_GenerateCredential>([this](auto& session, auto* msg) { OnGenerateCredential(session, msg); });
	RegisterMessageHandler<PSS::Request_VerifyCredential>([this](auto& session, auto* msg) { OnVerifyCredential(session, msg); });
	RegisterMessageHandler<PSS::Notify_UserLogout>([this](auto& session, auto* msg) { OnUserLogout(session, msg); });
}