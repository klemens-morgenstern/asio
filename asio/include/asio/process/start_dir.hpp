//
// process/start_dir.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASIO_PROCESS_START_DIR_HPP
#define ASIO_PROCESS_START_DIR_HPP

#include <detail/process/config.hpp>
#include <filesystem>

#if defined(__unix__)
#include <unistd.h>
#endif

namespace asio
{

struct process_start_dir
{
    process_start_dir(const std::filesystem::path &s) : s_(s) {}

#if defined(ASIO_HAS_IOCP)
    template <class Executor>
    void on_setup(Executor& exec) const
    {
        exec.work_dir = s_.c_str();
    }
#else
    template <class PosixExecutor>
    void on_exec_setup(PosixExecutor&) const
    {
        ::chdir(s_.c_str());
    }

    template <class Executor>
    void on_setup(Executor& exec) const
    {
    }
#endif

  private:
    std::filesystem::path s_;
};

}


#endif //ASIO_PROCESS_START_DIR_HPP
