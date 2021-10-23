//
// experimental/coro/volatile.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
#include <iostream>
#include <asio/detached.hpp>

#include "asio/co_spawn.hpp"
#include "asio/experimental/coro.hpp"
#include "asio/io_context.hpp"
#include "asio/thread_pool.hpp"
#include "asio/use_awaitable.hpp"
#include "../../unit_test.hpp"

using namespace asio::experimental;
namespace this_coro = asio::this_coro;

namespace coro {

asio::awaitable<void> volatile_test_impl()
{
  asio::thread_pool tp1, tp2, tp3;

  auto c = [](int i1, int i2, int i3, asio::thread_pool::executor_type e1,
              asio::thread_pool::executor_type e2,
              asio::thread_pool::executor_type e3)
                      -> asio::experimental::coro<int, int, asio::thread_pool::executor_type volatile >
           {

             asio::thread_pool::executor_type e = co_await asio::this_coro::executor;
             ASIO_CHECK(e1.running_in_this_thread());
             ASIO_CHECK(e1 == e);
             co_yield i1;

             e = co_await asio::this_coro::executor;
             ASIO_CHECK(e2.running_in_this_thread());
             ASIO_CHECK(e2 == e);
             co_yield i2;

             e = co_await asio::this_coro::executor;
             ASIO_CHECK(e3.running_in_this_thread());
             co_return i3;
           };

  auto d = c(1,2,3, tp1.get_executor(), tp2.get_executor(), tp3.get_executor());

  constexpr asio::use_awaitable_t<asio::thread_pool::executor_type> use_awaitable;

  ASIO_CHECK(1 == co_await co_spawn(tp1, d.async_resume(use_awaitable), asio::use_awaitable));
  ASIO_CHECK(2 == co_await co_spawn(tp2, d.async_resume(use_awaitable), asio::use_awaitable));
  ASIO_CHECK(3 == co_await co_spawn(tp3, d.async_resume(use_awaitable), asio::use_awaitable));

  tp1.stop();
  tp2.stop();
  tp3.stop();

  tp1.wait();
  tp2.wait();
  tp3.wait();
}

void volatile_test()
{
  asio::io_context ctx;

  co_spawn(ctx.get_executor(), volatile_test_impl(), asio::detached);

  ctx.run();
}


} // namespace coro

ASIO_TEST_SUITE
(
  "coro/cancel",
  ASIO_TEST_CASE(::coro::volatile_test)
)
