//
// process/posix/process_launcher.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASIO_DEFAULT_PROCESS_LAUNCHER_HPP
#define ASIO_DEFAULT_PROCESS_LAUNCHER_HPP
#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#include "asio/process/detail/handle.hpp"

#include "asio/detail/push_options.hpp"

#include <filesystem>
#include <sys/wait.h>

namespace asio
{

template<typename Executor>
struct basic_process;

struct default_process_launcher;

template<typename Init, typename Launcher = default_process_launcher>
concept on_setup_init = requires(Init initializer, Launcher launcher) { {initializer.on_setup(launcher)}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_success_init = requires(Init initializer, Launcher launcher) { {initializer.on_success(launcher)}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_error_init = requires(Init initializer, Launcher launcher) { {initializer.on_error(launcher, std::error_code())}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_fork_error_init = requires(Init initializer, Launcher launcher) { {initializer.on_fork_error(launcher, std::error_code())}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_exec_setup_init = requires(Init initializer, Launcher launcher) { {initializer.on_exec_setup(launcher)}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_exec_error_init = requires(Init initializer, Launcher launcher) { {initializer.on_exec_error(launcher, std::error_code())}; };


struct default_process_launcher
{
    std::error_code _ec;
    const char * _error_msg = nullptr;

    template<typename Initializer>
    void _on_setup(Initializer &&initializer) {}

    template<typename Initializer> requires on_setup_init<Initializer>
    void _on_setup(Initializer &&init) { init.on_setup(*this); }

    template<typename Initializer>
    void _on_error(Initializer &&initializer) {}

    template<typename Initializer> requires on_error_init<Initializer>
    void _on_error(Initializer &&init) { init.on_error(*this, _ec); }

    template<typename Initializer>
    void _on_success(Initializer &&initializer) {}

    template<typename Initializer> requires on_success_init<Initializer>
    void _on_success(Initializer &&init) { init.on_success(*this); }

    template<typename Initializer>
    void _on_fork_error(Initializer &&initializer) {}

    template<typename Initializer> requires on_fork_error_init<Initializer>
    void _on_fork_error(Initializer &&init) { init.on_success(*this); }

    template<typename Initializer>
    void _on_exec_setup(Initializer &&initializer) {}

    template<typename Initializer> requires on_exec_setup_init<Initializer>
    void _on_exec_setup(Initializer &&init) { init.on_exec_setup(*this); }

    template<typename Initializer>
    void _on_exec_error(Initializer &&initializer) {}

    template<typename Initializer> requires on_exec_error_init<Initializer>
    void _on_exec_error(Initializer &&init) { init.on_exec_error(*this, _ec); }


    void _write_error(int sink, const std::string & msg)
    {
        int data[2] = {_ec.value(),static_cast<int>(msg.size())};
        while (::write(sink, &data[0], sizeof(int) *2) == -1)
        {
            auto err = errno;

            if (err == EBADF)
                return;
            else if ((err != EINTR) && (err != EAGAIN))
                break;
        }
        while (::write(sink, &msg.front(), msg.size()) == -1)
        {
            auto err = errno;

            if (err == EBADF)
                return;
            else if ((err != EINTR) && (err != EAGAIN))
                break;
        }
    }

    std::string _msg_buffer;

    void _read_error(int source)
    {
        int data[2];

        _ec.clear();
        int count = 0;
        while ((count = ::read(source, &data[0], sizeof(int) *2 ) ) == -1)
        {
            //actually, this should block until it's read.
            auto err = errno;
            if ((err != EAGAIN ) && (err != EINTR))
                set_error(std::error_code(err, std::system_category()), "Error read pipe");
        }
        if (count == 0)
            return  ;

        asio::error_code ec(data[0], std::system_category());
        std::string msg(data[1], ' ');

        while (::read(source, &msg.front(), msg.size() ) == -1)
        {
            //actually, this should block until it's read.
            auto err = errno;
            if ((err == EBADF) || (err == EPERM))//that should occur on success, therefore return.
                return;
                //EAGAIN not yet forked, EINTR interrupted, i.e. try again
            else if ((err != EAGAIN ) && (err != EINTR))
                set_error(std::error_code(err, std::system_category()), "Error read pipe");
        }
        _msg_buffer = std::move(msg);
        set_error(ec, _msg_buffer.c_str());
    }

  public:
    void set_error(const std::error_code & ec, const char* msg = "Unknown Error.")
    {
        _ec = ec;
        _error_msg = msg;
    }

    template<typename Args>
    auto prepare_args(const std::filesystem::path &exe, Args && args)
    {
        std::vector<char*> vec{std::size(args) + 2, nullptr};
        vec[0] = const_cast<char*>(exe.c_str());
        std::transform(std::begin(args), std::end(args), vec.begin() + 1,
                       [](std::string_view sv){return const_cast<char*>(sv.data());});
        return vec;
    }

    template<typename Executor, typename Args, typename ... Inits>
    auto launch(Executor executor, const std::filesystem::path &exe, Args && args, Inits && ... inits) -> basic_process<Executor>
    {
        basic_process<Executor> proc = launch(std::move(executor), _ec, exe, std::forward<Args>(args), std::forward<Inits>(inits)...);
        if (_ec)
            asio::detail::throw_error(_ec, _error_msg != nullptr ? _error_msg : "process launch failed");
        return proc;
    }

    template<typename Executor, typename Args, typename ... Inits>
    auto launch(Executor executor, error_code & , const std::filesystem::path &exe, Args && args, Inits && ... inits) -> basic_process<Executor>
    {
        //arg store
        auto arg_store = prepare_args(exe, std::forward<Args>(args));
        auto cmd_line = arg_store.data();
        struct pipe_guard
        {
            int p[2];
            pipe_guard() : p{-1,-1} {}

            ~pipe_guard()
            {
                if (p[0] != -1)
                    ::close(p[0]);
                if (p[1] != -1)
                    ::close(p[1]);
            }
        };
        pid_t pid{-1};
        {
            pipe_guard p;

            if (::pipe(p.p) == -1)
                set_error(error_code{errno, asio::error::get_system_category()}, "pipe(2) failed");
            else if (::fcntl(p.p[1], F_SETFD, FD_CLOEXEC) == -1)
                set_error(error_code{errno, asio::error::get_system_category()}, "fcntl(2) failed");
            //this might throw, so we need to be sure our pipe is safe.

            if (!_ec)
                (_on_setup(inits),...);

            if (_ec)
            {
                (_on_error(inits),...);
                return basic_process<Executor>{executor};
            }
            executor.context().notify_fork(asio::execution_context::fork_prepare);
            pid = ::fork();
            if (pid == -1)
            {
                set_error(error_code{errno, asio::error::get_system_category()}, "fork() failed");
                (_on_error(inits),...);
                (_on_fork_error(inits),...);
                return basic_process<Executor>{executor};
            }
            else if (pid == 0)
            {
                executor.context().notify_fork(asio::execution_context::fork_child);
                ::close(p.p[0]);
                (_on_exec_setup(inits),...);

                ::execve(exe.c_str(), cmd_line, env);
                set_error(error_code{errno, asio::error::get_system_category()}, "execve failed");

                _write_error(p.p[1], _error_msg);
                ::close(p.p[1]);

                _exit(EXIT_FAILURE);
                return basic_process<Executor>{executor, pid};
            }
            executor.context().notify_fork(asio::execution_context::fork_parent);

            ::close(p.p[1]);
            p.p[1] = -1;
            _read_error(p.p[0]);

        }
        if (_ec)
        {
            (_on_error(inits),...);
            ::waitpid(pid, nullptr, O_NONBLOCK);
            return basic_process<Executor>{executor};
        }
        basic_process<Executor> proc{executor, pid};
        (_on_success(inits),...);

        if (_ec)
        {
            (_on_error(inits),...);
            return basic_process<Executor>{executor};
        }

        return proc;
    }

    char **env = ::environ;
};



}

#include "asio/detail/pop_options.hpp"

#endif //ASIO_DEFAULT_PROCESS_LAUNCHER_HPP
