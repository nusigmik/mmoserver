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

		// ���ڵ�
		void Decode(Buffer& read_buf)
		{
			// ó���� �����Ͱ� ������ �ݺ�
			while (read_buf.IsReadable())
			{
				// Decode MsgHeader
				if (!header_decoded_)
				{
					// ���ڵ尡 �����ϸ� ������.
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
		// ��� ���ڵ�
		bool DecodeHeader(Buffer& read_buf)
		{
			// ��� ������ ��ŭ ���� �������� ����
			if (!read_buf.IsReadable(HEADER_SIZE))
			{
				return false;
			}

			// TO DO : ��� ����? ��ȣȭ?
			read_buf.Read(header_.opCode);
			read_buf.Read(header_.payload_length);

			header_decoded_ = true;
			return true;
		}

		// �ٵ� ���ڵ�
		bool DecodeBody(Buffer& read_buf)
		{
			if (!read_buf.IsReadable(header_.payload_length))
				return false;

			// TO DO : Deserialize. �޽��� ��ü�� �����.
			// �Ϸ� �ڵ鷯 �ݹ�

			// body ������ ��ŭ ReaderIndex ����.
			read_buf.SkipBytes(header_.payload_length);
			// ��� �ʱ�ȭ
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