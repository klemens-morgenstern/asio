//
// experimental/px.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASIO_EXPERIMENTAL_PX_HPP
#define ASIO_EXPERIMENTAL_PX_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <queue>
#include <tuple>

#include "asio/detail/config.hpp"
#include "asio/detail/type_traits.hpp"
#include "asio/experimental/cancellation_condition.hpp"
#include "asio/experimental/co_spawn.hpp"
#include "asio/experimental/deferred.hpp"
#include "asio/experimental/impl/px.hpp"
#include "asio/experimental/parallel_group.hpp"
#include "asio/experimental/use_coro.hpp"

#include "asio/redirect_error.hpp"
#include "asio/steady_timer.hpp"

#include "asio/detail/push_options.hpp"

namespace asio
{
namespace experimental
{

template <typename Yield, typename Return, typename Executor>
struct coro;

/// The namespace for proactive extensions.
namespace px
{
namespace detail
{

struct ignore_t
{
  template<typename ... Args>
  void operator()(Args && ...) const {}
};

constexpr ignore_t ignore;

}

template<typename T>
struct is_stream : std::false_type {};

template<typename T, typename Executor>
struct is_stream<coro<T, void, Executor>> : std::bool_constant<!std::is_void<T>::value> {};

template<typename T, typename Executor>
struct is_stream<coro<T(), void, Executor>>: std::bool_constant<!std::is_void<T>::value> {};

template<typename T, typename Executor>
struct is_stream<coro<T() noexcept, void, Executor>>: std::bool_constant<!std::is_void<T>::value> {};

template<typename T>
constexpr auto is_stream_v = is_stream<T>::value;


/// A concept for any type that can be used as a stream.
template<typename T>
concept stream = is_stream_v<T>;


///Create an empty stream
/**
 *
 * @tparam T The type of the stream
 * @tparam Executor The Executor to be used by the stream
 * @return An empty stream<T>
 *
 * @image html http://reactivex.io/documentation/operators/images/empty.c.png
 */
template<typename T = int, typename Executor>
inline coro<T, void, Executor> empty(Executor)
{
    co_return ;
}

template<typename T = int, typename Executor>
coro<T, void, Executor> never(Executor executor)
{
    asio::steady_timer tm{executor, std::chrono::steady_clock::time_point::max()};
    co_await tm.async_wait(use_coro);
}

template<typename T = int, typename Executor, typename Error>
coro<T, void, Executor> error(Executor, Error error)
{
    throw error;
    co_return ;
}

template<typename Executor, typename Range>
coro<std::decay_t<decltype(*std::begin(std::declval<Range>()))> , void, Executor>
        from(Executor executor, Range range)
{
    for (auto && r : std::move(range))
        co_yield std::move(r);
}

template<typename Executor, typename T>
coro<T, void, Executor> from(Executor executor, std::initializer_list<T> range)
{
  for (auto && r : std::move(range))
    co_yield std::move(r);
}

template<typename Clock = std::chrono::steady_clock, typename Executor, typename Rep, typename Ratio>
coro<int, void, Executor>
interval(Executor executor, const std::chrono::duration<Rep, Ratio> dur)
{
    asio::basic_waitable_timer<Clock, wait_traits<Clock>, Executor> tm{executor};

    auto ex = Clock::now();
    int i = 0u;
    while ((co_await this_coro::cancellation_state).cancelled() == asio::cancellation_type::none)
    {
      tm.expires_at(ex += dur);
      co_await tm.async_wait(use_coro);
      co_yield i++;
    }
}


template<typename Executor, typename ... Args>
coro<std::tuple_element_t<0u, std::tuple<Args...>>, void, Executor>
just(Executor, Args ... args)
{
    (co_yield std::move(args), ...);
}


template<typename Executor, typename T>
coro<T, void, Executor>
range(Executor, T begin, T end)
{
    for (auto val = begin; val < end; val ++)
        co_yield val;
}


template<typename Executor, typename T>
coro<T, void, Executor>
repeat(Executor, T value, std::size_t Count)
{
    for (std::size_t val = 0u; val < Count; val ++)
        co_yield value;
}

template<typename Clock = std::chrono::steady_clock, typename Executor, typename Rep, typename Ratio>
coro<int, void, Executor>
timer(Executor executor, const std::chrono::duration<Rep, Ratio> dur)
{
    asio::basic_waitable_timer<Clock> tm{executor};
    tm.expires_after(dur);
    co_await tm.async_wait(use_coro);
    co_yield 0;
}

template<typename Op, typename ... Args>
struct op_bind
{
    constexpr op_bind(Args ... args) : args(std::move(args)...) {}
    std::tuple<Args...> args;
};

template<typename T, typename Executor, typename Op, typename ... Args>
constexpr auto operator|(coro<T, void, Executor> c, op_bind<Op, Args...> op)
{
    return std::apply(
            [&](Args ... args)
            {
                return Op{}(std::move(c), std::move(args)...);
            }, std::move(op.args));
}

struct buffer_t
{
    template<typename T, typename Executor>
    auto operator()(coro<T, void, Executor> c, const std::size_t count) const -> coro<std::vector<T>, void, Executor>
    {
        std::vector<T> res;
        while (auto v = co_await c)
        {
            res.push_back(std::move(*v));

            if (res.size() == count)
                co_yield std::exchange(res, {});
        }
    }

    template<typename T, typename Executor>
    auto operator()(coro<T, void, Executor> c, const std::size_t count, const std::size_t skip) const
            -> coro<std::vector<T>, void, Executor>
    {
        std::vector<T> res;

        const auto commit_cnt = std::max(count, skip);
        std::size_t received = 0u;
        while (auto v = co_await c)
        {
            received ++;
            if (res.size() < count)
                res.push_back(std::move(*v));

            if (received == commit_cnt)
            {
              received = 0u;
              co_yield std::exchange(res, {});
            }
        }
    }

    auto operator()( std::size_t count) const -> op_bind<buffer_t, std::size_t>
    {
        return {count};
    }
    auto operator()( std::size_t count, std::size_t skip) const-> op_bind<buffer_t, std::size_t, std::size_t>
    {
        return {count, skip};
    }
};

constexpr buffer_t buffer;

struct flat_map_t
{
   //TODO
};

constexpr flat_map_t flat_map;


struct concat_map_t
{
  template<typename T, typename Executor, typename Func>
    requires requires (T t, Executor exec,  Func f)
    {
      {f(exec, t).async_resume(use_coro)};
    }
  auto operator()(coro<T, void, Executor> c, Func func) -> decltype(func(c.get_executor(), std::declval<T>()))
  {
    while (auto n = co_await c)
    {
      auto sub = func(c.get_executor(), *n);
      while (auto v = co_await sub)
        co_yield std::move(*v);
    }
  }

  template<typename Func>
  auto operator()( Func && func) const -> op_bind<concat_map_t, std::decay_t<Func>>
  {
    return {std::forward<Func>(func)};
  }
};

constexpr concat_map_t concat_map;

struct switch_map_t
{
  //TODO
};

constexpr switch_map_t switch_map;


struct group_by_t
{
  // TODO
  /*template<typename Executor, typename ... Ts>
  struct impl_t
  {
    asio::basic_waitable_timer<chrono::steady_clock, wait_traits<chrono::steady_clock>, Executor> timer;

    impl_t(Executor exec) :  timer(exec, chrono::steady_clock::time_point::max()) {}

    std::queue<std::variant<Ts...>> v;
    std::exception_ptr ex;
    bool done{false};

    template<typename T, typename Func>
    static coro<void() noexcept, void, Executor> fetch(coro<T, void, Executor> c, Func func, std::shared_ptr<impl_t> impl)
    {
      try {
        while (c.is_open())
        {
          impl->push_back(func(co_await c));
          impl->timer.cancel();
        }
        impl->done = true;
      }
      catch (...)
      {
        impl->ex = std::current_exception();
      }
      impl->timer.cancel();
    }
  };

  template<typename T, typename Executor, typename Func, typename ...Ts>
  auto impl(coro<T, void, Executor> c, Func func, std::variant<Ts...> *v) -> std::tuple<coro<Ts, void, Executor>...>
  {
    auto p = std::make_shared<impl_t<Executor, Ts...>>(c.get_executor());

    p->fetch(std::move(c), std::move(func), p).async_resume([p]{});
  }

  template<typename T, typename Executor, typename Func>
  requires requires (T t, Executor exec,  Func f)
  {
    std::variant_size<decltype(f(t))>{};
  }

  auto operator()(coro<T, void, Executor> c, Func func)
    -> decltype(func(c.get_executor(), std::declval<T>()))
  {
    return impl(std::move(c), std::move(func), static_cast<decltype(f(std::declval<T>()))*>(nullptr));
  }*/
};

constexpr group_by_t group_by;

struct map_t
{
    template<typename T, typename Executor, typename Func>
    auto operator()(coro<T, void, Executor> c, Func func) const -> coro<std::invoke_result_t<Func, T>, void, Executor>
    {
        while (auto v = co_await c)
            co_yield func(std::move(*v));
    }

    template<typename Func>
    auto operator()(Func func) const -> op_bind<map_t, Func>
    {
        return op_bind<map_t, Func>(std::move(func));
    }
};

constexpr map_t map;


struct scan_t
{
  template<typename T, typename Executor, typename Func, typename Init = T>
  auto operator()(coro<T, void, Executor> c, Func func, Init init = T{}) const -> coro<std::invoke_result_t<Func, T, Init>, void, Executor>
  {
    while (auto v = co_await c)
      co_yield init = func(std::move(*v), init);
  }

  template<typename Func>
  auto operator()(Func func) const -> op_bind<scan_t, Func>
  {
    return op_bind<scan_t, Func>(std::move(func));
  }

  template<typename Func, typename Init>
  auto operator()(Func func, Init init) const -> op_bind<scan_t, Func, Init>
  {
    return op_bind<scan_t, Func, Init>(std::move(func), std::move(init));
  }
};

constexpr scan_t scan;

struct window_t
{
  //TODO
};

constexpr window_t window;

struct debounce_t
{
  template<typename Clock, typename T, typename Executor, typename Rep, typename Ratio>
  coro<T, void, Executor> operator()(coro<T, void, Executor> c, std::chrono::duration<Rep, Ratio> dur)
  {
    const auto max = Clock::time_point::max();
    basic_waitable_timer<Clock, wait_traits<Clock>, Executor> tim{c.get_executor(), max};
    std::optional<T> value;
    std::exception_ptr ex;

    auto cc =
            [](coro<T, void, Executor> c, std::exception_ptr & ex, std::chrono::duration<Rep, Ratio> dur,
               std::optional<T> &value,
               basic_waitable_timer<Clock, wait_traits<Clock>, Executor> &tim) -> coro<void() noexcept, void, Executor>
            {
                try
                {
                  while ((value = co_await c))
                    tim.expires_after(dur);
                }
                catch (...)
                {
                  ex = std::current_exception();
                }
                tim.expires_at(Clock::time_point::min());

            }(std::move(c), ex, dur, value, tim);

    cc.async_resume(detached);
    while (c.is_open())
    {
      error_code ec = error::operation_aborted;
      while (ec == error::operation_aborted && c.is_open() && !ex)
        co_await tim.async_wait(use_coro);

      if (ex)
        std::rethrow_exception(ex);

      tim.expires_at(max);
      if (!value)
        co_return ;
      co_yield std::exchange(value, std::nullopt);
    }
  }

  template<typename T, typename Executor, typename Rep, typename Ratio>
  coro<T, void, Executor> operator()(coro<T, void, Executor> c, std::chrono::duration<Rep, Ratio> dur)
  {
    return operator()<chrono::steady_clock>(std::move(c), dur);
  }
};

constexpr debounce_t debounce;


struct distinct_t : op_bind<distinct_t>
{
  template<typename T, typename Executor, typename Func = std::equal_to<void>, typename Init = T>
  auto operator()(coro<T, void, Executor> c, Func equal = {}, Init init = T{}) const -> coro<T, void, Executor>
  {
    while (auto v = co_await c)
    {
        if (!equal(init, *v))
          co_yield init = std::move(*v);
    }
  }

  auto operator()() const  -> op_bind<distinct_t>
  {
    return op_bind<distinct_t>();
  }

  template<typename Func>
  auto operator()(Func func) const -> op_bind<distinct_t, Func>
  {
    return op_bind<distinct_t, Func>(std::move(func));
  }

  template<typename Func, typename Init>
  auto operator()(Func func, Init init) const  -> op_bind<distinct_t, Func, Init>
  {
    return op_bind<distinct_t, Func, Init>(std::move(func), std::move(init));
  }

};

constexpr distinct_t distinct;


struct element_at_t
{
  template<typename T, typename Executor>
  auto operator()(coro<T, void, Executor> c, std::size_t at) const -> coro<T, void, Executor>
  {
    while (auto v = co_await c)
    {
      if (at-- == 0u)
      {
        co_yield *v;
        break;
      }
    }
  }

  auto operator()(std::size_t at) const  -> op_bind<element_at_t, std::size_t>
  {
    return op_bind<element_at_t, std::size_t>(std::move(at));
  }
};

constexpr element_at_t element_at;

struct filter_t
{
  template<typename T, typename Executor, typename Func>
  auto operator()(coro<T, void, Executor> c, Func predicate) const -> coro<T, void, Executor>
  {
    while (auto v = co_await c)
      if (predicate(*v))
        co_yield std::move(*v);

  }

  template<typename Func>
  auto operator()(Func func) const -> op_bind<filter_t, Func>
  {
    return op_bind<filter_t, Func>(std::move(func));
  }
};

constexpr filter_t filter;

struct first_t : op_bind<first_t>
{
  template<typename T, typename Executor>
  auto operator()(coro<T, void, Executor> c) const -> coro<T, void, Executor>
  {
    if (auto v = co_await c)
      co_yield std::move(*v);
  }

  auto operator()() const -> op_bind<first_t>
  {
    return op_bind<first_t>();
  }
};

constexpr first_t first;

struct ignore_elements_t : op_bind<ignore_elements_t>
{
  template<typename T, typename Executor>
  auto operator()(coro<T, void, Executor> c) const -> coro<T, void, Executor>
  {
    while (co_await c);
  }

  auto operator()() const -> op_bind<ignore_elements_t>
  {
    return op_bind<ignore_elements_t>();
  }
};

constexpr ignore_elements_t ignore_elements;

struct last_t : op_bind<last_t>
{
  template<typename T, typename Executor>
  auto operator()(coro<T, void, Executor> c) const -> coro<T, void, Executor>
  {
    std::optional<T> res;
    while (auto next = co_await c)
      res = next;

    if (res)
      co_yield std::move(*res);
  }

  auto operator()() const -> op_bind<last_t>
  {
    return op_bind<last_t>();
  }
};

constexpr last_t last;


struct sample_t
{
  template<typename T, typename Executor, typename Rep, typename Ratio>
  auto operator()(coro<T, void, Executor> c, std::chrono::duration<Rep, Ratio> dur) -> coro<T, void, Executor>;

  template<typename Rep, typename Ratio>
  auto operator()(std::chrono::duration<Rep, Ratio> dur) -> op_bind<sample_t, std::chrono::duration<Rep, Ratio>>
  {
    return op_bind<sample_t, std::chrono::duration<Rep, Ratio>>(dur);
  }
};

constexpr sample_t sample;


struct skip_t
{
  template<typename T, typename Executor>
  auto operator()(coro<T, void, Executor> c, std::size_t skip) const -> coro<T, void, Executor>
  {
    while ((skip-- > 0) && co_await c);

    while (auto v = co_await c)
      co_yield std::move(*v);
  }

  auto operator()(std::size_t skip) const -> op_bind<skip_t, std::size_t>
  {
    return op_bind<skip_t, std::size_t>(skip);
  }
};

constexpr skip_t skip;


struct skip_last_t
{
  template<typename T, typename Executor>
  auto operator()(coro<T, void, Executor> c, std::size_t skip) const -> coro<T, void, Executor>
  {
    std::deque<T> buffer_; // could be circular_buffer

    while (auto v = co_await c)
    {
      buffer_.push_back(std::move(*v));
      if (buffer_.size() > skip)
      {
        auto res = std::move(buffer_.front());
        buffer_.pop_front();
        co_yield std::move(res);
      }
    }

  }

  auto operator()(std::size_t skip) const -> op_bind<skip_last_t, std::size_t>
  {
    return op_bind<skip_last_t, std::size_t>(skip);
  }
};

constexpr skip_last_t skip_last;

struct take_t
{
  template<typename T, typename Executor>
  auto operator()(coro<T, void, Executor> c, std::size_t take) const  -> coro<T, void, Executor>
  {
    while (take-- > 0)
    {
      auto v = co_await c;
      if (!v)
        break;
      co_yield std::move(*v);
    }
  }

  auto operator()(std::size_t cnt) const -> op_bind<take_t, std::size_t>
  {
    return op_bind<take_t, std::size_t>(cnt);
  }
};

constexpr take_t take;

struct take_last_t
{
  template<typename T, typename Executor>
  auto operator()(coro<T, void, Executor> c, std::size_t take) const -> coro<T, void, Executor>
  {
    std::deque<T> buffer_; // could be circular_buffer
    while (auto v = co_await c)
    {
      buffer_.push_back(std::move(*v));
      if (buffer_.size() > take)
        buffer_.pop_front();
    }
    for (auto & v : buffer_)
      co_yield std::move(v);
  }

  auto operator()(std::size_t cnt) const -> op_bind<take_last_t, std::size_t>
  {
    return op_bind<take_last_t, std::size_t>(cnt);
  }
};

constexpr take_last_t take_last;

struct combine_latest_t
{
  template<typename Executor, typename ... Coros, typename Combiner>
  auto impl(Executor executor,  Coros ...  c, Combiner combiner) const

    -> coro<decltype(combiner(std::declval<typename Coros::yield_type>()...)), void, Executor>
  {
    detail::combiner cb{executor,
            [c = std::move(c)]<typename Token>(Token && token) mutable
            {
              return c.async_resume(std::forward<Token>(token));
            }...};
    cb.restart_all();

    while ((co_await this_coro::cancellation_state).cancelled() == cancellation_type::none)
    {
      auto [ec, idx, res] = co_await cb.async_wait(use_coro);

      std::exception_ptr ex;
      auto done = false;

      if (!std::apply([](auto && ... args){return (args && ...);}, res))
      {
        cb.restart(idx);
        continue;
      }

      auto fetch_steps =
              [&, &res = res](auto & arg)
              {
                auto & [e, val] = arg;
                if (e)
                {
                  if (!ex)
                    ex = e;
                  return false;
                }

                if (!val)
                  done = true;
                return val.has_value();
              };

      if (std::apply([&](auto & ... args)
          {
          return (fetch_steps(*args) && ... );
          }, res))
      {
        co_yield std::apply(
                [&](auto &&  ... args)
                {
                  return combiner(*std::move(get<1>(*args))...);
                }, res);
      }

      if (ex)
        std::rethrow_exception(ex);
      if (done)
        co_return ;

      cb.restart(idx);

    }
  }

  template<typename Combiner, stream Coro1, stream ... Coros>
    requires requires (Combiner c,
                       typename Coro1::yield_type v1,
                       typename Coros::yield_type ... v) {c(v1, v...);}
  auto operator()(Combiner combiner, Coro1 c1, Coros ...  c) const
  {
    return impl<typename Coro1::executor_type, Coro1, Coros...>(c1.get_executor(), std::move(c1), std::move(c)..., std::move(combiner));
  }

  template<stream Coro1, stream ... Coros>
  auto operator()(Coro1 c1, Coros ...  c) const
  {
    return impl<typename Coro1::executor_type, Coro1, Coros...>(c1.get_executor(), std::move(c1), std::move(c)...,
                          []<typename ... Args>(Args && ...args){return std::make_tuple(std::forward<Args>(args)...);});
  }
};

constexpr combine_latest_t combine_latest;


struct merge_t
{
  template<typename Executor, typename Coro1, typename ... Coros, typename Combiner>
  auto impl(Executor executor, Coro1 c1, Coros ...  c, Combiner combiner) const
    -> coro<decltype(combiner(std::in_place_index<0u>, std::declval<typename Coro1::yield_type>())), void,
            Executor>
  {
    detail::merger mg{executor,
                      [c = std::move(c1)]<typename Token>(Token && token) mutable
                      {
                        return c.async_resume(std::forward<Token>(token));
                      },
                      [c = std::move(c)]<typename Token>(Token && token) mutable
                      {
                        return c.async_resume(std::forward<Token>(token));
                      }...};

    mg.restart_all();
    while ((co_await this_coro::cancellation_state).cancelled() == cancellation_type::none)
    {
      auto [err, res] = co_await mg.async_wait(use_coro);

      if (err)
        continue;

      using res_type = std::optional<decltype(combiner(std::in_place_index<0u>, std::declval<typename Coro1::yield_type>()))>;
      res_type result;
      std::exception_ptr ex;

      auto step =
          [&, &rr = res]<std::size_t I>(std::in_place_index_t<I> ip)
          {
            if (I != rr.index())
              return false;

            auto [e, op] = get<I>(rr);
            if (e)
              ex = e;
            else if (op)
              result = combiner(ip, std::move(*op));

            return true;
          };

      [&]<std::size_t ... Idx>(std::index_sequence<Idx...>)
      {
        (step(std::in_place_index<Idx>) || ...);
      }(std::make_index_sequence<sizeof...(Coros) + 1u>{});

      if (ex)
        std::rethrow_exception(ex);

      if (!result)
        co_return;

      co_yield std::move(*result);
      mg.restart(res.index());
    }
  }

  template<typename Combiner, stream Coro1, stream ... Coros>
    requires (!stream<Combiner>)
  auto operator()(Combiner combiner, Coro1 c1, Coros ...  c) const
  {
    return this->impl<typename Coro1::executor_type, Coro1, Coros...>(
            c1.get_executor(), std::move(c1), std::move(c)..., std::move(combiner));
  }

  template<stream Coro1, stream ... Coros>
  auto operator()(Coro1 c1, Coros ...  c) const
  {
    return impl<typename Coro1::executor_type, Coro1, Coros...>(
                c1.get_executor(), std::move(c1), std::move(c)..., [](auto in_place, auto res)
            {
              using res_t = std::variant<typename Coro1::yield_type, typename Coros::yield_type ...>;
              return res_t(in_place, std::move(res));
            });
  }
};

/// some Comment for merge
constexpr merge_t merge;

struct start_with_t
{
  template<typename T, typename Executor, typename ... Args>
  auto operator()(coro<T, void, Executor> c, Args ... args) -> coro<T, void, Executor>
  {
    (co_yield std::move(args), ...);
    while (auto v = co_await c)
      co_yield std::move(*v);

  }

  template<typename ... Args>
  auto operator()(Args && ... args) const -> op_bind<start_with_t, std::decay_t<Args> ... >
  {
    return op_bind<start_with_t, std::decay_t<Args> ... >(std::forward<Args>(args)...);
  }
};

constexpr start_with_t start_with;

struct zip_t
{
  template<typename Coro1, typename ... Coros, typename Combiner>
  auto impl(Coro1 c1, Coros ...  c, Combiner combiner) const
      -> coro<decltype(combiner(
            std::declval<typename Coro1::yield_type>(),
            std::declval<typename Coros::yield_type>()...)), void, typename Coro1::executor_type>
  {
    while ((co_await this_coro::cancellation_state).cancelled() == cancellation_type::none)
    {
      auto res = co_await make_parallel_group(c1.async_resume(deferred), c.async_resume(deferred)...).
                  async_wait(wait_for_one_error(), use_coro);

      auto && [vals, done]
            = [&]<std::size_t ... Is>(std::index_sequence<Is...>)
              {
                auto throw_if = [](std::exception_ptr ex) {if (ex) std::rethrow_exception(ex);};
                (throw_if(get<(Is * 2) + 1>(res)), ...);
                auto done = (get<(Is * 2) + 2>(res) && ...);
                return std::make_pair(std::forward_as_tuple(std::move(get<(Is * 2) + 2>(res))...), done);
              }(std::make_index_sequence<sizeof...(Coros) + 1>{});

      if (!done)
        break;

      co_yield std::apply(
              [&]<typename ... Args>(Args && ... args)
              {
                return combiner(std::move(*args)...);
              }, vals);
    }

  }

  template<typename Combiner, stream ... Coros>
  requires requires (Combiner c, typename Coros::value_type ... v) {c(v...);}
  auto operator()(Combiner combiner, Coros ...  c) const
  {
    return impl<Coros...>(std::move(c)..., std::move(combiner));
  }

  template<stream ... Coros>
  auto operator()(Coros ...  c) const
  {
    return impl<Coros...>(std::move(c)...,
                          []<typename ... Args>(Args && ...args){return std::make_tuple(std::forward<Args>(args)...);});
  }
};

constexpr zip_t zip;


struct catch_t
{
  template<typename T, typename Executor, typename Func>
    requires requires (Func f, Executor exec, std::exception_ptr ex)
    {
      f(exec, ex).async_resume(use_coro);
    }
  auto operator()(coro<T, void, Executor> c, Func func) const -> coro<T, void, Executor>
  {
    std::exception_ptr ep;
    try
    {
      while (auto v = co_await c)
        co_yield std::move(*v);
    }
    catch (...)
    {
      ep = std::current_exception();
    }

    if (ep)
    {
      auto cc = func(c.get_executor(), ep);
      while (auto v = co_await cc)
        co_yield std::move(*v);
    }
  }
  template<typename Func>
  auto operator()(Func && func) const -> op_bind<catch_t, std::decay_t<Func>>
  {
    return op_bind<catch_t, std::decay_t<Func>>(std::forward<Func>(func));
  }
};

constexpr catch_t catch_;

struct retry_t
{
  template<typename Context, typename Func>
  requires requires (Context ctx, Func f)
  {
    f(ctx).async_resume(use_coro);
  }
  auto operator()(Context ctx, Func func) const -> decltype(func(ctx))
  {
    retry:
    try
    {
      auto c = func(ctx);
      while (auto v = co_await c)
        co_yield std::move(*v);
    }
    catch (...)
    {
      goto retry;
    }
  }
};

constexpr retry_t retry;


struct tap_t
{
  template<typename T, typename Executor,
          typename OnNext  = detail::ignore_t,
          typename OnError = detail::ignore_t,
          typename OnDone  = detail::ignore_t>
  auto operator()(coro<T, void, Executor> c,
                  OnNext  on_next  = detail::ignore,
                  OnError on_error = detail::ignore,
                  OnDone  on_done  = detail::ignore) const -> coro<T, void, Executor>
  {
    try
    {
      while (auto v = co_await c)
      {
        try {on_next(v);} catch(...) {}
        co_yield std::move(*v);
      }
      try {on_done();}catch(...){}
    }
    catch (...)
    {
      try{on_error(std::current_exception());}catch(...){}
      throw;
    }
  }

  template<typename OnNext = detail::ignore_t,
           typename OnError  = detail::ignore_t,
           typename OnDone = detail::ignore_t>
  auto operator()(OnNext  && on_next  = detail::ignore,
                  OnError && on_error = detail::ignore,
                  OnDone  && on_done  = detail::ignore) const
        -> op_bind<tap_t, std::decay_t<OnNext>, std::decay_t<OnError>, std::decay_t<OnDone>>
  {
    return op_bind<tap_t, std::decay_t<OnNext>, std::decay_t<OnError>, std::decay_t<OnDone>>(
            std::forward<OnNext>(on_next), std::forward<OnError>(on_error), std::forward<OnDone>(on_done));
  }
};

constexpr tap_t tap;


struct delay_t
{
  template<typename T, typename Executor, typename Rep, typename Ratio>
  auto operator()(coro<T, void, Executor> c, chrono::duration<Rep, Ratio> dur) const -> coro<T, void, Executor>
  {
    asio::steady_timer st{c.get_executor(), dur};
    co_await st.async_wait(use_coro);

    while (auto v = co_await c)
      co_yield std::move(*v);
  }

  template<typename Rep, typename Ratio>
  auto operator()(chrono::duration<Rep, Ratio> dur) const -> op_bind<delay_t, chrono::duration<Rep, Ratio>>
  {
    return op_bind<delay_t, chrono::duration<Rep, Ratio>>(dur);
  }
};

constexpr delay_t delay;

struct observe_on_t
{
  template<typename Executor_, typename T, typename Executor>
  auto operator()(Executor_ obs_on, coro<T, void, Executor> c) const -> coro<T, void, Executor_>
  {
    while (auto v = co_await c)
      co_yield std::move(*v);
  }

  template<typename Executor>
  struct bind
  {
    using executor_type = Executor;
    Executor executor;
  };

  template<typename Executor>
  bind<Executor> operator()(Executor exec) const
  {
    return exec;
  }

};

constexpr observe_on_t observe_on;

template<typename T, typename U, typename Executor, typename ObsExecutor>
constexpr auto operator|(coro<T, void, Executor> c, observe_on_t::bind<ObsExecutor> b)
{
  return observe_on(std::move(c), std::move(b.executor));
}

struct subscribe_t
{

  template<typename T, typename Executor, typename OnNext = detail::ignore_t,
          typename OnError = detail::ignore_t, typename OnDone = detail::ignore_t>
  auto impl(coro<T, void, Executor> c, OnNext on_next, OnError on_error, OnDone on_done, std::shared_ptr<void>) const -> coro<void() noexcept, void, Executor>
  {
    try
    {
      while ((co_await this_coro::cancellation_state).cancelled() == cancellation_type::none)
      {
        auto v = co_await c;
        if (!v)
          break;
        on_next(std::move(*v));
      }

      on_done();
    }
    catch(...)
    {
      on_error(std::current_exception());
    }
  }

  template<typename T, typename Executor, typename OnNext = detail::ignore_t,
           typename OnError = detail::ignore_t, typename OnDone = detail::ignore_t>
  void operator()(coro<T, void, Executor> c, OnNext on_next = detail::ignore,
                  OnError on_error = detail::ignore, OnDone on_done = detail::ignore,
                  cancellation_slot cancel_slot = {}) const
  {
    co_spawn(impl(std::move(c), std::move(on_next), std::move(on_error), std::move(on_done)),
             bind_cancellation_slot(cancel_slot, detached));
  }

  template<typename OnNext = detail::ignore_t,
          typename OnError  = detail::ignore_t,
          typename OnDone = detail::ignore_t>
  auto operator()(OnNext  && on_next  = detail::ignore,
                  OnError && on_error = detail::ignore,
                  OnDone  && on_done  = detail::ignore) const
  -> op_bind<subscribe_t, std::decay_t<OnNext>, std::decay_t<OnError>, std::decay_t<OnDone>>
  {
    return op_bind<subscribe_t, std::decay_t<OnNext>, std::decay_t<OnError>, std::decay_t<OnDone>>(
            std::forward<OnNext>(on_next), std::forward<OnError>(on_error), std::forward<OnDone>(on_done));
  }
};

constexpr subscribe_t subscribe;

struct timeout_t
{
  template<typename T, typename Executor, typename Rep, typename Ratio>
  auto operator()(coro<T, void, Executor> c, chrono::duration<Rep, Ratio> dur) const -> coro<T, void, Executor>
  {
    asio::steady_timer st{c.get_executor()};
    st.expires_after(dur);

    while ((co_await this_coro::cancellation_state).cancelled() == cancellation_type::none)
    {
      auto [w, cr, r, e] = co_await make_parallel_group(c.async_resume(deferred),
                                                        st.async_wait(deferred)).async_wait(wait_for_one_success(), use_coro);
      if (w[0] == 1u)
        throw system_error(error::timed_out);

      co_yield std::move(*r);
      st.expires_after(dur);
    }
  }

  template<typename Rep, typename Ratio>
  auto operator()(chrono::duration<Rep, Ratio> dur) const -> op_bind<timeout_t, chrono::duration<Rep, Ratio>>
  {
    return op_bind<timeout_t, chrono::duration<Rep, Ratio>>(dur);
  }

};

constexpr timeout_t timeout;

struct timestamp_t
{
  template<typename T, typename Executor, typename Clock = chrono::steady_clock>
  auto operator()(coro<T, void, Executor> c, Clock clk = Clock{}) const -> coro<std::pair<typename Clock::time_point, T>, void, Executor>
  {
    while (auto v = co_await c)
      co_yield std::pair<typename Clock::time_point, T>(clk.now(), std::move(*v));
  }

  auto operator()() const -> op_bind<timestamp_t>
  {
    return op_bind<timestamp_t>();
  }

  template<typename Clock>
  auto operator()(Clock clock) const -> op_bind<timestamp_t, Clock>
  {
    return op_bind<timestamp_t, Clock>(std::move(clock));
  }
};

constexpr timestamp_t timestamp;

struct all_t
{
  template<typename T, typename Executor, typename Predicate>
  auto operator()(coro<T, void, Executor> c, Predicate predicate) const -> coro<void, bool, Executor>
  {
    while (auto v = co_await c)
    {
      if (!predicate(*v))
        co_return false;
    }
    co_return true;
  }


  template<typename Predicate>
  auto operator()(Predicate predicate) const -> op_bind<timestamp_t, std::decay_t<Predicate>>
  {
    return op_bind<all_t, std::decay_t<Predicate>>(std::move(predicate));
  }
};

constexpr all_t all;


}
}
}

#include "asio/detail/pop_options.hpp"

#endif //ASIO_EXPERIMENTAL_PX_HPP
