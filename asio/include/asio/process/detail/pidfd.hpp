//
// process/detail/pipfd.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASIO_PROCESS_DETAIL_PIDFD_HPP
#define ASIO_PROCESS_DETAIL_PIDFD_HPP

#include "asio/detail/config.hpp"

#if !defined(ASIO_WINDOWS)

#include "asio/posix/basic_descriptor.hpp"

#include "asio/detail/push_options.hpp"

namespace asio
{
namespace detail
{

#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434   /* System call # on most architectures */
#endif

inline int pidfd_open(pid_t pid, unsigned int flags)
{
    return syscall(__NR_pidfd_open, pid, flags);
}

template<typename Executor = asio::any_io_executor>
struct basic_process_handle : protected posix::basic_descriptor<Executor>
{
    /// The type of the executor associated with the object.
    typedef Executor executor_type;

    /// Rebinds the descriptor type to another executor.
    template <typename Executor1>
    struct rebind_executor
    {
        /// The descriptor type when rebound to the specified executor.
        typedef basic_process_handle<Executor1> other;
    };

    /// The native representation of a descriptor.
    typedef int native_handle_type;

    native_handle_type native_handle() const { return pid_; }

    explicit basic_process_handle(int pid) : pid_(pid)
    {}

    basic_process_handle(const executor_type & ex) : posix::basic_descriptor<Executor>(ex) {}

    template <typename ExecutionContext>
    basic_process_handle(ExecutionContext& context, typename constraint<is_convertible<ExecutionContext&, execution_context&>::value>::type = 0)
        : posix::basic_descriptor<Executor>(context)
    {
    }

    basic_process_handle(const executor_type & ex, native_handle_type pid) : pid_(pid), posix::basic_descriptor<Executor>(ex, pidfd_open(pid, 0)) {}

    template <typename ExecutionContext>
    basic_process_handle(ExecutionContext& context,  native_handle_type pid,
                         typename constraint<is_convertible<ExecutionContext&, execution_context&>::value>::type = 0)
            : pid_(pid), posix::basic_descriptor<Executor>(context, pidfd_open(pid, 0))
    {}

    void assign(native_handle_type native_descriptor)
    {
        asio::error_code ec;
        pid_ = native_descriptor;
        auto pidfd = pidfd_open(pid_, 0);

        if (pidfd < 0)
            ec.assign(errno, asio::system_category());
        else
            posix::basic_descriptor<Executor>::assign(pidfd);

        asio::detail::throw_error(ec, "assign");
    }

    ASIO_SYNC_OP_VOID assign(const native_handle_type& native_descriptor,
                             asio::error_code& ec)
    {
        pid_ = native_descriptor;
        auto pidfd = pidfd_open(pid_, 0);
        if (pidfd < 0)
            ec.assign(errno, asio::system_category());
        else
            posix::basic_descriptor<Executor>::assign(pidfd);

        ASIO_SYNC_OP_VOID_RETURN(ec);
    }
    executor_type get_executor() {return posix::basic_descriptor<Executor>::get_executor();}

    ~basic_process_handle() {}

    basic_process_handle(const basic_process_handle & c) = delete;
    basic_process_handle(basic_process_handle && c) : pid_(c.pid_),
                                                      posix::basic_descriptor<Executor>(std::move(c.pid_descriptor_))
    {
        c.pid_ = -1;
    }
    basic_process_handle &operator=(const basic_process_handle & c) = delete;
    basic_process_handle &operator=(basic_process_handle && c)
    {
        pid_ = c.pid_;
        c.pid_ = -1;
        return *this;
    }

    void wait()
    {
        posix::basic_descriptor<Executor>::wait(posix::descriptor_base::wait_read);
    }

    ASIO_SYNC_OP_VOID wait(error_code & ec)
    {
        return posix::basic_descriptor<Executor>::wait(posix::descriptor_base::wait_read, ec);
    }

    template <ASIO_COMPLETION_TOKEN_FOR(void (asio::error_code))
            WaitHandler ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    ASIO_INITFN_AUTO_RESULT_TYPE(WaitHandler,
                                 void (asio::error_code))
    async_wait(ASIO_MOVE_ARG(WaitHandler) handler
               ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return posix::basic_descriptor<Executor>::async_wait(
                posix::descriptor_base::wait_read,
                ASIO_MOVE_OR_LVALUE(WaitHandler)(handler));
    }

    int id() const
    {
        return pid_;
    }
    bool valid() const
    {
        return pid_ != -1;
    }
  private:
    int pid_ {-1};
};


}
}

#include "asio/detail/pop_options.hpp"


#endif
#endif //ASIO_PROCESS_DETAIL_PIDFD_HPP
