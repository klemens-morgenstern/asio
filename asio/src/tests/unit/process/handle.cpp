//
// process/handle.cpp
// ~~~~~~~~~~~~~
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

// Test that header file is self-contained.
#include "asio/process/handle.hpp"
#include "asio/process.hpp"
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include "../unit_test.hpp"

namespace process
{

#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434   /* System call # on most architectures */
#endif

static int
pidfd_open(pid_t pid, unsigned int flags)
{
    return syscall(__NR_pidfd_open, pid, flags);
}

void dev_test()
{
    asio::io_context ctx;
    auto pid = ::fork();
    assert(pid >= 0u);
    if (pid == 0u)
    {
        ::sleep(1);
        ::exit(32);
    }

    asio::detail::basic_process_handle<> pc{ctx, pid};

    int done = false;

    pc.async_wait(
            [&](asio::error_code ec)
            {
                done = true;
                ASIO_CHECK(!ec);
                int result = -1;
                ::waitpid(pid, &result, WNOHANG);
                ASIO_CHECK(WIFEXITED(  result));
                ASIO_CHECK(WEXITSTATUS(result) == 32);
            });

    ctx.run();

    ASIO_CHECK(done);
}

}

ASIO_TEST_SUITE
(
    "process",
    ASIO_TEST_CASE(process::dev_test)
)
