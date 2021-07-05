//
// process/detail/posix.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASIO_PROCESS_DETAIL_POSIX_HANDLE_HPP
#define ASIO_PROCESS_DETAIL_POSIX_HANDLE_HPP

#include "asio/detail/config.hpp"

#if !defined(ASIO_WINDOWS) || !defined(__CYGWIN__)

#include <wait.h>
#include <unistd.h>

namespace asio {
namespace detail {
namespace process {

ASIO_CONSTEXPR static int still_active = 0x017f;
static_assert(WIFSTOPPED(still_active), "Expected still_active to indicate WIFSTOPPED");
static_assert(!WIFEXITED(still_active), "Expected still_active to not indicate WIFEXITED");
static_assert(!WIFSIGNALED(still_active), "Expected still_active to not indicate WIFSIGNALED");
static_assert(!WIFCONTINUED(still_active), "Expected still_active to not indicate WIFCONTINUED");


ASIO_CONSTEXPR inline int eval_exit_status(int code)
{
  if (WIFEXITED(code))
    return WEXITSTATUS(code);
  else if (WIFSIGNALED(code))
    return WTERMSIG(code);
  else
    return code;
}

ASIO_CONSTEXPR inline bool is_code_running(int code)
{
  return !WIFEXITED(code) && !WIFSIGNALED(code);
}

inline bool is_running(int pid, int & exit_code)
{
    if (!is_code_running(exit_code))
        return false;
    auto ret = ::waitpid(pid, &exit_code, WNOHANG);

    if (ret == -1)
        asio::detail::throw_error(
                asio::error_code(errno, asio::system_category()), "waitpid() failed");
    return is_code_running(exit_code);
}


void terminate_if_running(int pid)
{
    int exit_code;
    if (!is_code_running(exit_code))
        return;
    auto ret = ::waitpid(pid, &exit_code, WNOHANG);

    if (is_code_running(exit_code))
        ::kill(pid, SIGKILL);
}

void terminate(int pid) noexcept
{
    if (::kill(pid, SIGKILL) == -1)
        asio::detail::throw_error(
                asio::error_code(errno, asio::system_category()), "terminate() failed");

    ::waitpid(pid, nullptr, WNOHANG); //just to clean it up
}



}
}
}

#endif

#endif //ASIO_PROCESS_DETAIL_POSIX_HANDLE_HPP
