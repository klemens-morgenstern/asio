//
// process/limit_handles.cpp
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

#include "asio/process/limit_handles.hpp"

#include "../unit_test.hpp"

namespace limit_handles
{
void simple_test()
{
}

}


ASIO_TEST_SUITE
(
        "limit_handles",
        ASIO_TEST_CASE(limit_handles::simple_test)
)