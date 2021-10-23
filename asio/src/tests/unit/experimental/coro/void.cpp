//
// experimental/coro/void.cpp
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

#include "asio/experimental/coro.hpp"
#include "asio/io_context.hpp"
#include "asio/steady_timer.hpp"
#include "asio/this_coro.hpp"
#include <boost/scope_exit.hpp>
#include "../../unit_test.hpp"

using namespace asio::experimental;
namespace this_coro = asio::this_coro;

namespace coro {


auto coro_delay(asio::any_io_executor exec) noexcept
-> asio::experimental::coro<void(), void, void>
{
  asio::steady_timer timer{exec, std::chrono::milliseconds(10)};
  co_await timer;
}


auto coro_delay_noexcept(asio::any_io_executor exec) noexcept
  -> asio::experimental::coro<void() noexcept, asio::error_code, void>
{
  asio::steady_timer timer{exec, std::chrono::milliseconds(10)};
  auto ec = co_await timer;
  co_return ec;
}


auto coro_arg_delay(asio::any_io_executor exec) noexcept
-> asio::experimental::coro<int(asio::chrono::milliseconds), void, void>
{
  asio::steady_timer timer{exec, co_yield 0};
  co_await timer;
}


auto coro_arg_delay_noexcept(asio::any_io_executor exec) noexcept
-> asio::experimental::coro<int(asio::chrono::milliseconds) noexcept, asio::error_code, void>
{
  auto t = co_yield 0;
  std::cerr << "T: " << t.count() << std::endl;
  asio::steady_timer timer{exec, t};

  auto ec = co_await timer;
  ASIO_CHECK(!ec);
  co_return ec;
}

auto coro_caller(asio::io_context& ctx) -> asio::experimental::coro<void() noexcept>
{
  auto p1 = std::chrono::steady_clock::now();

  co_await coro_delay(ctx.get_executor());
  auto p2 = std::chrono::steady_clock::now();

  co_await coro_delay_noexcept(ctx.get_executor());
  auto p3 = std::chrono::steady_clock::now();

  auto cc = coro_arg_delay(ctx.get_executor());
  auto now = []{ return std::chrono::steady_clock::now(); };

  co_await cc(std::chrono::milliseconds(1000));
  co_await cc(std::chrono::milliseconds(5));
  auto p4 = std::chrono::steady_clock::now();


  auto cd = coro_arg_delay_noexcept(ctx.get_executor());
  co_await cd(std::chrono::milliseconds(1000));
  co_await cd(std::chrono::milliseconds(5));
  auto p5 = std::chrono::steady_clock::now();

  ASIO_CHECK((p2-p1) >= std::chrono::milliseconds(10));
  ASIO_CHECK((p3-p2) >= std::chrono::milliseconds(10));
  ASIO_CHECK((p4-p3) >= std::chrono::milliseconds(5));
  ASIO_CHECK((p5-p4) >= std::chrono::milliseconds(5));

  ASIO_CHECK((p2-p1) < std::chrono::milliseconds(15));
  ASIO_CHECK((p3-p2) < std::chrono::milliseconds(15));
  ASIO_CHECK((p4-p3) < std::chrono::milliseconds(10));
  ASIO_CHECK((p5-p4) < std::chrono::milliseconds(10));

}

void coro_simple_void()
{
  asio::io_context ctx;

  {
    auto p1 = std::chrono::steady_clock::now();
    coro_delay(ctx.get_executor()).resume(std::nothrow);
    auto p2 = std::chrono::steady_clock::now();
    coro_delay_noexcept(ctx.get_executor()).resume();
    auto p3 = std::chrono::steady_clock::now();

    auto cc = coro_arg_delay(ctx.get_executor());
    cc.resume(std::chrono::milliseconds(1000));
    cc.resume(std::chrono::milliseconds(5));
    auto p4 = std::chrono::steady_clock::now();


    auto cd = coro_arg_delay_noexcept(ctx.get_executor());
    cd.resume(std::chrono::milliseconds(1000));
    cd.resume(std::chrono::milliseconds(5));
    auto p5 = std::chrono::steady_clock::now();

    ASIO_CHECK((p2-p1) >= std::chrono::milliseconds(10));
    ASIO_CHECK((p3-p2) >= std::chrono::milliseconds(10));
    ASIO_CHECK((p4-p3) >= std::chrono::milliseconds(5));
    ASIO_CHECK((p5-p4) >= std::chrono::milliseconds(5));

    ASIO_CHECK((p2-p1) < std::chrono::milliseconds(15));
    ASIO_CHECK((p3-p2) < std::chrono::milliseconds(15));
    ASIO_CHECK((p4-p3) < std::chrono::milliseconds(10));
    ASIO_CHECK((p5-p4) < std::chrono::milliseconds(10));
  }

  auto k = coro_caller(ctx);
  asio::error_code res_ec;
  bool done = false;
  k.async_resume([&]{done = true;});

  ASIO_CHECK(!done);
  ctx.run();

  ASIO_CHECK(done);
}


} // namespace coro

ASIO_TEST_SUITE
(
  "coro/cancel",
  ASIO_TEST_CASE(::coro::coro_simple_void)
)
