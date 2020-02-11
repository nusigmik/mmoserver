#pragma once
#include "stdafx.h"

namespace PCS = ProtocolCS;

class DummyClient
{
public:
    DummyClient(const net::ClientConfig& config) {}
    ~DummyClient() {}

    // 클라이언트 이름.
    std::string GetName() {}

    // 접속.
    void Connect(const std::string& address, uint16_t port) {}
    // 종료.
    void Disconnect() {}
    // 종료될 때 까지 대기.
    void Wait() {}
    // 이 크라이언트가 실행되고 있는 IoServiceLoop 객체.
    const Ptr<net::EventLoop>& GetEventLoop() {}

private:
   
};