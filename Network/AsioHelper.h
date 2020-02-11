#pragma once

#include <type_traits>
#include <future>
#include <boost/asio/use_future.hpp>
#include <boost/asio.hpp>

using namespace boost;

namespace net {

	// Get a buffer that asio input sequence.
	template <typename BufferType>
	asio::const_buffers_1 const_buffer(const BufferType& buf)
	{
		return asio::buffer(asio::const_buffer(buf.Data() + buf.ReaderIndex(), buf.ReadableBytes()));
	}

	// Get a buffer that asio output sequence.
	template <typename BufferType>
	asio::mutable_buffers_1 mutable_buffer(BufferType& buf)
	{
		return asio::buffer(asio::mutable_buffer(buf.Data() + buf.WriterIndex(), buf.WritableBytes()));
	}

	template <class Service, class CompletionHandler>
	decltype(auto) post_future(Service& s, CompletionHandler&& handler)
	{
		using return_type = decltype(handler());
		//using return_type = std::result_of<CompletionHandler()>::type;

		auto promise = std::make_shared<std::promise<return_type>>();
		auto future = promise->get_future();
		s.post([promise, handler] {
			promise->set_value(handler());
		});

		return future;
	}

	template <class Service, class CompletionHandler>
	decltype(auto) dispatch_future(Service& s, CompletionHandler&& handler)
	{
		using return_type = decltype(handler());
		//using return_type = std::result_of<CompletionHandler()>::type;

		auto promise = std::make_shared<std::promise<return_type>>();
		auto future = promise->get_future();
		s.dispatch([promise, handler] {
			promise->set_value(handler());
		});

		return future;
	}

} // namespace net