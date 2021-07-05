//
// this_process.cpp
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

#include "asio/process/handle.hpp"
#include "asio/this_process.hpp"

#include "../unit_test.hpp"

namespace this_process
{
void simple_test()
{

}

}



ASIO_TEST_SUITE
(
        "this_process",
        ASIO_TEST_CASE(this_process::simple_test)
)