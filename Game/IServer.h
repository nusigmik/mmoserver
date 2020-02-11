#pragma once
#include <iostream>

class IServer
{
public:
	virtual ~IServer() {};
	virtual std::string GetName() = 0;
	virtual void SetName(const std::string& name) = 0;
    // 서버 시작
	virtual void Run() = 0;
	// 서버 종료
    virtual void Stop() = 0;
};
