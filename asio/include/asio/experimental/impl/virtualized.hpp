//
// experimental/impl/virtualized.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_EXPERIMENTAL_IMPL_VIRTUALIZED_HPP
#define ASIO_EXPERIMENTAL_IMPL_VIRTUALIZED_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/push_options.hpp"

namespace asio {

#if !defined(GENERATING_DOCUMENTATION)
namespace experimental {
namespace detail {

template<typename Initiation, typename InitArgs, typename ... Signatures>
struct virtual_async_operation_impl final : virtual_async_operation_base<Signatures...>
{
    using type = virtual_async_operation_impl;
    using allocator_base = typename associated_allocator<Initiation>::type;
    using allocator_type = typename std::allocator_traits<allocator_base>::template rebind_alloc<type>;


    Initiation initiation;
    InitArgs init_args;

    allocator_type allocator = allocator_type(get_associated_allocator(initiation));

    template<typename ...Inits>
    virtual_async_operation_impl(ASIO_MOVE_ARG(Initiation) initiation,
                                 ASIO_MOVE_ARG(Inits) ... init_args)
                                 :  initiation(ASIO_MOVE_CAST(Initiation)(initiation)),
                                    init_args(ASIO_MOVE_CAST(Inits)(init_args) ...) {}

    void destroy_and_deallocate_this() override
    {
        auto alloc = allocator;
        std::allocator_traits<allocator_type>::destroy(alloc, this);
        std::allocator_traits<allocator_type>::deallocate(alloc, this, sizeof(virtual_async_operation_impl));
    }

    template <typename CompletionToken, std::size_t... I>
    decltype(auto) invoke_helper(
            ASIO_MOVE_ARG(CompletionToken) token,
            std::index_sequence<I...>)
    {
        return asio::async_initiate<CompletionToken, Signatures...>(
                ASIO_MOVE_CAST(typename decay<Initiation>::type)(initiation),
                token, std::get<I>(ASIO_MOVE_CAST(InitArgs)(init_args))...);
    }

    void invoke(detail::completion_handler_erasure<Signatures...> handler) override
    {
        return this->invoke_helper(
                std::move(handler),
                std::make_index_sequence<std::tuple_size<InitArgs>::value>());
    }
};


template<typename ... Signatures, typename Initiation, typename ...InitArgs>
asio::experimental::virtual_async_operation<Signatures...> make_virtual_async_operation(
        ASIO_MOVE_ARG(Initiation) initiation,
        ASIO_MOVE_ARG(InitArgs)... args)
{
    using type = virtual_async_operation_impl<Initiation, std::tuple<InitArgs...>, Signatures...>;
    using delt = virtual_async_operation_deleter_t<Signatures...>;
    using ptr_t = std::unique_ptr<type, delt>;

    auto p = std::make_unique<virtual_async_operation_impl<Initiation, std::tuple<InitArgs...>, Signatures...>>(
            ASIO_MOVE_CAST(Initiation)(initiation),
            ASIO_MOVE_CAST(InitArgs)(args)...
            );
    return asio::experimental::virtual_async_operation<Signatures...>(ptr_t(p.release(), delt{}));
}

}

}


template <typename ... Signatures>
class async_result<experimental::virtualize_t, Signatures...>
{
public:
  template <typename Initiation, typename... InitArgs>
  static experimental::virtual_async_operation<Signatures...>
    initiate(ASIO_MOVE_ARG(Initiation) initiation,
      experimental::virtualize_t, ASIO_MOVE_ARG(InitArgs)... args)
  {
    return experimental::detail::make_virtual_async_operation<Signatures...>(
          ASIO_MOVE_CAST(Initiation)(initiation),
          ASIO_MOVE_CAST(InitArgs)(args)...);
    }
};

#endif // !defined(GENERATING_DOCUMENTATION)

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_EXPERIMENTAL_IMPL_VIRTUAL_DEFERRED_HPP
