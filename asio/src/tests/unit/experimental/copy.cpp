//
// copy.cpp
// ~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "asio/experimental/as_tuple.hpp"
#include "asio/experimental/copy.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/serial_port.hpp"
#include "asio/read.hpp"
#include "asio/read_until.hpp"
#include "asio/use_future.hpp"
#include "asio/write.hpp"

#include <fstream>
#include <asio/local/stream_protocol.hpp>
#include <asio/local/connect_pair.hpp>
#include <asio/experimental/awaitable_operators.hpp>

#include "../unit_test.hpp"

namespace copy
{


template<typename Socket>
struct full_wrapper
{
  Socket &socket;

  full_wrapper(Socket &socket) : socket(socket)
  {}

  template<typename Buffer, typename ReadHandler>
  auto async_read(const Buffer &buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return asio::async_read_until(socket, buffer, '\n', ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }

  template<typename Buffer, typename ReadHandler>
  auto async_write(const Buffer &buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return asio::async_write(socket, buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }

  template<typename Buffer>
  std::size_t read(const Buffer &buffer, asio::error_code &ec)
  {
    return asio::read_until(socket, buffer, '\n', ec);
  }

  template<typename Buffer>
  std::size_t write(const Buffer &buffer, asio::error_code &ec)
  {
    return asio::write(socket, buffer, ec);
  }

  using executor_type = typename Socket::executor_type;

  executor_type get_executor()
  { return socket.get_executor(); }
};


void compile_test(asio::ip::tcp::socket sk,
                  asio::serial_port sp,
                  full_wrapper<asio::ip::tcp::socket> fw)
{

  asio::experimental::copy(sk, sk);
  asio::experimental::copy(sp, sp);
  asio::experimental::copy(fw, fw);

  asio::experimental::copy(sk, fw);
  asio::experimental::copy(sp, sk);
  asio::experimental::copy(fw, sp);

  asio::experimental::copy(sk, sp);
  asio::experimental::copy(sp, fw);
  asio::experimental::copy(fw, sk);

  std::future<std::size_t> f;

  f = asio::experimental::async_copy(sk, sk, asio::use_future);
  f = asio::experimental::async_copy(sp, sp, asio::use_future);
  f = asio::experimental::async_copy(fw, fw, asio::use_future);

  f = asio::experimental::async_copy(sk, fw, asio::use_future);
  f = asio::experimental::async_copy(sp, sk, asio::use_future);
  f = asio::experimental::async_copy(fw, sp, asio::use_future);

  f = asio::experimental::async_copy(sk, sp, asio::use_future);
  f = asio::experimental::async_copy(sp, fw, asio::use_future);
  f = asio::experimental::async_copy(fw, sk, asio::use_future);
}

std::string test_data()
{
  std::ifstream fs{__FILE__};
  std::ostringstream sstr;
  sstr << fs.rdbuf();
  return sstr.str();
}

asio::awaitable<void> test_socket_to_socket(asio::io_context & ctx)
{
  auto dt = test_data();
  std::string sz;
  sz.reserve(dt.size());

  asio::local::stream_protocol::socket src_sink(ctx), src_src(ctx), sink_sink(ctx), sink_src(ctx);
  asio::local::connect_pair( src_sink,  src_src);
  asio::local::connect_pair(sink_sink, sink_src);

  using namespace asio::experimental::awaitable_operators;

  auto write =
          [](auto & src_sink, auto & sink_src, auto buf) -> asio::awaitable<std::tuple<asio::error_code, std::size_t>>
          {
            auto res = co_await asio::async_write(src_sink, buf, asio::experimental::as_tuple(asio::use_awaitable));
            auto [ec, w] = res;
            src_sink.close();
            co_return res;
          };

  auto copy =
          [](auto & src, auto & sink) -> asio::awaitable<std::tuple<asio::error_code, std::size_t>>
          {
            auto res = co_await asio::experimental::async_copy(src, sink, asio::experimental::as_tuple(asio::use_awaitable));
            sink.close();
            co_return res;
          };

  auto [write_err, written, cp, r]
      = co_await ( write(src_sink, sink_src, asio::buffer(dt))
            && copy(src_src, sink_sink)
            && asio::async_read(sink_src, asio::dynamic_buffer(sz), asio::experimental::as_tuple(asio::use_awaitable))
            );

  auto [copy_error, copied] = cp;
  auto [read_err, read] = r;

  ASIO_CHECK_MESSAGE(!copy_error || (copy_error == asio::error::eof),copy_error.message());
  ASIO_CHECK_MESSAGE(written   == dt.size(), written);
  ASIO_CHECK_MESSAGE(copied    == dt.size(), copied);
  ASIO_CHECK_MESSAGE(read_err  == asio::error::eof, read_err << " " << read_err.message());
  ASIO_CHECK_MESSAGE(read == dt.size(), read);
  ASIO_CHECK_MESSAGE(sz.size() == dt.size(), sz.size());
  co_return ;
};

void check_complete(std::exception_ptr ex)
{
  try
  {
    if (ex)
      std::rethrow_exception(ex);
  }
  catch (std::exception & e)
  {
    ASIO_WARN_MESSAGE(false, e.what());
    ASIO_CHECK(false);
  }
}

void tests()
{
  asio::io_context ctx;

#define ASIO_COMPLETE [](std::exception_ptr ex)

  asio::co_spawn(ctx, test_socket_to_socket(ctx), check_complete);
  ctx.run();
}

}

ASIO_TEST_SUITE
(
    "copy",
    ASIO_TEST_CASE(copy::tests)
)
