// TestServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <network.h>

void OnOpen(const Ptr<net::Session>& session)
{
    BOOST_LOG_TRIVIAL(info) << "OnOpen";
}

void OnClose(const Ptr<net::Session>& session, const net::CloseReason& reason)
{
    BOOST_LOG_TRIVIAL(info) << "OnClose";
}

void OnMessage(const Ptr<net::Session>& session, const uint8_t* buf, size_t size)
{
    BOOST_LOG_TRIVIAL(info) << "OnMessage";
}

int main()
{
    auto event_loop = std::make_shared<net::EventLoop>(2);
    net::ServerConfig config;
    config.event_loop = event_loop;

    auto server = net::NetServer::Create(config);
    server->RegisterSessionOpenedHandler(OnOpen);
    server->RegisterSessionClosedHandler(OnClose);
    server->RegisterMessageHandler(OnMessage);

    server->Start(8888);
    event_loop->Wait();

    return 0;
}