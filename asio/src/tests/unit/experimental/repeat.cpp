//
// experimental/repeater.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include <asio/awaitable.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/co_spawn.hpp>
#include <asio/experimental/deferred.hpp>
#include "asio/experimental/repeat.hpp"

#include "../unit_test.hpp"

namespace repeater
{

asio::awaitable<void> repeater_test_impl()
{
  int i = 0;
  auto r = asio::experimental::repeat(
            [&, exec = co_await asio::this_coro::executor](auto token)
            {
              return asio::co_spawn(exec,
                                    [&]() -> asio::awaitable<int> { co_return i++; },
                                    token);
            });

  ASIO_CHECK(0 == co_await r.async_repeat(asio::use_awaitable));
  ASIO_CHECK(1 == co_await r.async_repeat(asio::use_awaitable));
  ASIO_CHECK(2 == co_await r.async_repeat(asio::use_awaitable));
  ASIO_CHECK(3 == co_await r.async_repeat(asio::use_awaitable));
}

void repeater_test()
{
  asio::io_context ctx;
  asio::co_spawn(ctx, &repeater_test_impl, asio::detached);
  ctx.run();
}

}

ASIO_TEST_SUITE
(
        "experimental/repeat",
        ASIO_TEST_CASE(repeater::repeater_test)
)
