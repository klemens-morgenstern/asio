// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Test that header file is self-contained.
#include "asio/fiber.hpp"
#include "asio/experimental/as_tuple.hpp"
#include "asio/io_context.hpp"
#include "asio/steady_timer.hpp"
#include "asio/redirect_error.hpp"

#include "unit_test.hpp"

void test_results()
{
  asio::io_context ctx;
  int called = 0;
  
  auto check_e  = [&](std::exception_ptr e){called ++; ASIO_CHECK(!e);};
  auto check    = [&]{called ++;};
  auto check_i  = [&](int i) {called ++; ASIO_CHECK(i == 42);};
  auto check_ei = [&](std::exception_ptr e, int i){called ++; ASIO_CHECK(!e); ASIO_CHECK(i == 42);};

  auto check_t  = [&](std::exception_ptr e){called ++;
    try {
      ASIO_CHECK(e);
      if (e)
      std::rethrow_exception(e);
    }
    catch (std::runtime_error & re)
    {
      ASIO_CHECK(re.what() == std::string_view{"test"});
    }
    catch (...)
    {
      ASIO_CHECK(false);
    }
  };
  auto check_ti = [&](std::exception_ptr e, int i){check_t(e);};


  asio::async_fiber(ctx, [](asio::fiber_context ctx)          {}, check_e);
  asio::async_fiber(ctx, [](asio::fiber_context ctx) noexcept {}, check);

  asio::async_fiber(ctx, [](asio::fiber_context ctx)          {return 42;}, check_ei);
  asio::async_fiber(ctx, [](asio::fiber_context ctx) noexcept {return 42;}, check_i);

  asio::async_fiber(ctx.get_executor(), [](asio::fiber_context ctx)          {}, check_e);
  asio::async_fiber(ctx.get_executor(), [](asio::fiber_context ctx) noexcept {}, check);

  asio::async_fiber(ctx.get_executor(), [](asio::fiber_context ctx)          {return 42;}, check_ei);
  asio::async_fiber(ctx.get_executor(), [](asio::fiber_context ctx) noexcept {return 42;}, check_i);

  using io_fiber_context = asio::basic_fiber_context<asio::io_context::executor_type>;

  asio::async_fiber(ctx, [](io_fiber_context ctx)          {}, check_e);
  asio::async_fiber(ctx, [](io_fiber_context ctx) noexcept {}, check);

  asio::async_fiber(ctx, [](io_fiber_context ctx)          {return 42;}, check_ei);
  asio::async_fiber(ctx, [](io_fiber_context ctx) noexcept {return 42;}, check_i);

  asio::async_fiber(ctx.get_executor(), [](io_fiber_context ctx)          {}, check_e);
  asio::async_fiber(ctx.get_executor(), [](io_fiber_context ctx) noexcept {}, check);

  asio::async_fiber(ctx.get_executor(), [](io_fiber_context ctx)          {return 42;}, check_ei);
  asio::async_fiber(ctx.get_executor(), [](io_fiber_context ctx) noexcept {return 42;}, check_i);


  asio::async_fiber(ctx, [](asio::fiber_context ctx) {throw std::runtime_error("test"); }, check_t);
  asio::async_fiber(ctx, [](asio::fiber_context ctx) {throw std::runtime_error("test"); return 42;}, check_ti);
  ctx.run();
  ASIO_CHECK(called == 18);
}

template<typename Ctx, typename Token, typename ... Args>
void async_stub(Ctx && ctx, Token && token, Args && ... args)
{
  return asio::post(ctx, asio::experimental::deferred(
          [...args = std::forward<Args>(args)]() mutable
          {
              return asio::experimental::deferred.values(std::forward<Args>(args)...);
          }))(std::forward<Token>(token));
}

void test_fiber()
{

  asio::io_context ctx;

  int called = 0;

  asio::async_fiber(ctx,
                    [](asio::fiber_context ctx)
                    {
                      asio::post(ctx);
                      asio::post(ctx);
                      return 42;
                    },
                    [&](std::exception_ptr e, int i)
                    {
                      called++;
                      ASIO_CHECK(!e);
                      ASIO_CHECK(i == 42);
                    });

  asio::async_fiber(ctx,
                    [](asio::fiber_context ctx)
                    {
                      async_stub(ctx.get_executor(), ctx, std::make_exception_ptr(std::runtime_error("test")));
                      return 42;
                    },
                    [&](std::exception_ptr e, int i)
                    {
                      called++;
                      ASIO_CHECK(e);
                    });

  asio::async_fiber(ctx,
                    [](asio::fiber_context ctx)
                    {
                      async_stub(ctx.get_executor(), ctx, asio::error_code(asio::error::fault));
                      return 42;
                    },
                    [&](std::exception_ptr e, int i)
                    {
                      called++;
                      ASIO_CHECK(e);
                    });


  asio::async_fiber(ctx,
                    [](asio::fiber_context ctx)
                    {
                      asio::post(ctx);
                      asio::post(ctx);
                    },
                    [&](std::exception_ptr e)
                    {
                      called++;
                      ASIO_CHECK(!e);
                    });

  asio::async_fiber(ctx,
                    [](asio::fiber_context ctx)
                    {
                      async_stub(ctx.get_executor(), ctx, std::make_exception_ptr(std::runtime_error("test")));
                    },
                    [&](std::exception_ptr e)
                    {
                      called++;
                      ASIO_CHECK(e);
                    });

  asio::async_fiber(ctx,
                    [](asio::fiber_context ctx)
                    {
                      async_stub(ctx.get_executor(), ctx, asio::error_code(asio::error::fault));
                    },
                    [&](std::exception_ptr e)
                    {
                      called++;
                      ASIO_CHECK(e);
                    });

  ctx.run();
  ASIO_CHECK(called == 6);
}


void test_modifier_impl(asio::fiber_context ctx)
{
  using namespace std::chrono;
  asio::steady_timer tim1{ctx.get_executor(), 10ms},
                     tim2{ctx.get_executor(), std::chrono::steady_clock::time_point::min()};

  tim1.async_wait(ctx); // should work fine.
  tim1.expires_after(10ms);
  auto [ec] =  tim1.async_wait(asio::experimental::as_tuple(ctx));
  ASIO_CHECK(!ec);

  tim1.expires_after(10s);
  tim2.expires_after(10ms);
  tim2.async_wait([&](auto ec) {tim1.cancel();});
  std::tie(ec) =  tim1.async_wait(asio::experimental::as_tuple(ctx));
  ASIO_CHECK(ec == asio::error::operation_aborted);
}

void test_modifier()
{
  asio::io_context ctx;
  bool done = false;
  asio::async_fiber(ctx,
                    & test_modifier_impl,
                    [&](std::exception_ptr e) { ASIO_CHECK(!e); done = true; });
  ctx.run();
}

void test_cancel()
{
  asio::io_context ctx;
  bool done = false;
  asio::cancellation_signal sig;
  asio::async_fiber(ctx,
                    [](asio::fiber_context && ctx) noexcept
                    {
                      asio::steady_timer tim1{ctx.get_executor(), std::chrono::seconds(10)};
                      return tim1.async_wait(asio::experimental::as_tuple(ctx));

                    },
                    asio::bind_cancellation_slot(sig.slot(),
                      [&](std::tuple<asio::error_code> tup)
                      {
                          ASIO_CHECK(get<0>(tup) == asio::error::operation_aborted);
                          done = true;
                      }));

  asio::steady_timer st{ctx, std::chrono::milliseconds(50)};
  st.async_wait([&](auto ec) {ASIO_CHECK(!ec); sig.emit(asio::cancellation_type::all);});
  ctx.run();
}

ASIO_TEST_SUITE
(
  "fiber",
  ASIO_TEST_CASE(test_results)
  ASIO_TEST_CASE(test_fiber)
  ASIO_TEST_CASE(test_modifier)
  ASIO_TEST_CASE(test_cancel)
)
