//
// process/process.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

#include "asio/process/basic_process.hpp"

#include "../unit_test.hpp"

namespace asio
{

static_assert(asio::process_launcher<default_process_launcher>);
template struct basic_process<>;

}

namespace process
{
void simple_test()
{

    asio::io_context ctx;
    "PATH=/bin:/usr/sbin";
    "Path=C:\\Windows";

    asio::process proc{ctx.get_executor(), "/bin/foo", {"foo", "bar"}};

    asio::process wproc{ctx.get_executor(), "foo", {L"foo", L"bar"}};


}

}


ASIO_TEST_SUITE
(
        "process",
        ASIO_TEST_CASE(process::simple_test)
)