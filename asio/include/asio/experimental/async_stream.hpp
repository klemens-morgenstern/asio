//
// experimental/async_stream.hpp
// ~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_EXPERIMENTAL_ASYNC_STREAM_HPP
#define ASIO_EXPERIMENTAL_ASYNC_STREAM_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#include "asio/async_result.hpp"
#include "asio/error.hpp"
#include "asio/detail/type_traits.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace experimental
{

template<typename Signature, typename Condition, typename Token, typename Initiation, typename ... InitArgs>
struct repeat_impl;

template<typename R, typename ... Args, typename Condition, ASIO_COMPLETION_TOKEN_FOR(R(Args...)) Token, typename Initiation, typename ... InitArgs>
struct repeat_impl<R(Args...), Condition, Token, Initiation, InitArgs...>
{
    Condition condition;
    Token token;
    Initiation initiation;
    std::tuple<InitArgs...> args;


    repeat_impl(repeat_impl && ) = default;

    template<typename Condition_, typename Token_, typename Initiation_, typename ... Args_>
    repeat_impl(ASIO_MOVE_ARG(Condition_) condition,
                ASIO_MOVE_ARG(Token_) token,
                ASIO_MOVE_ARG(Initiation_) initiation,
                ASIO_MOVE_ARG(Args_) ... args) :
                        condition(ASIO_MOVE_CAST(Condition_)(condition)),
                        token(ASIO_MOVE_CAST(Token_)(token)),
                        initiation(ASIO_MOVE_CAST(Initiation_)(initiation)),
                        args(ASIO_MOVE_CAST(Args_)(args)...)
    {
    }

    void operator()(Args ... args)
    {
        if (this->condition(args...))
            this->repost();

        token(std::move(args)...); // should this post?
    }

    template <std::size_t... I>
    void repost_impl(std::index_sequence<I...>)
    {
        Initiation init = initiation;
        std::tuple<InitArgs...> args_ = args;
        asio::async_initiate<repeat_impl, R(Args...)>(
                std::move(init),
                std::move(*this),
                ASIO_MOVE_CAST(Args)(std::get<I>(args_))...);
    }

    void repost()
    {
        this->repost_impl(std::make_index_sequence<sizeof ... (Args)>{});
    }
};

template<typename R, typename ... Args, typename Condition, ASIO_COMPLETION_TOKEN_FOR(R(Args...)) Token, typename Initiation>
struct repeat_impl<R(Args...), Condition, Token, Initiation>
{
    Condition condition;
    Token token;
    Initiation initiation;


    repeat_impl(repeat_impl && ) = default;

    template<typename Condition_, typename Token_, typename Initiation_, typename ... Args_>
    repeat_impl(ASIO_MOVE_ARG(Condition_) condition,
                ASIO_MOVE_ARG(Token_) token,
                ASIO_MOVE_ARG(Initiation_) initiation) :
            condition(ASIO_MOVE_CAST(Condition_)(condition)),
            token(ASIO_MOVE_CAST(Token_)(token)),
            initiation(ASIO_MOVE_CAST(Initiation_)(initiation))
    {
    }

    template<typename ...  Args_>
    void operator()(Args_ &&... args)
    {
        if (this->condition(args...))
            this->repost();

        token(std::forward<Args_>(args)...); // should this post?
    }

    template <std::size_t... I>
    void repost_impl(std::index_sequence<I...>)
    {
        Initiation init = initiation;
        asio::async_initiate<repeat_impl, R(Args...)>(
                std::move(init), *this);
    }

    void repost()
    {
        this->repost_impl(std::make_index_sequence<sizeof ... (Args)>{});
    }
};

template <typename CompletionToken, typename Signature, typename Condition, typename Initiation, typename... Args>
inline typename constraint<
        detail::async_result_has_initiate_memfn<
                CompletionToken, Signature>::value,
        ASIO_INITFN_DEDUCED_RESULT_TYPE(CompletionToken, Signature,
                                  (async_result<typename decay<CompletionToken>::type,
                                        Signature>::initiate(declval<ASIO_MOVE_ARG(Initiation)>(),
                                                          declval<ASIO_MOVE_ARG(CompletionToken)>(),
                                                          declval<ASIO_MOVE_ARG(Args)>()...)))>::type
    async_stream_initiate(ASIO_MOVE_ARG(Condition) condition,
                          ASIO_MOVE_ARG(Initiation) initiation,
                          ASIO_MOVE_ARG(CompletionToken) token,
                          ASIO_MOVE_ARG(Args)... args)
{
    //maybe insert check that the token can receive multiple notifications here. we'll assume copyable means it works.

    typedef repeat_impl<Signature,
                        typename decay<Condition>::type,
                        typename decay<CompletionToken>::type,
                        typename decay<Initiation>::type,
                        typename decay<Args>::type ... > repeat_impl_t;

    repeat_impl_t(ASIO_MOVE_CAST(Condition)(condition),
                  ASIO_MOVE_CAST(CompletionToken)(token),
                  ASIO_MOVE_CAST(Initiation)(initiation),
                  ASIO_MOVE_CAST(Args)(args)...).repost();
}


}
}

#include "asio/detail/pop_options.hpp"


#endif //ASIO_ASYNC_STREAM_HPP
