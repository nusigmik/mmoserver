#include "EventLoop.h"

namespace net {

EventLoop::EventLoop(size_t thread_count)
	: work_(asio::make_work_guard(io_context_))
{
	if (thread_count == 0) {
		thread_count = std::max(static_cast<int>(std::thread::hardware_concurrency()), 1);
	}
	
	for (int i = 0; i < thread_count; i++)
	{
		threads_.emplace_back(std::thread([this]()
		{
				io_context_.run();
		}));
	}

	BOOST_LOG_TRIVIAL(info) << "Run EventLoop. thread_count:" << thread_count;
}

EventLoop::~EventLoop()
{
	Stop();
	Wait();
}

void EventLoop::Stop()
{
    work_.reset();
	io_context_.stop();

	BOOST_LOG_TRIVIAL(info) << "Stop EventLoop";
}

void EventLoop::Wait()
{
	for (auto& thread : threads_)
	{
		if (thread.joinable())
			thread.join();
	}
}

} // namespace net