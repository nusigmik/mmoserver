#include "NetServer.h"

namespace net {

Ptr<NetServer> NetServer::Create(const ServerConfig& config)
{
	return std::make_shared<NetServer>(config);
}

NetServer::NetServer(const ServerConfig & config)
	: config_(config)
	, state_(State::Ready)
{
	// 설정된 event_loop 없으면 생성.
	if (config_.event_loop == nullptr)
	{
		size_t thread_count = std::max<size_t>(config_.thread_count, 1);
		event_loop_ = std::make_shared<EventLoop>(thread_count);
	}
	else
	{
		event_loop_ = config_.event_loop;
	}
	
	// Free session id list 를 만든다. 
	for (int i = 1; i <= config_.max_session_count; i++)
	{
		free_session_id_.push_back(i);
	}
}

NetServer::~NetServer()
{
	Stop();
}

void NetServer::Start(uint16_t port)
{
	Start("0.0.0.0", port);
}

void NetServer::Start(std::string address, uint16_t port)
{
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(address), port);

	if (!(state_ == State::Ready))
	{
		BOOST_LOG_TRIVIAL(info) << "The tcp server can't start: state is not Ready";
		return;
	}

	Listen(endpoint);
	AcceptStart();
	state_ = State::Start;

	BOOST_LOG_TRIVIAL(info) << "The tcp server started: " << endpoint;
}

void NetServer::Stop()
{
	if (!(state_ == State::Start))
		return;

	acceptor_->close();
	// 모든 세션을 닫는다.
	for (auto& pair : sessions_)
	{
		pair.second->Close();
	}
	sessions_.clear();
	free_session_id_.clear();

	state_ = State::Stop;

	BOOST_LOG_TRIVIAL(info) << "The tcp server stopped";
}

void NetServer::Listen(tcp::endpoint endpoint)
{
	auto acceptor = std::make_unique<tcp::acceptor>(event_loop_->GetIoContext());

	acceptor->open(endpoint.protocol());
	acceptor->set_option(tcp::acceptor::reuse_address(true));
	acceptor->bind(endpoint);
	acceptor->listen(tcp::acceptor::max_connections);

	acceptor_ = std::move(acceptor);
}

inline void NetServer::AcceptStart()
{
	int id = 0;
	{
		std::lock_guard<std::mutex> guard(mutex_);
		if (free_session_id_.empty())
			return;

		// id 발급
		id = free_session_id_.front();
		free_session_id_.pop_front();
		// 세션 객체 생성.
	}

	// Create Session
	auto session = std::make_shared<Session>(event_loop_->GetIoContext(), id, config_);

	// Async accept
	acceptor_->async_accept(session->GetSocket(), [this, self = shared_from_this(), session](error_code error) mutable
	{
		if (state_ == State::Stop)
		{
			session->Close();
			return;
		}

		if (!error)
		{
			{
				std::lock_guard<std::mutex> guard(mutex_);
				// 세션 리스트에 추가.
				sessions_.emplace(session->GetID(), session);
			}
			// 핸들러 등록
			session->open_handler = [this](auto& s) {
				HandleSessionOpen(s);
			};
			session->close_handler = [this](auto& s, CloseReason reason) {
				HandleSessionClose(s, reason);
			};
			session->recv_handler = [this](auto& s, const uint8_t* buf, size_t bytes) {
				HandleSessionReceive(s, buf, bytes);
			};

			// 세션 시작
			session->Start();
		}
		else
		{
			{
				std::lock_guard<std::mutex> guard(mutex_);
				// id 를 돌려줌
				free_session_id_.push_back(session->GetID());
			}
			BOOST_LOG_TRIVIAL(info) << "The tcp server accept error: " << error.message();
		}

		// 새로운 접속을 받는다
		AcceptStart();
	});

	//accept_op_ = true;
}

inline void NetServer::HandleSessionOpen(const Ptr<Session>& session)
{
	if (session_opened_handler_)
		session_opened_handler_(session);
}

inline void NetServer::HandleSessionClose(const Ptr<Session>& session, CloseReason reason)
{
	if (session_closed_handler_)
		session_closed_handler_(session, reason);

	auto id = session->GetID();
	// 세션 리스트에서 지워준다.
	{
		std::lock_guard<std::mutex> guard(mutex_);
		sessions_.erase(id);
		// id 를 돌려줌
		free_session_id_.push_back(id);
	}

	AcceptStart();
}

inline void NetServer::HandleSessionReceive(const Ptr<Session>& session, const uint8_t* buf, size_t bytes)
{
	if (message_handler_)
		message_handler_(session, buf, bytes);
}

} // namespace net