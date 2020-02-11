#pragma once

#include <thread>
#include "Types.h"

namespace net {

	class EventLoop;

	struct ServerConfig
	{
		// execution
		Ptr<EventLoop>		event_loop = nullptr;
		std::size_t			thread_count = std::thread::hardware_concurrency(); // event_loop 이 설정되어 있으면 이값은 무시.
		// session
		size_t				max_session_count = 100000;
		size_t				min_receive_size = 1024 * 4;
		size_t				max_receive_buffer_size = std::numeric_limits<size_t>::max();
		// socket options
		bool				no_delay = false;
	};

	struct ClientConfig
	{
		// execution
		Ptr<EventLoop>		event_loop = nullptr;
		// session
		size_t				min_receive_size = 1024 * 4;
		size_t				max_receive_buffer_size = std::numeric_limits<size_t>::max();
		// socket options
		bool				no_delay = false;
	};

} // namespace net