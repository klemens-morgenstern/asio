//
// experimental/virtualized.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
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
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include "asio/experimental/virtualized.hpp"

#include "../unit_test.hpp"

void test()
{

    asio::io_context ctx;

    asio::experimental::virtual_async_operation<void()> op =
            asio::post(ctx, asio::experimental::virtualize);

    bool called = false;
    std::move(op)([&]{called = true;});
    ASIO_CHECK(!called);
    ctx.run();
    ASIO_CHECK(called);
}

ASIO_TEST_SUITE
(
  "experimental/virtual_deferred",
  ASIO_TEST_CASE(test)
)
