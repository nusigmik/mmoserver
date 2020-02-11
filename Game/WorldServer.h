#pragma once
#include <map>
#include <mutex>
#include <type_traits>
#include <chrono>
#include "Common.h"
#include "IServer.h"
#include "MySQL.h"

class ManagerClient;
class RemoteWorldClient;
class World;

// ���� ���� ����.
// ������ ������ �÷��� �Ǵ� ����.
class WorldServer : public IServer
{
public:
	WorldServer();
	virtual ~WorldServer();

    // ���� �̸�
	std::string GetName() override { return name_; }
	void SetName(const std::string& name) override { name_ = name; }

	// ���� ����
	void Run() override;

	// ���� ����
	void Stop() override;

	// ������ ����� ������ ���
	void Wait();

    // �� ������ ����ǰ� �ִ� EventLoop ��ü.
	const Ptr<net::EventLoop>& GetEventLoop() { return ev_loop_; }

    // DB Ŀ�ؼ� Ǯ.
	const Ptr<MySQLPool>& GetDB() { return db_conn_; }

    // ����Ʈ Ŭ���̾�Ʈ ��ü�� ��´�.
	const Ptr<RemoteWorldClient> GetRemoteClient(int session_id);
	const Ptr<RemoteWorldClient> GetRemoteClientByAccountUID(int account_uid);

    // ������ ����Ʈ Ŭ���̾�Ʈ ��ü�� ��´�.
	const Ptr<RemoteWorldClient> GetAuthedRemoteClient(int session_id);

    // ������ �ùķ��̼� �Ǵ� ��ü
    World* GetWorld() { return world_.get(); }

	void NotifyUnauthedAccess(const Ptr<net::Session>& session);

private:
	// Network message handler type.
	using MessageHandler = std::function<void(const Ptr<net::Session>&, const ProtocolCS::MessageRoot* message_root)>;
    
    // ������ ������Ʈ
    void DoUpdate(double delta_time);
    void ScheduleNextUpdate(const time_point& now, const duration& timestep);

    // ���ҽ� �ε�
    void LoadResources();

	template <typename T, typename Handler>
	void RegisterMessageHandler(Handler&& handler)
	{
		auto key = ProtocolCS::MessageTypeTraits<T>::enum_value;
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

	void AddRemoteClient(int session_id, Ptr<RemoteWorldClient> remote_client);
	void RemoveRemoteClient(int session_id);
	void RemoveRemoteClient(const Ptr<RemoteWorldClient>& remote_client);
	void ProcessRemoteClientDisconnected(const Ptr<RemoteWorldClient>& rc);
	
    // Handlers===================================================================================================
    void RegisterHandlers();
    
    void HandleMessage(const Ptr<net::Session>& session, const uint8_t* buf, size_t bytes);
	void HandleSessionOpened(const Ptr<net::Session>& session);
	void HandleSessionClosed(const Ptr<net::Session>& session, net::CloseReason reason);   
    // Message Handlers
	void OnLogin(const Ptr<net::Session>& session, const ProtocolCS::World::Request_Login* message);
    void OnLoadFinish(const Ptr<net::Session>& session, const ProtocolCS::World::Notify_LoadFinish* message);
    void OnActionMove(const Ptr<net::Session>& session, const ProtocolCS::World::Request_ActionMove* message);
	void OnActionSkill(const Ptr<net::Session>& session, const ProtocolCS::World::Request_ActionSkill* message);
    void OnRespawn(const Ptr<net::Session>& session, const ProtocolCS::World::Request_Respawn* message);
    void OnEnterGate(const Ptr<net::Session>& session, const ProtocolCS::World::Request_EnterGate* message);

	// ManagerClient Handlers=======================================================================================
	void RegisterManagerClientHandlers();

	std::mutex mutex_;

	Ptr<net::EventLoop> ev_loop_;
	Ptr<net::NetServer> net_server_;
	Ptr<MySQLPool> db_conn_;
	Ptr<ManagerClient> manager_client_;

	Ptr<strand> strand_;
	Ptr<timer_type> update_timer_;

	std::string name_;
	std::map<ProtocolCS::MessageType, MessageHandler> message_handlers_;
	std::map<int, Ptr<RemoteWorldClient>> remote_clients_;

	Ptr<World> world_;
};
