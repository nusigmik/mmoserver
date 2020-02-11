#pragma once

#include "Types.h"
#include "Config.h"
#include "ByteBuffer.h"
#include "EventLoop.h"
#include "AsioHelper.h"

namespace net {

	enum class NetEventType : int
	{
		Opened,
		Closed,
		//Changed,
		ConnectFailed,
	};

	class NetClient
	{
	public:
		// Network evnet handler.
		using NetEventHandler = std::function<void(const NetEventType&)>;
		// Message handler.
		using MessageHandler = std::function<void(const uint8_t*, size_t)>;

		using tcp = boost::asio::ip::tcp;
		using strand = asio::io_context::strand;

		// Create client instance.
		static Ptr<NetClient> Create(const ClientConfig& config);

		NetClient(const NetClient&) = delete;
		NetClient& operator=(const NetClient&) = delete;

		NetClient(const ClientConfig& config);
		~NetClient();

		virtual void Connect(const std::string& host, const std::string& service);
		virtual void Close();
		virtual bool IsConnected() const
		{
			return state_ == State::Connected;
		}

		// Inherited via Client
		virtual void Send(const uint8_t * data, size_t size)
		{
			auto buffer = std::make_shared<Buffer>(size);
			buffer->WriteBytes(data, size);
			Send(std::move(buffer));
		}

		virtual void Send(const Buffer & data)
		{
			Send(std::make_shared<Buffer>(data));
		}

		virtual void Send(Buffer&& data)
		{
			Send(std::make_shared<Buffer>(std::move(data)));
		}

		virtual void Send(Ptr<Buffer> message)
		{
			if (!IsConnected()) return;

			PendWrite(std::move(message));
		}

		virtual void RegisterNetEventHandler(const NetEventHandler& handler)
		{
			net_event_handler_ = handler;
		}

		virtual void RegisterMessageHandler(const MessageHandler& handler)
		{
			message_handler_ = handler;
		}

		virtual Ptr<EventLoop> GetEventLoop()
		{
			return event_loop_;
		}

		virtual strand& GetStrand()
		{
			return *strand_;
		}

	private:
		enum class State
		{
			Ready = 0,
			Connecting,
			Connected,
			Closed
		};

		struct Header
		{
			int32_t payload_len;
		};

		using MessageHandlerMap = std::map<uint16_t, MessageHandler>;

		void ConnectStart(tcp::resolver::iterator endpoint_iterator);
		void Read(size_t min_read_bytes);
		void HandleRead(const error_code & error, std::size_t bytes_transferred);

		void HandleReceiveData(Buffer& read_buf, size_t& next_read_size)
		{
			DecodeRecvData(read_buf, next_read_size);
		}

		bool PrepareRead(size_t min_prepare_bytes);
		void PendWrite(Ptr<Buffer> buf);
		void Write();
		void HandleWrite(const error_code & error);
		void HandleError(const error_code & error);
		void _Close();

		void EncodeSendData(Ptr<Buffer>& data)
		{
			// Make header
			Header header;
			header.payload_len = (int32_t)data->ReadableBytes();

			// To Do : 암호화나 압축 등..

			data->InsertBytes(data->ReaderIndex(), reinterpret_cast<uint8_t*>(&header.payload_len), 0, sizeof(header.payload_len));
			data->WriterIndex(data->WriterIndex() + sizeof(header.payload_len));
		}

		void DecodeRecvData(Buffer& buf, size_t&)
		{
			Header header = { 0 };
			// 처리할 데이터가 있으면 반복
			while (buf.IsReadable())
			{
				// Decode Header
				// 헤더 사이즈 만큼 받지 못했으면 리턴
				if (!buf.IsReadable(sizeof(Header)))
					return;

				// TO DO : 헤더 검증?
				buf.GetPOD(buf.ReaderIndex(), header.payload_len);

				// Decode Body
				// Header + Body 사이즈 만큼 받지 못했으면 리턴
				if (!buf.IsReadable(sizeof(Header) + header.payload_len))
					return;

				// 헤더 사이즈만큼 전진
				buf.SkipBytes(sizeof(Header));
				// TO DO : 복호화?

				// Call receive handler
				if (message_handler_)
					message_handler_(buf.Data() + buf.ReaderIndex(), header.payload_len);

				// 바디 사이즈만큼 전진
				buf.SkipBytes(header.payload_len);
			}
		}

		NetEventHandler net_event_handler_;
		MessageHandler message_handler_;

		std::unique_ptr<tcp::socket> socket_;
		std::unique_ptr<strand> strand_;

		Ptr<Buffer> read_buf_;
		std::vector<Ptr<Buffer>> pending_list_;
		std::vector<Ptr<Buffer>> sending_list_;

		ClientConfig	config_;
		State				state_;
		Ptr<EventLoop>	event_loop_;
		//std::mutex			mutex_;
	};

} // namespace net