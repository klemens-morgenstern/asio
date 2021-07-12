//
// experimental/repeat.hpp
// ~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_EXPERIMENTAL_REPEAT_HPP
#define ASIO_EXPERIMENTAL_REPEAT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#include "asio/async_result.hpp"
#include "asio/experimental/async_stream.hpp"
#include "asio/error.hpp"
#include "asio/detail/type_traits.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace experimental
{

/// Trait for detecting objects that are usable as deferred operations.
struct default_repeat_condition_t
{
    bool operator()(const error_code & ec) const
    {
        return !ec;
    }
    template<typename T>
    bool operator()(const error_code & ec, const T & ) const
    {
        return !ec;
    }
    bool operator()(std::exception_ptr ex) const
    {
        return !ex;
    }
    template<typename T>
    bool operator()(std::exception_ptr ex, const T & ) const
    {
        return !ex;
    }
};

template<typename Condition = default_repeat_condition_t>
struct repeat_t
{
    Condition cond;
    repeat_t(ASIO_MOVE_OR_LVALUE_TYPE(Condition) cond) : cond(ASIO_MOVE_OR_LVALUE(Condition)(cond))
    {
    }
    constexpr repeat_t() {}

    constexpr auto operator()() const {return repeat_t<default_repeat_condition_t>{};}

    template<typename Condition_>
    constexpr auto operator()(ASIO_MOVE_ARG(Condition_) condition) const {return repeat_t<typename decay<Condition_>::type>{ ASIO_MOVE_OR_LVALUE_TYPE(Condition_)(condition)};}
};

constexpr static repeat_t<> repeat{};


template<typename Signature, typename Condition, typename Initiation, typename ...InitArgs>
class repeated_async_operation
{
    typename decay<Initiation>::type initiation_;
    typedef std::tuple<typename decay<InitArgs>::type...> init_args_t;
    init_args_t init_args_;
    Condition condition;

    template <typename CompletionToken, std::size_t... I>
    decltype(auto) invoke_helper(
            ASIO_MOVE_ARG(CompletionToken) token,
            std::index_sequence<I...>)
    {
        return asio::experimental::async_stream_initiate<CompletionToken, Signature>(
                ASIO_MOVE_CAST(Condition)(condition),
                ASIO_MOVE_CAST(typename decay<Initiation>::type)(initiation_),
                ASIO_MOVE_CAST(CompletionToken)(token),
                std::get<I>(ASIO_MOVE_CAST(init_args_t)(init_args_))...
                        );
    }

  public:
    /// Construct a deferred asynchronous operation from the arguments to an
    /// initiation function object.
    template <typename C, typename I, typename... A>
    ASIO_CONSTEXPR explicit repeated_async_operation(
            ASIO_MOVE_ARG(C) condition,
            ASIO_MOVE_ARG(I) initiation,
            ASIO_MOVE_ARG(A)... init_args)
            : initiation_(ASIO_MOVE_CAST(I)(initiation)),
              init_args_(ASIO_MOVE_CAST(A)(init_args)...),
              condition(ASIO_MOVE_CAST(C)(condition))
    {
    }

    /// Initiate the asynchronous operation using the supplied completion token.
    template <ASIO_COMPLETION_TOKEN_FOR(Signature) CompletionToken>
    decltype(auto) operator()(
            ASIO_MOVE_ARG(CompletionToken) token) ASIO_RVALUE_REF_QUAL
    {
        return this->invoke_helper(
                ASIO_MOVE_CAST(CompletionToken)(token),
                std::index_sequence_for<InitArgs...>());
    }
};

}

template <typename Condition, typename Signature>
class async_result<experimental::repeat_t<Condition>, Signature>
{
  public:
    typedef typename experimental::repeated_async_operation<Signature, Condition, Signature> return_type;

    template <typename Initiation, typename... InitArgs>
    static auto initiate(
            ASIO_MOVE_ARG(Initiation) initiation,
            experimental::repeat_t<Condition> repeat,
            ASIO_MOVE_ARG(InitArgs)... args) -> experimental::repeated_async_operation<Signature, Condition, Initiation, InitArgs...>
    {
        return experimental::repeated_async_operation<
                Signature, Condition, Initiation, InitArgs...>(
                ASIO_MOVE_CAST(Condition)(repeat.cond),
                ASIO_MOVE_CAST(Initiation)(initiation),
                ASIO_MOVE_CAST(InitArgs)(args)...);
    }
};

}

#include "asio/detail/pop_options.hpp"


#endif //ASIO_EXPERIMENTAL_REPEAT_HPP
