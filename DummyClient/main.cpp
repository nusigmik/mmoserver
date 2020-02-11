// DummyClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "DummyClient.h"

int _tmain(int argc, _TCHAR* argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: cfg file path \n";
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}