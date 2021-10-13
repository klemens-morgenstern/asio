//
// experimental/px.cpp
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
#include "asio/awaitable.hpp"
#include "asio/detached.hpp"
#include "asio/co_spawn.hpp"
#include "asio/io_context.hpp"
#include "asio/experimental/awaitable_operators.hpp"
#include "asio/experimental/px.hpp"

#include "../unit_test.hpp"

namespace px
{

namespace px = asio::experimental::px;

template<typename T>
struct recording
{
  std::vector<std::pair<asio::chrono::steady_clock::time_point, T>> values;
  asio::chrono::steady_clock::time_point ended;
  std::exception_ptr error;
  bool timedout = false;
};

template<typename T>
asio::awaitable<recording<T>>
        record(asio::experimental::coro<T> c, std::size_t limit = std::numeric_limits<std::size_t>::max())
{
  recording<T> res;
  try
  {
    while (auto v = co_await c.async_resume(asio::use_awaitable))
    {
      res.values.emplace_back(asio::chrono::steady_clock::now(), std::move(*v));
      if (--limit == 0)
      {
        res.timedout = true;
        co_return res;
      }
    }

  }
  catch(...)
  {
    res.error = std::current_exception();
  }
  res.ended = asio::chrono::steady_clock::now();
  co_return res;
}

asio::awaitable<void> empty()
{
  auto e = co_await record(px::empty(co_await asio::this_coro::executor));
  ASIO_CHECK(e.values.empty());
  ASIO_CHECK(e.error == nullptr);
}

asio::awaitable<void> never()
{
  using asio::experimental::awaitable_operators::operator||;
  asio::steady_timer st{co_await asio::this_coro::executor, asio::chrono::milliseconds(10)};

  auto e = co_await (record(px::never(co_await asio::this_coro::executor)) || st.async_wait(asio::use_awaitable));
  ASIO_CHECK(e.index() == 1u);
}

asio::awaitable<void> error()
{
  auto e = co_await record(px::error(co_await asio::this_coro::executor, std::runtime_error("test_error")));
  ASIO_CHECK(e.values.empty());
  ASIO_CHECK(e.error != nullptr);
}

asio::awaitable<void> from()
{
  auto e = co_await record(px::from(co_await asio::this_coro::executor, {1, 2, 3, 4}));
  ASIO_CHECK(e.values.size() == 4);
  ASIO_CHECK(e.values[0].second == 1);
  ASIO_CHECK(e.values[1].second == 2);
  ASIO_CHECK(e.values[2].second == 3);
  ASIO_CHECK(e.values[3].second == 4);
  ASIO_CHECK(!e.error);

  std::vector<double> dv = {0.1, 2.3, 4.5, 6.7};
  auto d = co_await record(px::from(co_await asio::this_coro::executor, dv));
  ASIO_CHECK(d.values.size() == 4);
  ASIO_CHECK(d.values[0].second == 0.1);
  ASIO_CHECK(d.values[1].second == 2.3);
  ASIO_CHECK(d.values[2].second == 4.5);
  ASIO_CHECK(d.values[3].second == 6.7);
  ASIO_CHECK(!d.error);
}

asio::awaitable<void> interval()
{
  auto dur = asio::chrono::milliseconds(10);
  auto dur_low  = asio::chrono::milliseconds( 9);
  auto dur_high = asio::chrono::milliseconds(11);
  auto e = co_await record(
          px::interval(co_await asio::this_coro::executor, dur), 4);
  ASIO_CHECK(e.values.size() == 4);
  ASIO_CHECK(e.values[0].second == 0);
  ASIO_CHECK(e.values[1].second == 1);
  ASIO_CHECK(e.values[2].second == 2);
  ASIO_CHECK(e.values[3].second == 3);

  ASIO_CHECK((e.values[1].first - e.values[0].first) > dur_low);
  ASIO_CHECK((e.values[2].first - e.values[1].first) > dur_low);
  ASIO_CHECK((e.values[3].first - e.values[2].first) > dur_low);

  ASIO_CHECK((e.values[1].first - e.values[0].first) < dur_high);
  ASIO_CHECK((e.values[2].first - e.values[1].first) < dur_high);
  ASIO_CHECK((e.values[3].first - e.values[2].first) < dur_high);
  ASIO_CHECK(e.error == nullptr);
}

asio::awaitable<void> just()
{
  auto e = co_await record(px::just(co_await asio::this_coro::executor, 3,2,1));
  ASIO_CHECK(e.values.size() == 3u);
  ASIO_CHECK(e.values[0].second == 3);
  ASIO_CHECK(e.values[1].second == 2);
  ASIO_CHECK(e.values[2].second == 1);
  ASIO_CHECK(e.error == nullptr);
}

asio::awaitable<void> range()
{
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 1, 5));
  ASIO_CHECK(e.values.size() == 4);
  ASIO_CHECK(e.values[0].second == 1);
  ASIO_CHECK(e.values[1].second == 2);
  ASIO_CHECK(e.values[2].second == 3);
  ASIO_CHECK(e.values[3].second == 4);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> repeat()
{
  auto e = co_await record(px::repeat(co_await asio::this_coro::executor, 42, 3));
  ASIO_CHECK(e.values.size() == 3);
  ASIO_CHECK(e.values[0].second == 42);
  ASIO_CHECK(e.values[1].second == 42);
  ASIO_CHECK(e.values[2].second == 42);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> timer()
{
  auto e = co_await record(px::timer(co_await asio::this_coro::executor, asio::chrono::milliseconds(1)));
  ASIO_CHECK(e.values.size() == 1);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> buffer_count()
{
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 9) | px::buffer(3u));
  ASIO_CHECK(e.values.size() == 3);
  ASIO_CHECK(e.values[0].second[0] == 0);
  ASIO_CHECK(e.values[0].second[1] == 1);
  ASIO_CHECK(e.values[0].second[2] == 2);
  ASIO_CHECK(e.values[1].second[0] == 3);
  ASIO_CHECK(e.values[1].second[1] == 4);
  ASIO_CHECK(e.values[1].second[2] == 5);
  ASIO_CHECK(e.values[2].second[0] == 6);
  ASIO_CHECK(e.values[2].second[1] == 7);
  ASIO_CHECK(e.values[2].second[2] == 8);

  ASIO_CHECK(!e.error);
}

asio::awaitable<void> buffer_count_skip()
{
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 9) | px::buffer(2u, 3u));
  ASIO_CHECK(e.values.size() == 3);
  ASIO_CHECK(e.values[0].second.size() == 2);
  ASIO_CHECK(e.values[0].second[0] == 0);
  ASIO_CHECK(e.values[0].second[1] == 1);
  ASIO_CHECK(e.values[1].second.size() == 2);
  ASIO_CHECK(e.values[1].second[0] == 3);
  ASIO_CHECK(e.values[1].second[1] == 4);
  ASIO_CHECK(e.values[2].second.size() == 2);
  ASIO_CHECK(e.values[2].second[0] == 6);
  ASIO_CHECK(e.values[2].second[1] == 7);

  ASIO_CHECK(!e.error);
}

asio::awaitable<void> concat_map()
{
  auto l =
    [](asio::any_io_executor exec, int i) -> asio::experimental::coro<int>
    {
      co_yield i;
      co_yield i * 2;
      co_yield i * 3;
    };

  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 3) | px::concat_map(l));
  ASIO_CHECK(e.values.size() == 9);
  ASIO_CHECK(e.values[0].second == 0);
  ASIO_CHECK(e.values[1].second == 0);
  ASIO_CHECK(e.values[2].second == 0);
  ASIO_CHECK(e.values[3].second == 1);
  ASIO_CHECK(e.values[4].second == 2);
  ASIO_CHECK(e.values[5].second == 3);
  ASIO_CHECK(e.values[6].second == 2);
  ASIO_CHECK(e.values[7].second == 4);
  ASIO_CHECK(e.values[8].second == 6);

  ASIO_CHECK(!e.error);
}

asio::awaitable<void> map()
{
  auto l = [](int i)
           {
             return std::to_string(i);
           };

  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 3) | px::map(l));
  ASIO_CHECK(e.values.size() == 3);
  ASIO_CHECK(e.values[0].second == "0");
  ASIO_CHECK(e.values[1].second == "1");
  ASIO_CHECK(e.values[2].second == "2");
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> scan()
{
  auto l = [](int i, int j)
  {
    return i + j;
  };

  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 3) | px::scan(l, 1));
  ASIO_CHECK(e.values.size() == 3);
  ASIO_CHECK(e.values[0].second == 1);
  ASIO_CHECK(e.values[1].second == 2);
  ASIO_CHECK(e.values[2].second == 4);
  ASIO_CHECK(!e.error);
}


asio::awaitable<void> distinct()
{
  auto l = [](asio::any_io_executor exec) -> asio::experimental::coro<int>
  {
    co_yield 0;
    co_yield 0;
    co_yield 1;
    co_yield 1;
    co_yield 1;
    co_yield 2;
    co_yield 2;
    co_yield 1;
  };

  auto e = co_await record(l(co_await asio::this_coro::executor) | px::distinct);
  ASIO_CHECK(e.values.size() == 3);
  ASIO_CHECK(e.values[0].second == 1);
  ASIO_CHECK(e.values[1].second == 2);
  ASIO_CHECK(e.values[2].second == 1);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> element_at()
{
  auto l = [](int i, int j)
  {
    return i + j;
  };
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 4) | px::scan(l, 1) | px::element_at(2));
  ASIO_CHECK(e.values.size() == 1);
  ASIO_CHECK(e.values[0].second == 4);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> filter()
{
  auto l = [](int i)
  {
    return i % 2 == 0;
  };
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 6) | px::filter(l));
  ASIO_CHECK(e.values.size() == 3);
  ASIO_CHECK(e.values[0].second == 0);
  ASIO_CHECK(e.values[1].second == 2);
  ASIO_CHECK(e.values[2].second == 4);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> first()
{
  auto l = [](int i)
  {
    return i % 2 == 0;
  };
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 7, 20) | px::first());
  ASIO_CHECK(e.values.size() == 1);
  ASIO_CHECK(e.values[0].second == 7);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> ignore_elements()
{
  auto l = [](int i)
  {
    return i % 2 == 0;
  };
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 20) | px::ignore_elements());
  ASIO_CHECK(e.values.size() == 0);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> last()
{
  auto l = [](int i)
  {
    return i % 2 == 0;
  };
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 20) | px::last());
  ASIO_CHECK(e.values.size() == 1);
  ASIO_CHECK(e.values[0].second == 19);
  ASIO_CHECK(!e.error);
}


asio::awaitable<void> skip()
{
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 20) | px::skip(15));
  ASIO_CHECK(e.values.size() == 5);
  ASIO_CHECK(e.values[0].second == 15);
  ASIO_CHECK(e.values[1].second == 16);
  ASIO_CHECK(e.values[2].second == 17);
  ASIO_CHECK(e.values[3].second == 18);
  ASIO_CHECK(e.values[4].second == 19);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> skip_last()
{
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 20) | px::skip_last(15));
  ASIO_CHECK(e.values.size() == 5);
  ASIO_CHECK(e.values[0].second == 0);
  ASIO_CHECK(e.values[1].second == 1);
  ASIO_CHECK(e.values[2].second == 2);
  ASIO_CHECK(e.values[3].second == 3);
  ASIO_CHECK(e.values[4].second == 4);
  ASIO_CHECK(!e.error);
}


asio::awaitable<void> take()
{
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 20) | px::take(5));
  ASIO_CHECK(e.values.size() == 5);
  ASIO_CHECK(e.values[0].second == 0);
  ASIO_CHECK(e.values[1].second == 1);
  ASIO_CHECK(e.values[2].second == 2);
  ASIO_CHECK(e.values[3].second == 3);
  ASIO_CHECK(e.values[4].second == 4);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> take_last()
{
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 20) | px::take_last(5));
  ASIO_CHECK(e.values.size() == 5);
  ASIO_CHECK(e.values[0].second == 15);
  ASIO_CHECK(e.values[1].second == 16);
  ASIO_CHECK(e.values[2].second == 17);
  ASIO_CHECK(e.values[3].second == 18);
  ASIO_CHECK(e.values[4].second == 19);
  ASIO_CHECK(!e.error);
}


asio::awaitable<void> start_with()
{
  auto e = co_await record(px::range(co_await asio::this_coro::executor, 0, 5) | px::start_with(42, 43));
  ASIO_CHECK(e.values.size() == 7);
  ASIO_CHECK(e.values[0].second == 42);
  ASIO_CHECK(e.values[1].second == 43);
  ASIO_CHECK(e.values[2].second == 0);
  ASIO_CHECK(e.values[3].second == 1);
  ASIO_CHECK(e.values[4].second == 2);
  ASIO_CHECK(e.values[5].second == 3);
  ASIO_CHECK(e.values[6].second == 4);
  ASIO_CHECK(!e.error);
}

asio::awaitable<void> catch_()
{
  auto thrower = [](asio::any_io_executor) -> asio::experimental::coro<int>
  {
    co_yield 1;
    co_yield 2;
    throw std::runtime_error("test-error");
  };

  auto continue_ = [](asio::any_io_executor, std::exception_ptr ex) -> asio::experimental::coro<int>
  {
    ASIO_CHECK(ex != nullptr);
    co_yield 10;
    co_yield 11;
  };

  auto e = co_await record(thrower(co_await asio::this_coro::executor) | px::catch_(continue_));
  ASIO_CHECK(e.values.size() == 4);
  ASIO_CHECK(e.values[0].second == 1);
  ASIO_CHECK(e.values[1].second == 2);
  ASIO_CHECK(e.values[2].second == 10);
  ASIO_CHECK(e.values[3].second == 11);
  ASIO_CHECK(!e.error);
}


asio::awaitable<void> retry()
{
  int i = 0;
  auto source = [&](asio::any_io_executor) -> asio::experimental::coro<int>
  {
    while (++i < 6)
    {
      if (i == 3)
        throw std::runtime_error("test-error");
      co_yield i;
    }
  };

  auto e = co_await record(px::retry(co_await asio::this_coro::executor, source));
  ASIO_CHECK(e.values.size() == 4);
  ASIO_CHECK(e.values[0].second == 1);
  ASIO_CHECK(e.values[1].second == 2);
  ASIO_CHECK(e.values[2].second == 4);
  ASIO_CHECK(e.values[3].second == 5);
  ASIO_CHECK(!e.error);
}


asio::awaitable<void> timeout() {

  auto source = [&](asio::any_io_executor exec) -> asio::experimental::coro<int>
  {
    asio::steady_timer delay{exec};
    co_yield 0;
    co_yield 1;
    delay.expires_after(std::chrono::milliseconds(100));
    co_await delay.async_wait(asio::experimental::use_coro);
    co_yield 2;
    co_yield 3;
  };

  auto e = co_await record(source(co_await asio::this_coro::executor)
                          | px::timeout(std::chrono::milliseconds(15)));
  ASIO_CHECK(e.error != nullptr);
  ASIO_CHECK(e.values.size() >= 2);
  ASIO_CHECK(e.values[0].second == 0);
  ASIO_CHECK(e.values[1].second == 1);
}


asio::awaitable<void> timestamp()
{
  auto e = co_await record(px::interval(co_await asio::this_coro::executor, std::chrono::milliseconds(1))
                           | px::timestamp(std::chrono::steady_clock{}) | px::take(3));
  ASIO_CHECK(e.error == nullptr);
  ASIO_CHECK(e.values.size() == 3);
  ASIO_CHECK(e.values[0].first < e.values[1].first);
  ASIO_CHECK(e.values[1].first < e.values[2].first);
}


asio::awaitable<void> all()
{

  auto is_even = [](int i) { return i % 2 == 0;};
  auto false_c = px::all(px::range(co_await asio::this_coro::executor, 0, 10), is_even);
  auto true_c  = px::all(px::range(co_await asio::this_coro::executor, 0, 10) | px::map([](auto i){return i * 2;}), is_even);
  ASIO_CHECK(co_await true_c.async_resume(asio::use_awaitable));
  ASIO_CHECK(!(co_await false_c.async_resume(asio::use_awaitable)));
}


asio::awaitable<void> zip()
{

  auto e = co_await record(px::zip(px::range(co_await asio::this_coro::executor, 0, 3),
                                   px::range(co_await asio::this_coro::executor, 3, 6) | px::delay(std::chrono::milliseconds(5))));

  ASIO_CHECK(e.values.size() == 3);
  ASIO_CHECK(get<0>(e.values[0].second) == 0u);
  ASIO_CHECK(get<1>(e.values[0].second) == 3u);
  ASIO_CHECK(get<0>(e.values[1].second) == 1u);
  ASIO_CHECK(get<1>(e.values[1].second) == 4u);
  ASIO_CHECK(get<0>(e.values[2].second) == 2u);
  ASIO_CHECK(get<1>(e.values[2].second) == 5u);
}

asio::awaitable<void> merge()
{
  auto ts = px::map([](int val){return std::to_string(val);});
  auto e = co_await record(px::merge(px::interval(co_await asio::this_coro::executor, std::chrono::milliseconds(10)) | ts,
                                     px::interval(co_await asio::this_coro::executor, std::chrono::milliseconds(10)) | px::delay(std::chrono::milliseconds(5)))
                                             | px::take(7));

  ASIO_CHECK(e.values.size() == 7);
  ASIO_CHECK(get<0>(e.values[0].second) == "0");
  ASIO_CHECK(get<1>(e.values[1].second) ==  0);
  ASIO_CHECK(get<0>(e.values[2].second) == "1");
  ASIO_CHECK(get<1>(e.values[3].second) ==  1);
  ASIO_CHECK(get<0>(e.values[4].second) == "2");
  ASIO_CHECK(get<1>(e.values[5].second) ==  2);
  ASIO_CHECK(get<0>(e.values[6].second) == "3");
}



asio::awaitable<void> combine_latest()
{

  auto ts = px::map([](int val){return std::to_string(val);});
  auto e = co_await record(px::combine_latest(
                                     px::interval(co_await asio::this_coro::executor, std::chrono::milliseconds(10)) | ts,
                                     px::interval(co_await asio::this_coro::executor, std::chrono::milliseconds(10)) | px::delay(std::chrono::milliseconds(5)))
                           | px::take(7));

  ASIO_CHECK(e.values.size() == 7);
  ASIO_CHECK(get<0>(e.values[0].second) == "0");
  ASIO_CHECK(get<1>(e.values[0].second) ==  0);
  ASIO_CHECK(get<0>(e.values[1].second) == "1");
  ASIO_CHECK(get<1>(e.values[1].second) ==  0);
  ASIO_CHECK(get<0>(e.values[2].second) == "1");
  ASIO_CHECK(get<1>(e.values[2].second) ==  1);
  ASIO_CHECK(get<0>(e.values[3].second) == "2");
  ASIO_CHECK(get<1>(e.values[3].second) ==  1);
  ASIO_CHECK(get<0>(e.values[4].second) == "2");
  ASIO_CHECK(get<1>(e.values[4].second) ==  2);
  ASIO_CHECK(get<0>(e.values[5].second) == "3");
  ASIO_CHECK(get<1>(e.values[5].second) ==  2);
  ASIO_CHECK(get<0>(e.values[6].second) == "3");
  ASIO_CHECK(get<1>(e.values[6].second) ==  3);
}



void all_tests()
{
  asio::io_context ctx;
  asio::co_spawn(ctx, empty,             asio::detached);
  asio::co_spawn(ctx, never,             asio::detached);
  asio::co_spawn(ctx, from,              asio::detached);
  asio::co_spawn(ctx, interval,          asio::detached);
  asio::co_spawn(ctx, just,              asio::detached);
  asio::co_spawn(ctx, range,             asio::detached);
  asio::co_spawn(ctx, repeat,            asio::detached);
  asio::co_spawn(ctx, timer,             asio::detached);
  asio::co_spawn(ctx, buffer_count,      asio::detached);
  asio::co_spawn(ctx, buffer_count_skip, asio::detached);
  asio::co_spawn(ctx, concat_map,        asio::detached);
  asio::co_spawn(ctx, map,               asio::detached);
  asio::co_spawn(ctx, scan,              asio::detached);
  asio::co_spawn(ctx, distinct,          asio::detached);
  asio::co_spawn(ctx, element_at,        asio::detached);
  asio::co_spawn(ctx, filter,            asio::detached);
  asio::co_spawn(ctx, first,             asio::detached);
  asio::co_spawn(ctx, ignore_elements,   asio::detached);
  asio::co_spawn(ctx, last,              asio::detached);
  asio::co_spawn(ctx, skip,              asio::detached);
  asio::co_spawn(ctx, skip_last,         asio::detached);
  asio::co_spawn(ctx, take,              asio::detached);
  asio::co_spawn(ctx, take_last,         asio::detached);

  asio::co_spawn(ctx, start_with,        asio::detached);
  asio::co_spawn(ctx, catch_,            asio::detached);
  asio::co_spawn(ctx, retry,             asio::detached);
  asio::co_spawn(ctx, timeout,           asio::detached);
  asio::co_spawn(ctx, timestamp,         asio::detached);
  asio::co_spawn(ctx, all,               asio::detached);

  asio::co_spawn(ctx, zip,               asio::detached);
  asio::co_spawn(ctx, merge,             asio::detached);
  asio::co_spawn(ctx, combine_latest,    asio::detached);

  ctx.run();
}


}

ASIO_TEST_SUITE
(
        "experimental/px",
        ASIO_TEST_CASE(px::all_tests)
)
