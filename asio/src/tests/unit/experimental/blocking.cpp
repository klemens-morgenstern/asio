//
// experimental/blocking.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot d dot morgenstern at gmail dot com
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include <asio/post.hpp>
#include <asio/co_spawn.hpp>
#include <asio/awaitable.hpp>
#include "asio/experimental/blocking.hpp"

#include "../unit_test.hpp"

namespace blocking
{

void run_test()
{
    asio::io_context ctx;

    auto i = asio::co_spawn(ctx, []() -> asio::awaitable<int> {co_return 42;}, asio::experimental::blocking_run);

    ASIO_CHECK(i == 42);
    bool ran = false;
    asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {ran = true; co_return ;}, asio::experimental::blocking_run);
    ASIO_CHECK(ran);
    asio::steady_timer st{ctx};
    auto started_at = std::chrono::steady_clock::now();
    st.expires_after(std::chrono::milliseconds(100));
    st.async_wait(asio::experimental::blocking_run);
    ASIO_CHECK(std::chrono::steady_clock::now() >= started_at);
}


void run_for_test()
{
    asio::io_context ctx;

    auto i = asio::co_spawn(ctx,
                            []() -> asio::awaitable<int>
                              {
                                  asio::steady_timer st{co_await asio::this_coro::executor};
                                  st.expires_after(std::chrono::milliseconds(10000));
                                  co_await st.async_wait(asio::use_awaitable);
                                  co_return 42;
                                }, asio::experimental::blocking_run_for(std::chrono::milliseconds(10)));

    ASIO_CHECK(!i);

    i = asio::co_spawn(ctx,
                            []() -> asio::awaitable<int>
                            {
                                asio::steady_timer st{co_await asio::this_coro::executor};
                                st.expires_after(std::chrono::milliseconds(10));
                                co_await st.async_wait(asio::use_awaitable);
                                co_return 42;
                            }, asio::experimental::blocking_run_for(std::chrono::milliseconds(10000)));

    ASIO_CHECK(i);
    ASIO_CHECK(i == 42);
}



void run_until_test()
{
    asio::io_context ctx;

    auto i = asio::co_spawn(ctx,
                            []() -> asio::awaitable<int>
                            {
                                asio::steady_timer st{co_await asio::this_coro::executor};
                                st.expires_at(std::chrono::steady_clock::now() + std::chrono::milliseconds(10000));
                                co_await st.async_wait(asio::use_awaitable);
                                co_return 42;
                            }, asio::experimental::blocking_run_until(std::chrono::steady_clock::now() +  std::chrono::milliseconds(10)));

    ASIO_CHECK(!i);

    i = asio::co_spawn(ctx,
                       []() -> asio::awaitable<int>
                       {
                           asio::steady_timer st{co_await asio::this_coro::executor};
                           st.expires_at(std::chrono::steady_clock::now() +  std::chrono::milliseconds(10));
                           co_await st.async_wait(asio::use_awaitable);
                           co_return 42;
                       }, asio::experimental::blocking_run_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(10000)));

    ASIO_CHECK(i);
    ASIO_CHECK(i == 42);
}

void poll_test()
{
    asio::io_context ctx;

    auto i = asio::co_spawn(ctx, []() -> asio::awaitable<int> {co_return 42;}, asio::experimental::blocking_poll);

    ASIO_CHECK(i == 42);
    bool ran = false;
    asio::co_spawn(ctx, [&]() -> asio::awaitable<void> {ran = true; co_return ;}, asio::experimental::blocking_poll);
    ASIO_CHECK(ran);
    asio::steady_timer st{ctx};
    auto started_at = std::chrono::steady_clock::now();
    st.expires_after(std::chrono::milliseconds(100));
    st.async_wait(asio::experimental::blocking_poll);
    ASIO_CHECK(std::chrono::steady_clock::now() >= started_at);
}


void poll_for_test()
{
    asio::io_context ctx;

    auto i = asio::co_spawn(ctx,
                            []() -> asio::awaitable<int>
                            {
                                asio::steady_timer st{co_await asio::this_coro::executor};
                                st.expires_after(std::chrono::milliseconds(10000));
                                co_await st.async_wait(asio::use_awaitable);
                                co_return 42;
                            }, asio::experimental::blocking_poll_for(std::chrono::milliseconds(10)));

    ASIO_CHECK(!i);

    i = asio::co_spawn(ctx,
                       []() -> asio::awaitable<int>
                       {
                           asio::steady_timer st{co_await asio::this_coro::executor};
                           st.expires_after(std::chrono::milliseconds(10));
                           co_await st.async_wait(asio::use_awaitable);
                           co_return 42;
                       }, asio::experimental::blocking_poll_for(std::chrono::milliseconds(10000)));

    ASIO_CHECK(i);
    ASIO_CHECK(i == 42);
}



void poll_until_test()
{
    asio::io_context ctx;

    auto i = asio::co_spawn(ctx,
                            []() -> asio::awaitable<int>
                            {
                                asio::steady_timer st{co_await asio::this_coro::executor};
                                st.expires_at(std::chrono::steady_clock::now() + std::chrono::milliseconds(10000));
                                co_await st.async_wait(asio::use_awaitable);
                                co_return 42;
                            }, asio::experimental::blocking_poll_until(std::chrono::steady_clock::now() +  std::chrono::milliseconds(10)));

    ASIO_CHECK(!i);

    i = asio::co_spawn(ctx,
                       []() -> asio::awaitable<int>
                       {
                           asio::steady_timer st{co_await asio::this_coro::executor};
                           st.expires_at(std::chrono::steady_clock::now() +  std::chrono::milliseconds(10));
                           co_await st.async_wait(asio::use_awaitable);
                           co_return 42;
                       }, asio::experimental::blocking_poll_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(10000)));

    ASIO_CHECK(i);
    ASIO_CHECK(i == 42);
}


}

ASIO_TEST_SUITE
(
        "experimental/blocking",
        ASIO_TEST_CASE(blocking::run_test)
        ASIO_TEST_CASE(blocking::run_for_test)
        ASIO_TEST_CASE(blocking::run_until_test)
        ASIO_TEST_CASE(blocking::poll_test)
        ASIO_TEST_CASE(blocking::poll_for_test)
        ASIO_TEST_CASE(blocking::poll_until_test)
    )