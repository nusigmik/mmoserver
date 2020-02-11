#pragma once
#include "Common.h"

// 서버에 접속된 리모트 클라이언트
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
    
    // 네트워크 세션 ID
    int GetSessionID() const
    {
        return net_session_->GetID();
    }
    
    // 네트워크 세션 객체
    const Ptr<net::Session>& GetSession() const
    {
        return net_session_;
    }

    // 작업을 직렬화 실행
    template <typename Handler>
    void Dispatch(Handler&& handler)
    {
        net_session_->GetStrand().dispatch(std::forward<Handler>(handler));
    }

    // 네트워크로 데이터를 보낸다
    void Send(const uint8_t * data, size_t size)
    {
        net_session_->Send(data, size);
    }

    template <typename BufferT>
    void Send(BufferT&& data)
    {
        net_session_->Send(std::forward<BufferT>(data));
    }

    // 연결을 종료한다.
    void Disconnect()
    {
        net_session_->Close();
    }

    bool IsDisconnected()
    {
        return !net_session_->IsOpen();
    }

    // 클라이언트에서 연결을 끊었을때 callback. 하위 클래스에서 재정의 한다.
    virtual void OnDisconnected() {};

private:
    Ptr<net::Session> net_session_;
};
