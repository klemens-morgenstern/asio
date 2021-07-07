//
// timeout.cpp
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
#include "asio/experimental/timeout.hpp"

#include "asio/steady_timer.hpp"
#include "../unit_test.hpp"

namespace timeout {

void timeout_tester_for()
{
    using namespace asio;
    using asio::error_code;
    using namespace std::chrono;

    io_context ctx;

    steady_timer timer1{ctx}, timer2{ctx};


    timer1.expires_after(std::chrono::seconds(10));
    timer2.expires_after(std::chrono::milliseconds(10));

    error_code ec1, ec2;

    timer1.async_wait(experimental::with_timeout(std::chrono::milliseconds(10), [&ec1](error_code ec){ec1 = ec;}));
    timer2.async_wait(experimental::with_timeout(std::chrono::seconds(10), [&ec2](error_code ec){ec2 = ec;}));

    ctx.run();
    ASIO_CHECK(ec1 == error::operation_aborted);
    ASIO_CHECK(!ec2);
}

void timeout_tester_until()
{
    using namespace asio;
    using asio::error_code;
    using namespace std::chrono;

    io_context ctx;

    steady_timer timer1{ctx}, timer2{ctx};

    auto now = std::chrono::steady_clock::now();

    timer1.expires_at( now + std::chrono::seconds(10));
    timer2.expires_at( now + std::chrono::milliseconds(10));

    error_code ec1, ec2;

    timer1.async_wait(experimental::with_timeout(now + std::chrono::milliseconds(10), [&ec1](error_code ec){ec1 = ec;}));
    timer2.async_wait(experimental::with_timeout(now + std::chrono::seconds(10), [&ec2](error_code ec){ec2 = ec;}));

    ctx.run();
    ASIO_CHECK(ec1 == error::operation_aborted);
    ASIO_CHECK(!ec2);
}

} // namespace promise

ASIO_TEST_SUITE
(
        "promise",
        ASIO_TEST_CASE(timeout::timeout_tester_for)
                ASIO_TEST_CASE(timeout::timeout_tester_until)
)
