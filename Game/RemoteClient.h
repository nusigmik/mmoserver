#pragma once
#include "Common.h"

// ������ ���ӵ� ����Ʈ Ŭ���̾�Ʈ
class RemoteClient : public std::enable_shared_from_this<RemoteClient>
{
public:
    RemoteClient(const RemoteClient&) = delete;
    RemoteClient& operator=(const RemoteClient&) = delete;

    RemoteClient(const Ptr<net::Session>& net_session)
        : net_session_(net_session)
    {
        assert(net_session != nullptr);
        assert(net_session->IsOpen());
    }
    virtual ~RemoteClient() {}
    
    // ��Ʈ��ũ ���� ID
    int GetSessionID() const
    {
        return net_session_->GetID();
    }
    
    // ��Ʈ��ũ ���� ��ü
    const Ptr<net::Session>& GetSession() const
    {
        return net_session_;
    }

    // �۾��� ����ȭ ����
    template <typename Handler>
    void Dispatch(Handler&& handler)
    {
        net_session_->GetStrand().dispatch(std::forward<Handler>(handler));
    }

    // ��Ʈ��ũ�� �����͸� ������
    void Send(const uint8_t * data, size_t size)
    {
        net_session_->Send(data, size);
    }

    template <typename BufferT>
    void Send(BufferT&& data)
    {
        net_session_->Send(std::forward<BufferT>(data));
    }

    // ������ �����Ѵ�.
    void Disconnect()
    {
        net_session_->Close();
    }

    bool IsDisconnected()
    {
        return !net_session_->IsOpen();
    }

    // Ŭ���̾�Ʈ���� ������ �������� callback. ���� Ŭ�������� ������ �Ѵ�.
    virtual void OnDisconnected() {};

private:
    Ptr<net::Session> net_session_;
};
