#pragma once
#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>
#include <boost\program_options.hpp>
#include "Singleton.h"

namespace po = boost::program_options;

// 전역 서버 설정
class Settings : public Singleton<Settings>
{
public:
	std::string   name; // 서버 이름
	std::string   bind_address;
	uint16_t      bind_port;
	size_t        thread_count;
	size_t        max_session_count;
	size_t	      min_receive_size;
	size_t	      max_receive_buffer_size;
	bool	      no_delay; // tcp nodelay option
	std::string   db_host; // DB 접속 주소
	std::string   db_user;
	std::string   db_password;
	std::string   db_schema;
	size_t        db_connection_pool;
	std::string	  manager_address; // 매니저 서버 접속 주소
	uint16_t      manager_port; // 매니저 서버 접속 포트

	template <typename CharT>
	bool Load(CharT* filepath)
	{
		// step 1 : 옵션 설명 정의
		po::options_description desc("Allowed options");
		desc.add_options()
			("Server.name", po::value<std::string>(&name)->default_value(""))
			("Server.bind-address", po::value<std::string>(&bind_address)->default_value("0.0.0.0"))
			("Server.bind-port", po::value<uint16_t>())
			("Server.thread", po::value<size_t>(&thread_count)->default_value(std::thread::hardware_concurrency()))
			("Server.max-session", po::value<size_t>(&max_session_count)->default_value(10000))
			("Server.min-receive-size", po::value<size_t>(&min_receive_size)->default_value(1024 * 4))
			("Server.max-buffer-size", po::value<size_t>(&max_receive_buffer_size)->default_value(std::numeric_limits<size_t>::max()))
			("Server.no-delay", po::value<bool>(&no_delay)->default_value(false))
			("DB.host", po::value<std::string>())
			("DB.user", po::value<std::string>())
			("DB.password", po::value<std::string>())
			("DB.schema", po::value<std::string>())
			("DB.conn-pool", po::value<size_t>(&db_connection_pool)->default_value(1))
			("Manager.address", po::value<std::string>(&manager_address)->default_value("0.0.0.0"))
			("Manager.port", po::value<uint16_t>(&manager_port)->default_value(0))
			;

		// step 2 :명령행 옵션 분석
		po::variables_map vm;
		try
		{
			std::ifstream ifs(filepath);
			if (!ifs.is_open())
			{
				std::cerr << "Can not open file: " << filepath << "\n";
				return false;
			}
			po::store(po::parse_config_file(ifs, desc), vm);

            //po::store(po::parse_config_file(filepath, desc), vm);
		}
		catch (po::unknown_option & e)
		{
			// 예외 1: 없는 옵션을 사용한 경우
			std::cerr << e.what() << "\n";
			return false;
		}
		catch (po::invalid_option_value & e)
		{
			// 예외 2: 옵션 값의 오류가 발생한 경우
			std::cerr << e.what() << std::endl;
			return false;
		}
		catch (po::invalid_command_line_syntax & e)
		{
			// 예외 3: 옵션 값이 없을 경우
			std::cerr << e.what() << "\n";
			return false;
		}
        catch (po::reading_file & e)
        {
            // 예외 4: 파일 open 실패
            std::cerr << e.what() << "\n";
            return false;
        }
		catch (std::exception & e)
		{
			std::cerr << e.what() << "\n";
			return false;
		}

		po::notify(vm);

		// step 3 : 옵션 처리
		if (vm.count("Server.bind-port"))
		{
			bind_port = vm["Server.bind-port"].as<uint16_t>();
		}
		else
		{
			std::cerr << "Server.bind-port required" << "\n";
			return false;
		}

		if (vm.count("DB.host"))
		{
			db_host = vm["DB.host"].as<std::string>();
		}
		else
		{
			std::cerr << "DB.host required" << "\n";
			return false;
		}

		if (vm.count("DB.user"))
		{
			db_user = vm["DB.user"].as<std::string>();
		}
		else
		{
			std::cerr << "DB.user required" << "\n";
			return false;
		}

		if (vm.count("DB.password"))
		{
			db_password = vm["DB.password"].as<std::string>();
		}
		else
		{
			std::cerr << "DB.pass required" << "\n";
			return false;
		}

		if (vm.count("DB.schema"))
		{
			db_schema = vm["DB.schema"].as<std::string>();
		}
		else
		{
			std::cerr << "DB.schema required" << "\n";
			return false;
		}
		return true;
	}
};
