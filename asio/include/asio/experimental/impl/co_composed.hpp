//
// experimental/impl/co_composed.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_IMPL_EXPERIMENTAL_CO_COMPOSED_HPP
#define ASIO_IMPL_EXPERIMENTAL_CO_COMPOSED_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include <new>
#include <tuple>
#include <variant>
#include "asio/associated_cancellation_slot.hpp"
#include "asio/associator.hpp"
#include "asio/async_result.hpp"
#include "asio/cancellation_state.hpp"
#include "asio/detail/composed_work.hpp"
#include "asio/detail/recycling_allocator.hpp"
#include "asio/detail/throw_error.hpp"
#include "asio/detail/type_traits.hpp"
#include "asio/error.hpp"

#if defined(ASIO_HAS_STD_COROUTINE)
# include <coroutine>
#else // defined(ASIO_HAS_STD_COROUTINE)
# include <experimental/coroutine>
#endif // defined(ASIO_HAS_STD_COROUTINE)

#if defined(ASIO_ENABLE_HANDLER_TRACKING)
# if defined(ASIO_HAS_SOURCE_LOCATION)
#  include "asio/detail/source_location.hpp"
# endif // defined(ASIO_HAS_SOURCE_LOCATION)
#endif // defined(ASIO_ENABLE_HANDLER_TRACKING)

#include "asio/detail/push_options.hpp"

namespace asio {
namespace experimental {
namespace detail {

#if defined(ASIO_HAS_STD_COROUTINE)
using std::coroutine_handle;
using std::suspend_always;
using std::suspend_never;
#else // defined(ASIO_HAS_STD_COROUTINE)
using std::experimental::coroutine_handle;
using std::experimental::suspend_always;
using std::experimental::suspend_never;
#endif // defined(ASIO_HAS_STD_COROUTINE)

using asio::detail::composed_io_executors;
using asio::detail::composed_work;
using asio::detail::composed_work_guard;
using asio::detail::get_composed_io_executor;
using asio::detail::make_composed_io_executors;
using asio::detail::recycling_allocator;
using asio::detail::throw_error;

template <typename Executors, typename Handler, typename Return>
class co_composed_state;

template <typename Executors, typename Handler, typename Return>
class co_composed_handler_base;

template <typename Executors, typename Handler, typename Return>
class co_composed_promise;

template <completion_signature... Signatures>
class co_composed_returns
{
};

struct co_composed_on_suspend
{
  void (*fn_)(void*) = nullptr;
  void* arg_ = nullptr;
};

template <typename... T>
struct co_composed_completion : std::tuple<T&&...>
{
  template <typename... U>
  co_composed_completion(U&&... u) noexcept
    : std::tuple<T&&...>(std::forward<U>(u)...)
  {
  }
};

template <typename Executors, typename Handler,
    typename Return, typename Signature>
class co_composed_state_return_overload;

template <typename Executors, typename Handler,
    typename Return, typename R, typename... Args>
class co_composed_state_return_overload<
    Executors, Handler, Return, R(Args...)>
{
public:
  using derived_type = co_composed_state<Executors, Handler, Return>;
  using promise_type = co_composed_promise<Executors, Handler, Return>;
  using return_type = std::tuple<Args...>;

  void on_cancellation_complete_with(Args... args)
  {
    derived_type& state = *static_cast<derived_type*>(this);
    state.return_value_ = std::make_tuple(std::move(args)...);
    state.cancellation_on_suspend_fn(
        [](void* p)
        {
          auto& promise = *static_cast<promise_type*>(p);

          co_composed_handler_base<Executors, Handler,
            Return> composed_handler(promise);

          Handler handler(std::move(promise.state().handler_));
          return_type result(
              std::move(
                std::get<return_type>(promise.state().return_value_) // std::variant
                ));

          co_composed_handler_base<Executors, Handler,
            Return>(std::move(composed_handler));

          std::apply(std::move(handler), std::move(result));
        });
  }
};

template <typename Executors, typename Handler, typename Return>
class co_composed_state_return;

template <typename Executors, typename Handler, typename... Signatures>
class co_composed_state_return<
    Executors, Handler, co_composed_returns<Signatures...>>
  : public co_composed_state_return_overload<Executors,
      Handler, co_composed_returns<Signatures...>, Signatures>...
{
public:
  using co_composed_state_return_overload<Executors,
    Handler, co_composed_returns<Signatures...>,
      Signatures>::on_cancellation_complete_with...;

private:
  template <typename, typename, typename, typename>
    friend class co_composed_promise_return_overload;
  template <typename, typename, typename, typename>
    friend class co_composed_state_return_overload;

  std::variant<std::monostate,
    typename co_composed_state_return_overload<
      Executors, Handler, co_composed_returns<Signatures...>,
        Signatures>::return_type...> return_value_;
};

template <typename Executors, typename Handler,
    typename Return, typename... Signatures>
struct co_composed_state_default_cancellation_on_suspend_impl;

template <typename Executors, typename Handler, typename Return>
struct co_composed_state_default_cancellation_on_suspend_impl<
    Executors, Handler, Return>
{
  static constexpr void (*fn())(void*)
  {
    return nullptr;
  }
};

template <typename Executors, typename Handler, typename Return,
    typename R, typename... Args, typename... Signatures>
struct co_composed_state_default_cancellation_on_suspend_impl<
    Executors, Handler, Return, R(Args...), Signatures...>
{
  static constexpr void (*fn())(void*)
  {
    return co_composed_state_default_cancellation_on_suspend_impl<
      Executors, Handler, Return, Signatures...>::fn();
  }
};

template <typename Executors, typename Handler, typename Return,
    typename R, typename... Args, typename... Signatures>
struct co_composed_state_default_cancellation_on_suspend_impl<Executors,
    Handler, Return, R(asio::error_code, Args...), Signatures...>
{
  using promise_type = co_composed_promise<Executors, Handler, Return>;
  using return_type = std::tuple<asio::error_code, Args...>;

  static constexpr void (*fn())(void*)
  {
    if constexpr ((is_constructible<Args>::value && ...))
    {
      return [](void* p)
      {
        auto& promise = *static_cast<promise_type*>(p);

        co_composed_handler_base<Executors, Handler,
          Return> composed_handler(promise);

        Handler handler(std::move(promise.state().handler_));

        co_composed_handler_base<Executors, Handler,
          Return>(std::move(composed_handler));

        std::move(handler)(
            asio::error_code(asio::error::operation_aborted),
            Args{}...);
      };
    }
    else
    {
      return co_composed_state_default_cancellation_on_suspend_impl<
        Executors, Handler, Return, Signatures...>::fn();
    }
  }
};

template <typename Executors, typename Handler, typename Return,
    typename R, typename... Args, typename... Signatures>
struct co_composed_state_default_cancellation_on_suspend_impl<Executors,
    Handler, Return, R(std::exception_ptr, Args...), Signatures...>
{
  using promise_type = co_composed_promise<Executors, Handler, Return>;
  using return_type = std::tuple<std::exception_ptr, Args...>;

  static constexpr void (*fn())(void*)
  {
    if constexpr ((is_constructible<Args>::value && ...))
    {
      return [](void* p)
      {
        auto& promise = *static_cast<promise_type*>(p);

        co_composed_handler_base<Executors, Handler,
          Return> composed_handler(promise);

        Handler handler(std::move(promise.state().handler_));

        co_composed_handler_base<Executors, Handler,
          Return>(std::move(composed_handler));

        std::move(handler)(
            std::make_exception_ptr(
              asio::system_error(
                asio::error::operation_aborted, "co_await")),
            Args{}...);
      };
    }
    else
    {
      return co_composed_state_default_cancellation_on_suspend_impl<
        Executors, Handler, Return, Signatures...>::fn();
    }
  }
};

template <typename Executors, typename Handler, typename Return>
struct co_composed_state_default_cancellation_on_suspend;

template <typename Executors, typename Handler, typename... Signatures>
struct co_composed_state_default_cancellation_on_suspend<
    Executors, Handler, co_composed_returns<Signatures...>>
  : co_composed_state_default_cancellation_on_suspend_impl<Executors,
      Handler, co_composed_returns<Signatures...>, Signatures...>
{
};

template <typename Executors, typename Handler, typename Return>
class co_composed_state_cancellation
{
public:
  using cancellation_slot_type = cancellation_slot;

  cancellation_slot_type get_cancellation_slot() const noexcept
  {
    return cancellation_state_.slot();
  }

  cancellation_state get_cancellation_state() const noexcept
  {
    return cancellation_state_;
  }

  void reset_cancellation_state()
  {
    cancellation_state_ = cancellation_state(
        (get_associated_cancellation_slot)(
          static_cast<co_composed_state<Executors, Handler, Return>*>(
            this)->handler()));
  }

  template <typename Filter>
  void reset_cancellation_state(Filter filter)
  {
    cancellation_state_ = cancellation_state(
        (get_associated_cancellation_slot)(
          static_cast<co_composed_state<Executors, Handler, Return>*>(
            this)->handler()), filter, filter);
  }

  template <typename InFilter, typename OutFilter>
  void reset_cancellation_state(InFilter&& in_filter, OutFilter&& out_filter)
  {
    cancellation_state_ = cancellation_state(
        (get_associated_cancellation_slot)(
          static_cast<co_composed_state<Executors, Handler, Return>*>(
            this)->handler()),
        std::forward<InFilter>(in_filter),
        std::forward<OutFilter>(out_filter));
  }

  cancellation_type_t cancelled() const noexcept
  {
    return cancellation_state_.cancelled();
  }

  void clear_cancellation_slot() noexcept
  {
    cancellation_state_.slot().clear();
  }

  [[nodiscard]] bool throw_if_cancelled() const noexcept
  {
    return throw_if_cancelled_;
  }

  void throw_if_cancelled(bool b) noexcept
  {
    throw_if_cancelled_ = b;
  }

  [[nodiscard]] bool complete_if_cancelled() const noexcept
  {
    return complete_if_cancelled_;
  }

  void complete_if_cancelled(bool b) noexcept
  {
    complete_if_cancelled_ = b;
  }

private:
  template <typename, typename, typename>
    friend class co_composed_promise;
  template <typename, typename, typename, typename>
    friend class co_composed_state_return_overload;

  void cancellation_on_suspend_fn(void (*fn)(void*))
  {
    cancellation_on_suspend_fn_ = fn;
  }

  void check_for_cancellation_on_transform()
  {
    if (throw_if_cancelled_ && !!cancelled())
      throw_error(asio::error::operation_aborted, "co_await");
  }

  bool check_for_cancellation_on_suspend(
      co_composed_promise<Executors, Handler, Return>& promise) noexcept
  {
    if (complete_if_cancelled_ && !!cancelled() && cancellation_on_suspend_fn_)
    {
      promise.state().work_.reset();
      promise.state().on_suspend_->fn_ = cancellation_on_suspend_fn_;
      promise.state().on_suspend_->arg_ = &promise;
      return false;
    }
    return true;
  }

  cancellation_state cancellation_state_;
  void (*cancellation_on_suspend_fn_)(void*) =
    co_composed_state_default_cancellation_on_suspend<
      Executors, Handler, Return>::fn();
  bool throw_if_cancelled_ = false;
  bool complete_if_cancelled_ = true;
};

template <typename Executors, typename Handler, typename Return>
  requires is_same<
    typename associated_cancellation_slot<
      Handler, cancellation_slot
    >::asio_associated_cancellation_slot_is_unspecialised,
    void>::value
class co_composed_state_cancellation<Executors, Handler, Return>
{
public:
  void reset_cancellation_state()
  {
  }

  template <typename Filter>
  void reset_cancellation_state(Filter)
  {
  }

  template <typename InFilter, typename OutFilter>
  void reset_cancellation_state(InFilter&&, OutFilter&&)
  {
  }

  cancellation_type_t cancelled() const noexcept
  {
    return cancellation_type::none;
  }

  void clear_cancellation_slot() noexcept
  {
  }

  [[nodiscard]] bool throw_if_cancelled() const noexcept
  {
    return false;
  }

  void throw_if_cancelled(bool) noexcept
  {
  }

  [[nodiscard]] bool complete_if_cancelled() const noexcept
  {
    return false;
  }

  void complete_if_cancelled(bool) noexcept
  {
  }

private:
  template <typename, typename, typename>
    friend class co_composed_promise;
  template <typename, typename, typename, typename>
    friend class co_composed_state_return_overload;

  void cancellation_on_suspend_fn(void (*)(void*))
  {
  }

  void check_for_cancellation_on_transform() noexcept
  {
  }

  bool check_for_cancellation_on_suspend(
      co_composed_promise<Executors, Handler, Return>&) noexcept
  {
    return true;
  }
};

template <typename Executors, typename Handler, typename Return>
class co_composed_state
  : public co_composed_state_return<Executors, Handler, Return>,
    public co_composed_state_cancellation<Executors, Handler, Return>
{
public:
  using io_executor_type = typename composed_work_guard<
    typename composed_work<Executors>::head_type>::executor_type;

  template <typename H>
  co_composed_state(composed_io_executors<Executors>&& executors,
      H&& h, co_composed_on_suspend& on_suspend)
    : work_(std::move(executors)),
      handler_(std::forward<H>(h)),
      on_suspend_(&on_suspend)
  {
    this->reset_cancellation_state(enable_terminal_cancellation());
  }

  io_executor_type get_io_executor() const noexcept
  {
    return work_.head_.get_executor();
  }

  template <typename... Args>
  [[nodiscard]] co_composed_completion<Args...> complete(Args&&... args)
    requires requires { declval<Handler>()(std::forward<Args>(args)...); }
  {
    return co_composed_completion<Args...>(std::forward<Args>(args)...);
  }

  const Handler& handler() const noexcept
  {
    return handler_;
  }

private:
  template <typename, typename, typename>
    friend class co_composed_handler_base;
  template <typename, typename, typename>
    friend class co_composed_promise;
  template <typename, typename, typename, typename>
    friend class co_composed_promise_return_overload;
  template <typename, typename, typename>
    friend class co_composed_state_cancellation;
  template <typename, typename, typename, typename>
    friend class co_composed_state_return_overload;
  template <typename, typename, typename, typename...>
    friend struct co_composed_state_default_cancellation_on_suspend_impl;

  composed_work<Executors> work_;
  Handler handler_;
  co_composed_on_suspend* on_suspend_;
};

template <typename Executors, typename Handler, typename Return>
class co_composed_handler_cancellation
{
public:
  using cancellation_slot_type = cancellation_slot;

  cancellation_slot_type get_cancellation_slot() const noexcept
  {
    return static_cast<
      const co_composed_handler_base<Executors, Handler, Return>*>(
        this)->promise().state().get_cancellation_slot();
  }
};

template <typename Executors, typename Handler, typename Return>
  requires is_same<
    typename associated_cancellation_slot<
      Handler, cancellation_slot
    >::asio_associated_cancellation_slot_is_unspecialised,
    void>::value
class co_composed_handler_cancellation<Executors, Handler, Return>
{
};

template <typename Executors, typename Handler, typename Return>
class co_composed_handler_base :
  public co_composed_handler_cancellation<Executors, Handler, Return>
{
public:
  co_composed_handler_base(
      co_composed_promise<Executors, Handler, Return>& p) noexcept
    : p_(&p)
  {
  }

  co_composed_handler_base(co_composed_handler_base&& other) noexcept
    : p_(std::exchange(other.p_, nullptr))
  {
  }

  ~co_composed_handler_base()
  {
    if (p_) [[unlikely]]
      p_->destroy();
  }

  co_composed_promise<Executors, Handler, Return>& promise() const noexcept
  {
    return *p_;
  }

protected:
  void resume(void* result)
  {
    co_composed_on_suspend on_suspend{};
    std::exchange(p_, nullptr)->resume(p_, result, on_suspend);
    if (on_suspend.fn_)
      on_suspend.fn_(on_suspend.arg_);
  }

private:
  co_composed_promise<Executors, Handler, Return>* p_;
};

template <typename Executors, typename Handler,
    typename Return, typename Signature>
class co_composed_handler;

template <typename Executors, typename Handler,
    typename Return, typename R, typename... Args>
class co_composed_handler<Executors, Handler, Return, R(Args...)>
  : public co_composed_handler_base<Executors, Handler, Return>
{
public:
  using co_composed_handler_base<Executors,
    Handler, Return>::co_composed_handler_base;

  using result_type = std::tuple<typename decay<Args>::type...>;

  template <typename... T>
  void operator()(T&&... args)
  {
    result_type result(std::forward<T>(args)...);
    this->resume(&result);
  }

  static auto on_resume(void* result)
  {
    auto& args = *static_cast<result_type*>(result);
    if constexpr (sizeof...(Args) == 0)
      return;
    else if constexpr (sizeof...(Args) == 1)
      return std::move(std::get<0>(args));
    else
      return std::move(args);
  }
};

template <typename Executors, typename Handler,
    typename Return, typename R, typename... Args>
class co_composed_handler<Executors, Handler,
    Return, R(asio::error_code, Args...)>
  : public co_composed_handler_base<Executors, Handler, Return>
{
public:
  using co_composed_handler_base<Executors,
    Handler, Return>::co_composed_handler_base;

  using args_type = std::tuple<typename decay<Args>::type...>;
  using result_type = std::tuple<asio::error_code, args_type>;

  template <typename... T>
  void operator()(const asio::error_code& ec, T&&... args)
  {
    result_type result(ec, args_type(std::forward<T>(args)...));
    this->resume(&result);
  }

  static auto on_resume(void* result)
  {
    auto& [ec, args] = *static_cast<result_type*>(result);
    throw_error(ec);
    if constexpr (sizeof...(Args) == 0)
      return;
    else if constexpr (sizeof...(Args) == 1)
      return std::move(std::get<0>(args));
    else
      return std::move(args);
  }
};

template <typename Executors, typename Handler,
    typename Return, typename R, typename... Args>
class co_composed_handler<Executors, Handler,
    Return, R(std::exception_ptr, Args...)>
  : public co_composed_handler_base<Executors, Handler, Return>
{
public:
  using co_composed_handler_base<Executors,
    Handler, Return>::co_composed_handler_base;

  using args_type = std::tuple<typename decay<Args>::type...>;
  using result_type = std::tuple<std::exception_ptr, args_type>;

  template <typename... T>
  void operator()(std::exception_ptr ex, T&&... args)
  {
    result_type result(std::move(ex), args_type(std::forward<T>(args)...));
    this->resume(&result);
  }

  static auto on_resume(void* result)
  {
    auto& [ex, args] = *static_cast<result_type*>(result);
    if (ex)
      std::rethrow_exception(ex);
    if constexpr (sizeof...(Args) == 0)
      return;
    else if constexpr (sizeof...(Args) == 1)
      return std::move(std::get<0>(args));
    else
      return std::move(args);
  }
};

template <typename Executors, typename Handler, typename Return>
class co_composed_promise_return;

template <typename Executors, typename Handler>
class co_composed_promise_return<Executors, Handler, co_composed_returns<>>
{
public:
  auto final_suspend() noexcept
  {
    return suspend_never();
  }

  void return_void() noexcept
  {
  }
};

template <typename Executors, typename Handler,
    typename Return, typename Signature>
class co_composed_promise_return_overload;

template <typename Executors, typename Handler,
    typename Return, typename R, typename... Args>
class co_composed_promise_return_overload<
    Executors, Handler, Return, R(Args...)>
{
public:
  using derived_type = co_composed_promise<Executors, Handler, Return>;
  using return_type = std::tuple<Args...>;

  void return_value(std::tuple<Args...>&& value)
  {
    derived_type& promise = *static_cast<derived_type*>(this);
    promise.state().return_value_ = std::move(value);
    promise.state().work_.reset();
    promise.state().on_suspend_->arg_ = this;
    promise.state().on_suspend_->fn_ =
      [](void* p)
      {
        auto& promise = *static_cast<derived_type*>(p);

        co_composed_handler_base<Executors, Handler,
          Return> composed_handler(promise);

        Handler handler(std::move(promise.state().handler_));
        return_type result(
            std::move(
              std::get<return_type>(promise.state().return_value_) // std::variant
            )); 

        co_composed_handler_base<Executors, Handler,
          Return>(std::move(composed_handler));

        std::apply(std::move(handler), std::move(result));
      };
  }
};

template <typename Executors, typename Handler, typename... Signatures>
class co_composed_promise_return<Executors,
    Handler, co_composed_returns<Signatures...>>
  : public co_composed_promise_return_overload<Executors,
      Handler, co_composed_returns<Signatures...>, Signatures>...
{
public:
  auto final_suspend() noexcept
  {
    return suspend_always();
  }

  using co_composed_promise_return_overload<Executors, Handler,
    co_composed_returns<Signatures...>, Signatures>::return_value...;

private:
  template <typename, typename, typename, typename>
    friend class co_composed_promise_return_overload;
};

template <typename Executors, typename Handler, typename Return>
class co_composed_promise
  : public co_composed_promise_return<Executors, Handler, Return>
{
public:
  template <typename... Args>
  void* operator new(std::size_t size,
      co_composed_state<Executors, Handler, Return>& state, Args&&...)
  {
    block_allocator_type allocator(
      (get_associated_allocator)(state.handler_,
        recycling_allocator<void>()));

    block* base_ptr = std::allocator_traits<block_allocator_type>::allocate(
        allocator, blocks(sizeof(allocator_type)) + blocks(size));

    new (static_cast<void*>(base_ptr)) allocator_type(std::move(allocator));

    return base_ptr + blocks(sizeof(allocator_type));
  }

  template <typename C, typename... Args>
  void* operator new(std::size_t size, C&&,
      co_composed_state<Executors, Handler, Return>& state, Args&&...)
  {
    return co_composed_promise::operator new(size, state);
  }

  void operator delete(void* ptr, std::size_t size)
  {
    block* base_ptr = static_cast<block*>(ptr) - blocks(sizeof(allocator_type));

    allocator_type* allocator_ptr = std::launder(
        static_cast<allocator_type*>(static_cast<void*>(base_ptr)));

    block_allocator_type block_allocator(std::move(*allocator_ptr));
    allocator_ptr->~allocator_type();

    std::allocator_traits<block_allocator_type>::deallocate(block_allocator,
        base_ptr, blocks(sizeof(allocator_type)) + blocks(size));
  }

  template <typename... Args>
  co_composed_promise(
      co_composed_state<Executors, Handler, Return>& state, Args&&...)
    : state_(state)
  {
  }

  template <typename C, typename... Args>
  co_composed_promise(C&&,
      co_composed_state<Executors, Handler, Return>& state, Args&&...)
    : state_(state)
  {
  }

  void destroy() noexcept
  {
    coroutine_handle<co_composed_promise>::from_promise(*this).destroy();
  }

  void resume(co_composed_promise*& owner, void* result,
      co_composed_on_suspend& on_suspend)
  {
    state_.on_suspend_ = &on_suspend;
    state_.clear_cancellation_slot();
    owner_ = &owner;
    result_ = result;
    coroutine_handle<co_composed_promise>::from_promise(*this).resume();
  }

  co_composed_state<Executors, Handler, Return>& state() noexcept
  {
    return state_;
  }

  void get_return_object() noexcept
  {
  }

  auto initial_suspend() noexcept
  {
    return suspend_never();
  }

  void unhandled_exception()
  {
    if (owner_)
      *owner_ = this;
    throw;
  }

  template <async_operation Op>
  auto await_transform(Op&& op
#if defined(ASIO_ENABLE_HANDLER_TRACKING)
# if defined(ASIO_HAS_SOURCE_LOCATION)
      , asio::detail::source_location location
        = asio::detail::source_location::current()
# endif // defined(ASIO_HAS_SOURCE_LOCATION)
#endif // defined(ASIO_ENABLE_HANDLER_TRACKING)
    )
  {
    class [[nodiscard]] awaitable
    {
    public:
      awaitable(Op&& op, co_composed_promise& promise
#if defined(ASIO_ENABLE_HANDLER_TRACKING)
# if defined(ASIO_HAS_SOURCE_LOCATION)
          , const asio::detail::source_location& location
# endif // defined(ASIO_HAS_SOURCE_LOCATION)
#endif // defined(ASIO_ENABLE_HANDLER_TRACKING)
        )
        : op_(std::forward<Op>(op)),
          promise_(promise)
#if defined(ASIO_ENABLE_HANDLER_TRACKING)
# if defined(ASIO_HAS_SOURCE_LOCATION)
        , location_(location)
# endif // defined(ASIO_HAS_SOURCE_LOCATION)
#endif // defined(ASIO_ENABLE_HANDLER_TRACKING)
      {
      }

      constexpr bool await_ready() const noexcept
      {
        return false;
      }

      void await_suspend(coroutine_handle<co_composed_promise>)
      {
        if (promise_.state_.check_for_cancellation_on_suspend(promise_))
        {
          promise_.state_.on_suspend_->arg_ = this;
          promise_.state_.on_suspend_->fn_ =
            [](void* p)
            {
#if defined(ASIO_ENABLE_HANDLER_TRACKING)
# if defined(ASIO_HAS_SOURCE_LOCATION)
              ASIO_HANDLER_LOCATION((
                  static_cast<awaitable*>(p)->location_.file_name(),
                  static_cast<awaitable*>(p)->location_.line(),
                  static_cast<awaitable*>(p)->location_.function_name()));
# endif // defined(ASIO_HAS_SOURCE_LOCATION)
#endif // defined(ASIO_ENABLE_HANDLER_TRACKING)
              std::forward<Op>(static_cast<awaitable*>(p)->op_)(
                  co_composed_handler<Executors, Handler,
                    Return, completion_signature_of_t<Op>>(
                      static_cast<awaitable*>(p)->promise_));
            };
        }
      }

      auto await_resume()
      {
        return co_composed_handler<Executors, Handler, Return,
          completion_signature_of_t<Op>>::on_resume(promise_.result_);
      }

    private:
      Op&& op_;
      co_composed_promise& promise_;
#if defined(ASIO_ENABLE_HANDLER_TRACKING)
# if defined(ASIO_HAS_SOURCE_LOCATION)
      asio::detail::source_location location_;
# endif // defined(ASIO_HAS_SOURCE_LOCATION)
#endif // defined(ASIO_ENABLE_HANDLER_TRACKING)
    };

    state_.check_for_cancellation_on_transform();
    return awaitable{std::forward<Op>(op), *this
#if defined(ASIO_ENABLE_HANDLER_TRACKING)
# if defined(ASIO_HAS_SOURCE_LOCATION)
        , location
# endif // defined(ASIO_HAS_SOURCE_LOCATION)
#endif // defined(ASIO_ENABLE_HANDLER_TRACKING)
      };
  }

  template <typename... Args>
  auto yield_value(co_composed_completion<Args...>&& result)
  {
    class [[nodiscard]] awaitable
    {
    public:
      awaitable(co_composed_completion<Args...>&& result,
          co_composed_promise& promise)
        : result_(std::move(result)),
          promise_(promise)
      {
      }

      constexpr bool await_ready() const noexcept
      {
        return false;
      }

      void await_suspend(coroutine_handle<co_composed_promise>)
      {
        promise_.state_.work_.reset();
        promise_.state_.on_suspend_->arg_ = this;
        promise_.state_.on_suspend_->fn_ =
          [](void* p)
          {
            awaitable& a = *static_cast<awaitable*>(p);

            co_composed_handler_base<Executors, Handler,
              Return> composed_handler(a.promise_);

            Handler handler(std::move(a.promise_.state_.handler_));
            std::tuple<typename decay<Args>::type...> result(
                std::move(static_cast<std::tuple<Args&&...>>(a.result_)));

            co_composed_handler_base<Executors, Handler,
              Return>(std::move(composed_handler));

            std::apply(std::move(handler), std::move(result));
          };
      }

      void await_resume() noexcept
      {
      }

    private:
      co_composed_completion<Args...> result_;
      co_composed_promise& promise_;
    };

    return awaitable{std::move(result), *this};
  }

private:
  using allocator_type =
    associated_allocator_t<Handler, recycling_allocator<void>>;

  union block
  {
    std::max_align_t max_align;
    alignas(allocator_type) char pad[alignof(allocator_type)];
  };

  using block_allocator_type =
    typename std::allocator_traits<allocator_type>
      ::template rebind_alloc<block>;

  static constexpr std::size_t blocks(std::size_t size)
  {
    return (size + sizeof(block) - 1) / sizeof(block);
  }

  co_composed_state<Executors, Handler, Return>& state_;
  co_composed_promise** owner_ = nullptr;
  void* result_ = nullptr;
};

template <typename Implementation, typename Executors, typename... Signatures>
class initiate_co_composed
{
public:
  using executor_type = typename composed_io_executors<Executors>::head_type;

  template <typename I>
  initiate_co_composed(I&& impl, composed_io_executors<Executors>&& executors)
    : implementation_(std::forward<I>(impl)),
      executors_(std::move(executors))
  {
  }

  executor_type get_executor() const noexcept
  {
    return executors_.head_;
  }

  template <typename Handler, typename... InitArgs>
  void operator()(Handler&& handler, InitArgs&&... init_args) const &
  {
    using handler_type = typename decay<Handler>::type;
    using returns_type = co_composed_returns<Signatures...>;
    co_composed_on_suspend on_suspend{};
    implementation_(
        co_composed_state<Executors, handler_type, returns_type>(
          executors_, std::forward<Handler>(handler), on_suspend),
        std::forward<InitArgs>(init_args)...);
    if (on_suspend.fn_)
      on_suspend.fn_(on_suspend.arg_);
  }

  template <typename Handler, typename... InitArgs>
  void operator()(Handler&& handler, InitArgs&&... init_args) &&
  {
    using handler_type = typename decay<Handler>::type;
    using returns_type = co_composed_returns<Signatures...>;
    co_composed_on_suspend on_suspend{};
    std::move(implementation_)(
        co_composed_state<Executors, handler_type, returns_type>(
          std::move(executors_), std::forward<Handler>(handler), on_suspend),
        std::forward<InitArgs>(init_args)...);
    if (on_suspend.fn_)
      on_suspend.fn_(on_suspend.arg_);
  }

private:
  Implementation implementation_;
  composed_io_executors<Executors> executors_;
};

template <typename... Signatures, typename Implementation, typename Executors>
inline initiate_co_composed<Implementation, Executors, Signatures...>
make_initiate_co_composed(Implementation&& implementation,
    composed_io_executors<Executors>&& executors)
{
  return initiate_co_composed<
    typename decay<Implementation>::type, Executors, Signatures...>(
        std::forward<Implementation>(implementation), std::move(executors));
}

} // namespace detail

template <completion_signature... Signatures,
    typename Implementation, typename... IoObjectsOrExecutors>
inline auto co_composed(Implementation&& implementation,
    IoObjectsOrExecutors&&... io_objects_or_executors)
{
  return detail::make_initiate_co_composed<Signatures...>(
      std::forward<Implementation>(implementation),
      detail::make_composed_io_executors(
        detail::get_composed_io_executor(
          std::forward<IoObjectsOrExecutors>(
            io_objects_or_executors))...));
}

} // namespace experimental

#if !defined(GENERATING_DOCUMENTATION)

template <template <typename, typename> class Associator,
    typename Executors, typename Handler, typename Return,
    typename Signature, typename DefaultCandidate>
struct associator<Associator,
    experimental::detail::co_composed_handler<
      Executors, Handler, Return, Signature>,
    DefaultCandidate>
  : Associator<Handler, DefaultCandidate>
{
  static typename Associator<Handler, DefaultCandidate>::type
  get(const experimental::detail::co_composed_handler<
        Executors, Handler, Return, Signature>& h) ASIO_NOEXCEPT
  {
    return Associator<Handler, DefaultCandidate>::get(
        h.promise().state().handler());
  }

  static ASIO_AUTO_RETURN_TYPE_PREFIX2(
      typename Associator<Handler, DefaultCandidate>::type)
  get(const experimental::detail::co_composed_handler<
        Executors, Handler, Return, Signature>& h,
      const DefaultCandidate& c) ASIO_NOEXCEPT
    ASIO_AUTO_RETURN_TYPE_SUFFIX((
      Associator<Handler, DefaultCandidate>::get(
        h.promise().state().handler(), c)))
  {
    return Associator<Handler, DefaultCandidate>::get(
        h.promise().state().handler(), c);
  }
};

#endif // !defined(GENERATING_DOCUMENTATION)

} // namespace asio

#if !defined(GENERATING_DOCUMENTATION)
# if defined(ASIO_HAS_STD_COROUTINE)
namespace std {
# else // defined(ASIO_HAS_STD_COROUTINE)
namespace std { namespace experimental {
# endif // defined(ASIO_HAS_STD_COROUTINE)

template <typename C, typename Executors,
    typename Handler, typename Return, typename... Args>
struct coroutine_traits<void, C&,
    asio::experimental::detail::co_composed_state<
      Executors, Handler, Return>,
    Args...>
{
  using promise_type =
    asio::experimental::detail::co_composed_promise<
      Executors, Handler, Return>;
};

template <typename C, typename Executors,
    typename Handler, typename Return, typename... Args>
struct coroutine_traits<void, C&&,
    asio::experimental::detail::co_composed_state<
      Executors, Handler, Return>,
    Args...>
{
  using promise_type =
    asio::experimental::detail::co_composed_promise<
      Executors, Handler, Return>;
};

template <typename Executors, typename Handler,
    typename Return, typename... Args>
struct coroutine_traits<void,
    asio::experimental::detail::co_composed_state<
      Executors, Handler, Return>,
    Args...>
{
  using promise_type =
    asio::experimental::detail::co_composed_promise<
      Executors, Handler, Return>;
};

# if defined(ASIO_HAS_STD_COROUTINE)
} // namespace std
# else // defined(ASIO_HAS_STD_COROUTINE)
}} // namespace std::experimental
# endif // defined(ASIO_HAS_STD_COROUTINE)
#endif // !defined(GENERATING_DOCUMENTATION)

#include "asio/detail/pop_options.hpp"

#endif // ASIO_IMPL_EXPERIMENTAL_CO_COMPOSED_HPP
