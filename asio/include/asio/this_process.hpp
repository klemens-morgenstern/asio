//
// process/this_process.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ASIO_THIS_PROCESS_HPP
#define ASIO_THIS_PROCESS_HPP

#include "asio/detail/config.hpp"

#if defined(ASIO_WINDOWS)
#include "asio/detail/windows_env.hpp"
#else
#include "asio/detail/posix_this_process.hpp"
#endif


#include "asio/detail/push_options.hpp"


namespace asio
{
namespace this_process
{

using posix_traits = std::char_traits<char>;

/// Get the process id of the current process.
#if defined(ASIO_WINDOWS)
#else
using asio::detail::this_process::posix::get_id;
#endif

namespace env
{

#if defined(ASIO_WINDOWS)

using asio::detail::this_process::posix::env::key_char_traits; // needs to be case insensitive
using asio::detail::this_process::posix::env::value_char_traits;
using asio::detail::this_process::posix::env::char_type;

using asio::detail::this_process::posix::env::equality_sign;
using asio::detail::this_process::posix::env::separator;

using asio::detail::this_process::posix::env::get;
using asio::detail::this_process::posix::env::set;
using asio::detail::this_process::posix::env::unset;

using asio::detail::this_process::posix::env::native_handle; // this needs to be owning on windows.

#else

using asio::detail::this_process::posix::env::key_char_traits;
using asio::detail::this_process::posix::env::value_char_traits;
using asio::detail::this_process::posix::env::char_type;

using asio::detail::this_process::posix::env::equality_sign;
using asio::detail::this_process::posix::env::separator;

using asio::detail::this_process::posix::env::get;
using asio::detail::this_process::posix::env::set;
using asio::detail::this_process::posix::env::unset;

using asio::detail::this_process::posix::env::native_handle; // this needs to be owning on windows.

#endif

struct key;   //similar to filesystem::path with key_char_traits<char_type>
struct value; //similar to filesystem::path with value_char_traits<char_type>
struct key_value; //similar to filesystem::path with char_traits<char_type>

struct key_view;       //similar to filesystem::path_view (http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1030r4.pdf) with key_char_traits<char_type>
struct value_view;     //similar to filesystem::path_view (http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1030r4.pdf) with value_char_traits<char_type>
struct key_value_view; //similar to filesystem::path_view (http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1030r4.pdf) with char_traits<char_type>



}
}
}


#include "asio/detail/pop_options.hpp"

#endif //ASIO_THIS_PROCESS_HPP
