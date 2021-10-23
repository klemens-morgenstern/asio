//
// experimental/impl/coro.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASIO_EXPERIMENTAL_IMPL_CORO_HPP
#define ASIO_EXPERIMENTAL_IMPL_CORO_HPP

#include "asio/detail/config.hpp"
#include "asio/detail/push_options.hpp"

namespace asio
{
namespace experimental
{

template <typename Yield, typename Return, typename Executor>
struct coro;

namespace detail
{

struct coro_cancellation_source
{
  cancellation_slot slot;
  cancellation_state state;
  bool throw_if_cancelled_ = true;

  void reset_cancellation_state()
  {
    state = cancellation_state(slot);
  }

  template <typename Filter>
  void reset_cancellation_state(ASIO_MOVE_ARG(Filter) filter)
  {
    state = cancellation_state(slot, ASIO_MOVE_CAST(Filter)(filter));
  }

  template <typename InFilter, typename OutFilter>
  void reset_cancellation_state(ASIO_MOVE_ARG(InFilter) in_filter,
                                ASIO_MOVE_ARG(OutFilter) out_filter)
  {
    state = cancellation_state(slot,
                               ASIO_MOVE_CAST(InFilter)(in_filter),
                               ASIO_MOVE_CAST(OutFilter)(out_filter));
  }

  bool throw_if_cancelled() const
  {
    return throw_if_cancelled_;
  }

  void throw_if_cancelled(bool value)
  {
    throw_if_cancelled_ = value;
  }


};

template <typename Signature, typename Return, typename Executor>
struct coro_promise;

template <typename T>
struct is_noexcept : std::false_type
{
};

template <typename Return, typename... Args>
struct is_noexcept<Return(Args...)> : std::false_type
{
};

template <typename Return, typename... Args>
struct is_noexcept<Return(Args...) noexcept> : std::true_type
{
};

template <typename T>
constexpr bool is_noexcept_v = is_noexcept<T>::value;

template <typename T>
struct coro_error;

template <>
struct coro_error<asio::error_code>
{
  static asio::error_code invalid()
  {
    return error::fault;
  }

  static asio::error_code cancelled()
  {
    return error::operation_aborted;
  }

  static asio::error_code interrupted()
  {
    return error::interrupted;
  }

  static asio::error_code done()
  {
    return error::broken_pipe;
  }
};

template <>
struct coro_error<std::exception_ptr>
{
  static std::exception_ptr invalid()
  {
    return std::make_exception_ptr(
            asio::system_error(
                    coro_error<asio::error_code>::invalid()));
  }

  static std::exception_ptr cancelled()
  {
    return std::make_exception_ptr(
            asio::system_error(
                    coro_error<asio::error_code>::cancelled()));
  }

  static std::exception_ptr interrupted()
  {
    return std::make_exception_ptr(
            asio::system_error(
                    coro_error<asio::error_code>::interrupted()));
  }

  static std::exception_ptr done()
  {
    return std::make_exception_ptr(
            asio::system_error(
                    coro_error<asio::error_code>::done()));
  }
};

template <typename T, typename Coroutine >
struct coro_with_arg
{
  using coro_t = Coroutine;
  T value;
  coro_t& coro;

  struct awaitable_t
  {
    T value;
    coro_t& coro;

    constexpr static bool await_ready() { return false; }

    template <typename Y, typename R, typename E>
      requires std::is_void_v<typename coro_t::executor_type>
    auto await_suspend(coroutine_handle<coro_promise<Y, R, E>> h) -> coroutine_handle<>
    {
      auto& hp = h.promise();
      if constexpr (!detail::coro_promise<Y, R, E>::is_noexcept)
        if ((hp.cancel->state.cancelled() != cancellation_type::none) && hp.cancel->throw_if_cancelled_)
          asio::detail::throw_error(asio::error::operation_aborted, "coro-cancelled");

      coro.coro_->awaited_from = h;
      coro.coro_->reset_error();
      coro.coro_->input_ = std::move(value);
      return coro.coro_->get_handle();
    }


    template <typename Y, typename R, typename E>
      requires (!std::is_void_v<typename coro_t::executor_type>)
    auto await_suspend(coroutine_handle<coro_promise<Y, R, E>> h)
      -> coroutine_handle<>
    {
      auto& hp = h.promise();
      if constexpr (!coro_promise<Y, R, E>::is_noexcept)
        if ((hp.cancel->state.cancelled() != cancellation_type::none) && hp.cancel->throw_if_cancelled_)
          asio::detail::throw_error(asio::error::operation_aborted, "coro-cancelled");

      if (hp.get_executor() == coro.get_executor())
      {
        coro.coro_->awaited_from = h;
        coro.coro_->reset_error();
        coro.coro_->input_ = std::move(value);
        coro.coro_->cancel = hp.cancel;
        return coro.coro_->get_handle();
      }
      else
      {
        coro.coro_->awaited_from =
                dispatch_coroutine(asio::prefer(hp.get_executor(), execution::outstanding_work.tracked),
                [h]() mutable { h.resume(); });
        coro.coro_->reset_error();
        coro.coro_->input_ = std::move(value);

        struct cancel_handler
        {
          using src = std::pair<cancellation_signal, detail::coro_cancellation_source>;
          std::shared_ptr<src> st = std::make_shared<src>();

          cancel_handler(E e, coro_t& coro) : e(e), coro_(coro.coro_)
          {
            st->second.state = cancellation_state(st->second.slot = st->first.slot());
          }

          E e;
          typename coro_t::promise_type* coro_;

          void operator()(cancellation_type ct)
          {
            asio::dispatch(e, [ct, p = coro_, st = st]() mutable
            {
              auto & [sig, state] = *st;
              sig.emit(ct);
            });
          }
        };

        if (hp.cancel->state.slot().is_connected())
          hp.cancel->state.slot().template emplace<cancel_handler>(coro.get_executor(), coro);

        auto hh = detail::coroutine_handle<typename coro_t::promise_type>::from_promise(*coro.coro_);
        return dispatch_coroutine(
                coro.coro_->get_executor(), [hh]() mutable { hh.resume(); });
      }

    }

    auto await_resume() -> typename coro_t::result_type
    {
      if constexpr (!std::is_void_v<typename coro_t::executor_type>)
        coro.coro_->cancel = nullptr;
      coro.coro_->rethrow_if();
      return std::move(coro.coro_->result_);
    }
  };

  template <typename CompletionToken>
  auto async_resume(CompletionToken&& token) &&
  {
    return coro.async_resume(std::move(value),
                             std::forward<CompletionToken>(token));
  }

  auto operator co_await() &&
  {
    return awaitable_t{std::move(value), coro};
  }
};

template<bool IsNoexcept>
struct coro_promise_error;

template<>
struct coro_promise_error<false>
{
  std::exception_ptr error_;

  void reset_error()
  { error_ = std::exception_ptr{}; }

  void unhandled_exception()
  { error_ = std::current_exception(); }

  void rethrow_if()
  {
    if (error_)
      std::rethrow_exception(error_);
  }
};

template<>
struct coro_promise_error<true>
{
  void reset_error()
  {}

  void unhandled_exception() noexcept
  { throw; }

  void rethrow_if()
  {}
};


template<typename T = void>
struct yield_input
{
  T &value;
  coroutine_handle<> awaited_from{noop_coroutine()};

  bool await_ready() const noexcept
  {
    return false;
  }
  template<typename U>
  coroutine_handle<> await_suspend(coroutine_handle<U>) noexcept
  {
    return std::exchange(awaited_from, noop_coroutine());
  }

  T await_resume() const noexcept
  {
    return std::move(value);
  }
};

template<>
struct yield_input<void>
{
  coroutine_handle<> awaited_from{noop_coroutine()};

  bool await_ready() const noexcept
  {
    return false;
  }

  auto await_suspend(coroutine_handle<>) noexcept
  {
    return std::exchange(awaited_from, noop_coroutine());
  }

  constexpr void await_resume() const noexcept
  {}
};

struct coro_awaited_from
{
  coroutine_handle<> awaited_from{noop_coroutine()};

  auto final_suspend() noexcept
  {
    struct suspendor
    {
      coroutine_handle<> awaited_from;

      constexpr static bool await_ready() noexcept
      { return false; }

      auto await_suspend(coroutine_handle<>) noexcept
      {
        return std::exchange(awaited_from, noop_coroutine());
      }

      constexpr static void await_resume() noexcept
      {}
    };

    return suspendor{std::exchange(awaited_from, noop_coroutine())};
  }

  ~coro_awaited_from()
  {
    awaited_from.resume();
  }//must be on the right executor
};

template<typename Yield, typename Input, typename Return>
struct coro_promise_exchange : coro_awaited_from
{
  using result_type = coro_result_t<Yield, Return>;

  result_type result_;
  Input input_;

  auto yield_value(Yield &&y)
  {
    result_ = std::move(y);
    return yield_input<Input>{input_, std::exchange(awaited_from, noop_coroutine())};
  }

  auto yield_value(const Yield &y)
  {
    result_ = y;
    return yield_input<Input>{input_, std::exchange(awaited_from, noop_coroutine())};
  }

  void return_value(const Return &r)
  { result_ = r; }

  void return_value(Return &&r)
  { result_ = std::move(r); }
};

template<typename YieldReturn>
struct coro_promise_exchange<YieldReturn, void, YieldReturn> : coro_awaited_from
{
  using result_type = coro_result_t<YieldReturn, YieldReturn>;

  result_type result_;

  auto yield_value(const YieldReturn &y)
  {
    result_ = y;
    return yield_input<void>{std::exchange(awaited_from, noop_coroutine())};
  }

  auto yield_value(YieldReturn &&y)
  {
    result_ = std::move(y);
    return yield_input<void>{std::exchange(awaited_from, noop_coroutine())};
  }

  void return_value(const YieldReturn &r)
  { result_ = r; }

  void return_value(YieldReturn &&r)
  { result_ = std::move(r); }
};

template<typename Yield, typename Return>
struct coro_promise_exchange<Yield, void, Return> : coro_awaited_from
{
  using result_type = coro_result_t<Yield, Return>;

  result_type result_;

  auto yield_value(const Yield &y)
  {
    result_.template emplace<0>(y);
    return yield_input<void>{std::exchange(awaited_from, noop_coroutine())};
  }

  auto yield_value(Yield &&y)
  {
    result_.template emplace<0>(std::move(y));
    return yield_input<void>{std::exchange(awaited_from, noop_coroutine())};
  }

  void return_value(const Return &r)
  { result_.template emplace<1>(r); }

  void return_value(Return &&r)
  { result_.template emplace<1>(std::move(r)); }
};

template<typename Yield, typename Input>
struct coro_promise_exchange<Yield, Input, void> : coro_awaited_from
{
  using result_type = coro_result_t<Yield, void>;

  result_type result_;
  Input input_;

  auto yield_value(Yield &&y)
  {
    result_ = std::move(y);
    return yield_input<Input>{input_,
                              std::exchange(awaited_from, noop_coroutine())};
  }

  auto yield_value(const Yield &y)
  {
    result_ = y;
    return yield_input<Input>{input_,
                              std::exchange(awaited_from, noop_coroutine())};
  }

  void return_void()
  { result_.reset(); }
};

template<typename Return>
struct coro_promise_exchange<void, void, Return> : coro_awaited_from
{
  using result_type = coro_result_t<void, Return>;

  result_type result_;

  void yield_value();

  void return_value(const Return &r)
  { result_ = r; }

  void return_value(Return &&r)
  { result_ = std::move(r); }
};


template<>
struct coro_promise_exchange<void, void, void> : coro_awaited_from
{
  void return_void()
  {}

  void yield_value();
};

template<typename Yield>
struct coro_promise_exchange<Yield, void, void> : coro_awaited_from
{
  using result_type = coro_result_t<Yield, void>;

  result_type result_;

  auto yield_value(const Yield &y)
  {
    result_ = y;
    return yield_input<void>{std::exchange(awaited_from, noop_coroutine())};
  }

  auto yield_value(Yield &&y)
  {
    result_ = std::move(y);
    return yield_input<void>{std::exchange(awaited_from, noop_coroutine())};
  }

  void return_void()
  { result_.reset(); }
};

template<typename Yield, typename Return, typename Executor>
struct coro_promise final :
        coro_promise_allocator<coro<Yield, Return, Executor>>,
        coro_promise_error<coro_traits<Yield, Return, Executor>::is_noexcept>,
        coro_promise_exchange<
                typename coro_traits<Yield, Return, Executor>::yield_type,
                typename coro_traits<Yield, Return, Executor>::input_type,
                typename coro_traits<Yield, Return, Executor>::return_type>
{
  using coro_type = coro<Yield, Return, Executor>;

  auto handle()
  { return coroutine_handle<coro_promise>::from_promise(this); }

  using executor_type = std::remove_volatile_t<Executor>;

  std::conditional_t<std::is_volatile_v<Executor> && !std::is_default_constructible_v<Executor>,
                     std::optional<executor_type>, executor_type> executor_;

  std::optional<coro_cancellation_source> cancel_source;
  coro_cancellation_source * cancel;

  using allocator_type =
  typename std::allocator_traits<associated_allocator_t < Executor>>::
  template rebind_alloc<std::byte>;
  using traits = coro_traits<Yield, Return, Executor>;

  using input_type = typename traits::input_type;
  using yield_type = typename traits::yield_type;
  using return_type = typename traits::return_type;
  using error_type = typename traits::error_type;
  using result_type = typename traits::result_type;
  constexpr static bool is_noexcept = traits::is_noexcept;

  auto get_executor() const -> executor_type
  {
    if constexpr (std::is_volatile_v<Executor>)
    {
      if (!executor_)
        asio::detail::throw_exception(execution::bad_executor());
      if constexpr (std::is_default_constructible_v<executor_type>)
        return executor_;
      else
        return *executor_;
    }
    else
      return executor_;
  }

  bool has_executor() const
  {
    if constexpr (std::is_volatile_v<Executor>)
      return executor_;
    else
      return true;
  }

  auto get_handle()
  {
    return coroutine_handle<coro_promise>::from_promise(*this);
  }

  template<typename... Args>
  coro_promise(executor_type executor, Args &&...) noexcept
          : executor_(std::move(executor))
  {
  }

  template<typename First, typename... Args>
  coro_promise(First &&, executor_type executor, Args &&...) noexcept
          : executor_(std::move(executor))
  {
  }

  template<typename First, execution_context Context, typename... Args>
  coro_promise(First &&, Context &&ctx, Args &&...) noexcept
          : executor_(ctx.get_executor())
  {
  }


  template<execution_context Context, typename... Args>
  coro_promise(Context &&ctx, Args &&...) noexcept
          : executor_(ctx.get_executor())
  {
  }

  template<typename = void>
    requires std::is_volatile_v<Executor>
  coro_promise() {}

  auto get_return_object()
  {
    return coro<Yield, Return, Executor>{this};
  }

  auto initial_suspend() noexcept
  {
    return suspend_always{};
  }

  using coro_promise_exchange<
          typename coro_traits<Yield, Return, Executor>::yield_type,
          typename coro_traits<Yield, Return, Executor>::input_type,
          typename coro_traits<Yield, Return, Executor>::return_type>::yield_value;

  auto await_transform(this_coro::executor_t) const
  {
    struct exec_helper
    {
      const executor_type &executor_;

      constexpr static bool await_ready() noexcept
      { return true; }

      constexpr static void await_suspend(coroutine_handle<>) noexcept
      {}

      executor_type await_resume() const noexcept
      {
        return executor_;
      }
    };

    if constexpr (std::is_volatile_v<Executor>)
    {
      if (!executor_)
        asio::detail::throw_exception(execution::bad_executor());
      if constexpr (std::is_default_constructible_v<executor_type>)
        return exec_helper{executor_};
      else
        return exec_helper{*executor_};
    }
    else
      return exec_helper{executor_};
  }

  auto await_transform(this_coro::cancellation_state_t) const
  {
    struct exec_helper
    {
      const asio::cancellation_state &value;

      constexpr static bool await_ready() noexcept
      { return true; }

      constexpr static void await_suspend(coroutine_handle<>) noexcept
      {}

      asio::cancellation_state await_resume() const noexcept
      {
        return value;
      }
    };
    assert(cancel);
    return exec_helper{cancel->state};
  }

  // This await transformation resets the associated cancellation state.
  auto await_transform(this_coro::reset_cancellation_state_0_t) noexcept
  {
    struct result
    {
      detail::coro_cancellation_source * src_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume() const
      {
        return src_->reset_cancellation_state();
      }
    };

    return result{cancel};
  }


  // This await transformation resets the associated cancellation state.
  template <typename Filter>
  auto await_transform(
          this_coro::reset_cancellation_state_1_t<Filter> reset) noexcept
  {
    struct result
    {
      detail::coro_cancellation_source* src_;
      Filter filter_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume()
      {
        return src_->reset_cancellation_state(ASIO_MOVE_CAST(Filter)(filter_));
      }
    };

    return result{cancel, ASIO_MOVE_CAST(Filter)(reset.filter)};
  }

  // This await transformation resets the associated cancellation state.
  template <typename InFilter, typename OutFilter>
  auto await_transform(
          this_coro::reset_cancellation_state_2_t<InFilter, OutFilter> reset)
  noexcept
  {
    struct result
    {
      detail::coro_cancellation_source* src_;
      InFilter in_filter_;
      OutFilter out_filter_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume()
      {
        return src_->reset_cancellation_state(
                ASIO_MOVE_CAST(InFilter)(in_filter_),
                ASIO_MOVE_CAST(OutFilter)(out_filter_));
      }
    };

    return result{cancel,
                  ASIO_MOVE_CAST(InFilter)(reset.in_filter),
                  ASIO_MOVE_CAST(OutFilter)(reset.out_filter)};
  }

  // This await transformation determines whether cancellation is propagated as
  // an exception.
  auto await_transform(this_coro::throw_if_cancelled_0_t) noexcept
     requires (!is_noexcept)
  {
    struct result
    {
      detail::coro_cancellation_source* src_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume()
      {
        return src_->throw_if_cancelled();
      }
    };

    return result{cancel};
  }

  // This await transformation sets whether cancellation is propagated as an
  // exception.
  auto await_transform(this_coro::throw_if_cancelled_1_t throw_if_cancelled) noexcept
    requires (!is_noexcept)
  {
    struct result
    {
      detail::coro_cancellation_source* src_;
      bool value_;

      bool await_ready() const noexcept
      {
        return true;
      }

      void await_suspend(coroutine_handle<void>) noexcept
      {
      }

      auto await_resume()
      {
        src_->throw_if_cancelled(value_);
      }
    };

    return result{cancel, throw_if_cancelled.value};
  }


  template<typename Yield_, typename Return_, typename Executor_>
  auto await_transform(coro<Yield_, Return_, Executor_> &kr) -> decltype(auto)
  {
    return kr;
  }

  template<typename Yield_, typename Return_, typename Executor_>
  auto await_transform(coro<Yield_, Return_, Executor_> &&kr)
  {
    return std::move(kr);
  }

  template<typename T_, typename Coroutine >
  auto await_transform(coro_with_arg<T_, Coroutine> &&kr) -> decltype(auto)
  {
    return std::move(kr);
  }

  template<typename T_>
  requires
  requires(T_ t) {{ t.async_wait(use_coro) }; }
  auto await_transform(T_ &t) -> decltype(auto)
  {
    return await_transform(t.async_wait(use_coro));
  }

  template<typename... Ts>
  auto await_transform(coro_init_handler<Executor, Ts...> &&kr) const
  {
    assert(cancel);
    if constexpr (is_noexcept)
      return std::move(kr).as_noexcept(cancel->state.slot());
    else
    {
      if ((cancel->state.cancelled() != cancellation_type::none) && cancel->throw_if_cancelled_)
        asio::detail::throw_error(asio::error::operation_aborted, "coro-cancelled");
      return std::move(kr).as_throwing(cancel->state.slot());
    }

  }
};


template<typename Yield, typename Return>
struct coro_promise<Yield, Return, void> final :
        coro_promise_error<coro_traits<Yield, Return, void>::is_noexcept>,
        coro_promise_exchange<
                typename coro_traits<Yield, Return, void>::yield_type,
                typename coro_traits<Yield, Return, void>::input_type,
                typename coro_traits<Yield, Return, void>::return_type>
{
  using coro_type = coro<Yield, Return, void>;

  auto handle()
  {
    return coroutine_handle<coro_promise>::from_promise(this);
  }

  using executor_type = void;

  using traits = coro_traits<Yield, Return, void>;

  using input_type  = typename traits::input_type;
  using yield_type  = typename traits::yield_type;
  using return_type = typename traits::return_type;
  using error_type  = typename traits::error_type;
  using result_type = typename traits::result_type;
  constexpr static bool is_noexcept = traits::is_noexcept;

  void get_executor() const { }

  auto get_handle()
  {
    return coroutine_handle<coro_promise>::from_promise(*this);
  }
  auto get_return_object()
  {
    return coro<Yield, Return, void>{this};
  }

  auto initial_suspend() noexcept
  {
    return suspend_always{};
  }

  using coro_promise_exchange<
          typename coro_traits<Yield, Return, void>::yield_type,
          typename coro_traits<Yield, Return, void>::input_type,
          typename coro_traits<Yield, Return, void>::return_type>::yield_value;

  auto await_transform(this_coro::executor_t) const
  {
    return detail::suspend_never();
  }

  template<typename Yield_, typename Return_, typename Executor_>
  auto await_transform(coro<Yield_, Return_, Executor_> &kr) -> decltype(auto)
  {
    return kr;
  }

  template<typename Yield_, typename Return_, typename Executor_>
  auto await_transform(coro<Yield_, Return_, Executor_> &&kr)
  {
    return std::move(kr);
  }

  template<typename T_, typename Coroutine >
  auto await_transform(coro_with_arg<T_, Coroutine> &&kr) -> decltype(auto)
  {
    return std::move(kr);
  }

  template<typename T_>
    requires requires(T_ t) {{ t.wait() }; }  && (!is_noexcept)
  auto await_transform(T_ &t) -> decltype(auto)
  {
    struct result
    {
      T_ & waitable_object;

      bool await_ready() const noexcept { return true; }
      void await_suspend(coroutine_handle<void>) noexcept { }
      auto await_resume() { return waitable_object.wait(); }
    };
    return result{t};
  }


  template<typename T_>
    requires requires(T_ t, error_code ec) {{ t.wait(ec) }; } && is_noexcept
  auto await_transform(T_ &t) -> decltype(auto)
  {
    struct result
    {
      T_ & waitable_object;

      bool await_ready() const noexcept { return true; }
      void await_suspend(coroutine_handle<void>) noexcept { }
      auto await_resume()
      {
        error_code ec;
        if constexpr (std::is_void_v<decltype(waitable_object.wait(ec))>)
        {
          waitable_object.wait(ec);
          return ec;
        }
        else
          return std::make_pair(waitable_object.wait(ec), ec);

      }
    };
    return result{t};
  }

  result_type resume()
  {
    this->reset_error();
    auto ch = detail::coroutine_handle<coro_promise<Yield, Return, void>>::from_promise(*this);
    ch.resume();

    if constexpr (!is_noexcept)
      if (this->error_)
        std::rethrow_exception(this->error_);

    if constexpr (!std::is_void_v<result_type>)
      return std::move(this->result_);
  }
  template<typename T>
  result_type resume(T && t)
  {
    this->input_ = std::forward<T>(t);
    this->reset_error();
    auto ch = detail::coroutine_handle<coro_promise<Yield, Return, void>>::from_promise(*this);

    ch.resume();

    if constexpr (!is_noexcept)
      if (this->error_)
        std::rethrow_exception(this->error_);

    if constexpr (!std::is_void_v<result_type>)
      return std::move(this->result_);
  }

  auto resume(std::nothrow_t)
    -> std::conditional_t<std::is_void_v<result_type>, error_type, std::variant<error_type, result_type>>
  {
    this->reset_error();
    auto ch = detail::coroutine_handle<coro_promise<Yield, Return, void>>::from_promise(*this);
    ch.resume();

    if constexpr (!is_noexcept)
      if (this->error_)
        return this->error_;

    if constexpr (!std::is_void_v<result_type>)
      return {std::in_place_index<1u>, std::move(this->result_)};
    else
      return std::exception_ptr{};

  }

  template<typename T>
  auto resume(T && t, std::nothrow_t)
    -> std::conditional_t<std::is_void_v<result_type>, error_type, std::variant<error_type, result_type>>
  {
    this->input_ = std::forward<T>(t);
    this->reset_error();
    auto ch = detail::coroutine_handle<coro_promise<Yield, Return, void>>::from_promise(*this);
    ch.resume();

    if constexpr (!is_noexcept)
      if (this->error_)
        return this->error_;

    if constexpr (!std::is_void_v<result_type>)
      return {std::in_place_index<1u>, std::move(this->result_)};
    else
      return std::exception_ptr{};
  }

};

} // namespace detail

template <typename Yield, typename Return, typename Executor>
struct coro<Yield, Return, Executor>::awaitable_t
{
  coro& coro_;

  constexpr static bool await_ready() { return false; }

  template <typename Y, typename R, typename E>
    requires std::is_void_v<Executor>
  auto await_suspend(detail::coroutine_handle<detail::coro_promise<Y, R, E>> h) -> detail::coroutine_handle<>
  {
    auto& hp = h.promise();
    if constexpr (!detail::coro_promise<Y, R, E>::is_noexcept)
      if ((hp.cancel->state.cancelled() != cancellation_type::none) && hp.cancel->throw_if_cancelled_)
        asio::detail::throw_error(asio::error::operation_aborted, "coro-cancelled");

    coro_.coro_->awaited_from  = h;
    coro_.coro_->reset_error();

    return coro_.coro_->get_handle();

  }


  template <typename Y, typename R, typename E>
    requires (!std::is_void_v<Executor>)
  auto await_suspend(detail::coroutine_handle<detail::coro_promise<Y, R, E>> h) -> detail::coroutine_handle<>
  {
    auto& hp = h.promise();
    if constexpr (!detail::coro_promise<Y, R, E>::is_noexcept)
      if ((hp.cancel->state.cancelled() != cancellation_type::none) && hp.cancel->throw_if_cancelled_)
        asio::detail::throw_error(asio::error::operation_aborted, "coro-cancelled");

    if (hp.get_executor() == coro_.get_executor())
    {
      coro_.coro_->awaited_from  = h;
      coro_.coro_->cancel = hp.cancel;
      coro_.coro_->reset_error();

      return coro_.coro_->get_handle();
    }
    else
    {
      coro_.coro_->awaited_from = detail::dispatch_coroutine(
              asio::prefer(hp.get_executor(), execution::outstanding_work.tracked),
              [h]() mutable
              {
                h.resume();
              });
      coro_.coro_->reset_error();

      struct cancel_handler
      {
        using src = std::pair<cancellation_signal, detail::coro_cancellation_source>;
        std::shared_ptr<src> st = std::make_shared<src>();

        cancel_handler(E e, coro& coro) : e(e), coro_(coro.coro_)
        {
          st->second.state = cancellation_state(st->second.slot = st->first.slot());
        }

        E e;
        typename coro::promise_type* coro_;

        void operator()(cancellation_type ct)
        {
          asio::dispatch(e, [ct, p = coro_, st = st]() mutable
                         {
                            auto & [sig, state] = *st;
                            sig.emit(ct);
                         });
        }
      };
      if (hp.cancel->state.slot().is_connected())
        hp.cancel->state.slot().template emplace<cancel_handler>(coro_.get_executor(), coro_);

      auto hh = detail::coroutine_handle<detail::coro_promise<Yield, Return, Executor>>::from_promise(*coro_.coro_);
      return detail::dispatch_coroutine(
              coro_.coro_->get_executor(), [hh]() mutable { hh.resume(); });
    }
  }

  auto await_resume() -> result_type
  {
    if constexpr (!std::is_void_v<Executor>)
      coro_.coro_->cancel = nullptr;

    coro_.coro_->rethrow_if();
    if constexpr (!std::is_void_v<result_type>)
      return std::move(coro_.coro_->result_);
  }
};


template <typename Yield, typename Return, typename Executor>
struct coro<Yield, Return, Executor>::initiate_async_resume
{
  typedef std::remove_volatile_t<Executor> executor_type;

  explicit initiate_async_resume(coro* self)
          : self_(self)
  {
  }

  executor_type get_executor() const noexcept
  {
    return self_->get_executor();
  }

  template <typename E, typename WaitHandler>
  auto handle(E exec, WaitHandler&& handler,
              std::true_type /* error is noexcept */,
              std::true_type  /* result is void */)  //noexcept
  {
    return [this, self = self_,
            h = std::forward<WaitHandler>(handler),
            exec = std::move(exec)]() mutable
    {
      assert(self);
      auto ch =
              detail::coroutine_handle<promise_type>::from_promise(*self->coro_);
      assert(ch && !ch.done());
      assert(self->coro_->awaited_from == detail::noop_coroutine());

      self->coro_->awaited_from =
              detail::post_coroutine(std::move(exec), std::move(h));
      self->coro_->reset_error();
      ch.resume();
    };
  }

  template <typename E, typename WaitHandler>
  requires (!std::is_void_v<result_type>)
  auto handle(E exec, WaitHandler&& handler,
              std::true_type /* error is noexcept */,
              std::false_type  /* result is void */)  //noexcept
  {
    return [this, self = self_,
            h = std::forward<WaitHandler>(handler),
            exec = std::move(exec)]() mutable
    {
      assert(self);

      auto ch =
              detail::coroutine_handle<promise_type>::from_promise(*self->coro_);
      assert(ch && !ch.done());
      assert(self->coro_->awaited_from == detail::noop_coroutine());

      self->coro_->awaited_from =
              detail::post_coroutine(exec,
                                     [self, h = std::move(h)]() mutable
                                     {
                                       std::move(h)(std::move(self->coro_->result_));
                                     });
      self->coro_->reset_error();
      ch.resume();
    };
  }

  template <typename E, typename WaitHandler>
  auto handle(E exec, WaitHandler&& handler,
              std::false_type /* error is noexcept */,
              std::true_type  /* result is void */)
  {
    return [this, self = self_,
            h = std::forward<WaitHandler>(handler),
            exec = std::move(exec)]() mutable
    {
      if (!self)
      {
        asio::post(exec,
                   [h = std::move(h)]() mutable
                   {
                     h(detail::coro_error<error_type>::invalid());
                   });
        return;
      }

      auto ch =
              detail::coroutine_handle<promise_type>::from_promise(*self->coro_);
      if (!ch)
      {
        asio::post(exec,
                   [h = std::move(h)]() mutable
                   {
                     h(detail::coro_error<error_type>::invalid());
                   });
      }
      else if (ch.done())
      {
        asio::post(exec,
                   [h = std::move(h)]() mutable
                   {
                     h(detail::coro_error<error_type>::done());
                   });
      }
      else
      {
        assert(self->coro_->awaited_from == detail::noop_coroutine());
        self->coro_->awaited_from =
                detail::post_coroutine(exec,
                                       [self, h = std::move(h)]() mutable
                                       {
                                         std::move(h)(std::move(self->coro_->error_));
                                       });
        self->coro_->reset_error();
        ch.resume();
      }
    };
  }

  template <typename E, typename WaitHandler>
  auto handle(E exec, WaitHandler&& handler,
              std::false_type /* error is noexcept */,
              std::false_type  /* result is void */)
  {
    return [this, self = self_,
            h = std::forward<WaitHandler>(handler),
            exec = std::move(exec)]() mutable
    {
      if (!self)
      {
        asio::post(exec,
                   [h = std::move(h)]() mutable
                   {
                     h(detail::coro_error<error_type>::invalid(), result_type{});
                   });
        return;
      }

      auto ch =
              detail::coroutine_handle<promise_type>::from_promise(*self->coro_);
      if (!ch)
      {
        asio::post(exec,
                   [h = std::move(h)]() mutable
                   {
                     h(detail::coro_error<error_type>::invalid(), result_type{});
                   });
      }
      else if (ch.done())
      {
        asio::post(exec,
                   [h = std::move(h)]() mutable
                   {
                     h(detail::coro_error<error_type>::done(), result_type{});
                   });
      }
      else
      {
        assert(self->coro_->awaited_from == detail::noop_coroutine());
        self->coro_->awaited_from =
                detail::post_coroutine(exec,
                                       [h = std::move(h), self]() mutable
                                       {
                                         std::move(h)(std::move(self->coro_->error_),
                                                      std::move(self->coro_->result_));
                                       });
        self->coro_->reset_error();
        ch.resume();
      }
    };
  }

  template <typename WaitHandler>
  auto prepare_executor(WaitHandler&& handler)
  {
    if constexpr (std::is_volatile_v<Executor>)
    {
      auto e = get_associated_executor(handler);;
      self_->coro_->executor_ = e;
      return asio::prefer(e, execution::outstanding_work.tracked);
    }
    else
      return asio::prefer(get_associated_executor(handler, get_executor()), execution::outstanding_work.tracked);
  }

  template <typename WaitHandler>
  void operator()(WaitHandler&& handler)
  {
    const auto exec = prepare_executor(handler);

    auto c = self_->coro_;
    c->cancel = &c->cancel_source.emplace();
    c->cancel->state = cancellation_state(c->cancel->slot = get_associated_cancellation_slot(handler));
    asio::dispatch(get_executor(),
                   handle(exec, std::forward<WaitHandler>(handler),
                          std::integral_constant<bool, is_noexcept>{},
                          std::is_void<result_type>{}));
  }

  template <typename WaitHandler, typename Input>
  void operator()(WaitHandler&& handler, Input&& input)
  {
    const auto exec =
            asio::prefer(
                    get_associated_executor(handler, get_executor()),
                    execution::outstanding_work.tracked);

    auto c = self_->coro_;
    c->cancel = &c->cancel_source.emplace();
    c->cancel->state = cancellation_state(c->cancel->slot = get_associated_cancellation_slot(handler));
    asio::dispatch(get_executor(),
                   [h = handle(exec, std::forward<WaitHandler>(handler),
                               std::integral_constant<bool, is_noexcept>{},
                               std::is_void<result_type>{}),
                           in = std::forward<Input>(input), self = self_]() mutable
                   {
                     self->coro_->input_ = std::move(in);
                     std::move(h)();
                   });
  }

 private:
  coro* self_;
};

} // namespace experimental
} // namespace asio




#include "asio/detail/pop_options.hpp"

#endif //ASIO_EXPERIMENTAL_IMPL_CORO_HPP
