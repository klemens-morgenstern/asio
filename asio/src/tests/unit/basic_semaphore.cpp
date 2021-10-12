//
// basic_semaphore.cpp
// ~~~~~~~~~~~~~~~~~~~~
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
#include "asio/basic_semaphore.hpp"
#include "unit_test.hpp"

namespace semaphore_test
{

asio::awaitable<void> test_impl()
{

  bool f1 = false, f2 = false, f3 = false, f4 = false;
  asio::error_code e1, e2, e3, e4;

  {
    asio::counting_semaphore<> sm{co_await asio::this_coro::executor};
    sm.async_aquire([&](asio::error_code ec){ e1 = ec; f1 = true; });
    sm.async_aquire([&](asio::error_code ec){ e2 = ec; f2 = true; });
    sm.async_aquire([&](asio::error_code ec){ e3 = ec; f3 = true; });
    sm.async_aquire([&](asio::error_code ec){ e4 = ec; f4 = true; });

    co_await asio::post(asio::use_awaitable);

    ASIO_CHECK(!f1);
    ASIO_CHECK(!f2);
    ASIO_CHECK(!f3);
    ASIO_CHECK(!f4);

    ASIO_CHECK(!e1);
    ASIO_CHECK(!e2);
    ASIO_CHECK(!e3);
    ASIO_CHECK(!e4);

    sm.release(1);
    co_await asio::post(asio::use_awaitable);
    ASIO_CHECK( f1);
    ASIO_CHECK(!f2);
    ASIO_CHECK(!f3);
    ASIO_CHECK(!f4);

    ASIO_CHECK(!e1);
    ASIO_CHECK(!e2);
    ASIO_CHECK(!e3);
    ASIO_CHECK(!e4);

    sm.release(2);
    co_await asio::post(asio::use_awaitable);

    ASIO_CHECK( f1);
    ASIO_CHECK( f2);
    ASIO_CHECK( f3);
    ASIO_CHECK(!f4);

    ASIO_CHECK(!e1);
    ASIO_CHECK(!e2);
    ASIO_CHECK(!e3);
    ASIO_CHECK(!e4);
  }

  co_await asio::post(asio::use_awaitable);

  ASIO_CHECK(f1);
  ASIO_CHECK(f2);
  ASIO_CHECK(f3);
  ASIO_CHECK(f4);

  ASIO_CHECK(!e1);
  ASIO_CHECK(!e2);
  ASIO_CHECK(!e3);
  ASIO_CHECK( e4);

}

void test()
{
  asio::io_context ctx;
  asio::co_spawn(ctx, test_impl(), asio::detached);
  ctx.run();
}

}

ASIO_TEST_SUITE
(
        "basic_semaphore",
        ASIO_TEST_CASE(semaphore_test::test)
)
