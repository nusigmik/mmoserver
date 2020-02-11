#pragma once
#include <cstdint>
#include "ByteBuffer.h"
#include "Session.h"


namespace net 
{
	/*
	class MessageDecoder
	{
	public:
		MessageDecoder(Session& session)
			: session_(session)
		{

		}

		struct Header
		{
			int16_t payload_length;
			int16_t message_type;
		};

		const int HEADER_SIZE = sizeof(Header);

		// 디코드
		void Decode(Buffer& read_buf)
		{
			// 처리할 데이터가 있으면 반복
			while (read_buf.IsReadable())
			{
				// Decode MsgHeader
				if (!header_decoded_)
				{
					// 디코드가 실패하면 나간다.
					if (!DecodeHeader(read_buf))
						break;
				}

				// Decode Body
				if (header_decoded_)
				{
					if (!DecodeBody(read_buf))
						break;
				}
			}
		}

	private:
		// 헤더 디코드
		bool DecodeHeader(Buffer& read_buf)
		{
			// 헤더 사이즈 만큼 받지 못했으면 리턴
			if (!read_buf.IsReadable(HEADER_SIZE))
			{
				return false;
			}

			// TO DO : 헤더 검증? 복호화?
			read_buf.Read(header_.opCode);
			read_buf.Read(header_.payload_length);

			header_decoded_ = true;
			return true;
		}

		// 바디 디코드
		bool DecodeBody(Buffer& read_buf)
		{
			if (!read_buf.IsReadable(header_.payload_length))
				return false;

			// TO DO : Deserialize. 메시지 객체로 만든다.
			// 완료 핸들러 콜백

			// body 사이즈 만큼 ReaderIndex 전진.
			read_buf.SkipBytes(header_.payload_length);
			// 헤더 초기화
			header_decoded_ = false;
			header_.opCode = 0;
			header_.payload_length = 0;

			return true;
		}

		Session& session_;

		Header header_;
		bool header_decoded_ = false;
	};
	*/
}