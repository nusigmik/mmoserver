#pragma once
#include <protocol_cs_generated.h>

namespace ProtocolCS {

	template <typename Peer, typename T>
	void Send(Peer& peer, flatbuffers::FlatBufferBuilder& fbb, const flatbuffers::Offset<T>& offset_message)
	{
        auto offset_root = CreateMessageRoot(fbb, MessageTypeTraits<T>::enum_value, offset_message.Union());
        FinishMessageRootBuffer(fbb, offset_root);
		peer.Send(fbb.GetBufferPointer(), fbb.GetSize());
	}

	template <typename Peer, typename T>
	void Send(Peer& peer, const T& message)
	{
		flatbuffers::FlatBufferBuilder fbb;
        auto offset_message = T::TableType::Pack(fbb, &message);
        Send(peer, fbb, offset_message);
	}
}
