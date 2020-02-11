#include "stdafx.h"
#include "ManagerClient.h"
#include "protocol_ss_helper.h"

namespace PSS = ProtocolSS;

ManagerClient::ManagerClient(const net::ClientConfig& config, IServer* owner, std::string client_name, ServerType type)
	: owner_(owner)
	, session_id_(0)
	, name_(client_name)
    , type_(type)
{
	net_client_ = net::NetClient::Create(config);
	ev_loop_ = net_client_->GetEventLoop();
}

ManagerClient::~ManagerClient()
{
	Disconnect();
}

void ManagerClient::Connect(const std::string& address, uint16_t port)
{
	net_client_->RegisterNetEventHandler([this](const net::NetEventType& et) {
		if (net::NetEventType::Opened == et)
		{
			HandleConnected(true);
		}
		else if (net::NetEventType::ConnectFailed == et)
		{
			HandleConnected(false);
		}
		else if (net::NetEventType::Closed == et)
		{
			HandleDisconnected();
		}
	});
	net_client_->RegisterMessageHandler([this](auto* buf, auto bytes)
	{
		HandleMessage(buf, bytes);
	});

	// 메시지 핸들러 등록.
	RegisterHandlers();

	// Frame Update 시작.
	auto& strand = net_client_->GetStrand();
	update_timer_ = std::make_shared<timer_type>(ev_loop_->GetIoContext());
	ScheduleNextUpdate(clock_type::now(), TIME_STEP);

	// 접속 시작.
	net_client_->Connect(address, std::to_string(port));
	
	BOOST_LOG_TRIVIAL(info) << "ManagerClient Start : " << GetName();
}

void ManagerClient::Disconnect()
{
	net_client_->Close();
}

void ManagerClient::Wait()
{
	ev_loop_->Wait();
}

void ManagerClient::RequestGenerateCredential(int session_id, int account_uid)
{
    if (GetSessionId() == 0) return;

    PSS::Request_GenerateCredentialT req_msg;
	req_msg.session_id = session_id;
	req_msg.account_uid = account_uid;

	PSS::Send(*net_client_, req_msg);
}

void ManagerClient::RequestVerifyCredential(int session_id, const uuid & credential)
{
    if (GetSessionId() == 0) return;

    PSS::Request_VerifyCredentialT req_msg;
	req_msg.session_id = session_id;
	req_msg.credential = boost::uuids::to_string(credential);

	PSS::Send(*net_client_, req_msg);
}

void ManagerClient::NotifyUserLogout(int account_uid)
{
    if (GetSessionId() == 0) return;

    PSS::Notify_UserLogoutT req_msg;
	req_msg.account_uid = account_uid;

	PSS::Send(*net_client_, req_msg);
}

void ManagerClient::HandleConnected(bool success)
{
	if (success)
	{
		// Manager 연결 성공. 로그인 시도.
        PSS::Request_LoginT msg;
		msg.client_name = GetName();
		msg.client_type = (int)type_;
		
		PSS::Send(*net_client_, msg);
	}
	else
	{
		// Manager 연결 실패.
        if (OnConnected)
    		OnConnected(PSS::ErrorCode::LOGIN_CONNECTION_FAILED);
	}
}

void ManagerClient::HandleDisconnected()
{
	// Manager 연결이 끊어짐.
    if (OnDisconnected)
    	OnDisconnected();
}

void ManagerClient::HandleMessage(const uint8_t * buf, size_t bytes)
{
	// flatbuffer 메시지로 디시리얼라이즈
	const auto* message_root = PSS::GetMessageRoot(buf);
	if (message_root == nullptr)
	{
		BOOST_LOG_TRIVIAL(info) << "Invalid MessageRoot";
		return;
	}

	auto message_type = message_root->message_type();
	auto iter = message_handlers_.find(message_type);
	if (iter == message_handlers_.end())
	{
		BOOST_LOG_TRIVIAL(info) << "Can not find the message handler. message_type : " << PSS::EnumNameMessageType(message_type);
		return;
	}

	try
	{
		// 메시지 핸들러를 실행
		iter->second(message_root);
	}
	catch (std::exception& e)
	{
		BOOST_LOG_TRIVIAL(info) << "Exception: " << e.what();
	}
}

void ManagerClient::ScheduleNextUpdate(const time_point & now, const duration & timestep)
{
	auto& strand = net_client_->GetStrand();
	auto update_time = now + timestep;
	update_timer_->expires_at(update_time);
	update_timer_->async_wait(strand.wrap([this, start_time = now, timestep](auto& error)
	{
		if (error) return;

		auto now = clock_type::now();
		double delta_time = double_seconds(now - start_time).count();
		ScheduleNextUpdate(now, timestep);
		DoUpdate(delta_time);
	}));
}

void ManagerClient::RegisterHandlers()
{
    RegisterMessageHandler<PSS::RelayMessage>([this](const PSS::RelayMessage* message)
    {
        if (OnRelayMessage)
            OnRelayMessage(message);
    });
    RegisterMessageHandler<PSS::Reply_Login>([this](const PSS::Reply_Login* message)
    {
        SetSessionId(message->session_id());

        if (OnConnected)
            OnConnected(message->error_code());
    });
    RegisterMessageHandler<PSS::Reply_GenerateCredential>([this](const PSS::Reply_GenerateCredential* message)
    {
        if (OnReplyGenerateCredential)
            OnReplyGenerateCredential(message);
    });
    RegisterMessageHandler<PSS::Reply_VerifyCredential>([this](const PSS::Reply_VerifyCredential* message)
    {
        if (OnReplyVerifyCredential)
            OnReplyVerifyCredential(message);
    });
    RegisterMessageHandler<PSS::Notify_ServerList>([this](const PSS::Notify_ServerList* message)
    {
        std::unordered_map<int, ServerInfo> server_list;
        auto list = message->server_list();
        for (size_t i = 0; i < list->Length(); i++)
        {
            auto* server_info = list->Get(i);
            server_list.emplace(server_info->session_id(), ServerInfo{ server_info->session_id(), std::string(server_info->name()->c_str()), (ServerType)server_info->type() });
#ifdef _DEBUG
            auto& value = server_list[server_info->session_id()];
            BOOST_LOG_TRIVIAL(debug) << "Server info -> session id:" << std::get<0>(value) << " server name:" << std::get<1>(value) << " server type:" << std::get<2>(value) << "\n";
#endif // _DEBUG
        }

        std::lock_guard<std::mutex> guard(mutex_);
        server_list_.swap(server_list);
    });
}