//
// experimental/blocking.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot d dot morgenstern at gmail dot com
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_EXPERIMENTAL_BLOCKING_HPP
#define ASIO_EXPERIMENTAL_BLOCKING_HPP

#include <asio/any_io_executor.hpp>
#include <asio/system_context.hpp>
#include <asio/thread_pool.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_future.hpp>

#include <optional>

namespace asio
{

namespace detail
{

namespace experimental
{

template<typename Rep, typename Period, typename Executor>
struct blocking_run_for_impl
{
    std::chrono::duration<Rep, Period> duration;

    blocking_run_for_impl(const std::chrono::duration<Rep, Period> &duration) : duration(duration)
    {}
};

template<typename Clock, typename Duration, typename Executor>
struct blocking_run_until_impl
{
    std::chrono::time_point<Clock, Duration> time_point;

    blocking_run_until_impl(const std::chrono::time_point<Clock, Duration> &time_point) : time_point(time_point)
    {}
};


template<typename Rep, typename Period, typename Executor>
struct blocking_poll_for_impl
{
    std::chrono::duration<Rep, Period> duration;

    blocking_poll_for_impl(const std::chrono::duration<Rep, Period> &duration) : duration(duration)
    {}
};

template<typename Clock, typename Duration, typename Executor>
struct blocking_poll_until_impl
{
    std::chrono::time_point<Clock, Duration> time_point;

    blocking_poll_until_impl(const std::chrono::time_point<Clock, Duration> &time_point) : time_point(time_point)
    {}
};

}

}

namespace experimental
{

template <typename Executor = any_io_executor>
struct blocking_run_t
{
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, system_context>::value, "not supported yet");
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, thread_pool>::value, "not supported yet");
};

constexpr blocking_run_t<> blocking_run;

template <typename Executor = any_io_executor>
struct blocking_run_for_t
{
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, system_context>::value, "not supported yet");
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, thread_pool>::value, "not supported yet");

    template <typename Rep, typename Period>
    detail::experimental::blocking_run_for_impl<Rep, Period, Executor> operator()(const std::chrono::duration<Rep, Period> &duration) const
    {
        return detail::experimental::blocking_run_for_impl<Rep, Period, Executor>(duration);
    }
};

constexpr blocking_run_for_t<> blocking_run_for;

template <typename Executor = any_io_executor>
struct blocking_run_until_t
{
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, system_context>::value, "not supported yet");
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, thread_pool>::value, "not supported yet");

    template <typename Clock, typename Duration>
    detail::experimental::blocking_run_until_impl<Clock, Duration, Executor> operator()(const std::chrono::time_point<Clock, Duration> &time_point) const
    {
        return detail::experimental::blocking_run_until_impl<Clock, Duration, Executor>(time_point);
    }
};

constexpr blocking_run_until_t<> blocking_run_until;



template <typename Executor = any_io_executor>
struct blocking_poll_t
{
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, system_context>::value, "not supported yet");
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, thread_pool>::value, "not supported yet");
};

constexpr blocking_poll_t<> blocking_poll;


template <typename Executor = any_io_executor>
struct blocking_poll_for_t
{
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, system_context>::value, "not supported yet");
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, thread_pool>::value, "not supported yet");

    template <typename Rep, typename Period>
    detail::experimental::blocking_poll_for_impl<Rep, Period, Executor> operator()(const std::chrono::duration<Rep, Period> &duration) const
    {
        return detail::experimental::blocking_poll_for_impl<Rep, Period, Executor>(duration);
    }
};

constexpr blocking_poll_for_t<> blocking_poll_for;




template <typename Executor = any_io_executor>
struct blocking_poll_until_t
{
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, system_context>::value, "not supported yet");
    static_assert(!is_same<typename query_result<Executor, execution::context_t>::type, thread_pool>::value, "not supported yet");

    template <typename Clock, typename Duration>
    detail::experimental::blocking_poll_until_impl<Clock, Duration, Executor> operator()(const std::chrono::time_point<Clock, Duration> &time_point) const
    {
        return detail::experimental::blocking_poll_until_impl<Clock, Duration, Executor>(time_point);
    }
};

constexpr blocking_poll_until_t<> blocking_poll_until;

}

namespace detail
{
namespace experimental
{

inline void do_blocking_run(io_context & ctx, std::atomic<bool> & done)
{
    if (ctx.stopped())
        ctx.restart();

    while (!done.load())
        try
        {
            ctx.run_one();
        }
        catch (...)
        {
            asio::post([ex = std::current_exception()]{std::rethrow_exception(ex);});
        }

}

inline void do_blocking_run(io_context::executor_type exec, std::atomic<bool> & done)
{
    do_blocking_run(exec.context(), done);
}

inline void do_blocking_run(any_io_executor exec, std::atomic<bool> & done)
{
    if (auto * io_exec = exec.target<io_context::executor_type>())
        do_blocking_run(*io_exec, done);
    else
        asio::detail::throw_exception(std::logic_error("blocking not implemented for given executor"));
}

}

}

template <typename Executor, typename R, typename T>
struct async_result<experimental::blocking_run_t<Executor>, R(T)>
{
    using return_type = T;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                experimental::blocking_run_t<Executor>, InitArgs... args) 
    {
        const auto exec = initiation.get_executor();

        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](T t) ASIO_NOEXCEPT
                {
                    result = std::move(t);
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run(exec, done);

        return std::move(result).value();
    }
};

template <typename Executor, typename R>
struct async_result<experimental::blocking_run_t<Executor>, R(std::exception_ptr)>
{
    using return_type = void;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                experimental::blocking_run_t<Executor>, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](std::exception_ptr t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run(exec, done);
        if (error)
            std::rethrow_exception(error);
    }
};

template <typename Executor, typename R>
struct async_result<experimental::blocking_run_t<Executor>, R(error_code)>
{
    using return_type = void;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                experimental::blocking_run_t<Executor>, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code  t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run(exec, done);
        if (error)
            throw system_error(error, "blocking_run failed with error code");
    }
};



template <typename Executor, typename R, typename T>
struct async_result<experimental::blocking_run_t<Executor>, R(std::exception_ptr, T)>
{
    using return_type = T;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                experimental::blocking_run_t<Executor>, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::optional<T> result;
        std::atomic<bool> done{false};

        std::move(initiation)(
                [&](std::exception_ptr ex, T value)
                {
                    result = std::move(value);
                    error = ex;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run(exec, done);
        if (error)
            std::rethrow_exception(error);
        return std::move(result).value();
    }
};

template <typename Executor, typename R, typename T>
struct async_result<experimental::blocking_run_t<Executor>, R(error_code, T)>
{
    using return_type = T;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                experimental::blocking_run_t<Executor>, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code ec, T value)
                {
                    result = std::move(value);
                    error = ec;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run(exec, done);
        if (error)
            throw system_error(error, "blocking_run failed with error code");
        return std::move(result).value();
    }
};

namespace detail
{
namespace experimental
{

template<typename Rep, typename Period>
inline void do_blocking_run_for(io_context & ctx, std::atomic<bool> & done, std::chrono::duration<Rep, Period> dur)
{
    if (ctx.stopped())
        ctx.restart();

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::chrono::duration<Rep, Period>  remaining = dur;

    while (!done.load() && remaining.count() >= 0)
        try
        {
            ctx.run_one_for(dur);
            remaining = dur - std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(std::chrono::steady_clock::now() - start);
        }
        catch (...)
        {
            asio::post([ex = std::current_exception()]{std::rethrow_exception(ex);});
        }
}

template<typename Rep, typename Period>
inline void do_blocking_run_for(io_context::executor_type exec, std::atomic<bool> & done, std::chrono::duration<Rep, Period> dur)
{
    do_blocking_run_for(exec.context(), done, dur);
}

template<typename Rep, typename Period>
inline void do_blocking_run_for(any_io_executor exec, std::atomic<bool> & done, std::chrono::duration<Rep, Period> dur)
{
    if (auto * io_exec = exec.target<io_context::executor_type>())
        do_blocking_run_for(*io_exec, done, dur);
    else
        asio::detail::throw_exception(std::logic_error("blocking not implemented for given executor"));
}
}
}


template <typename Rep, typename Period, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_run_for_impl<Rep, Period, Executor>, R(T)>
{
    using return_type = std::optional<T>;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_run_for_impl<Rep, Period, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](T t) ASIO_NOEXCEPT
                {
                    result = std::move(t);
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run_for(exec, done, to.blocking_run_for_impl);

        return result;
    }
};

template <typename Rep, typename Period, typename Executor, typename R>
struct async_result<detail::experimental::blocking_run_for_impl<Rep, Period, Executor>, R(std::exception_ptr)>
{
    using return_type = bool;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_run_for_impl<Rep, Period, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](std::exception_ptr t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run_for(exec, done, to.blocking_run_for_impl);
        if (error)
            std::rethrow_exception(error);
        return done.load();
    }
};

template <typename Rep, typename Period, typename Executor, typename R>
struct async_result<detail::experimental::blocking_run_for_impl<Rep, Period, Executor>, R(error_code)>
{
    using return_type = bool;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_run_for_impl<Rep, Period, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run_for(exec, done, to.blocking_run_for_impl);
        if (error)
            throw system_error(error, "blocking_run failed with error code");

        return done.load();
    }
};



template <typename Rep, typename Period, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_run_for_impl<Rep, Period, Executor>, R(std::exception_ptr, T)>
{
    using return_type = std::optional<T>;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_run_for_impl<Rep, Period, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::optional<T> result;
        std::atomic<bool> done{false};

        std::move(initiation)(
                [&](std::exception_ptr ex, T value) ASIO_NOEXCEPT
                {
                    result = std::move(value);
                    error = ex;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run_for(exec, done, to.duration);
        if (error)
            std::rethrow_exception(error);
        return result;
    }
};

template <typename Rep, typename Period, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_run_for_impl<Rep, Period, Executor>, R(error_code, T)>
{
    using return_type = std::optional<T>;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_run_for_impl<Rep, Period, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code ec, T value) ASIO_NOEXCEPT
                {
                    result = std::move(value);
                    error = ec;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run_for(exec, done, to.blocking_run_for_impl);
        if (error)
            throw system_error(error, "blocking_run failed with error code");
        return result;
    }
};



namespace detail
{
namespace experimental
{

template<typename Clock, typename Duration>
inline void do_blocking_run_until(io_context & ctx, std::atomic<bool> & done, std::chrono::time_point<Clock, Duration> time_point)
{
    if (ctx.stopped())
        ctx.restart();

    while (!done.load() && (time_point >= Clock::now()))
        try
        {
            ctx.run_one_until(time_point);
        }
        catch (...)
        {
            asio::post([ex = std::current_exception()]{std::rethrow_exception(ex);});
        }
}

template<typename Clock, typename Duration>
inline void do_blocking_run_until(io_context::executor_type exec, std::atomic<bool> & done, std::chrono::time_point<Clock, Duration> time_point)
{
    do_blocking_run_until(exec.context(), done, time_point);
}

template<typename Clock, typename Duration>
inline void do_blocking_run_until(any_io_executor exec, std::atomic<bool> & done, std::chrono::time_point<Clock, Duration> time_point)
{
    if (auto * io_exec = exec.target<io_context::executor_type>())
        do_blocking_run_until(*io_exec, done, time_point);
    else
        asio::detail::throw_exception(std::logic_error("blocking not implemented for given executor"));
}
}
}


template<typename Clock, typename Duration, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_run_until_impl<Clock, Duration, Executor>, R(T)>
{
    using return_type = std::optional<T>;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_run_until_impl<Clock, Duration, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](T t) ASIO_NOEXCEPT
                {
                    result = std::move(t);
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run_until(exec, done, to.time_point);
        return result;
    }
};

template<typename Clock, typename Duration, typename Executor, typename R>
struct async_result<detail::experimental::blocking_run_until_impl<Clock, Duration,Executor>, R(std::exception_ptr)>
{
    using return_type = bool;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_run_until_impl<Clock, Duration, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](std::exception_ptr t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run_until(exec, done, to.time_point);
        if (error)
            std::rethrow_exception(error);
        return done.load();
    }
};

template<typename Clock, typename Duration, typename Executor, typename R>
struct async_result<detail::experimental::blocking_run_until_impl<Clock, Duration, Executor>, R(error_code)>
{
    using return_type = bool;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_run_until_impl<Clock, Duration,Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run_until(exec, done, to.time_point);
        if (error)
            throw system_error(error, "blocking_run failed with error code");

        return done.load();
    }
};

template<typename Clock, typename Duration, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_run_until_impl<Clock, Duration, Executor>, R(std::exception_ptr, T)>
{
    using return_type = std::optional<T>;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_run_until_impl<Clock, Duration, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::optional<T> result;
        std::atomic<bool> done{false};

        std::move(initiation)(
                [&](std::exception_ptr ex, T value) ASIO_NOEXCEPT
                {
                    result = std::move(value);
                    error = ex;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run_until(exec, done, to.time_point);
        if (error)
            std::rethrow_exception(error);
        return result;
    }
};



template<typename Clock, typename Duration, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_run_until_impl<Clock, Duration, Executor>, R(error_code, T)>
{
    using return_type = std::optional<T>;


    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_run_until_impl<Clock, Duration, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code ec, T value) ASIO_NOEXCEPT
                {
                    result = std::move(value);
                    error = ec;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_run_until(exec, done, to.time_point);
        if (error)
            throw system_error(error, "blocking_run failed with error code");
        return result;
    }
};


// poll_one starting here

namespace detail
{
namespace experimental
{

inline void do_blocking_poll(io_context & ctx, std::atomic<bool> & done)
{
    if (ctx.stopped())
        ctx.restart();

    while (!done.load())
        try
        {
            ctx.poll_one();
        }
        catch (...)
        {
            asio::post([ex = std::current_exception()]{std::rethrow_exception(ex);});
        }
}

inline void do_blocking_poll(io_context::executor_type exec, std::atomic<bool> & done)
{
    do_blocking_poll(exec.context(), done);
}

inline void do_blocking_poll(any_io_executor exec, std::atomic<bool> & done)
{
    if (auto * io_exec = exec.target<io_context::executor_type>())
        do_blocking_poll(*io_exec, done);
    else
        asio::detail::throw_exception(std::logic_error("blocking not implemented for given executor"));
}

}

}

template <typename Executor, typename R, typename T>
struct async_result<experimental::blocking_poll_t<Executor>, R(T)>
{
    using return_type = T;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                experimental::blocking_poll_t<Executor>, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](T t) ASIO_NOEXCEPT
                {
                    result = std::move(t);
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll(exec, done);

        return std::move(result).value();
    }
};

template <typename Executor, typename R>
struct async_result<experimental::blocking_poll_t<Executor>, R(std::exception_ptr)>
{
    using return_type = void;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                experimental::blocking_poll_t<Executor>, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](std::exception_ptr t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll(exec, done);
        if (error)
            std::rethrow_exception(error);
    }
};

template <typename Executor, typename R>
struct async_result<experimental::blocking_poll_t<Executor>, R(error_code)>
{
    using return_type = void;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                experimental::blocking_poll_t<Executor>, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code  t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll(exec, done);
        if (error)
            throw system_error(error, "blocking_poll failed with error code");
    }
};



template <typename Executor, typename R, typename T>
struct async_result<experimental::blocking_poll_t<Executor>, R(std::exception_ptr, T)>
{
    using return_type = T;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                experimental::blocking_poll_t<Executor>, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::optional<T> result;
        std::atomic<bool> done{false};

        std::move(initiation)(
                [&](std::exception_ptr ex, T value) ASIO_NOEXCEPT
                {
                    result = std::move(value);
                    error = ex;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll(exec, done);
        if (error)
            std::rethrow_exception(error);
        return std::move(result).value();
    }
};

template <typename Executor, typename R, typename T>
struct async_result<experimental::blocking_poll_t<Executor>, R(error_code, T)>
{
    using return_type = T;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                experimental::blocking_poll_t<Executor>, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code ec, T value) ASIO_NOEXCEPT
                {
                    result = std::move(value);
                    error = ec;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll(exec, done);
        if (error)
            throw system_error(error, "blocking_poll failed with error code");
        return std::move(result).value();
    }
};

namespace detail
{
namespace experimental
{

template<typename Rep, typename Period>
inline void do_blocking_poll_for(io_context & ctx, std::atomic<bool> & done, std::chrono::duration<Rep, Period> dur)
{
    if (ctx.stopped())
        ctx.restart();

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::chrono::duration<Rep, Period>  remaining = dur;

    while (!done.load() && remaining.count() >= 0)
        try
        {
            ctx.poll_one();
            remaining = dur - std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(std::chrono::steady_clock::now() - start);
        }
        catch (...)
        {
            asio::post([ex = std::current_exception()]{std::rethrow_exception(ex);});
        }
}

template<typename Rep, typename Period>
inline void do_blocking_poll_for(io_context::executor_type exec, std::atomic<bool> & done, std::chrono::duration<Rep, Period> dur)
{
    do_blocking_poll_for(exec.context(), done, dur);
}

template<typename Rep, typename Period>
inline void do_blocking_poll_for(any_io_executor exec, std::atomic<bool> & done, std::chrono::duration<Rep, Period> dur)
{
    if (auto * io_exec = exec.target<io_context::executor_type>())
        do_blocking_poll_for(*io_exec, done, dur);
    else
        asio::detail::throw_exception(std::logic_error("blocking not implemented for given executor"));
}
}
}



template <typename Rep, typename Period, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_poll_for_impl<Rep, Period, Executor>, R(T)>
{
    using return_type = std::optional<T>;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_poll_for_impl<Rep, Period, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](T t) ASIO_NOEXCEPT
                {
                    result = std::move(t);
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll_for(exec, done, to.blocking_poll_for_impl);

        return result;
    }
};

template <typename Rep, typename Period, typename Executor, typename R>
struct async_result<detail::experimental::blocking_poll_for_impl<Rep, Period, Executor>, R(std::exception_ptr)>
{
    using return_type = bool;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_poll_for_impl<Rep, Period, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](std::exception_ptr t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll_for(exec, done, to.blocking_poll_for_impl);
        if (error)
            std::rethrow_exception(error);
        return done.load();
    }
};

template <typename Rep, typename Period, typename Executor, typename R>
struct async_result<detail::experimental::blocking_poll_for_impl<Rep, Period, Executor>, R(error_code)>
{
    using return_type = bool;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_poll_for_impl<Rep, Period, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll_for(exec, done, to.blocking_poll_for_impl);
        if (error)
            throw system_error(error, "blocking_poll failed with error code");

        return done.load();
    }
};



template <typename Rep, typename Period, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_poll_for_impl<Rep, Period, Executor>, R(std::exception_ptr, T)>
{
    using return_type = std::optional<T>;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_poll_for_impl<Rep, Period, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::optional<T> result;
        std::atomic<bool> done{false};

        std::move(initiation)(
                [&](std::exception_ptr ex, T value) ASIO_NOEXCEPT
                {
                    result = std::move(value);
                    error = ex;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll_for(exec, done, to.duration);
        if (error)
            std::rethrow_exception(error);
        return result;
    }
};

template <typename Rep, typename Period, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_poll_for_impl<Rep, Period, Executor>, R(error_code, T)>
{
    using return_type = std::optional<T>;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_poll_for_impl<Rep, Period, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code ec, T value) ASIO_NOEXCEPT
                {
                    result = std::move(value);
                    error = ec;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll_for(exec, done, to.blocking_poll_for_impl);
        if (error)
            throw system_error(error, "blocking_poll failed with error code");
        return result;
    }
};



namespace detail
{
namespace experimental
{

template<typename Clock, typename Duration>
inline void do_blocking_poll_until(io_context & ctx, std::atomic<bool> & done, std::chrono::time_point<Clock, Duration> time_point)
{
    if (ctx.stopped())
        ctx.restart();

    while (!done.load() && (time_point >= Clock::now()))
        try
        {
            ctx.poll_one();
        }
        catch (...)
        {
            asio::post([ex = std::current_exception()]{std::rethrow_exception(ex);});
        }
}

template<typename Clock, typename Duration>
inline void do_blocking_poll_until(io_context::executor_type exec, std::atomic<bool> & done, std::chrono::time_point<Clock, Duration> time_point)
{
    do_blocking_poll_until(exec.context(), done, time_point);
}

template<typename Clock, typename Duration>
inline void do_blocking_poll_until(any_io_executor exec, std::atomic<bool> & done, std::chrono::time_point<Clock, Duration> time_point)
{
    if (auto * io_exec = exec.target<io_context::executor_type>())
        do_blocking_poll_until(*io_exec, done, time_point);
    else
        asio::detail::throw_exception(std::logic_error("blocking not implemented for given executor"));
}
}
}



template<typename Clock, typename Duration, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_poll_until_impl<Clock, Duration, Executor>, R(T)>
{
    using return_type = std::optional<T>;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_poll_until_impl<Clock, Duration, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](T t) ASIO_NOEXCEPT
                {
                    result = std::move(t);
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll_until(exec, done, to.time_point);
        return result;
    }
};

template<typename Clock, typename Duration, typename Executor, typename R>
struct async_result<detail::experimental::blocking_poll_until_impl<Clock, Duration,Executor>, R(std::exception_ptr)>
{
    using return_type = bool;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_poll_until_impl<Clock, Duration, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](std::exception_ptr t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll_until(exec, done, to.time_point);
        if (error)
            std::rethrow_exception(error);
        return done.load();
    }
};

template<typename Clock, typename Duration, typename Executor, typename R>
struct async_result<detail::experimental::blocking_poll_until_impl<Clock, Duration, Executor>, R(error_code)>
{
    using return_type = bool;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_poll_until_impl<Clock, Duration,Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code t) ASIO_NOEXCEPT
                {
                    error = t;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll_until(exec, done, to.time_point);
        if (error)
            throw system_error(error, "blocking_poll failed with error code");

        return done.load();
    }
};

template<typename Clock, typename Duration, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_poll_until_impl<Clock, Duration, Executor>, R(std::exception_ptr, T)>
{
    using return_type = std::optional<T>;

    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_poll_until_impl<Clock, Duration, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        std::exception_ptr error;
        std::optional<T> result;
        std::atomic<bool> done{false};

        std::move(initiation)(
                [&](std::exception_ptr ex, T value) ASIO_NOEXCEPT
                {
                    result = std::move(value);
                    error = ex;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll_until(exec, done, to.time_point);
        if (error)
            std::rethrow_exception(error);
        return result;
    }
};



template<typename Clock, typename Duration, typename Executor, typename R, typename T>
struct async_result<detail::experimental::blocking_poll_until_impl<Clock, Duration, Executor>, R(error_code, T)>
{
    using return_type = std::optional<T>;


    template <typename Initiation, typename... InitArgs>
    static return_type initiate(Initiation initiation,
                                detail::experimental::blocking_poll_until_impl<Clock, Duration, Executor> to, InitArgs... args)
    {
        const auto exec = initiation.get_executor();

        error_code error;
        std::optional<T> result;
        std::atomic<bool> done{false};
        std::move(initiation)(
                [&](error_code ec, T value) ASIO_NOEXCEPT
                {
                    result = std::move(value);
                    error = ec;
                    done.store(true);
                }, std::move(args)...);

        detail::experimental::do_blocking_poll_until(exec, done, to.time_point);
        if (error)
            throw system_error(error, "blocking_poll failed with error code");
        return result;
    }
};



}


#endif //ASIO_EXPERIMENTAL_BLOCKING_HPP
