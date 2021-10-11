/*
Copyright ©2021 Specta Omnis LLC
Copyright ©2021 Richard Hodges (richard@power.trade)

All Rights Reserved.

IN NO EVENT SHALL SPECTRA OMNIS LLC or RICHARD HODGES BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
EVEN IF SPECTRA OMNIS LLC OR RICHARD HODGES HAS BEEN ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.

SPECTRA OMNIS LLC AND RICHARD HODGES SPECIFICALLY DISCLAIMS ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO
PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
#ifndef ASIO_BASIC_PROCESS_HPP
#define ASIO_BASIC_PROCESS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#include "asio/any_io_executor.hpp"
#include "asio/process/detail/handle.hpp"

#include "asio/detail/push_options.hpp"

#include <type_traits>
#include <filesystem>

namespace asio
{

template<typename Init, typename Launcher>
concept process_initializer =
   (  requires(Init initializer, Launcher launcher) { {initializer.on_setup(launcher)}; }
   || requires(Init initializer, Launcher launcher) { {initializer.on_success(launcher)}; }
   || requires(Init initializer, Launcher launcher) { {initializer.on_error(launcher, std::error_code())}; }
);

template<typename Launcher, typename Args = std::initializer_list<std::string_view>, typename ...Initializers>
concept process_launcher =
(
        std::is_convertible_v<decltype(*std::begin(std::declval<Args>())), std::  string_view> ||
        std::is_convertible_v<decltype(*std::begin(std::declval<Args>())), std:: wstring_view> ||
        std::is_convertible_v<decltype(*std::begin(std::declval<Args>())), std::u8string_view>
)  &&
requires(any_io_executor exec, Launcher launcher, Args a, ::std::filesystem::path p, Initializers ... initializers) {
    { launcher.set_error(std::error_code(), "message") };
    { launcher.launch(exec, p, a, initializers...) } ;
} &&  (process_initializer<Launcher, Initializers> &&  ...);

class default_process_launcher;


template<typename Executor = any_io_executor>
struct basic_process
{
    using executor_type = Executor;
    executor_type get_executor() {return _process_handle.get_executor();}

    using pid_type = detail::pid_type;

    // Provides access to underlying operating system facilities
    using native_handle_type = typename detail::basic_process_handle<executor_type>::native_handle_type;

    // Construct a child from a property list and launch it.
    template<process_initializer<default_process_launcher>  ... Inits>
    explicit basic_process(
                    executor_type executor,
                    const std::filesystem::path& exe,
                    std::initializer_list<std::wstring_view> args,
                    Inits&&... inits);

    // Construct a child from a property list and launch it with a custom process launcher
    template<process_launcher Launcher, process_initializer<Launcher> ... Inits>
    explicit basic_process(
                    executor_type executor,
                    const std::filesystem::path& exe,
                    std::initializer_list<std::wstring_view> args,
                    Inits&&... inits,
                    Launcher&& launcher) : basic_process(launcher.launch(std::move(executor), exe, args, std::forward<Inits>(inits)...)) {}

    // Construct a child from a property list and launch it.
    template<process_initializer<default_process_launcher>  ... Inits>
    explicit basic_process(
                    executor_type executor,
                    const std::filesystem::path& exe,
                    std::initializer_list<std::string_view> args,
                    Inits&&... inits);

    // Construct a child from a property list and launch it with a custom process launcher
    template<process_launcher Launcher, process_initializer<Launcher> ... Inits>
    explicit basic_process( executor_type executor,
                            const std::filesystem::path& exe,
                            std::initializer_list<std::string_view> args,
                            Inits&&... inits,
                            Launcher&& launcher) : basic_process(launcher.launch(std::move(executor), exe, args, std::forward<Inits>(inits)...)) {}

    // Construct a child from a property list and launch it.
    template<typename Args, process_initializer<default_process_launcher>  ... Inits>
    explicit basic_process(
            executor_type executor,
            const std::filesystem::path& exe,
            Args&& args, Inits&&... inits);

    // Construct a child from a property list and launch it with a custom process launcher
    template<process_launcher Launcher, typename Args,
            process_initializer<Launcher> ... Inits>
    explicit basic_process(
                     executor_type executor,
                     const std::filesystem::path& exe,
                     Args&& args,
                     Inits&&... inits,
                     Launcher&& launcher) : basic_process(launcher.launch(std::move(executor), exe, args, std::forward<Inits>(inits)...)) {}

    // Attach to an existing process
    explicit basic_process(executor_type exec, pid_type pid) : _process_handle{std::move(exec), pid} {}

    // Create an invalid handle
    explicit basic_process(executor_type exec) : _process_handle{std::move(exec)} {}

    // An empty process is similar to a default constructed thread. It holds an empty
    // handle and is a place holder for a process that is to be launched later.
    basic_process() = default;

    basic_process(const basic_process&) = delete;
    basic_process& operator=(const basic_process&) = delete;

    basic_process(basic_process&& lhs) : _attached(lhs._attached), _terminated(lhs._terminated), _exit_status{lhs._exit_status}, _process_handle(std::move(lhs._process_handle))
    {
        lhs._attached = false;
    }
    basic_process& operator=(basic_process&& lhs)
    {
        _attached = lhs._attached;
        _terminated = lhs._terminated;
        _exit_status = lhs._exit_status;
        _process_handle = std::move(lhs._process_handle);
        lhs._attached = false;
        return *this;
    }
    // tbd behavior
    ~basic_process()
    {
        if (_attached && !_terminated)
            detail::process::terminate_if_running(_process_handle.native_handle());
    }

    void interrupt()
    {
        error_code ec;
        request_exit(ec);
        if (ec)
            throw system_error(ec, "interrupt failed");

    }
    void interrupt(error_code & ec)
    {
        detail::process::interrupt(_process_handle.id(), ec);
    }

    void request_exit()
    {
        error_code ec;
        request_exit(ec);
        if (ec)
            throw system_error(ec, "request_exit failed");
    }
    void request_exit(error_code & ec)
    {
        detail::process::request_exit(_process_handle.id(), ec);
    }

    void terminate()
    {
        error_code ec;
        terminate(ec);
        if (ec)
            throw system_error(ec, "terminate failed");
    }
    void terminate(error_code & ec)
    {
        detail::process::terminate(_process_handle.id(), _exit_status , ec);
    }

    void wait() { _process_handle.wait(); }
    void wait(error_code & ec) { _process_handle.wait(ec); }

    template <ASIO_COMPLETION_TOKEN_FOR(void (asio::error_code))
    WaitHandler ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    ASIO_INITFN_AUTO_RESULT_TYPE(WaitHandler,
                                 void (asio::error_code))
    async_wait(ASIO_MOVE_ARG(WaitHandler) handler
    ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return _process_handle.async_wait(ASIO_MOVE_OR_LVALUE(WaitHandler)(handler));
    }

    // private member functions
    void detach()
    {
        _attached = false;
    }
    void join() {wait();}
    bool joinable() {return _attached; }

    native_handle_type native_handle() const {return _process_handle.native_handle(); }
    int exit_code() const
    {
        return detail::process::eval_exit_status(_exit_status);
    }

    pid_t id() const {return _process_handle.id();}

    int native_exit_code() const
    {
        return _exit_status;
    }
    bool running()
    {
        error_code ec;
        int exit_code ;
        auto r =  detail::process::is_running(_process_handle.id(), exit_code, ec);
        if (!ec)
            _exit_status = exit_code;
        else
            throw system_error(ec, "running failed");

        return r;
    }
    bool running(error_code & ec) noexcept
    {
        int exit_code ;
        auto r =  detail::process::is_running(_process_handle.id(), exit_code, ec);
        if (!ec)
            _exit_status = exit_code;
        return r;
    }

    bool valid() const { return _process_handle.valid(); }
    explicit operator bool() const {return valid(); }

  private:
    detail::basic_process_handle<Executor> _process_handle;
    bool _attached{true};
    bool _terminated{false};
    int _exit_status{detail::process::still_active};
};

using process = basic_process<>;

}

#include "asio/detail/push_options.hpp"
#include "asio/process/launcher.hpp"

template<typename Executor>
template<asio::process_initializer<asio::default_process_launcher>  ... Inits>
asio::basic_process<Executor>::basic_process(
            executor_type executor,
            const std::filesystem::path& exe,
            std::initializer_list<std::wstring_view> args,
            Inits&&... inits) :
                basic_process(std::move(executor), exe, args,
                      std::forward<Inits>(inits)..., default_process_launcher{})
{
}


template<typename Executor>
template<asio::process_initializer<asio::default_process_launcher>  ... Inits>
asio::basic_process<Executor>::basic_process(
            executor_type executor,
            const std::filesystem::path& exe,
            std::initializer_list<std::string_view> args,
            Inits&&... inits) :
                basic_process(std::move(executor), exe, args,
                              std::forward<Inits>(inits)..., default_process_launcher{})
{
}


template<typename Executor>
template<typename Args, asio::process_initializer<asio::default_process_launcher>  ... Inits>
asio::basic_process<Executor>::basic_process(
            executor_type executor,
            const std::filesystem::path& exe,
            Args&& args, Inits&&... inits)
 : basic_process(std::move(executor), exe, args, std::forward<Inits>(inits)..., default_process_launcher{})
{
}




#endif //ASIO_BASIC_PROCESS_HPP
