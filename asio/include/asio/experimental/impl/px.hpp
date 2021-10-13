//
// impl/post.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_EXPERIMENTAL_IMPL_PX_HPP
#define ASIO_EXPERIMENTAL_IMPL_PX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#include "asio/steady_timer.hpp"

#include "asio/detail/push_options.hpp"

namespace asio
{
namespace experimental
{
namespace px
{
namespace detail
{

struct px_probe {};

template <typename T>
struct px_probe_result;

template <typename... Args>
struct px_probe_result<void(Args...)> {
  typedef std::tuple<Args...> type;
};

}
}
}

template <typename R, typename... Args>
class async_result<experimental::px::detail::px_probe, R(Args...)> {
 public:
  typedef experimental::px::detail::px_probe_result<void(Args...)> return_type;

  template <typename Initiation, typename... InitArgs>
  static return_type initiate(Initiation&&, experimental::px::detail::px_probe, InitArgs&&...)
  {
    return return_type{};
  }
};

namespace experimental
{
namespace px
{
namespace detail
{

template<typename Executor, typename... Ops>
struct merger {
  merger(Executor executor, Ops &&... ops)
          : impl_(std::make_shared<impl>(std::move(executor), std::move(ops)...)) {}

  using result_type = std::variant<typename decltype(std::declval<Ops>()(px_probe{}))::type ...>;
  using signature = void(result_type);

  template<typename CompletionToken>
  auto async_wait(CompletionToken &&completion_token)
  {
    return impl_->async_wait(impl_, std::forward<CompletionToken>(completion_token));
  }

  void restart_all() {
    [&]<std::size_t... Is>(std::index_sequence<Is...>)
    {
      (restart(std::in_place_index<Is>), ...);
    }
            (std::make_index_sequence<sizeof...(Ops)>{});
  }

  template<std::size_t I>
  void restart(std::in_place_index_t<I> t)
  {
    get < I > (impl_->ops)(
            bind_executor(get_executor(),
                          bind_cancellation_slot(impl_->cancel_signals[I].slot(),
                                                 [impl = impl_, t](auto ... args) {
                                                   impl->complete(t, std::move(args)...);
                                                 })));
  }

  void restart(std::size_t I)
  {
    [&]<std::size_t... Is>(std::index_sequence<Is...>)
    {
      ([&]<std::size_t Idx>(std::in_place_index_t<Idx> t)
      {
        if (I == Idx) restart(t);
      }(std::in_place_index<Is>), ...);
    }
    (std::make_index_sequence<sizeof...(Ops)>{});
  }

  using executor_type = Executor;

  executor_type get_executor() { return impl_->timer.get_executor(); }

  void cancel(asio::cancellation_type type = asio::cancellation_type::all)
  {
    asio::post(get_executor(), [impl = impl_, type]{impl->cancel(type);});
  }

  ~merger()
  {
    cancel();
    impl_->timer.expires_at(chrono::steady_clock::time_point::min());
  }

  std::size_t pending()
  {
    return impl_->in_flight;
  }

 private:
  struct impl {
    asio::steady_timer timer;

    impl(Executor exec, Ops &&... ops)
           : timer(std::move(exec), asio::chrono::steady_clock::time_point::max()), ops(std::move(ops)...)
    {
      std::fill(completion_order.begin(), completion_order.end(), std::numeric_limits<std::size_t>::max());
    }

    ~impl()
    {
    }

    std::array<std::size_t, sizeof...(Ops)> completion_order;
    std::tuple<std::optional<typename decltype(std::declval<Ops>()(px_probe{}))::type>...> results;
    std::array<asio::cancellation_signal, sizeof...(Ops)> cancel_signals;

    template<std::size_t I, typename... Args>
    void complete(std::in_place_index_t<I>, Args &&... args)
    {
      get < I > (results).emplace(std::forward<Args>(args)...);
      *std::find(completion_order.begin(), completion_order.end(), std::numeric_limits<std::size_t>::max()) = I;
      cancel_signals[I].slot().clear();
      timer.cancel();
    }

    void cancel(asio::cancellation_type type)
    {
      for (auto &c: cancel_signals)
        c.emit(type);
    }

    template<typename CompletionToken>
    auto async_wait(std::shared_ptr<impl> p, CompletionToken &&completion_token) {
      constexpr auto mx = std::numeric_limits<std::size_t>::max();
      auto done_already = completion_order[0] != mx;

      auto transform =
              asio::experimental::deferred(
                      [this, p](error_code ec = {}) {
                        result_type res;

                        if (completion_order[0] != mx) // something available
                        {
                          ec.clear();
                          const auto idx = completion_order[0];
                          *std::shift_left(completion_order.begin(), completion_order.end(), 1) = mx;

                          auto step = [&]<std::size_t I>(std::in_place_index_t<I>) {
                            if (I == idx)
                              res.template emplace<I>(*std::exchange(std::get<I>(results), std::nullopt));
                          };

                          [&]<std::size_t ... Is>(std::index_sequence<Is...>) {
                            (step(std::in_place_index<Is>), ...);
                          }(std::make_index_sequence<sizeof...(Ops)>{});

                        }
                        return asio::experimental::deferred.values(std::make_tuple(ec, std::move(res)));
                      });

      if (done_already)
        return asio::post(timer.get_executor(), transform)
                (std::forward<CompletionToken>(completion_token));
      else
        return timer.async_wait(transform)(std::forward<CompletionToken>(completion_token));
    }
    std::tuple<Ops...> ops;
  };

  std::shared_ptr<impl> impl_;
};

template<typename Executor, typename... Ops>
struct combiner
{

  combiner(Executor executor, Ops &&... ops)
          : impl_(std::make_shared<impl>(std::move(executor), std::move(ops)...)) {}

  using result_type = std::variant<typename decltype(std::declval<Ops>()(px_probe{}))::type ...>;
  using signature = void(result_type);

  template<typename CompletionToken>
  auto async_wait(CompletionToken &&completion_token)
  {
    return impl_->async_wait(impl_, std::forward<CompletionToken>(completion_token));
  }

  void restart_all() {
    [&]<std::size_t... Is>(std::index_sequence<Is...>)
    {
      (restart(std::in_place_index<Is>), ...);
    }
            (std::make_index_sequence<sizeof...(Ops)>{});
  }

  template<std::size_t I>
  void restart(std::in_place_index_t<I> t)
  {
    get < I > (impl_->ops)(
            bind_executor(get_executor(),
                          bind_cancellation_slot(impl_->cancel_signals[I].slot(),
                                                 [impl = impl_, t](auto ... args) {
                                                   impl->complete(t, std::move(args)...);
                                                 })));
  }

  void restart(std::size_t I)
  {
    [&]<std::size_t... Is>(std::index_sequence<Is...>)
    {
      ([&]<std::size_t Idx>(std::in_place_index_t<Idx> t)
      {
        if (I == Idx) restart(t);
      }(std::in_place_index<Is>), ...);
    }
            (std::make_index_sequence<sizeof...(Ops)>{});
  }

  using executor_type = Executor;

  executor_type get_executor() { return impl_->timer.get_executor(); }

  void cancel(asio::cancellation_type type = asio::cancellation_type::all)
  {
    asio::post(get_executor(), [impl = impl_, type]{impl->cancel(type);});
  }

  ~combiner()
  {
    cancel();
    impl_->timer.expires_at(chrono::steady_clock::time_point::min());
  }

  std::size_t pending()
  {
    return impl_->in_flight;
  }

 private:
  struct impl {
    asio::steady_timer timer;

    impl(Executor exec, Ops &&... ops)
            : timer(std::move(exec), asio::chrono::steady_clock::time_point::max()), ops(std::move(ops)...)
    {
      std::fill(completion_order.begin(), completion_order.end(), std::numeric_limits<std::size_t>::max());
    }

    ~impl()
    {
    }

    std::array<std::size_t, sizeof...(Ops)> completion_order;
    std::tuple<std::optional<typename decltype(std::declval<Ops>()(px_probe{}))::type>...> results;
    std::array<asio::cancellation_signal, sizeof...(Ops)> cancel_signals;

    template<std::size_t I, typename... Args>
    void complete(std::in_place_index_t<I>, Args &&... args)
    {
      get < I > (results).emplace(std::forward<Args>(args)...);
      *std::find(completion_order.begin(), completion_order.end(), std::numeric_limits<std::size_t>::max()) = I;
      cancel_signals[I].slot().clear();
      timer.cancel();
    }

    void cancel(asio::cancellation_type type)
    {
      for (auto &c: cancel_signals)
        c.emit(type);
    }

    template<typename CompletionToken>
    auto async_wait(std::shared_ptr<impl> p, CompletionToken &&completion_token) {
      constexpr auto mx = std::numeric_limits<std::size_t>::max();
      auto done_already = completion_order[0] != mx;

      auto transform =
              asio::experimental::deferred(
                      [this, p](error_code ec = {}) {
                        std::size_t idx = std::numeric_limits<std::size_t>::max();
                        if (completion_order[0] != mx) // something available
                        {
                          ec.clear();
                          idx = completion_order[0];
                          *std::shift_left(completion_order.begin(), completion_order.end(), 1) = mx;
                        }

                        return asio::experimental::deferred.values(
                                std::make_tuple(ec, idx, results));
                      });

      if (done_already)
        return asio::post(timer.get_executor(), transform)
                (std::forward<CompletionToken>(completion_token));
      else
        return timer.async_wait(transform)(std::forward<CompletionToken>(completion_token));
    }
    std::tuple<Ops...> ops;
  };

  std::shared_ptr<impl> impl_;
};


}
}
}

}


#include "asio/detail/pop_options.hpp"

#endif //ASIO_EXPERIMENTAL_IMPL_PX_HPP
