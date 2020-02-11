#pragma once
#include <map>
#include <mutex>
#include <type_traits>
#include <chrono>
#include "Common.h"
#include "protocol_ss_generated.h"

enum ServerType : int
{
	None,
	Login_Server,
	World_Server
};

using ServerInfo = std::tuple<int, std::string, ServerType>;

class IServer;

// 매니저 서버에 접속하는 클라이언트.
// 인증 요청, 확인 및 응답을 받는다.
class ManagerClient
{
public:
	ManagerClient(const net::ClientConfig& config, IServer* owner, std::string client_name, ServerType type);
	virtual ~ManagerClient();

    int GetSessionId() { return session_id_; }
    // 클라이언트 이름.
	std::string GetName() { return name_; }
	// 접속.
	void Connect(const std::string& address, uint16_t port);
	// 종료.
	void Disconnect();
	// 종료될 때 까지 대기.
	void Wait();
    // 이 클라이언트가 실행되고 있는 IoServiceLoop 객체.
	const Ptr<net::EventLoop>& GetEventLoop() { return ev_loop_; }
    // 인증키 생성을 요청.
	void RequestGenerateCredential(int session_id, int account_uid);
    // 인증키 검증을 요청.
	void RequestVerifyCredential(int session_id, const uuid& credential);
    // 유저의 로그아웃을 통지.
	void NotifyUserLogout(int account_uid);
    // Manager 서버에 접속해 있는 다른 서버들 정보.
    const std::unordered_map<int, ServerInfo> GetServerList() { return  server_list_; }
    
    // 릴레이 메시지를 전송
    template<class T>
    void SendRelayMessage(int destination, const T& message)
    {
        if (GetSessionId() == 0) return;

        ProtocolSS::SendRelay(*net_client_, session_id_, std::vector<int>{ destination }, message);
    }
    template<class T>
    void SendRelayMessage(const std::vector<int>& destinations, const T& message)
    {
        if (GetSessionId() == 0) return;

        ProtocolSS::SendRelay(*net_client_, session_id_, destinations, message);
    }
    // 모든 서버에게 릴레이 메시지 전송
    template<class T>
    void SendRelayMessage(const T& message)
    {
        if (GetSessionId() == 0) return;

        std::vector<int> destinations;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            std::for_each(std::begin(server_list_), std::end(server_list_), [this, &destinations](auto& e)
            {
                if (std::get<0>(e.second) != GetSessionId())
                    destinations.emplace_back(std::get<0>(e.second));
            });
        }
        ProtocolSS::SendRelay(*net_client_, session_id_, destinations, message);
    }

	std::function<void(ProtocolSS::ErrorCode)> OnConnected;
	std::function<void()> OnDisconnected;
	std::function<void(const ProtocolSS::Reply_GenerateCredential*)> OnReplyGenerateCredential;
	std::function<void(const ProtocolSS::Reply_VerifyCredential*)> OnReplyVerifyCredential;
    std::function<void(const ProtocolSS::RelayMessage*)> OnRelayMessage;

private:
    // Network message handler type.
    using MessageHandler = std::function<void(const ProtocolSS::MessageRoot* message_root)>;

    void SetSessionId(int session_id) { session_id_ = session_id; }

    // 프레임 업데이트
    void DoUpdate(double delta_time) {};
    void ScheduleNextUpdate(const time_point& now, const duration& timestep);

    template <typename T, typename Handler>
    void RegisterMessageHandler(Handler&& handler)
    {
        auto key = ProtocolSS::MessageTypeTraits<T>::enum_value;
        auto func = [handler = std::forward<Handler>(handler)](const ProtocolSS::MessageRoot* message_root)
        {
            auto* message = message_root->message_as<T>();
            if (message == nullptr)
            {
                BOOST_LOG_TRIVIAL(info) << "Can not cast message_type : " << ProtocolSS::EnumNameMessageType(ProtocolSS::MessageTypeTraits<T>::enum_value);
            }
            handler(message);
        };

        message_handlers_.insert(std::pair<decltype(key), decltype(func)>(key, func));
    }

    // Handlers
    void RegisterHandlers();

    void HandleConnected(bool success);
    void HandleDisconnected();
	void HandleMessage(const uint8_t* buf, size_t bytes);

	std::mutex mutex_;

	Ptr<net::EventLoop> ev_loop_;
	Ptr<net::NetClient> net_client_;
	Ptr<timer_type> update_timer_;
	
	std::map<ProtocolSS::MessageType, MessageHandler> message_handlers_;

    IServer*	owner_;

    int         session_id_;
    std::string name_;
    ServerType	type_;

    std::unordered_map<int, ServerInfo> server_list_;
};
