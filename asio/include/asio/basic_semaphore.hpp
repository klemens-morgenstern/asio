//
// basic_counting_semaphore.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_BASIC_SEMAPHORE_HPP
#define ASIO_BASIC_SEMAPHORE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include <chrono>
#include <cstddef>
#include "asio/any_io_executor.hpp"
#include "asio/basic_waitable_timer.hpp"

#if defined(ASIO_HAS_MOVE)
# include <utility>
#endif // defined(ASIO_HAS_MOVE)

#include "asio/detail/push_options.hpp"
#include "asio/compose.hpp"

namespace asio
{

template<std::ptrdiff_t Max = std::numeric_limits<std::ptrdiff_t>::max(),
         typename Executor = any_io_executor>
struct basic_counting_semaphore
{
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  /// Rebinds the semaphore type to another executor.
  template <typename Executor1>
  struct rebind_executor
  {
    /// The semaphore type when rebound to the specified executor.
    typedef basic_counting_semaphore<Max, Executor1> other;
  };

  /// Constructor.
  /**
   * This constructor creates a semaphore.
   *
   * @param ex The I/O executor that the semaphore will use, by default, to
   * dispatch handlers for any asynchronous operations performed on the semaphore.
   */
  explicit basic_counting_semaphore(const executor_type & ex)
    : timer_(ex, chrono::steady_clock::time_point::max())
  {
  }

  /// Constructor.
  /**
   * This constructor creates a semaphore without setting an expiry time. The
   * expires_at() or expires_after() functions must be called to set an expiry
   * time before the semaphore can be waited on.
   *
   * @param context An execution context which provides the I/O executor that
   * the semaphore will use, by default, to dispatch handlers for any asynchronous
   * operations performed on the semaphore.
   */
  template <typename ExecutionContext>
  explicit basic_counting_semaphore(ExecutionContext& context,
      typename constraint<
        is_convertible<ExecutionContext&, execution_context&>::value
      >::type = 0)
    : timer_(context, chrono::steady_clock::time_point::max())
  {
  }

#if defined(ASIO_HAS_MOVE) || defined(GENERATING_DOCUMENTATION)
  /// Move-construct a basic_counting_semaphore from another.
  /**
   * This constructor moves a semaphore from one object to another.
   *
   * @param other The other basic_counting_semaphore object from which the move will
   * occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_counting_semaphore(const executor_type&)
   * constructor.
   */
  basic_counting_semaphore(basic_counting_semaphore&& other)
    : timer_(std::move(other.timer_))
  {
  }

  /// Move-assign a basic_counting_semaphore from another.
  /**
   * This assignment operator moves a semaphore from one object to another. Cancels
   * any outstanding asynchronous operations associated with the target object.
   *
   * @param other The other basic_counting_semaphore object from which the move will
   * occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_counting_semaphore(const executor_type&)
   * constructor.
   */
  basic_counting_semaphore& operator=(basic_counting_semaphore&& other)
  {
    timer_ = std::move(other.semaphore);
    return *this;
  }

  // All semaphores have access to each other's implementations.
  template <std::ptrdiff_t max, typename Executor1>
  friend class basic_counting_semaphore;

  /// Move-construct a basic_counting_semaphore from another.
  /**
   * This constructor moves a semaphore from one object to another.
   *
   * @param other The other basic_counting_semaphore object from which the move will
   * occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_counting_semaphore(const executor_type&)
   * constructor.
   */
  template <typename Executor1>
  basic_counting_semaphore(
      basic_counting_semaphore<Max, Executor1>&& other,
      typename constraint<
          is_convertible<Executor1, Executor>::value
      >::type = 0)
    : timer_(std::move(other.timer_))
  {
  }

  /// Move-assign a basic_counting_semaphore from another.
  /**
   * This assignment operator moves a semaphore from one object to another. Cancels
   * any outstanding asynchronous operations associated with the target object.
   *
   * @param other The other basic_counting_semaphore object from which the move will
   * occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_counting_semaphore(const executor_type&)
   * constructor.
   */
  template <typename Executor1>
  typename constraint<
    is_convertible<Executor1, Executor>::value,
          basic_counting_semaphore&
  >::type operator=(basic_counting_semaphore<Max, Executor1>&& other)
  {
    basic_counting_semaphore tmp(std::move(other));
    timer_ = std::move(tmp.timer_);
    return *this;
  }
#endif
  /// Destroys the timer.
  /**
   * This function destroys the timer, cancelling any outstanding asynchronous
   * wait operations associated with the timer as if by calling @c cancel.
   */
  ~basic_counting_semaphore()
  {
  }

  /// Get the executor associated with the object.
  executor_type get_executor() ASIO_NOEXCEPT
  {
    return timer_.get_executor();
  }

  /// Returns the internal counter's maximum possible value, which is greater than or equal to LeastMaxValue.
  constexpr std::ptrdiff_t max() {return Max;}

  /// Atomically increments the internal counter by the value of update.
  /** Anyone waiting for the counter to be greater than 0, such as due to being blocked in acquire,
   * will subsequently be unblocked.
   */
  void release(std::ptrdiff_t update = 1)
  {
    counter_ += update;
    timer_.notify();
  }


  /// Perform a blocking acquire on the semaphore.
  /**
   * This function is used to acquire from the semaphore. This function
   * blocks and does not return until the semaphore has released.
   *
   * @throws asio::system_error Thrown on failure.
   */
  void aquire()
  {
    while (true)
    {
      auto res = --counter_;
      if (res >= 0u)
        return;

      counter_++;
      timer_.wait();
    }
  }

  /// Perform a blocking acquire on the semaphore.
  /**
   * This function is used to acquire from the semaphore. This function
   * blocks and does not return until the semaphore has released.
   *
   * @param ec Set to indicate what error occurred, if any.
   */
  void wait(asio::error_code& ec)
  {
    while (!ec)
    {
      auto res = --counter_;
      if (res >= 0u)
        return;

      counter_++;
      timer_.wait(ec);
    }
  }

  /// Start an asynchronous wait on the timer.
  /**
   * This function may be used to initiate an asynchronous wait against the
   * timer. It always returns immediately.
   *
   * For each call to async_wait(), the supplied handler will be called exactly
   * once. The handler will be called when:
   *
   * @li The timer has expired.
   *
   * @li The timer was cancelled, in which case the handler is passed the error
   * code asio::error::operation_aborted.
   *
   * @param handler The handler to be called when the timer expires. Copies
   * will be made of the handler as required. The function signature of the
   * handler must be:
   * @code void handler(
   *   const asio::error_code& error // Result of operation.
   * ); @endcode
   * Regardless of whether the asynchronous operation completes immediately or
   * not, the handler will not be invoked from within this function. On
   * immediate completion, invocation of the handler will be performed in a
   * manner equivalent to using asio::post().
   *
   * @par Per-Operation Cancellation
   * This asynchronous operation supports cancellation for the following
   * asio::cancellation_type values:
   *
   * @li @c cancellation_type::terminal
   *
   * @li @c cancellation_type::partial
   *
   * @li @c cancellation_type::total
   */
  template <
          ASIO_COMPLETION_TOKEN_FOR(void (asio::error_code))
          WaitHandler ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  ASIO_INITFN_AUTO_RESULT_TYPE(WaitHandler,
                               void (asio::error_code))
  async_aquire(
          ASIO_MOVE_ARG(WaitHandler) handler
          ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  {
    return async_compose<WaitHandler, void(asio::error_code)>(
              async_aquire_impl_{counter_, timer_}, handler, timer_);
  }

 private:
  struct async_aquire_impl_
  {
    std::atomic<std::ptrdiff_t> &counter;
    basic_waitable_timer<chrono::steady_clock, asio::wait_traits<chrono::steady_clock>, Executor > &timer;
    template <typename Self>
    void operator()(Self& self, asio::error_code ec = {})
    {
      if (ec)
      {
        self.complete(ec);
        return;
      }

      auto res = --counter;
      if (res >= 0u)
      {
        self.complete(error_code{});
        return;
      }

      counter++;
      timer.async_wait(std::move(self));

    }
  };

  std::atomic<std::ptrdiff_t> counter_{0u};
  static_assert(std::atomic<std::ptrdiff_t>::is_always_lock_free, "Needs to be lockfree");
  basic_waitable_timer<chrono::steady_clock, asio::wait_traits<chrono::steady_clock>, Executor > timer_;
};

template<std::ptrdiff_t Max = std::numeric_limits<std::ptrdiff_t>::max()>
using counting_semaphore = basic_counting_semaphore<Max, any_io_executor>;

template<typename Executor = any_io_executor>
using basic_binary_semaphore = basic_counting_semaphore<1, Executor>;

using binary_semaphore = basic_counting_semaphore<1>;

}

#include "asio/detail/pop_options.hpp"


#endif //ASIO_BASIC_SEMAPHORE_HPP
