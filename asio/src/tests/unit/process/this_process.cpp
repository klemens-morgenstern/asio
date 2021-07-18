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
    namespace env = asio::this_process::env;
    for (auto [key, value] : env::view())
    {
        auto ptr = ::getenv(key.string().c_str());
        ASIO_CHECK(ptr == value);
        ASIO_CHECK(env::get(key) == ptr);
        std::cout << "Key: " << key << " - " << value <<  " == '"  << ptr << "'" << std::endl;
        for (auto pt : value)
            std::cout << "    Val: " << pt << std::endl;


    }

    {
        auto v = env::view();
        auto itr = std::find_if(v.begin(), v.end(), [](env::key_value_pair kvp) {return kvp.key_view() == "ASIO_ENV_TEST";});
        ASIO_CHECK(itr == v.end());
        asio::error_code ec;
        ASIO_CHECK(env::get("ASIO_ENV_TEST", ec).empty());
    }
    {
        asio::error_code ec;
        env::set("ASIO_ENV_TEST", "123", ec);
        ASIO_CHECK_MESSAGE(!ec, ec.message());
        auto v = env::view();
        auto itr = std::find_if(v.begin(), v.end(), [](env::key_value_pair kvp) {return kvp.key_view() == "ASIO_ENV_TEST";});
        ASIO_CHECK(itr != v.end());
        ASIO_CHECK(itr->value_view() == "123");
        ASIO_CHECK(env::get("ASIO_ENV_TEST") == "123");
    }
    {
        env::unset("ASIO_ENV_TEST");
        auto v = env::view();
        auto itr = std::find_if(v.begin(), v.end(), [](env::key_value_pair kvp) {return kvp.key_view() == "ASIO_ENV_TEST";});
        ASIO_CHECK(itr == v.end());
        asio::error_code ec;

        ASIO_CHECK(env::get("ASIO_ENV_TEST", ec).empty());
    }
}

}



ASIO_TEST_SUITE
(
        "this_process",
        ASIO_TEST_CASE(this_process::simple_test)
)