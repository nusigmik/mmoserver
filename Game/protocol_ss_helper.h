#pragma once
#include <protocol_ss_generated.h>

namespace ProtocolSS {

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

    template <typename Peer, typename T>
    void SendRelay(Peer& peer, flatbuffers::FlatBufferBuilder& fbb, int source, const std::vector<int>& destinations, const flatbuffers::Offset<T>& offset_message)
    {
        auto offset_relay = CreateRelayMessageDirect(fbb, source, &destinations, MessageTypeTraits<T>::enum_value, offset_message.Union());
        Send(peer, fbb, offset_relay);
    }

    template <typename Peer, typename T>
    void SendRelay(Peer& peer, int source, const std::vector<int>& destinations, const T& message)
    {
        flatbuffers::FlatBufferBuilder fbb;
        auto offset_message = T::TableType::Pack(fbb, &message);
        SendRelay(peer, fbb, source, destinations, offset_message);
    }
}
