//
// experimental/virtualized.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASIO_EXPERIMENTAL_VIRTUALIZED_HPP
#define ASIO_EXPERIMENTAL_VIRTUALIZED_HPP


#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)


#include "asio/detail/config.hpp"
#include <tuple>
#include "asio/associator.hpp"
#include "asio/async_result.hpp"
#include "asio/experimental/detail/completion_handler_erasure.hpp"
#include "asio/detail/type_traits.hpp"

#include "asio/detail/push_options.hpp"

namespace asio
{
#if !defined(GENERATING_DOCUMENTATION)

namespace experimental
{
template<typename ... Signatures>
struct virtual_async_operation;

namespace detail {

template<typename ... Signatures>
struct virtual_async_operation_base
{
    virtual void invoke(detail::completion_handler_erasure<Signatures...> handler) = 0;
    virtual void destroy_and_deallocate_this() = 0;
    virtual ~virtual_async_operation_base() = default;
};

template<typename ... Signatures>
struct virtual_async_operation_deleter_t
{
    void operator()(virtual_async_operation_base<Signatures...> * p)
    {
        p->destroy_and_deallocate_this();
    }
};

template<typename ... Signatures, typename Initiation, typename ...InitArgs>
asio::experimental::virtual_async_operation<Signatures...> make_virtual_async_operation(
        ASIO_MOVE_ARG(Initiation) initiation,
        ASIO_MOVE_ARG(InitArgs)... args);

}

#endif

template<typename ... Signatures>
struct virtual_async_operation
{
    virtual_async_operation() = delete;
    template<typename CompletionToken>
    auto operator()(CompletionToken && token) &&
    {
        assert(impl);
        return async_initiate<CompletionToken, Signatures...>(initiate(this), token);
    }

  private:
    struct initiate
    {
        virtual_async_operation * self_;
        initiate(virtual_async_operation * self) : self_(self) {}


        void operator()(detail::completion_handler_erasure<Signatures...> handler) const
        {
            self_->impl->invoke(std::move(handler));
        }
    };

    explicit virtual_async_operation(
            std::unique_ptr<detail::virtual_async_operation_base<Signatures...>,
                            detail::virtual_async_operation_deleter_t<Signatures...>> impl) : impl(std::move(impl)) {}

    template<typename ... Signatures_, typename Initiation, typename ...InitArgs>
    friend asio::experimental::virtual_async_operation<Signatures_...>
            asio::experimental::detail::make_virtual_async_operation(
                    ASIO_MOVE_ARG(Initiation) initiation,
                    ASIO_MOVE_ARG(InitArgs) ... args);

    std::unique_ptr<detail::virtual_async_operation_base<Signatures...>,
                    detail::virtual_async_operation_deleter_t<Signatures...>>  impl;
};

/// Class used to specify that an asynchronous operation should return a
/// type erased function object to lazily launch the operation.
/**
 * The virtualize_t class is used to indicate that an asynchronous operation
 * should return a function object which is itself an initiation function. A
 * virtualize_t object may be passed as a completion token to an asynchronous
 * operation, typically using the special value @c asio::virtualized. For
 * example:
 *
 * @code asio::experimental::virtual_async_operation<void(error_code, std::size_t)> my_sender
 *   = my_socket.async_read_some(my_buffer,
 *       asio::experimental::virtualized); @endcode
 *
 * The initiating function (async_read_some in the above example) returns a
 * function object that will lazily initiate the operation.
 */
class virtualize_t
{
  public:
    /// Default constructor.
    ASIO_CONSTEXPR virtualize_t()
    {
    }

    /// Adapts an executor to add the @c deferred_t completion token as the
    /// default.
    template<typename InnerExecutor>
    struct executor_with_default : InnerExecutor
    {
        /// Specify @c deferred_t as the default completion token type.
        typedef virtualize_t default_completion_token_type;

        /// Construct the adapted executor from the inner executor type.
        template<typename InnerExecutor1>
        executor_with_default(const InnerExecutor1 &ex,
                              typename constraint<
                                      conditional<
                                              !is_same<InnerExecutor1, executor_with_default>::value,
                                              is_convertible<InnerExecutor1, InnerExecutor>,
                                              false_type
                                      >::type::value
                              >::type = 0) ASIO_NOEXCEPT
                : InnerExecutor(ex)
        {
        }
    };

    /// Type alias to adapt an I/O object to use @c deferred_t as its
    /// default completion token type.
#if defined(ASIO_HAS_ALIAS_TEMPLATES) \
 || defined(GENERATING_DOCUMENTATION)
    template <typename T>
  using as_default_on_t = typename T::template rebind_executor<
      executor_with_default<typename T::executor_type> >::other;
#endif // defined(ASIO_HAS_ALIAS_TEMPLATES)
    //   || defined(GENERATING_DOCUMENTATION)

    /// Function helper to adapt an I/O object to use @c deferred_t as its
    /// default completion token type.
    template<typename T>
    static typename decay<T>::type::template rebind_executor<
            executor_with_default<typename decay<T>::type::executor_type>
    >::other
    as_default_on(ASIO_MOVE_ARG(T)object)
    {
        return typename decay<T>::type::template rebind_executor<
                executor_with_default<typename decay<T>::type::executor_type>
        >::other(ASIO_MOVE_CAST(T)(object));
    }
};

/// A special value, similar to std::nothrow.
/**
 * See the documentation for asio::experimental::deferred_t for a usage
 * example.
 */
#if defined(ASIO_HAS_CONSTEXPR) || defined(GENERATING_DOCUMENTATION)
constexpr virtualize_t virtualize;
#elif defined(ASIO_MSVC)
__declspec(selectany) virtualize_t virtualized;
#endif

}
}

#include "asio/detail/pop_options.hpp"
#include "asio/experimental/impl/virtualized.hpp"

#endif //ASIO_EXPERIMENTAL_VIRTUALIZED_HPP
