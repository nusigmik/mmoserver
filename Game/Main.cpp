// Game.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Settings.h"
#include "ManagerServer.h"
#include "LoginServer.h"
#include "WorldServer.h"

int run_servers()
{
    puts("Enterable commands:\n");
    puts("m: Start manager server.\n");
    puts("l: Start login server.\n");
    puts("w: Start world server.\n");
    puts("q: Quit.\n");

    std::vector<Ptr<IServer>> server_list;
    std::string input;

    while (true)
    {
        std::cin >> input;

        if (input == "m")
        {
            if (!Settings::GetInstance().Load("manager.cfg"))
                return 0;

            auto server = std::make_shared<ManagerServer>();
            server->Run();
            server_list.push_back(server);
        }
        else if (input == "l")
        {
            if (!Settings::GetInstance().Load("login.cfg"))
                return 0;

            auto server = std::make_shared<LoginServer>();
            server->Run();
            server_list.push_back(server);
        }
        else if (input == "w")
        {
            if (!Settings::GetInstance().Load("world.cfg"))
                return 0;

            auto server = std::make_shared<WorldServer>();
            server->Run();
            server_list.push_back(server);
        }
        else if (input == "q")
        {
            break;
        }
    }

    for (auto& e : server_list)
    {
        e->Stop();
    }

    return 0;
}

template <typename CharT>
int run_server(CharT server_mode)
{
    puts("q: Quit.\n");

    Ptr<IServer> server;
    if (server_mode == 'm')
    {
        if (!Settings::GetInstance().Load("manager.cfg"))
            return 0;

        server = std::make_shared<ManagerServer>();
        server->Run();
    }
    else if (server_mode == 'l')
    {
        if (!Settings::GetInstance().Load("login.cfg"))
            return 0;

        server = std::make_shared<LoginServer>();
        server->Run();
    }
    else if (server_mode == 'w')
    {
        if (!Settings::GetInstance().Load("world.cfg"))
            return 0;

        server = std::make_shared<WorldServer>();
        server->Run(); 
    }
    else
    {
        return 0;
    }

    while (true)
    {
        std::string input;
        std::cin >> input;

        if (input == "q")
        {
            break;
        }
    }

    return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	try
	{
        return run_servers();
        //return run_server(*argv[1]);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception: " << e.what() <<"\n";
	}

    return 0;
}

