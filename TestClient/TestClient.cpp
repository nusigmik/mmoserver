// TestClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <network.h>

void OnEvent(net::NetEventType ev)
{
    BOOST_LOG_TRIVIAL(info) << "OnEvent " << static_cast<int>(ev);
}

void OnMessage(const uint8_t* buf, size_t size)
{
    BOOST_LOG_TRIVIAL(info) << "OnMessage";
}

int main()
{
    auto event_loop = std::make_shared<net::EventLoop>(2);
    net::ClientConfig config;
    config.event_loop = event_loop;

    auto client = net::NetClient::Create(config);
    client->RegisterNetEventHandler(OnEvent);
    client->RegisterMessageHandler(OnMessage);

    client->Connect("127.0.0.1", "8888");
    event_loop->Wait();

    return 0;
}