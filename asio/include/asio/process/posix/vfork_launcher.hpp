//
// process/posix/vfork_launcher.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASIO_VFORK_LAUNCHER_HPP
#define ASIO_VFORK_LAUNCHER_HPP

#include "asio/detail/config.hpp"

#if !defined(ASIO_WINDOWS)

#include "asio/posix/basic_descriptor.hpp"

#include "asio/detail/push_options.hpp"

namespace asio
{
namespace posix
{

template<typename Executor = asio::any_io_executor>
struct vfork_launcher
{
    using Executor = executor_type;

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

  public:
    void set_error(const std::error_code & ec, const char* msg = "Unknown Error.")
    {
        _ec = ec;
        _error_msg = msg;
    }

    template<typename Args>
    auto prepare_args(const std::filesystem::path &exe, Args && args)
    {
        std::vector<char*> vec{args.size() + 2, nullptr};
        vec[0] = const_cast<char*>(exe.c_str());
        std::transform(std::begin(args), std::end(args), vec.begin() + 1, [](std::string_view sv){return const_cast<char*>(sv.data());});
        return vec;
    }
    template<typename Args, typename ... Inits>
    auto launch(executor_type executor, const std::filesystem::path &exe, Args && args, Inits && ... inits) -> process
    {
        process proc = launch(std::move(executor), exec, std::forward<Args>(args), std::forward<Inits>(inits)...);
        if (_ec)
            asio::detail::throw_error(_ec, _error_msg != nullptr ? _error_msg : "process launch failed");
        return proc;
    }

    template<typename Args, typename ... Inits>
    auto launch(executor_type executor, error_code & ec, const std::filesystem::path &exe, Args && args, Inits && ... inits) -> process
    {
        //arg store
        auto arg_store = prepare_args(exe, std::forward<Args>(args));
        auto cmd_line = arg_store.data();

        if (!_ec)
            (_on_setup(inits),...);

        if (_ec)
        {
            (_on_error(inits),...);
            return {executor};
        }
        executor.context().notify_fork(asio::execution_context::fork_prepare);
        pid = ::fork();
        if (pid == -1)
        {
            set_error(get_last_error(), "fork() failed");
            (_on_error(inits),...);
            (_on_fork_error(inits),...);
            return {};
        }
        else if (pid == 0)
        {
            executor.context().notify_fork(asio::execution_context::fork_child);
            ::close(p.p[0]);
            (_on_exec_setup(inits),...);

            ::execve(exe.c_str(), cmd_line, env);
            set_error(get_last_error(), "execve failed");
            _exit(EXIT_FAILURE);
            return {executor};
        }
        executor.context().notify_fork(asio::execution_context::fork_parent);

        ::close(p.p[1]);
        p.p[1] = -1;

        if (_ec)
        {
            (_on_error(inits),...);
            return {executor};
        }
        process proc{pid};
        (_on_success(inits),...);

        if (_ec)
        {
            (_on_error(inits),...);
            return {executor};
        }

        return proc;
    }


    char **env = ::environ;
};


}
}
#endif //ASIO_VFORK_LAUNCHER_HPP
