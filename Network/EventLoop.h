#pragma once

#include <thread>
#include <atomic>
#include <vector>
#include <boost/asio/executor_work_guard.hpp>
#include "Types.h"

namespace net {

	// Run event processing loop.
	class EventLoop
	{
	public:
		EventLoop(const EventLoop&) = delete;
		EventLoop& operator=(const EventLoop&) = delete;

		explicit EventLoop(size_t thread_count = 1);
		~EventLoop();

		void Stop();
		void Wait();

		asio::io_context& GetIoContext()
		{
			return io_context_;
		}

	private:

		asio::io_context io_context_;
		asio::executor_work_guard<asio::io_context::executor_type> work_;
		std::vector<std::thread> threads_;
	};

} // namespace net
