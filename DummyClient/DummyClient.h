#pragma once
#include "stdafx.h"

namespace PCS = ProtocolCS;

class DummyClient
{
public:
    DummyClient(const net::ClientConfig& config) {}
    ~DummyClient() {}

    // Ŭ���̾�Ʈ �̸�.
    std::string GetName() {}

    // ����.
    void Connect(const std::string& address, uint16_t port) {}
    // ����.
    void Disconnect() {}
    // ����� �� ���� ���.
    void Wait() {}
    // �� ũ���̾�Ʈ�� ����ǰ� �ִ� IoServiceLoop ��ü.
    const Ptr<net::EventLoop>& GetEventLoop() {}

private:
   
};