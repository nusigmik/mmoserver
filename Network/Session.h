#pragma once

#include "Types.h"
#include "ByteBuffer.h"
#include "Config.h"
#include "AsioHelper.h"

namespace net {

	enum class CloseReason {
		ActiveClose = 0,	// 서버에서 연결을 끊음.
		Disconnected,		// 클라이언트에서 연결이 끊김.
		Timeout				//
	};

	class Session : public std::enable_shared_from_this<Session>
	{
	public:
		DECLARE_CLASS_PTR(Session)

		using tcp = boost::asio::ip::tcp;
		using strand = asio::io_context::strand;

		Session(const Session&) = delete;
		Session& operator=(const Session&) = delete;

		Session(asio::io_context& io_context, int id, const ServerConfig& config);

		virtual ~Session();

		int GetID() const;

		bool GetRemoteEndpoint(std::string& ip, uint16_t& port) const;

		bool IsOpen() const;

		// 세션을 시작한다.
		virtual void Start();

		virtual void Close();

		virtual void Send(const uint8_t * data, size_t size)
		{
			auto buffer = std::make_shared<Buffer>(size);
			buffer->WriteBytes(data, size);

			Send(std::move(buffer));
		}

		virtual void Send(const Buffer& data)
		{
			Send(std::make_shared<Buffer>(data));
		}

		virtual void Send(Buffer&& data)
		{
			Send(std::make_shared<Buffer>(std::move(data)));
		}

		virtual void Send(Ptr<Buffer> data)
		{
			if (!IsOpen()) return;

			PendWrite(data);
		}

		tcp::socket& GetSocket() { return *socket_; }

		strand& GetStrand() { return *strand_; }

		std::function<void(const Ptr<Session>&)> open_handler;
		std::function<void(const Ptr<Session>&, CloseReason reason)> close_handler;
		std::function<void(const Ptr<Session>&, uint8_t*, size_t)> recv_handler;

	private:
		enum class State
		{
			Ready,
			Opened,
			Closed
		};

		struct Header
		{
			int32_t payload_len;
		};

		void Read(size_t min_read_bytes);
		void HandleRead(const error_code & error, std::size_t bytes_transferred);
		bool PrepareRead(size_t min_prepare_bytes);

		// Parse Message
		void HandleReceiveData(Buffer& read_buf, size_t& next_read_size)
		{
			DecodeRecvData(read_buf, next_read_size);
		}

		void PendWrite(const Ptr<Buffer>& buf);
		void Write();
		void HandleWrite(const error_code& error);
		void HandleError(const error_code& error);
		void _Close(CloseReason reason);

		void EncodeSendData(Ptr<Buffer>& data)
		{
			// Make Header
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
				if (recv_handler)
					recv_handler(shared_from_this(), buf.Data() + buf.ReaderIndex(), header.payload_len);

				// 바디 사이즈만큼 전진
				buf.SkipBytes(header.payload_len);
			}
		}

		std::unique_ptr<tcp::socket> socket_;
		std::unique_ptr<strand> strand_;

		int  id_;
		State state_;

		Ptr<Buffer> read_buf_;
		std::vector<Ptr<Buffer>> pending_list_;
		std::vector<Ptr<Buffer>> sending_list_;

		// config
		bool	no_delay_ = false;
		size_t	min_receive_size_;
		size_t	max_receive_buffer_size_;
	};

} // namespace net