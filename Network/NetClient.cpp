#include "NetClient.h"

namespace net {

Ptr<NetClient> NetClient::Create(const ClientConfig& config)
{
	return std::make_shared<NetClient>(config);
}

NetClient::NetClient(const ClientConfig& config)
	: config_(config)
	, state_(State::Ready)
{
	event_loop_ = config_.event_loop;

	// 설정된 event_loop 이 없으면 생성.
	if (event_loop_ == nullptr)
	{
		size_t thread_count = 1;
		event_loop_ = std::make_shared<EventLoop>(thread_count);
	}

	socket_ = std::make_unique<tcp::socket>(event_loop_->GetIoContext());
	strand_ = std::make_unique<strand>(event_loop_->GetIoContext());
}

NetClient::~NetClient()
{
	Close();
}

void NetClient::Connect(const std::string & host, const std::string & service)
{
	asio::dispatch(*strand_, [this, host, service]
	{
		if (state_ != State::Ready)
		{
			BOOST_LOG_TRIVIAL(info) << "Can not connect: state is not ready";
			return;
		}

		tcp::resolver resolver(event_loop_->GetIoContext());
		auto endpoint_iterator = resolver.resolve({ host, service });

		ConnectStart(endpoint_iterator);
		state_ = State::Connecting;
	});
}

void NetClient::Close()
{
	asio::dispatch(*strand_, [this]()
	{
		_Close();
	});
}

void NetClient::ConnectStart(tcp::resolver::iterator endpoint_iterator)
{
	tcp::resolver::iterator end;
	boost::asio::async_connect(*socket_, endpoint_iterator, end, asio::bind_executor(*strand_,
		[this](const error_code& error, tcp::resolver::iterator i)
	{
		if (state_ == State::Closed)
		{
			if (!error)
			{
				error_code ec;
				socket_->shutdown(tcp::socket::shutdown_both, ec);
				socket_->close();
			}
			socket_.release();
			return;
		}

		if (!error)
		{
			// Start
			// socket option
			socket_->set_option(tcp::no_delay(config_.no_delay));
			// read 버퍼 생성
			size_t initial_capacity = config_.min_receive_size;
			size_t max_capacity = config_.max_receive_buffer_size;
			read_buf_ = std::make_shared<Buffer>(initial_capacity, max_capacity);
			state_ = State::Connected;

			// Read
			Read(config_.min_receive_size);
			if (net_event_handler_) net_event_handler_(NetEventType::Opened);
		}
		else
		{
			HandleError(error);
			_Close();
			if (net_event_handler_) net_event_handler_(NetEventType::ConnectFailed);
		}
	}));
}

inline void NetClient::Read(size_t min_read_bytes)
{
	if (!IsConnected())
		return;

	if (!PrepareRead(min_read_bytes))
	{
		_Close();
		if (net_event_handler_) net_event_handler_(NetEventType::Closed);
		return;
	}

	auto asio_buf = mutable_buffer(*read_buf_);
	socket_->async_read_some(boost::asio::buffer(asio_buf), asio::bind_executor(*strand_,
		[this](const error_code& error, std::size_t bytes_transferred)
	{
		HandleRead(error, bytes_transferred);
	}));
}

inline void NetClient::HandleRead(const error_code & error, std::size_t bytes_transferred)
{
	if (!IsConnected())
		return;

	if (error)
	{
		HandleError(error);
		_Close();
		if (net_event_handler_) net_event_handler_(NetEventType::Closed);
		return;
	}

	// 버퍼의 WriterIndex 을 전송받은 데이터의 크기만큼 전진
	read_buf_->WriterIndex(read_buf_->WriterIndex() + bytes_transferred);

	size_t next_read_size = 0;
	// 받은 데이타 처리
	HandleReceiveData(*read_buf_, next_read_size);

	size_t prepare_size = std::max<size_t>((size_t)next_read_size, config_.min_receive_size);
	Read(prepare_size);
}

inline bool NetClient::PrepareRead(size_t min_prepare_bytes)
{
	// 읽을게 없으면 포지션을 리셋해준다.
	if (read_buf_->ReadableBytes() == 0)
	{
		read_buf_->Clear();
	}

	try
	{
		if (read_buf_->WritableBytes() < min_prepare_bytes)
		{
			read_buf_->DiscardReadBytes();
		}
		read_buf_->EnsureWritable(min_prepare_bytes);
	}
	catch (const std::exception& e)
	{
		BOOST_LOG_TRIVIAL(warning) << "Prepare read exception: " << e.what();
		read_buf_->Clear();
		return false;
	}

	return true;
}

inline void NetClient::PendWrite(Ptr<Buffer> buf)
{
	asio::dispatch(*strand_, [this, buf = std::move(buf)]() mutable
	{
		if (!IsConnected())
			return;

		pending_list_.emplace_back(std::move(buf));
		if (sending_list_.empty())
		{
			Write();
		}
	});
}

inline void NetClient::Write()
{
	assert(sending_list_.empty());

	if (!IsConnected())
		return;

	if (pending_list_.empty())
		return;

	// TO DO : sending_list_.swap(pending_list_);

	// Scatter-Gather I/O
	std::vector<asio::const_buffer> bufs;
	for (auto& buffer : pending_list_)
	{
		EncodeSendData(buffer);
		bufs.emplace_back(const_buffer(*buffer));
		sending_list_.emplace_back(std::move(buffer));
	}
	pending_list_.clear();

	boost::asio::async_write(*socket_, bufs, asio::bind_executor(*strand_,
		[this](error_code const& ec, std::size_t)
		{
			HandleWrite(ec);
		}));
}

inline void NetClient::HandleWrite(const error_code & error)
{
	if (!IsConnected())
		return;

	if (error)
	{
		HandleError(error);
		_Close();
		if (net_event_handler_) net_event_handler_(NetEventType::Closed);
		return;
	}

	sending_list_.clear();
	if (!pending_list_.empty())
	{
		Write();
	}
}

inline void NetClient::HandleError(const error_code & error)
{
	// 상대방이 연결을 끊은것이다.
	if (error == boost::asio::error::eof || error == boost::asio::error::operation_aborted)
	{
		return;
	}

	BOOST_LOG_TRIVIAL(info) << "Socket error: " << error.message();
}

void NetClient::_Close()
{
	if (state_ == State::Closed)
		return;

	boost::system::error_code ec;
	socket_->shutdown(tcp::socket::shutdown_both, ec);
	socket_->close();
	state_ = State::Closed;
}

} // namespace net