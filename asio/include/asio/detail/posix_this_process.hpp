//
// process/this_process/detail/posix_this_process.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_THIS_PROCESS_POSIX_THIS_PROCESS_HPP
#define ASIO_DETAIL_THIS_PROCESS_POSIX_THIS_PROCESS_HPP

#include "asio/detail/config.hpp"
#include "asio/detail/push_options.hpp"

#include <filesystem>

namespace asio
{
namespace detail
{
namespace this_process
{
namespace posix
{

namespace env
{

template<typename Char>
using key_char_traits = std::char_traits<Char>;

template<typename Char>
using value_char_traits = std::char_traits<Char>;

using char_type = char;

constexpr char equality_sign = '=';
constexpr char separator = ':';

inline std::basic_string<char_type, value_char_traits<char_type>> get(
        const std::basic_string<char_type, key_char_traits<char_type>> & key,
        error_code & ec)
{
    auto res = ::getenv(key.c_str());
    if (res == nullptr)
    {
        ec.assign(errno, asio::error::get_system_category());
        return {};
    }
    return res;
}

inline void set(const std::basic_string<char_type, key_char_traits<char_type>> & key,
                const std::basic_string<char_type, value_char_traits<char_type>> & value,
                error_code & ec)
{
    if (::setenv(key.c_str(), value.c_str(), true))
        ec.assign(errno, asio::error::get_system_category());
}

inline void unset(const std::basic_string<char_type, key_char_traits<char_type>> & key,
                  error_code & ec)
{
    if (::unsetenv(key.c_str()))
        ec.assign(errno, asio::error::get_system_category());
}


using native_handle = char**;
using native_iterator = char**;

inline native_handle load() { return ::environ; }

char ** find_end(native_handle nh)
{
    while (*nh != nullptr)
        nh++;
    return nh;
}


}

inline pid_t get_id() {return ::getpid(); }

}
}
}
}

#endif