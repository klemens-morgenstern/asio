//
// repeat.cpp
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
#include <asio/post.hpp>
#include "asio/experimental/repeat.hpp"

#include "asio/steady_timer.hpp"
#include "../unit_test.hpp"

namespace repeat
{

void single_shot()
{
    asio::io_context ioc;


    asio::steady_timer tim{ioc, std::chrono::milliseconds(10)};

    std::vector<asio::error_code> res;

    tim.async_wait(asio::experimental::repeat)(
            [&](asio::error_code ec)
            {
                res.push_back(ec);
                tim.expires_after(std::chrono::milliseconds(10));
            });

    ioc.run();

    ASIO_CHECK( res.size() == 2);
    ASIO_CHECK(!res.at(0));
    ASIO_CHECK( res.at(1));

}

} // repeat


ASIO_TEST_SUITE
(
        "promise",
        ASIO_TEST_CASE(repeat::single_shot)
)
