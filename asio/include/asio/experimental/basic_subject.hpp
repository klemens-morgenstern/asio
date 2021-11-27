//
// impl/experimental/basic_subject.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_BASIC_SUBJECT_HPP
#define ASIO_BASIC_SUBJECT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/execution_context.hpp"
#include "asio/detail/config.hpp"
#include "asio/async_result.hpp"
#include "asio/bind_cancellation_slot.hpp"
#include "asio/detail/mutex.hpp"
#include "asio/detail/type_traits.hpp"
#include "asio/experimental/channel.hpp"
#include "asio/experimental/deferred.hpp"
#include "asio/experimental/detail/channel_message.hpp"
#include "asio/experimental/detail/channel_send_functions.hpp"
#include "asio/experimental/parallel_group.hpp"

#include "asio/detail/push_options.hpp"

namespace asio
{
namespace experimental
{

template<typename Channel>
struct basic_subscription
{
  basic_subscription() = default;
  basic_subscription(const basic_subscription &) = delete;
  basic_subscription(basic_subscription && ) = default;

  basic_subscription& operator=(const basic_subscription &) = delete;
  basic_subscription& operator=(basic_subscription && ) = default;

  typedef Channel channel_type;
  typedef typename channel_type::executor_type executor_type;

  /// Try to receive a message without blocking.
  /**
   * Fails if the buffer is full and there are no waiting receive operations.
   *
   * @returns @c true on success, @c false on failure.
   */
  template <typename Handler>
  bool try_receive(ASIO_MOVE_ARG(Handler) handler)
  {
    return impl_->try_receive(ASIO_MOVE_CAST(Handler)(handler));
  }

  /// Asynchronously receive a message.
  template <typename CompletionToken
  ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  auto async_receive(
          ASIO_MOVE_ARG(CompletionToken) token
          ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
#if !defined(GENERATING_DOCUMENTATION)
  -> decltype(std::declval<channel_type>().async_receive(ASIO_MOVE_CAST(CompletionToken)(token)))
#endif // !defined(GENERATING_DOCUMENTATION)
  {
    return impl_->async_receive(ASIO_MOVE_CAST(CompletionToken)(token));
  }

  explicit basic_subscription(std::shared_ptr<channel_type> impl) : impl_(std::move(impl)) {}

  bool is_open() const
  {
    return impl_ & impl_->is_open();
  }

 private:
  std::shared_ptr<channel_type> impl_;
};

namespace detail
{

template<typename Derived, typename Executor, typename Signature>
struct subject_send_function;

template<typename Derived, typename Executor, typename R, typename ... Args>
struct subject_send_function<Derived, Executor, R(Args...)>
{

  template <typename... Args2>
  typename enable_if<
          is_constructible<detail::channel_message<R(Args...)>, int, Args2...>::value,
          std::size_t
  >::type try_send(ASIO_MOVE_ARG(Args2)... args)
  {
    typedef typename Derived::channel_type channel_type;
    std::size_t res = 0u;
    static_cast<Derived &>(*this).for_each(
            [&](const channel_type & ct)
            {
              if (ct.try_send(args...))
                res++;
            });
  }

  template <typename... Args2>
  typename enable_if<
          is_constructible<detail::channel_message<R(Args...)>, int, Args2...>::value,
          std::size_t
  >::type try_send_n(std::size_t count, ASIO_MOVE_ARG(Args2)... args)
  {
    typedef typename Derived::channel_type channel_type;
    std::size_t res = count;
    static_cast<Derived &>(*this).for_each(
            [&](const channel_type & ct)
            {
              auto sz = ct.try_send(args...);
              res = std::min(sz, res);
            });
    return res;
  }

  template <
          ASIO_COMPLETION_TOKEN_FOR(void (asio::error_code))
          CompletionToken ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
  auto async_send(Args... args,
                  ASIO_MOVE_ARG(CompletionToken) token
                  ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
  {
    typedef typename Derived::channel_type channel_type;
    auto this_ = static_cast<Derived*>(this);
    std::vector<std::shared_ptr<channel_type>> locked = this_->lock_();
    auto cpl = std::make_shared<completion_handler>(this_->get_executor(), locked.size());

    this_->cache_(args...);
    auto itr = cpl->cancels.begin();
    for (auto & sub : locked)
      sub->async_send(args...,
                     bind_cancellation_slot((itr++)->slot(),
                      [cpl](error_code ec)
                      {
                        cpl->complete(ec);
                      }));

    return cpl->channel.async_receive(
            asio::experimental::deferred(
                    [cpl](error_code ec, int)
                    {
                      return asio::experimental::deferred.values(ec);
                    })
            )(std::forward<CompletionToken>(token));
  }
 private:
  struct completion_handler
  {
    channel<Executor, void(error_code, int)> channel;
    std::size_t count;
    std::vector<asio::cancellation_signal> cancels{count};

    completion_handler(Executor exec, std::size_t cnt) : channel(std::move(exec)), count(cnt)
    {
    }

    ~completion_handler()
    {
      channel.try_send(asio::error::broken_pipe, 0);
    }
    void complete(asio::error_code ec)
    {
      if (ec)
        channel.try_send(ec, 0);
      else if (--count == 0)
        channel.try_send(ec, 0);
    }
  };
};

template<typename Signature>
struct signature_message;

template<typename R>
struct signature_message<R()>
{
  signature_message() = default;
  signature_message(signature_message && ) noexcept = default;

  template<typename Executor, typename Traits, typename ... Signatures>
  void resend(basic_channel<Executor, Traits, Signatures... > & chan) const
  {
    chan.try_send();
  }
};

template<typename R, typename Arg>
struct signature_message<R(Arg)>
{
  signature_message(Arg arg) : arg(std::move(arg)) {}
  signature_message(signature_message &&) noexcept (std::is_nothrow_move_constructible_v<Arg>) = default;
  Arg arg;

  template<typename Executor, typename Traits, typename ... Signatures>
  void resend(basic_channel<Executor, Traits, Signatures... > & chan) const
  {
    chan.try_send(arg);
  }
};

template<typename R, typename Arg0, typename Arg1>
struct signature_message<R(Arg0, Arg1)>
{
  signature_message(Arg0 arg0, Arg1 arg1) : arg0(std::move(arg0)), arg1(std::move(arg1)) {}

  signature_message(signature_message && ) noexcept(std::is_nothrow_move_constructible_v<Arg0> &&
                                                    std::is_nothrow_move_constructible_v<Arg1>) = default;
  Arg0 arg0;
  Arg1 arg1;

  template<typename Executor, typename Traits, typename ... Signatures>
  void resend(basic_channel<Executor, Traits, Signatures... > & chan) const
  {
    chan.try_send(arg0, arg1);
  }
};


template<typename R, typename ... Args>
struct signature_message<R(Args...)>
{
  signature_message(Args ... args) : args(std::move(args)...) {}
  signature_message(signature_message && ) noexcept((std::is_nothrow_move_constructible_v<Args> && ...)) = default;
  std::tuple<Args...> args;

  template<typename Executor, typename Traits, typename ... Signatures>
  void resend(basic_channel<Executor, Traits, Signatures... > & chan) const
  {
    std::apply(
            [&](const auto & ... args)
            {
              chan.try_send(args...);
            });
  }
};

template<typename ... Signatures>
struct signature_payload
{
  typedef std::variant<signature_message<Signatures>...> type;
};

template<typename Signature>
struct signature_payload<Signature>
{
  typedef signature_message<Signature> type;
};
}


/// A channel for messages.
template <typename Executor, typename Traits, typename... Signatures>
class basic_subject
#if !defined(GENERATING_DOCUMENTATION)
        : public detail::subject_send_function<
                basic_subject<Executor, Traits, Signatures...>,
                Executor, Signatures> ...
#endif // !defined(GENERATING_DOCUMENTATION)
{
  template<typename D, typename E, typename S>
  friend struct detail::subject_send_function;

 public:


  using detail::subject_send_function<basic_subject<Executor, Traits, Signatures...>, Executor, Signatures>::async_send...;
  using detail::subject_send_function<basic_subject<Executor, Traits, Signatures...>, Executor, Signatures>::try_send...;
  using detail::subject_send_function<basic_subject<Executor, Traits, Signatures...>, Executor, Signatures>::try_send_n...;

  /// The underlying channel type.
  typedef basic_channel<Executor, Traits, Signatures... > channel_type;

  typedef basic_subscription<channel_type> subscription;

  /// The type of the executor associated with the channel.
  typedef Executor executor_type;

  /// Rebinds the channel type to another executor.
  template <typename Executor1>
  struct rebind_executor
  {
    /// The channel type when rebound to the specified executor.
    typedef basic_subject<Executor1, Traits, Signatures...> other;
  };

  /// The traits type associated with the channel.
  typedef typename Traits::template rebind<Signatures...>::other traits_type;

  /// Construct a basic_subject.
  /**
   * This constructor creates and channel.
   *
   * @param ex The I/O executor that the channel will use, by default, to
   * dispatch handlers for any asynchronous operations performed on the channel.
   *
   * @param max_buffer_size The maximum number of messages that may be buffered
   * in the channel.
   */
  basic_subject(const executor_type& ex, std::size_t max_buffer_size = 1)
          : executor_(ex), max_buffer_size_(max_buffer_size)
  {
  }

  /// Construct and open a basic_subject.
  /**
   * This constructor creates and opens a channel.
   *
   * @param context An execution context which provides the I/O executor that
   * the channel will use, by default, to dispatch handlers for any asynchronous
   * operations performed on the channel.
   *
   * @param max_buffer_size The maximum number of messages that may be buffered
   * in the channel.
   */
  template <typename ExecutionContext>
  basic_subject(ExecutionContext& context, std::size_t max_buffer_size = 1,
                typename constraint<
                        is_convertible<ExecutionContext&, execution_context&>::value,
                        defaulted_constraint
                >::type = defaulted_constraint())
          : executor_(context.get_executor()), max_buffer_size_(max_buffer_size)
  {
  }

#if defined(ASIO_HAS_MOVE) || defined(GENERATING_DOCUMENTATION)
  /// Move-construct a basic_subject from another.
  /**
   * This constructor moves a channel from one object to another.
   *
   * @param other The other basic_subject object from which the move will occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&) constructor.
   */
  basic_subject(basic_subject&& other)
    : subscribers_(other.subscribers_),
      executor_(other.executor_),
      max_buffer_size_(other.max_buffer_size_)
  {
  }

  /// Move-assign a basic_subject from another.
  /**
   * This assignment operator moves a channel from one object to another.
   * Cancels any outstanding asynchronous operations associated with the target
   * object.
   *
   * @param other The other basic_subject object from which the move will occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&)
   * constructor.
   */
  basic_subject& operator=(basic_subject&& other)
  {
    if (this != &other)
    {
      executor_.~executor_type();
      new (&executor_) executor_type(other.executor_);
      subscribers_ = std::move(other.service_);
      max_buffer_size_ = other.max_buffer_size_;
    }
    return *this;
  }

  // All channels have access to each other's implementations.
  template <typename, typename, typename...>
  friend class basic_subject;

  /// Move-construct a basic_subject from another.
  /**
   * This constructor moves a channel from one object to another.
   *
   * @param other The other basic_subject object from which the move will occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&)
   * constructor.
   */
  template <typename Executor1>
  basic_subject(
      basic_subject<Executor1, Traits, Signatures...>&& other,
      typename constraint<
          is_convertible<Executor1, Executor>::value
      >::type = 0)
    : subscribers_(std::move(other.subscribers_)),
      executor_(other.executor_),
      max_buffer_size_(other.max_buffer_size_)
  {
  }

  /// Move-assign a basic_subject from another.
  /**
   * This assignment operator moves a channel from one object to another.
   * Cancels any outstanding asynchronous operations associated with the target
   * object.
   *
   * @param other The other basic_subject object from which the move will
   * occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&)
   * constructor.
   */
  template <typename Executor1>
  typename constraint<
    is_convertible<Executor1, Executor>::value,
    basic_subject&
  >::type operator=(basic_subject<Executor1, Traits, Signatures...>&& other)
  {
    if (this != &other)
    {
      executor_.~executor_type();
      new (&executor_) executor_type(other.executor_);
      subscribers_ = std::move(other.subscribers_);
      max_buffer_size_ = other.max_buffer_size_;
    }
    return *this;
  }
#endif // defined(ASIO_HAS_MOVE) || defined(GENERATING_DOCUMENTATION)

  /// Destructor.
  ~basic_subject()
  {
    close();
  }

  /// Get the executor associated with the object.
  executor_type get_executor() ASIO_NOEXCEPT
  {
    return executor_;
  }

  /// Reset the channel to its initial state.
  void reset()
  {
    close();
    subscribers_->clear();
  }

  /// Close the channel.
  void close()
  {
    for_each_([](channel_type & c) {c.close(); });
  }

  /// Cancel all asynchronous operations waiting on the channel.
  /**
   * All outstanding send operations will complete with the error
   * @c asio::experimental::error::channel_canceld. Outstanding receive
   * operations complete with the result as determined by the channel traits.
   */
  void cancel()
  {
    for_each_([](channel_type & c) {c.cancel(); });
  }

  /// Determine whether a message can be received without blocking.
  bool ready() const ASIO_NOEXCEPT
  {
    return std::all_of(subscribers_.begin(), subscribers_.end(),
                       [](const std::weak_ptr<channel_type> & wp)
                       {
                          auto p = wp.lock();
                          return !p || p->ready();
                       });
  }
#if defined(GENERATING_DOCUMENTATION)

  /// Try to send a message without blocking.
  /**
   * Fails if the buffer is full and there are no waiting receive operations.
   *
   * @returns @c true on success, @c false on failure.
   */
  template <typename... Args>
  bool try_send(ASIO_MOVE_ARG(Args)... args);

  /// Try to send a number of messages without blocking.
  /**
   * @returns The number of messages that were sent.
   */
  template <typename... Args>
  std::size_t try_send_n(std::size_t count, ASIO_MOVE_ARG(Args)... args);

  /// Asynchronously send a message.
  template <typename... Args,
      ASIO_COMPLETION_TOKEN_FOR(void (asio::error_code))
        CompletionToken ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  auto async_send(ASIO_MOVE_ARG(Args)... args,
      ASIO_MOVE_ARG(CompletionToken) token);

#endif // defined(GENERATING_DOCUMENTATION)
  std::size_t subscribers() const
  {
    std::size_t sz{0u};
    for_each_([&](const channel_type &) {sz++;});
    return sz;
  }

  subscription subscribe()
  {
    auto ptr = std::make_shared<channel_type>(get_executor(), std::max(max_buffer_size_, static_cast<std::size_t>(1u)));
    subscribers_.emplace_back(ptr);
    return subscription{ptr};
  }

 private:

  template<typename Func>
  void for_each_(Func func)
  {
    for (auto itr = subscribers_.begin(); itr != subscribers_.end();)
    {
      if (auto p = itr->lock())
      {
        func(*p);
        itr++;
      }
      else
        itr = subscribers_.erase(itr);
    }
  }
  //template <typename T>
  //inline typename associated_allocator<T>::type
  //get_associated_allocator(const T& t) ASIO_NOEXCEPT
  //{
  //  return associated_allocator<T>::get(t);
  //}

  auto lock_() -> std::vector<std::shared_ptr<channel_type>>
  {
    std::vector<std::shared_ptr<channel_type>> res{};
    res.reserve(subscribers_.size());
    for (auto itr = subscribers_.begin(); itr != subscribers_.end();)
    {
      if (auto p = itr->lock())
      {
        res.push_back(std::move(p));
        itr++;
      }
      else
        itr = subscribers_.erase(itr);
    }
    return res;
  }
  //lock_

  // Disallow copying and assignment.
  basic_subject(const basic_subject&) ASIO_DELETED;
  basic_subject& operator=(const basic_subject&) ASIO_DELETED;

  template <typename, typename, typename...>
  friend class detail::channel_send_functions;

  // Helper function to get an executor's context.
  template <typename T>
  static execution_context& get_context(const T& t,
                                        typename enable_if<execution::is_executor<T>::value>::type* = 0)
  {
    return asio::query(t, execution::context);
  }

  // Helper function to get an executor's context.
  template <typename T>
  static execution_context& get_context(const T& t,
                                        typename enable_if<!execution::is_executor<T>::value>::type* = 0)
  {
    return t.context();
  }

  // The associated executor.
  Executor executor_;

  std::vector<std::weak_ptr<channel_type>> subscribers_;
  std::size_t max_buffer_size_{0u};

  template<typename ... Args>
  void cache_(Args && ... ) {}
};


/// A channel for messages.
template <typename Executor, typename Traits, typename... Signatures>
class basic_behaviour_subject
#if !defined(GENERATING_DOCUMENTATION)
        : public detail::subject_send_function<
                basic_behaviour_subject<Executor, Traits, Signatures...>,
                Executor, Signatures> ...
#endif // !defined(GENERATING_DOCUMENTATION)
{
  template<typename D, typename E, typename S>
  friend struct detail::subject_send_function;

 public:


  using detail::subject_send_function<basic_behaviour_subject<Executor, Traits, Signatures...>, Executor, Signatures>::async_send...;
  using detail::subject_send_function<basic_behaviour_subject<Executor, Traits, Signatures...>, Executor, Signatures>::try_send...;
  using detail::subject_send_function<basic_behaviour_subject<Executor, Traits, Signatures...>, Executor, Signatures>::try_send_n...;

  /// The underlying channel type.
  typedef basic_channel<Executor, Traits, Signatures... > channel_type;

  typedef basic_subscription<channel_type> subscription;

  /// The payload type.
  typedef typename detail::signature_payload<Signatures...>::type payload_type;
  ;

  /// The type of the executor associated with the channel.
  typedef Executor executor_type;

  /// Rebinds the channel type to another executor.
  template <typename Executor1>
  struct rebind_executor
  {
    /// The channel type when rebound to the specified executor.
    typedef basic_subject<Executor1, Traits, Signatures...> other;
  };

  /// The traits type associated with the channel.
  typedef typename Traits::template rebind<Signatures...>::other traits_type;

  /// Construct a basic_subject.
  /**
   * This constructor creates and channel.
   *
   * @param ex The I/O executor that the channel will use, by default, to
   * dispatch handlers for any asynchronous operations performed on the channel.
   *
   * @param max_buffer_size The maximum number of messages that may be buffered
   * in the channel.
   */
  basic_behaviour_subject(const executor_type& ex,
                          payload_type init = payload_type(),
                          std::size_t max_buffer_size = 1)
          : executor_(ex), payload_(init), max_buffer_size_(max_buffer_size)
  {
  }

  /// Construct and open a basic_subject.
  /**
   * This constructor creates and opens a channel.
   *
   * @param context An execution context which provides the I/O executor that
   * the channel will use, by default, to dispatch handlers for any asynchronous
   * operations performed on the channel.
   *
   * @param max_buffer_size The maximum number of messages that may be buffered
   * in the channel.
   */
  template <typename ExecutionContext>
  basic_behaviour_subject(ExecutionContext& context,
                          payload_type init = payload_type(),
                          std::size_t max_buffer_size = 1,
                typename constraint<
                        is_convertible<ExecutionContext&, execution_context&>::value,
                        defaulted_constraint
                >::type = defaulted_constraint())
          : executor_(context.get_executor()), payload_(std::move(init)), max_buffer_size_(max_buffer_size)
  {
  }

#if defined(ASIO_HAS_MOVE) || defined(GENERATING_DOCUMENTATION)
  /// Move-construct a basic_subject from another.
  /**
   * This constructor moves a channel from one object to another.
   *
   * @param other The other basic_subject object from which the move will occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&) constructor.
   */
  basic_behaviour_subject(basic_behaviour_subject&& other)
    : subscribers_(other.subscribers_),
      executor_(other.executor_),
      payload_(std::move(other.payload_)),
      max_buffer_size_(other.max_buffer_size_)
  {
  }

  /// Move-assign a basic_subject from another.
  /**
   * This assignment operator moves a channel from one object to another.
   * Cancels any outstanding asynchronous operations associated with the target
   * object.
   *
   * @param other The other basic_subject object from which the move will occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&)
   * constructor.
   */
  basic_behaviour_subject& operator=(basic_behaviour_subject&& other)
  {
    if (this != &other)
    {
      executor_.~executor_type();
      new (&executor_) executor_type(other.executor_);
      subscribers_ = std::move(other.service_);
      payload_ = std::move(other.payload_);
      max_buffer_size_ = other.max_buffer_size_;
    }
    return *this;
  }

  // All channels have access to each other's implementations.
  template <typename, typename, typename...>
  friend class basic_subject;

  /// Move-construct a basic_subject from another.
  /**
   * This constructor moves a channel from one object to another.
   *
   * @param other The other basic_subject object from which the move will occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&)
   * constructor.
   */
  template <typename Executor1>
  basic_behaviour_subject(
      basic_subject<Executor1, Traits, Signatures...>&& other,
      typename constraint<
          is_convertible<Executor1, Executor>::value
      >::type = 0)
    : subscribers_(std::move(other.subscribers_)),
      executor_(other.executor_),
      payload_(std::move(other.payload_)),
      max_buffer_size_(other.max_buffer_size_)
  {
  }

  /// Move-assign a basic_subject from another.
  /**
   * This assignment operator moves a channel from one object to another.
   * Cancels any outstanding asynchronous operations associated with the target
   * object.
   *
   * @param other The other basic_subject object from which the move will
   * occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&)
   * constructor.
   */
  template <typename Executor1>
  typename constraint<
    is_convertible<Executor1, Executor>::value,
    basic_behaviour_subject&
  >::type operator=(basic_subject<Executor1, Traits, Signatures...>&& other)
  {
    if (this != &other)
    {
      executor_.~executor_type();
      new (&executor_) executor_type(other.executor_);
      subscribers_ = std::move(other.subscribers_);
      payload_ = std::move(other.payload_);
    }
    return *this;
  }
#endif // defined(ASIO_HAS_MOVE) || defined(GENERATING_DOCUMENTATION)

  /// Destructor.
  ~basic_behaviour_subject()
  {
    close();
  }

  /// Get the executor associated with the object.
  executor_type get_executor() ASIO_NOEXCEPT
  {
    return executor_;
  }

  /// Reset the channel to its initial state.
  void reset()
  {
    close();
    subscribers_->clear();
  }

  /// Close the channel.
  void close()
  {
    for_each_([](channel_type & c) {c.close(); });
  }

  /// Cancel all asynchronous operations waiting on the channel.
  /**
   * All outstanding send operations will complete with the error
   * @c asio::experimental::error::channel_canceld. Outstanding receive
   * operations complete with the result as determined by the channel traits.
   */
  void cancel()
  {
    for_each_([](channel_type & c) {c.cancel(); });
  }

  /// Determine whether a message can be received without blocking.
  bool ready() const ASIO_NOEXCEPT
  {
    return std::all_of(subscribers_.begin(), subscribers_.end(),
                       [](const std::weak_ptr<channel_type> & wp)
                       {
                         auto p = wp.lock();
                         return !p || p->ready();
                       });
  }
#if defined(GENERATING_DOCUMENTATION)

  /// Try to send a message without blocking.
  /**
   * Fails if the buffer is full and there are no waiting receive operations.
   *
   * @returns @c true on success, @c false on failure.
   */
  template <typename... Args>
  bool try_send(ASIO_MOVE_ARG(Args)... args);

  /// Try to send a number of messages without blocking.
  /**
   * @returns The number of messages that were sent.
   */
  template <typename... Args>
  std::size_t try_send_n(std::size_t count, ASIO_MOVE_ARG(Args)... args);

  /// Asynchronously send a message.
  template <typename... Args,
      ASIO_COMPLETION_TOKEN_FOR(void (asio::error_code))
        CompletionToken ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  auto async_send(ASIO_MOVE_ARG(Args)... args,
      ASIO_MOVE_ARG(CompletionToken) token);

#endif // defined(GENERATING_DOCUMENTATION)
  std::size_t subscribers() const
  {
    std::size_t sz{0u};
    for_each_([&](const channel_type &) {sz++;});
    return sz;
  }

  subscription subscribe()
  {
    auto ptr = std::make_shared<channel_type>(get_executor(), max_buffer_size_);
    resend_(payload_, *ptr);

    subscribers_.emplace_back(ptr);
    return subscription{ptr};
  }

 private:


  template<typename Signature>
  static void resend_(const detail::signature_message<Signature> & msg, channel_type & chan)
  {
    msg.resend(chan);
  }


  template<typename ... Sigs>
  static void resend_(const std::variant<Sigs...> & msg, channel_type & chan)
  {
    visit([&](auto & pl)
              {
                pl.resend(chan);
              }, msg);
  }

  template<typename Func>
  void for_each_(Func func)
  {
    for (auto itr = subscribers_.begin(); itr != subscribers_.end();)
    {
      if (auto p = itr->lock())
      {
        func(*p);
        itr++;
      }
      else
        itr = subscribers_.erase(itr);
    }
  }
  //template <typename T>
  //inline typename associated_allocator<T>::type
  //get_associated_allocator(const T& t) ASIO_NOEXCEPT
  //{
  //  return associated_allocator<T>::get(t);
  //}

  auto lock_() -> std::vector<std::shared_ptr<channel_type>>
  {
    std::vector<std::shared_ptr<channel_type>> res{};
    res.reserve(subscribers_.size());
    for (auto itr = subscribers_.begin(); itr != subscribers_.end();)
    {
      if (auto p = itr->lock())
      {
        res.push_back(std::move(p));
        itr++;
      }
      else
        itr = subscribers_.erase(itr);
    }
    return res;
  }
  //lock_

  // Disallow copying and assignment.
  basic_behaviour_subject(const basic_behaviour_subject&) ASIO_DELETED;
  basic_behaviour_subject& operator=(const basic_behaviour_subject&) ASIO_DELETED;

  template <typename, typename, typename...>
  friend class detail::channel_send_functions;

  // Helper function to get an executor's context.
  template <typename T>
  static execution_context& get_context(const T& t,
                                        typename enable_if<execution::is_executor<T>::value>::type* = 0)
  {
    return asio::query(t, execution::context);
  }

  // Helper function to get an executor's context.
  template <typename T>
  static execution_context& get_context(const T& t,
                                        typename enable_if<!execution::is_executor<T>::value>::type* = 0)
  {
    return t.context();
  }

  // The associated executor.
  Executor executor_;

  std::vector<std::weak_ptr<channel_type>> subscribers_;
  std::size_t max_buffer_size_{0u};

  payload_type payload_;

  template<typename ... Args>
  void cache_(Args && ... args)
  {
    payload_ = payload_type{ args...};
  }
};


/// A channel for messages.
template <typename Executor, typename Traits, typename... Signatures>
class basic_replay_subject
#if !defined(GENERATING_DOCUMENTATION)
        : public detail::subject_send_function<
                basic_replay_subject<Executor, Traits, Signatures...>,
                Executor, Signatures> ...
#endif // !defined(GENERATING_DOCUMENTATION)
{
  template<typename D, typename E, typename S>
  friend struct detail::subject_send_function;

 public:

  using detail::subject_send_function<basic_replay_subject<Executor, Traits, Signatures...>, Executor, Signatures>::async_send...;
  using detail::subject_send_function<basic_replay_subject<Executor, Traits, Signatures...>, Executor, Signatures>::try_send...;
  using detail::subject_send_function<basic_replay_subject<Executor, Traits, Signatures...>, Executor, Signatures>::try_send_n...;

  /// The underlying channel type.
  typedef basic_channel<Executor, Traits, Signatures... > channel_type;

  typedef basic_subscription<channel_type> subscription;

  /// The payload type.
  typedef typename detail::signature_payload<Signatures...>::type payload_type;
  ;

  /// The type of the executor associated with the channel.
  typedef Executor executor_type;

  /// Rebinds the channel type to another executor.
  template <typename Executor1>
  struct rebind_executor
  {
    /// The channel type when rebound to the specified executor.
    typedef basic_subject<Executor1, Traits, Signatures...> other;
  };

  /// The traits type associated with the channel.
  typedef typename Traits::template rebind<Signatures...>::other traits_type;

  /// Construct a basic_subject.
  /**
   * This constructor creates and channel.
   *
   * @param ex The I/O executor that the channel will use, by default, to
   * dispatch handlers for any asynchronous operations performed on the channel.
   *
   * @param max_buffer_size The maximum number of messages that may be buffered
   * in the channel.
   */
  basic_replay_subject(const executor_type& ex,
                          std::size_t replay_size,
                          std::size_t max_buffer_size = 1)
          : executor_(ex), replay_size_(replay_size_), max_buffer_size_(max_buffer_size)
  {
  }

  /// Construct and open a basic_subject.
  /**
   * This constructor creates and opens a channel.
   *
   * @param context An execution context which provides the I/O executor that
   * the channel will use, by default, to dispatch handlers for any asynchronous
   * operations performed on the channel.
   *
   * @param max_buffer_size The maximum number of messages that may be buffered
   * in the channel.
   */
  template <typename ExecutionContext>
  basic_replay_subject(ExecutionContext& context,
                          std::size_t replay_size,
                          std::size_t max_buffer_size = 1,
                          typename constraint<
                                  is_convertible<ExecutionContext&, execution_context&>::value,
                                  defaulted_constraint
                          >::type = defaulted_constraint())
          : executor_(context.get_executor()), replay_size_(replay_size), max_buffer_size_(max_buffer_size)
  {
  }

#if defined(ASIO_HAS_MOVE) || defined(GENERATING_DOCUMENTATION)
  /// Move-construct a basic_subject from another.
  /**
   * This constructor moves a channel from one object to another.
   *
   * @param other The other basic_subject object from which the move will occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&) constructor.
   */
  basic_replay_subject(basic_replay_subject&& other)
    : subscribers_(other.subscribers_),
      executor_(other.executor_),
      replay_size_(other.replay_size_),
      max_buffer_size_(other.max_buffer_size_)
  {
  }

  /// Move-assign a basic_subject from another.
  /**
   * This assignment operator moves a channel from one object to another.
   * Cancels any outstanding asynchronous operations associated with the target
   * object.
   *
   * @param other The other basic_subject object from which the move will occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&)
   * constructor.
   */
  basic_replay_subject& operator=(basic_replay_subject&& other)
  {
    if (this != &other)
    {
      executor_.~executor_type();
      new (&executor_) executor_type(other.executor_);
      subscribers_ = std::move(other.service_);
      replay_size_ = std::move(other.replay_size_);
      max_buffer_size_ = other.max_buffer_size_;
    }
    return *this;
  }

  // All channels have access to each other's implementations.
  template <typename, typename, typename...>
  friend class basic_subject;

  /// Move-construct a basic_subject from another.
  /**
   * This constructor moves a channel from one object to another.
   *
   * @param other The other basic_subject object from which the move will occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&)
   * constructor.
   */
  template <typename Executor1>
  basic_replay_subject(
      basic_subject<Executor1, Traits, Signatures...>&& other,
      typename constraint<
          is_convertible<Executor1, Executor>::value
      >::type = 0)
    : subscribers_(std::move(other.subscribers_)),
      executor_(other.executor_),
      replay_size_(std::move(other.replay_size_)),
      max_buffer_size_(other.max_buffer_size_)
  {
  }

  /// Move-assign a basic_subject from another.
  /**
   * This assignment operator moves a channel from one object to another.
   * Cancels any outstanding asynchronous operations associated with the target
   * object.
   *
   * @param other The other basic_subject object from which the move will
   * occur.
   *
   * @note Following the move, the moved-from object is in the same state as if
   * constructed using the @c basic_subject(const executor_type&)
   * constructor.
   */
  template <typename Executor1>
  typename constraint<
    is_convertible<Executor1, Executor>::value,
    basic_replay_subject&
  >::type operator=(basic_subject<Executor1, Traits, Signatures...>&& other)
  {
    if (this != &other)
    {
      executor_.~executor_type();
      new (&executor_) executor_type(other.executor_);
      subscribers_ = std::move(other.subscribers_);
      replay_size_ = std::move(other.replay_size_);
    }
    return *this;
  }
#endif // defined(ASIO_HAS_MOVE) || defined(GENERATING_DOCUMENTATION)

  /// Destructor.
  ~basic_replay_subject()
  {
    close();
  }

  /// Get the executor associated with the object.
  executor_type get_executor() ASIO_NOEXCEPT
  {
    return executor_;
  }

  /// Reset the channel to its initial state.
  void reset()
  {
    close();
    subscribers_->clear();
  }

  /// Close the channel.
  void close()
  {
    for_each_([](channel_type & c) {c.close(); });
  }

  /// Cancel all asynchronous operations waiting on the channel.
  /**
   * All outstanding send operations will complete with the error
   * @c asio::experimental::error::channel_canceld. Outstanding receive
   * operations complete with the result as determined by the channel traits.
   */
  void cancel()
  {
    for_each_([](channel_type & c) {c.cancel(); });
  }

  /// Determine whether a message can be received without blocking.
  bool ready() const ASIO_NOEXCEPT
  {
    return std::all_of(subscribers_.begin(), subscribers_.end(),
                       [](const std::weak_ptr<channel_type> & wp)
                       {
                         auto p = wp.lock();
                         return !p || p->ready();
                       });
  }
#if defined(GENERATING_DOCUMENTATION)

  /// Try to send a message without blocking.
  /**
   * Fails if the buffer is full and there are no waiting receive operations.
   *
   * @returns @c true on success, @c false on failure.
   */
  template <typename... Args>
  bool try_send(ASIO_MOVE_ARG(Args)... args);

  /// Try to send a number of messages without blocking.
  /**
   * @returns The number of messages that were sent.
   */
  template <typename... Args>
  std::size_t try_send_n(std::size_t count, ASIO_MOVE_ARG(Args)... args);

  /// Asynchronously send a message.
  template <typename... Args,
      ASIO_COMPLETION_TOKEN_FOR(void (asio::error_code))
        CompletionToken ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  auto async_send(ASIO_MOVE_ARG(Args)... args,
      ASIO_MOVE_ARG(CompletionToken) token);

#endif // defined(GENERATING_DOCUMENTATION)
  std::size_t subscribers() const
  {
    std::size_t sz{0u};
    for_each_([&](const channel_type &) {sz++;});
    return sz;
  }

  subscription subscribe()
  {
    auto ptr = std::make_shared<channel_type>(get_executor(), std::max(max_buffer_size_, replay_size_));
    for (auto & pl : buffer_)
      resend_(pl, *ptr);

    subscribers_.emplace_back(ptr);
    return subscription{ptr};
  }

 private:


  template<typename Signature>
  static void resend_(const detail::signature_message<Signature> & msg, channel_type & chan)
  {
    msg.resend(chan);
  }


  template<typename ... Sigs>
  static void resend_(const std::variant<Sigs...> & msg, channel_type & chan)
  {
    visit([&](auto & pl)
          {
            pl.resend(chan);
          }, msg);
  }

  template<typename Func>
  void for_each_(Func func)
  {
    for (auto itr = subscribers_.begin(); itr != subscribers_.end();)
    {
      if (auto p = itr->lock())
      {
        func(*p);
        itr++;
      }
      else
        itr = subscribers_.erase(itr);
    }
  }
  //template <typename T>
  //inline typename associated_allocator<T>::type
  //get_associated_allocator(const T& t) ASIO_NOEXCEPT
  //{
  //  return associated_allocator<T>::get(t);
  //}

  auto lock_() -> std::vector<std::shared_ptr<channel_type>>
  {
    std::vector<std::shared_ptr<channel_type>> res{};
    res.reserve(subscribers_.size());
    for (auto itr = subscribers_.begin(); itr != subscribers_.end();)
    {
      if (auto p = itr->lock())
      {
        res.push_back(std::move(p));
        itr++;
      }
      else
        itr = subscribers_.erase(itr);
    }
    return res;
  }
  //lock_

  // Disallow copying and assignment.
  basic_replay_subject(const basic_replay_subject&) ASIO_DELETED;
  basic_replay_subject& operator=(const basic_replay_subject&) ASIO_DELETED;

  template <typename, typename, typename...>
  friend class detail::channel_send_functions;

  // Helper function to get an executor's context.
  template <typename T>
  static execution_context& get_context(const T& t,
                                        typename enable_if<execution::is_executor<T>::value>::type* = 0)
  {
    return asio::query(t, execution::context);
  }

  // Helper function to get an executor's context.
  template <typename T>
  static execution_context& get_context(const T& t,
                                        typename enable_if<!execution::is_executor<T>::value>::type* = 0)
  {
    return t.context();
  }

  // The associated executor.
  Executor executor_;

  std::vector<std::weak_ptr<channel_type>> subscribers_;
  std::size_t replay_size_{0u};
  std::size_t max_buffer_size_{0u};

  std::deque<payload_type> buffer_; // can be circular_buffer

  template<typename ... Args>
  void cache_(Args && ... args)
  {
    buffer_.emplace_back(args...);
    if (buffer_.size() > replay_size_)
      buffer_.pop_front();
  }
};


}
}

#include "asio/detail/pop_options.hpp"

#endif //ASIO_BASIC_SUBJECT_HPP
