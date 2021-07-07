//
// timeout.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_EXPERIMENTAL_TIMEOUT_HPP
#define ASIO_EXPERIMENTAL_TIMEOUT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include <tuple>
#include "asio/steady_timer.hpp"
#include "asio/bind_cancellation_slot.hpp"
#include "asio/detail/type_traits.hpp"

#include "asio/detail/push_options.hpp"

namespace asio
{
namespace experimental
{

template<typename T, typename Timer = asio::steady_timer>
struct with_timeout_at_t
{
    typedef typename Timer::time_point time_point;
    time_point expires_at;

    /// The type of the timer waiting
    typedef Timer timer_type;

    typedef T target_type;
    target_type target;
    template<typename U>
    with_timeout_at_t(time_point expires_at, ASIO_MOVE_ARG(U) u)
        : expires_at(expires_at), target(ASIO_MOVE_CAST(U)(u))
    {}
};

template<typename T, typename Timer = asio::steady_timer>
struct with_timeout_after_t
{
    typedef typename Timer::duration duration;

    duration expires_after;

    /// The type of the timer waiting
    typedef Timer timer_type;

    typedef T target_type;
    target_type target;

    template<typename U>
    with_timeout_after_t(duration expires_after, ASIO_MOVE_ARG(U) u)
            : expires_after(expires_after), target(ASIO_MOVE_CAST(U)(u))
    {}
};

template<typename Handler, typename Timer = asio::steady_timer>
with_timeout_at_t<typename decay<Handler>::type, Timer> with_timeout(
        const typename Timer::time_point & tp,
        ASIO_MOVE_ARG(Handler) handler)
{
    return with_timeout_at_t<typename decay<Handler>::type, Timer>(tp, ASIO_MOVE_CAST(Handler)(handler));
}

template<typename Handler, typename Timer = asio::steady_timer>
with_timeout_after_t<typename decay<Handler>::type, Timer> with_timeout(
        const typename Timer::duration & d,
        ASIO_MOVE_ARG(Handler) handler)
{
    return with_timeout_after_t<typename decay<Handler>::type, Timer>(d, ASIO_MOVE_CAST(Handler)(handler));
}

}

template<typename Handler, typename Timer>
struct with_timeout_binder
{
    Timer timer;
    std::shared_ptr<cancellation_signal> signal;
    Handler handler;

    /// The type of the associated cancellation slot.
    typedef cancellation_slot cancellation_slot_type;

    cancellation_slot_type get_cancellation_slot() const ASIO_NOEXCEPT {return signal->slot();}

    template<typename Handler_>
    with_timeout_binder(ASIO_MOVE_ARG(Handler_) handler_, typename Timer::time_point tp)
            : timer(get_associated_executor(handler_), tp), signal(std::allocate_shared<cancellation_signal>(get_associated_allocator(handler_))),
              handler(ASIO_MOVE_CAST(Handler_)(handler_))
    {
        timer.async_wait([signal=signal](const error_code & )
                         {
                             signal->emit(cancellation_type::all);
                         });
    }
    template<typename Handler_>
    with_timeout_binder(ASIO_MOVE_ARG(Handler_) handler_, typename Timer::duration d, const cancellation_slot & slot)
            : timer(get_associated_executor(handler_), d), signal(std::allocate_shared<cancellation_signal>(get_associated_allocator(handler_))),
              handler(ASIO_MOVE_CAST(Handler_)(handler_))
    {
        timer.async_wait(
                bind_cancellation_slot(slot,
                                       [signal=signal](const error_code &)
                                       {
                                           signal->emit(cancellation_type::all);
                                       }));
    }

    template<typename Handler_>
    with_timeout_binder(ASIO_MOVE_ARG(Handler_) handler_, typename Timer::duration d)
            : timer(get_associated_executor(handler_), d), signal(std::allocate_shared<cancellation_signal>(get_associated_allocator(handler_))),
              handler(ASIO_MOVE_CAST(Handler_)(handler_))
    {
        timer.async_wait([signal=signal](const error_code & )
                         {
                             signal->emit(cancellation_type::all);
                         });
    }
    template<typename Handler_>
    with_timeout_binder(ASIO_MOVE_ARG(Handler_) handler_, typename Timer::time_point tp, const cancellation_slot & slot)
            : timer(get_associated_executor(handler_), tp), signal(std::allocate_shared<cancellation_signal>(get_associated_allocator(handler_))),
              handler(ASIO_MOVE_CAST(Handler_)(handler_))
    {
        timer.async_wait(
                bind_cancellation_slot(slot,
                                       [signal=signal](const error_code &)
                                       {
                                           signal->emit(cancellation_type::all);
                                       }));
    }

    template<typename ... Args>
    void operator()(Args&& ... args)
    {
        handler(ASIO_MOVE_CAST(Args)(args)...);
    }
};

template<typename Handler, typename Timer, typename CancellationSlot1>
struct associated_cancellation_slot<with_timeout_binder<Handler, Timer>, CancellationSlot1>
{
    typedef CancellationSlot1 type;

    static type get(const with_timeout_binder<Handler, Timer>& b,
                    const CancellationSlot1& = CancellationSlot1()) ASIO_NOEXCEPT
    {
        return b.get_cancellation_slot();
    }
};

template <typename Handler, typename Timer, typename Signature>
struct async_result<experimental::with_timeout_at_t<Handler, Timer>, Signature>
{
    typedef Handler target_type;


    template <typename Initiation>
    struct init_wrapper
    {
        typedef typename Timer::time_point time_point;
        template <typename Init>
        init_wrapper(const time_point & tp, ASIO_MOVE_ARG(Init) init)
                : time_point_(tp),
                  initiation_(ASIO_MOVE_CAST(Init)(init))
        {
        }

        template <typename Handler_, typename... Args>
        void operator()(
            ASIO_MOVE_ARG(Handler_) handler,
            ASIO_MOVE_ARG(Args)... args)
        {
            const auto cancel_slot = get_associated_cancellation_slot(handler);
            if (cancel_slot.is_connected())
                return ASIO_MOVE_CAST(Initiation)(initiation_)(
                        with_timeout_binder<Handler, Timer>(ASIO_MOVE_CAST(Handler_)(handler), time_point_, cancel_slot),
                        ASIO_MOVE_CAST(Args)(args)...);
            else
                return ASIO_MOVE_CAST(Initiation)(initiation_)(
                        with_timeout_binder<Handler, Timer>(ASIO_MOVE_CAST(Handler_)(handler), time_point_),
                        ASIO_MOVE_CAST(Args)(args)...);
        }

        template <typename Handler_, typename... Args>
        void operator()(
            ASIO_MOVE_ARG(Handler_) handler,
            ASIO_MOVE_ARG(Args)... args) const
        {
            const auto cancel_slot = get_associated_cancellation_slot(handler);
            if (cancel_slot.is_connected())
                return ASIO_MOVE_CAST(Initiation)(initiation_)(
                        with_timeout_binder(ASIO_MOVE_CAST(Handler_)(handler), time_point_, cancel_slot),
                        ASIO_MOVE_CAST(Args)(args)...);
            else
                return ASIO_MOVE_CAST(Initiation)(initiation_)(
                        with_timeout_binder(ASIO_MOVE_CAST(Handler_)(handler), time_point_),
                        ASIO_MOVE_CAST(Args)(args)...);
        }

        time_point time_point_;
        Initiation initiation_;
    };

    template <typename Initiation, typename RawCompletionToken, typename... Args>
    static ASIO_INITFN_DEDUCED_RESULT_TYPE(T, Signature,
                                           (async_result<Handler, Signature>::initiate(
                                                   declval<init_wrapper<typename decay<Initiation>::type> >(),
                                                   declval<Handler>(), declval<ASIO_MOVE_ARG(Args)>()...)))
    initiate(
            ASIO_MOVE_ARG(Initiation) initiation,
            ASIO_MOVE_ARG(RawCompletionToken) token,
            ASIO_MOVE_ARG(Args)... args)
    {
        return async_initiate<Handler, Signature>(
                init_wrapper<typename decay<Initiation>::type>(
                        token.expires_at,
                        ASIO_MOVE_CAST(Initiation)(initiation)),
                        token.target,
                        ASIO_MOVE_CAST(Args)(args)...);
    }
};




template <typename Handler, typename Timer, typename Signature>
struct async_result<experimental::with_timeout_after_t<Handler, Timer>, Signature>
{
    typedef Handler target_type;


    template <typename Initiation>
    struct init_wrapper
    {
        typedef typename Timer::duration duration;
        template <typename Init>
        init_wrapper(const duration & d, ASIO_MOVE_ARG(Init) init)
                : duration_(d),
                  initiation_(ASIO_MOVE_CAST(Init)(init))
        {
        }

        template <typename Handler_, typename... Args>
        void operator()(
                ASIO_MOVE_ARG(Handler_) handler,
                ASIO_MOVE_ARG(Args)... args)
        {
            const auto cancel_slot = get_associated_cancellation_slot(handler);
            if (cancel_slot.is_connected())
                return ASIO_MOVE_CAST(Initiation)(initiation_)(
                        with_timeout_binder<Handler, Timer>(ASIO_MOVE_CAST(Handler_)(handler), duration_, cancel_slot),
                        ASIO_MOVE_CAST(Args)(args)...);
            else
                return ASIO_MOVE_CAST(Initiation)(initiation_)(
                        with_timeout_binder<Handler, Timer>(ASIO_MOVE_CAST(Handler_)(handler), duration_),
                        ASIO_MOVE_CAST(Args)(args)...);
        }

        template <typename Handler_, typename... Args>
        void operator()(
                ASIO_MOVE_ARG(Handler_) handler,
                ASIO_MOVE_ARG(Args)... args) const
        {
            const auto cancel_slot = get_associated_cancellation_slot(handler);
            if (cancel_slot.is_connected())
                return ASIO_MOVE_CAST(Initiation)(initiation_)(
                        with_timeout_binder(ASIO_MOVE_CAST(Handler_)(handler), duration_, cancel_slot),
                        ASIO_MOVE_CAST(Args)(args)...);
            else
                return ASIO_MOVE_CAST(Initiation)(initiation_)(
                        with_timeout_binder(ASIO_MOVE_CAST(Handler_)(handler), duration_),
                        ASIO_MOVE_CAST(Args)(args)...);
        }

        duration   duration_;
        Initiation initiation_;
    };

    template <typename Initiation, typename RawCompletionToken, typename... Args>
    static ASIO_INITFN_DEDUCED_RESULT_TYPE(T, Signature,
                                           (async_result<Handler, Signature>::initiate(
                                                   declval<target_type>(),
                                                   declval<Handler>(), declval<ASIO_MOVE_ARG(Args)>()...)))
    initiate(
            ASIO_MOVE_ARG(Initiation) initiation,
            ASIO_MOVE_ARG(RawCompletionToken) token,
            ASIO_MOVE_ARG(Args)... args)
    {
        return async_initiate<Handler, Signature>(
                init_wrapper<typename decay<Initiation>::type>(
                        token.expires_after,
                        ASIO_MOVE_CAST(Initiation)(initiation)),
                token.target,
                ASIO_MOVE_CAST(Args)(args)...);
    }
};


}


#include "asio/detail/pop_options.hpp"

#endif //ASIO_TIMEOUT_HPP
