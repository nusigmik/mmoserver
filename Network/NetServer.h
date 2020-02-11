#pragma once

#include <mutex>
#include <map>
#include <deque>
#include "Session.h"
#include "EventLoop.h"

namespace net {

	class NetServer : public std::enable_shared_from_this<NetServer>
	{
	public:
		enum class State
		{
			Ready = 0,
			Start,
			Stop
		};

		using tcp = asio::ip::tcp;
		using strand = asio::io_context::strand;

		// Handler to be notified on session creation.
		using SessionOpenedHandler = std::function<void(const Ptr<Session>&/*session*/)>;

		// Handler to be notified on session close. This one ignores the close reason.
		using SessionClosedHandler = std::function<void(const Ptr<Session>&/*session*/, const CloseReason&)>;

		// Receive handler type.
		using MessageHandler = std::function<void(const Ptr<Session>&/*session*/, const uint8_t*, size_t)>;

		// Create server instance.
		static Ptr<NetServer> Create(const ServerConfig& config);

		NetServer(const NetServer&) = delete;
		NetServer& operator=(const NetServer&) = delete;

		NetServer(const ServerConfig& config);
		virtual ~NetServer();

		virtual void Start(uint16_t port);
		virtual void Start(std::string address, uint16_t port);
		virtual void Stop();

		State GetState()
		{
			return state_;
		}

		Ptr<EventLoop> GetEventLoop()
		{
			return event_loop_;
		}

		Ptr<Session> GetSession(int session_id)
		{
			std::lock_guard<std::mutex> lock_guard(mutex_);
			auto iter = sessions_.find(session_id);
			return (iter != sessions_.end()) ? iter->second : nullptr;
		}

		virtual void RegisterSessionOpenedHandler(const SessionOpenedHandler& handler)
		{
			session_opened_handler_ = handler;
		}

		virtual void RegisterSessionClosedHandler(const SessionClosedHandler& handler)
		{
			session_closed_handler_ = handler;
		}

		virtual void RegisterMessageHandler(const MessageHandler& handler)
		{
			message_handler_ = handler;
		}

	private:

		void Listen(tcp::endpoint endpoint);
		void AcceptStart();

		// Session Handler.
		void HandleSessionOpen(const Ptr<Session>& session);
		void HandleSessionClose(const Ptr<Session>& session, CloseReason reason);
		void HandleSessionReceive(const Ptr<Session>& session, const uint8_t* buf, size_t bytes);

		// handler
		SessionOpenedHandler	session_opened_handler_;
		SessionClosedHandler	session_closed_handler_;
		MessageHandler			message_handler_;

		ServerConfig			config_;
		State					state_;
		Ptr<EventLoop>			event_loop_;
		std::mutex				mutex_;
		std::deque<int>					free_session_id_;
		std::map<int, Ptr<Session>>	sessions_;

		std::unique_ptr<tcp::acceptor> acceptor_;
		// bool accept_op_ = false;
	};

} // namespace net