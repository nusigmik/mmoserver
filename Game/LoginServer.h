#pragma once
#include <map>
#include <mutex>
#include <type_traits>
#include <chrono>
#include "Common.h"
#include "IServer.h"
#include "MySQL.h"

class ManagerClient;
class RemoteLoginClient;

// 로그인 서버.
// 계정 인증 및 생성.
// 캐릭터 생성 및 삭제.
class LoginServer : public IServer
{
public:
	LoginServer();
	virtual ~LoginServer();

    // 서버 이름
	std::string GetName() override { return name_; }
	void SetName(const std::string& name) override { name_ = name; }

	// 서버 시작
	void Run() override;

	// 서버 종료
	void Stop() override;

	// 서버가 종료될 때가지 대기
	void Wait();

    // 이 서버가 실행되고 있는 IoServiceLoop 객체.
	const Ptr<net::EventLoop>& GetEventLoop() { return ev_loop_; }

    // DB 커넥션 풀.
	const Ptr<MySQLPool>& GetDB() { return db_conn_; }

    // 리모트 클라이언트 객체를 얻는다.
	const Ptr<RemoteLoginClient> GetRemoteClient(int session_id);
	const Ptr<RemoteLoginClient> GetRemoteClientByAccountUID(int account_uid);

    // 인증된 리모트 클라이언트 객체를 얻는다.
	const Ptr<RemoteLoginClient> GetAuthedRemoteClient(int session_id);

	void NotifyUnauthedAccess(const Ptr<net::Session>& session);
private:
	// Network message handler type.
	using MessageHandler = std::function<void(const Ptr<net::Session>&, const ProtocolCS::MessageRoot* message_root)>;

    // 프레임 업데이트
    void DoUpdate(double delta_time);
    void ScheduleNextUpdate(const time_point& now, const duration& timestep);

	template <typename T, typename Handler>
	void RegisterMessageHandler(Handler&& handler)
	{
		auto key = PCS::MessageTypeTraits<T>::enum_value;
		auto func = [handler = std::forward<Handler>(handler)](const Ptr<net::Session>& session, const ProtocolCS::MessageRoot* message_root)
		{
			auto* message = message_root->message_as<T>();
            if (message == nullptr)
            {
                BOOST_LOG_TRIVIAL(info) << "Can not cast message_type : " << ProtocolCS::EnumNameMessageType(ProtocolCS::MessageTypeTraits<T>::enum_value);
            }
			handler(session, message);
		};

		message_handlers_.insert(std::pair<decltype(key), decltype(func)>(key, func));
	}

	void AddRemoteClient(int session_id, Ptr<RemoteLoginClient> remote_client);
	void RemoveRemoteClient(int session_id);
	void RemoveRemoteClient(const Ptr<RemoteLoginClient>& remote_client);
	void ProcessRemoteClientDisconnected(const Ptr<RemoteLoginClient>& rc);

	// Handlers===================================================================================================
	void RegisterHandlers();

	void HandleMessage(const Ptr<net::Session>& session, const uint8_t* buf, size_t bytes);
	void HandleSessionOpened(const Ptr<net::Session>& session);
	void HandleSessionClosed(const Ptr<net::Session>& session, net::CloseReason reason);
    // Message Handlers
	void OnJoin(const Ptr<net::Session>& session, const ProtocolCS::Login::Request_Join* message);
	void OnLogin(const Ptr<net::Session>& session, const ProtocolCS::Login::Request_Login* message);
	void OnCreateHero(const Ptr<net::Session>& session, const ProtocolCS::Login::Request_CreateHero* message);
	void OnHeroList(const Ptr<net::Session>& session, const ProtocolCS::Login::Request_HeroList* message);
	void OnDeleteHero(const Ptr<net::Session>& session, const ProtocolCS::Login::Request_DeleteHero* message);

	// ManagerClient Handlers=======================================================================================
	void RegisterManagerClientHandlers();

	std::mutex mutex_;

	Ptr<net::EventLoop> ev_loop_;
	Ptr<net::NetServer> net_server_;

	Ptr<ManagerClient> manager_client_;

	Ptr<strand> strand_;
	Ptr<timer_type> update_timer_;

	Ptr<MySQLPool> db_conn_;

	std::string name_;
	std::map<ProtocolCS::MessageType, MessageHandler> message_handlers_;
	std::map<int, Ptr<RemoteLoginClient>> remote_clients_;
};
