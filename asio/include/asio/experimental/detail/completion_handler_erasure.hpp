//
// experimental/detail/completion_handler_erasure.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_EXPERIMENTAL_DETAIL_COMPLETION_HANDLER_ERASURE_HPP
#define ASIO_EXPERIMENTAL_DETAIL_COMPLETION_HANDLER_ERASURE_HPP

#include "asio/cancellation_signal.hpp"
#include "asio/associated_allocator.hpp"
#include "asio/associated_cancellation_slot.hpp"
#include "asio/dispatch.hpp"

namespace asio {

class any_io_executor;

namespace experimental {
namespace detail {

template<typename ... Signatures>
struct completion_handler_erasure_base;

template<typename Func, typename Executor, typename ...Signatures>
struct completion_handler_erasure_impl;

template<typename ... Signatures>
struct completion_handler_erasure_deleter_t
{
    template<typename Executor, typename Func>
    static std::unique_ptr<completion_handler_erasure_base<Signatures...>, completion_handler_erasure_deleter_t>
    make(Executor exec, Func&& func)
    {
        using type = completion_handler_erasure_impl<std::remove_reference_t<Func>, Executor , Signatures...>;
        using allocator_base = typename associated_allocator<Executor>::type;

        using alloc_type =  typename std::allocator_traits<allocator_base>::template rebind_alloc<type>;
        auto alloc = alloc_type(get_associated_allocator(exec));
        auto size = sizeof(type);
        auto p = std::allocator_traits<alloc_type>::allocate(alloc, size);
        auto res = std::unique_ptr<type, completion_handler_erasure_deleter_t>(p, completion_handler_erasure_deleter_t{});
        std::allocator_traits<alloc_type>::construct(alloc, p, std::move(exec),
                                                     std::move(alloc), std::forward<Func>(func));
        return res;
    }

    void operator()(
            completion_handler_erasure_base<Signatures...> * p)
    {
        p->destroy_and_deallocate_this();
    }
};

template<typename Signature>
struct completion_handler_erasure_base_call;

template<typename Return,  typename ... Args>
struct completion_handler_erasure_base_call<Return(Args...)>
{
    virtual Return call(Args ...args) = 0;
};

template<typename Return,  typename ... Args>
struct completion_handler_erasure_base<Return(Args...)> //optimized for simple signature
{
  virtual Return call(Args ...args) = 0;
  virtual void destroy_and_deallocate_this() = 0;
  virtual cancellation_slot get_cancellation_slot() = 0;
  virtual ~completion_handler_erasure_base() = default;
};

template<typename ... Signatures>
struct completion_handler_erasure_base : completion_handler_erasure_base_call<Signatures>...
{
    using completion_handler_erasure_base_call<Signatures>::call...;
    virtual cancellation_slot get_cancellation_slot() = 0;
    virtual void destroy_and_deallocate_this() = 0;
    virtual ~completion_handler_erasure_base() = default;
};


template<typename Base, typename Impl, typename Signature>
struct completion_handler_erasure_impl_call;

template<typename Base, typename Impl, typename Return,  typename ... Args>
struct completion_handler_erasure_impl_call<Base, Impl, Return(Args...)> : virtual Base
{
    Return call(Args ...args) override final
    {
        std::move(static_cast<Impl*>(this)->func)(static_cast<Args>(args)...);
    }
};

template<typename Impl, typename Return,  typename ... Args>
struct completion_handler_erasure_impl_call<completion_handler_erasure_base<Return(Args...)>, Impl, Return(Args...)> : completion_handler_erasure_base<Return(Args...)>
{
    Return call(Args ...args) override final
    {
        std::move(static_cast<Impl*>(this)->func)(static_cast<Args>(args)...);
    }
};

template<typename Func, typename Executor, typename ... Signatures>
struct completion_handler_erasure_impl final
     : completion_handler_erasure_impl_call<completion_handler_erasure_base<Signatures...>,
             completion_handler_erasure_impl<Func, Executor, Signatures...>,  Signatures...>
{
  using allocator_base = typename associated_allocator<Executor>::type;
  using allocator_type = typename std::allocator_traits<allocator_base>::template rebind_alloc<completion_handler_erasure_impl>;

  cancellation_slot get_cancellation_slot() override {return cancellation; }


  completion_handler_erasure_impl(Executor&& exec, allocator_type && allocator, Func&& func)
    : executor(std::move(exec)), allocator(std::move(allocator)), cancellation(get_associated_cancellation_slot(func)), func(std::move(func))
  {
  }

  void destroy_and_deallocate_this() override
  {
      struct destroyer
      {
          completion_handler_erasure_impl * target;
          allocator_type allocator;

          destroyer(completion_handler_erasure_impl * target, allocator_type allocator) : target(target), allocator(std::move(allocator)) {}
          destroyer(destroyer && lhs) : target(std::exchange(lhs.target, nullptr)), allocator(std::move(lhs.allocator)) {}
          ~destroyer()
          {
              if (target != nullptr)
                  (*this)();
          }
          void operator() ()
          {
              std::allocator_traits<allocator_type>::destroy(allocator, target);
              std::allocator_traits<allocator_type>::deallocate(allocator, target, sizeof(completion_handler_erasure_impl));
              target = nullptr;
          }
      };
      asio::dispatch(executor, destroyer(this, std::move(allocator)));
  }

  Executor executor;
  allocator_type allocator;
  cancellation_slot cancellation;
  Func func;
};

template<typename Signature>
struct completion_handler_erasure;

template<typename Return,  typename ... Args>
struct completion_handler_erasure<Return(Args...)>
{
  using deleter_t = completion_handler_erasure_deleter_t<Return(Args...)>;

  completion_handler_erasure(const completion_handler_erasure&) = delete;
  completion_handler_erasure(completion_handler_erasure&&) = default;
  completion_handler_erasure& operator=(
      const completion_handler_erasure&) = delete;
  completion_handler_erasure& operator=(
      completion_handler_erasure&&) = default;

  constexpr completion_handler_erasure() = default;

  constexpr completion_handler_erasure(nullptr_t)
    : completion_handler_erasure()
  {
  }

  template<typename Executor, typename Func>
  completion_handler_erasure(Executor exec, Func&& func)
    : impl_(deleter_t::make(std::move(exec), std::forward<Func>(func)))
  {
  }

    template<typename Func>
    completion_handler_erasure(Func&& func)
            : impl_(deleter_t::make(get_associated_executor(func), std::forward<Func>(func)))
    {
    }

  ~completion_handler_erasure()
  {
    if (auto f = std::exchange(impl_, nullptr); f != nullptr)
    {
        auto p = f.release();
        p->destroy_and_deallocate_this();
    }
  }

  Return operator()(Args ... args)
  {
    if (auto f = std::exchange(impl_, nullptr); f != nullptr)
      f->call(std::move(args)...);
  }

  cancellation_slot get_cancellation_slot() const {return impl_.get_cancellation_slot();};

  constexpr bool operator==(nullptr_t) const noexcept {return impl_ == nullptr;}
  constexpr bool operator!=(nullptr_t) const noexcept {return impl_ != nullptr;}
  constexpr bool operator!() const noexcept {return impl_ == nullptr;}

private:
  std::unique_ptr<completion_handler_erasure_base<Return(Args...)>, deleter_t> impl_;
};

} // namespace detail
} // namespace experimental
} // namespace asio

#endif // ASIO_EXPERIMENTAL_DETAIL_COMPLETION_HANDLER_ERASURE_HPP
