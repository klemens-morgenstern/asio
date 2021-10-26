//
// experimental/copy.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_EXPERIMENTAL_COPY_HPP
#define ASIO_EXPERIMENTAL_COPY_HPP

#include "asio/detail/config.hpp"

#include <mutex>

#include "asio/associated_allocator.hpp"
#include "asio/associated_cancellation_slot.hpp"
#include "asio/associated_executor.hpp"
#include "asio/write.hpp"
#include "asio/buffer.hpp"
#include "asio/error_code.hpp"
#include "asio/detail/type_traits.hpp"

#include "asio/detail/push_options.hpp"

namespace asio
{

template <typename Protocol, typename Executor>
class basic_stream_socket;

template <typename Protocol, typename Executor>
class basic_datagram_socket;

namespace experimental
{

struct copy_signature_probe {};

template <typename T>
struct copy_signature_probe_result
{
  typedef T type;
};

}

template <typename R>
class async_result<experimental::copy_signature_probe, R(error_code, std::size_t)>
{
 public:
  typedef experimental::copy_signature_probe_result<void(error_code, std::size_t)>
          return_type;

  template <typename Initiation, typename... InitArgs>
  ASIO_CONSTEXPR static return_type initiate(
          ASIO_MOVE_ARG(Initiation),
          experimental::copy_signature_probe,
          ASIO_MOVE_ARG(InitArgs)...)
  {
    return return_type{};
  }
};

namespace experimental
{


/// The default traits
struct copy_source_buffer_default_mutable
{
  template<typename Allocator>
  using buffer_type = std::vector<char, typename std::allocator_traits<Allocator>::template rebind_alloc<char>>;

  template<typename Allocator = std::allocator<char>>
  static buffer_type<Allocator> make_buffer(Allocator allocator = std::allocator<char>())
  {
    //65k so we always can read a full IP frame
    return buffer_type<Allocator>{std::numeric_limits<unsigned short>::max(), ' ', allocator};
  }

  template<typename Allocator = std::allocator<char>>
  static auto read_buffer(buffer_type<Allocator> & read_buf) -> asio::mutable_buffer
  {
    return asio::buffer(read_buf);
  }

  template<typename Allocator = std::allocator<char>>
  static auto write_buffer(const buffer_type<Allocator> & write_buf,
                    std::size_t size,
                    std::size_t offset = 0u) -> asio::const_buffer
  {
    return asio::buffer(write_buf, size) + offset;
  }
};

struct copy_source_buffer_default_dynamic
{
  template<typename Allocator>
  using buffer_type = std::vector<char, typename std::allocator_traits<Allocator>::template rebind_alloc<char>>;

  template<typename Allocator = std::allocator<char>>
  static buffer_type<Allocator> make_buffer(Allocator allocator = std::allocator<char>())
  {
    //65k so we always can read a full IP frame
    return buffer_type<Allocator>(allocator);
  }

  template<typename Allocator = std::allocator<char>>
  static auto read_buffer(buffer_type<Allocator> & read_buf)
        -> asio::dynamic_vector_buffer<char, typename buffer_type<Allocator>::allocator_type>
  {
    return asio::dynamic_buffer(read_buf);
  }

  template<typename Allocator = std::allocator<char>>
  static auto write_buffer(const buffer_type<Allocator> & write_buf,
                           std::size_t size, std::size_t offset = 0u) -> asio::const_buffer
  {
    return asio::buffer(write_buf, size) + offset;
  }
};

namespace detail
{

template<typename Source, typename = void>
struct deduce_copy_source_traits;

template<typename Source>
struct deduce_copy_source_traits<Source,
        typename enable_if<is_same<typename decltype(
        std::declval<Source>()
                .async_read(
                        std::declval<dynamic_vector_buffer<char, std::allocator<char>>>(),
                        copy_signature_probe{}))::type, void(error_code, std::size_t)>::value>::type
>
{
  ASIO_CONSTEXPR static bool is_raw_data = true;

  typedef deduce_copy_source_traits enable;
  typedef deduce_copy_source_traits enable_async;

  typedef Source source_type;
  typedef copy_source_buffer_default_dynamic buffer_traits;

  template<typename Buffer, typename ReadHandler>
  static auto async_read(Source &source, Buffer &&buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return source.async_read(buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }

  template<typename Buffer>
  static std::size_t read(Source &source, Buffer &&buffer, error_code & ec)
  {
    return source.read(buffer, ec);
  }
};

template<typename Source>
struct deduce_copy_source_traits<Source,
        typename enable_if<is_same<typename decltype(
        std::declval<Source>()
                .async_read_some(
                        std::declval<mutable_buffer>(),
                        copy_signature_probe{}))::type, void(error_code, std::size_t)>::value>::type
>
{
  ASIO_CONSTEXPR static bool is_raw_data = true;

  typedef deduce_copy_source_traits enable;
  typedef deduce_copy_source_traits enable_async;

  typedef Source source_type;
  typedef copy_source_buffer_default_mutable buffer_traits;

  template<typename Buffer, typename ReadHandler>
  static auto async_read(Source &source, Buffer &&buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return source.async_read_some(buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }

  template<typename Buffer>
  static std::size_t read(Source &source, Buffer &&buffer, error_code & ec)
  {
    return source.read_some(buffer, ec);
  }
};

template<typename Source>
struct deduce_copy_source_traits<Source,
        typename enable_if<is_same<typename decltype(
        std::declval<Source>()
                .async_receive(
                        std::declval<mutable_buffer>(),
                        copy_signature_probe{}))::type, void(error_code, std::size_t)>::value>::type
>
{
  ASIO_CONSTEXPR static bool is_raw_data = true;

  typedef deduce_copy_source_traits enable;
  typedef deduce_copy_source_traits enable_async;

  typedef Source source_type;
  typedef copy_source_buffer_default_mutable buffer_traits;

  template<typename Buffer, typename ReadHandler>
  static auto async_read(Source &source, Buffer &&buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return source.async_receive(buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }
  template<typename Buffer>
  static std::size_t read(Source &source, Buffer &&buffer, error_code & ec)
  {
    return source.receive(buffer, ec);
  }
};

template<typename Sink, typename = void>
struct deduce_copy_sink_traits;

template<typename Sink>
struct deduce_copy_sink_traits<Sink,
        typename enable_if<is_same<typename decltype(
        std::declval<Sink>()
                .async_write_some(
                        std::declval<const_buffer>(),
                        copy_signature_probe{}))::type, void(error_code, std::size_t)>::value>::type
>
{
  ASIO_CONSTEXPR static bool is_raw_data = true;

  typedef deduce_copy_sink_traits enable;
  typedef deduce_copy_sink_traits enable_async;

  typedef Sink sink_type;

  template<typename Buffer, typename ReadHandler>
  static auto async_write(Sink &sink, const Buffer &buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return sink.async_write_some(buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }
  template<typename Buffer>
  static std::size_t write(Sink &sink, Buffer &&buffer, error_code & ec)
  {
    return sink.write_some(buffer, ec);
  }
};

template<typename Sink>
struct deduce_copy_sink_traits<Sink,
        typename enable_if<is_same<typename decltype(
        std::declval<Sink>()
                .async_write(
                        std::declval<const_buffer>(),
                        copy_signature_probe{}))::type, void(error_code, std::size_t)>::value>::type
>
{
  ASIO_CONSTEXPR static bool is_raw_data = true;

  typedef deduce_copy_sink_traits enable;
  typedef deduce_copy_sink_traits enable_async;

  typedef Sink sink_type;

  template<typename Buffer, typename ReadHandler>
  static auto async_write(Sink &sink, const Buffer &buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return sink.async_write(buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }
  template<typename Buffer>
  static std::size_t write(Sink &sink, Buffer &&buffer, error_code & ec)
  {
    return sink.write(buffer, ec);
  }
};

template<typename Sink>
struct deduce_copy_sink_traits<Sink,
        typename enable_if<is_same<typename decltype(
        std::declval<Sink>()
                .async_send(
                        std::declval<const_buffer>(),
                        copy_signature_probe{}))::type, void(error_code, std::size_t)>::value, Sink>::type
>
{
  ASIO_CONSTEXPR static bool is_raw_data = true;

  typedef std::vector<char> preferred_type;
  typedef deduce_copy_sink_traits enable;
  typedef deduce_copy_sink_traits enable_async;

  typedef Sink sink_type;

  template<typename Buffer, typename ReadHandler>
  static auto async_write(Sink &sink, const Buffer &buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return sink.async_send(buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }
  template<typename Buffer>
  static std::size_t write(Sink &sink, Buffer &&buffer, error_code & ec)
  {
    return sink.send(buffer, ec);
  }
};

}

template<typename T> struct   copy_sink_traits : detail::deduce_copy_sink_traits<T> {};
template<typename T> struct copy_source_traits : detail::deduce_copy_source_traits<T> {};

template<typename Protocol, typename Executor>
struct copy_sink_traits<basic_stream_socket<Protocol, Executor>>
{
  ASIO_CONSTEXPR static bool is_raw_data = true;

  typedef copy_sink_traits enable;
  typedef copy_sink_traits enable_async;

  typedef basic_stream_socket<Protocol, Executor> sink_type;

  template<typename Buffer, typename ReadHandler>
  static auto async_write(basic_stream_socket<Protocol, Executor> &sink,
                             const Buffer &buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return sink.async_write_some(buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }
  template<typename Buffer>
  static std::size_t write(sink_type &sink, Buffer &&buffer, error_code & ec)
  {
    return sink.write_some(buffer, ec);
  }
};

template<typename Protocol, typename Executor>
struct copy_sink_traits<basic_datagram_socket<Protocol, Executor>>
{
  ASIO_CONSTEXPR static bool is_raw_data = true;

  typedef copy_sink_traits enable;
  typedef copy_sink_traits enable_async;

  typedef basic_datagram_socket<Protocol, Executor> sink_type;

  template<typename Buffer, typename ReadHandler>
  static auto async_write(basic_datagram_socket<Protocol, Executor> &sink,
                             const Buffer &buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return sink.async_send(buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }

  template<typename Buffer>
  static std::size_t write(sink_type &sink, Buffer &&buffer, error_code & ec)
  {
    return sink.send(buffer, ec);
  }
};

template<typename Protocol, typename Executor>
struct copy_source_traits<basic_stream_socket<Protocol, Executor>>
{
  ASIO_CONSTEXPR static bool is_raw_data = true;

  typedef copy_source_traits enable;
  typedef copy_source_traits enable_async;

  typedef basic_stream_socket<Protocol, Executor> source_type;
  typedef copy_source_buffer_default_mutable buffer_traits;

  template<typename Buffer, typename ReadHandler>
  static auto async_read(basic_stream_socket<Protocol, Executor> &source,
                             const Buffer &buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return source.async_read_some(buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }

  template<typename Buffer>
  static std::size_t read(source_type &source, Buffer &&buffer, error_code & ec)
  {
    return source.read_some(buffer, ec);
  }
};


template<typename Protocol, typename Executor>
struct copy_source_traits<basic_datagram_socket<Protocol, Executor>>
{
  ASIO_CONSTEXPR static bool is_raw_data = true;

  typedef copy_source_traits enable;
  typedef copy_source_traits enable_async;
  typedef basic_datagram_socket<Protocol, Executor> source_type;
  typedef copy_source_buffer_default_mutable buffer_type;

  template<typename Buffer, typename ReadHandler>
  static auto async_read(basic_datagram_socket<Protocol, Executor> &source, const Buffer &buffer, ASIO_MOVE_ARG(ReadHandler)read_handler)
  -> ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler, void (boost::system::error_code, std::size_t))
  {
    return source.async_read(buffer, ASIO_MOVE_CAST(ReadHandler)(read_handler));
  }

  template<typename Buffer>
  static std::size_t read(source_type &source, Buffer &&buffer, error_code & ec)
  {
    return source.read(buffer, ec);
  }
};

// can be specialized to use things like sendfile and TransmitFile
template<typename Source,
         typename Sink,
         typename SourceTraits = typename copy_source_traits<Source>::enable,
         typename SinkTraits   = typename copy_sink_traits<Sink>::enable>
struct copy_traits
{
  typedef SourceTraits  source_traits;
  typedef   SinkTraits    sink_traits;
  typedef typename source_traits::buffer_traits buffer_traits;

  typedef Source  source_type;
  typedef   Sink    sink_type;
};

template<
        typename Source,
        typename Sink,
        typename Traits = copy_traits<Source, Sink>,
        // this is so we can later on value/typed streams like channels or coros.
        // raw_data means it uses a buffer
        typename = typename enable_if<Traits::source_traits::is_raw_data &&
                                      Traits::  sink_traits::is_raw_data>::type>
std::size_t copy(
        Source & source,
        Sink & sink,
        error_code & ec)
{
  typedef typename Traits::source_traits        source_traits;
  typedef typename Traits::  sink_traits          sink_traits;
  typedef typename source_traits::buffer_traits buffer_traits;

  std::size_t res{0};

  auto buf = buffer_traits::make_buffer();
  while (ec)
  {
    const std::size_t read = source_traits::read(source, buffer_traits::read_buffer(buf), ec);
    error_code ew;

    std::size_t all_written{0u};
    for (std::size_t write = sink_traits::write(sink, buffer_traits::write_buffer(buf, read), ew);
         (all_written != read) && !ew;
         write = sink_traits::write(sink, buffer_traits::write_buffer(buf, read - all_written, all_written), ew));

    if (!ew && ec)
      ec = ew;
  }

  return res;
}

template<
        typename Source,
        typename Sink,
        typename Traits = copy_traits<Source, Sink>>
std::size_t copy(
        Source & source,
        Sink & sink)
{
  error_code  ec;
  auto res = asio::experimental::copy(source, sink, ec);

  if (ec)
    asio::detail::throw_error(ec, "copy");

  return res;
}


// can be specialized to use things like sendfile and TransmitFile
template<typename Source,
        typename Sink,
        typename SourceTraits = typename copy_source_traits<Source>::enable_async,
        typename SinkTraits   = typename copy_sink_traits<Sink>::enable_async>
struct async_copy_traits
{
  typedef SourceTraits  source_traits;
  typedef   SinkTraits    sink_traits;
  typedef typename source_traits::buffer_traits buffer_traits;

  typedef Source  source_type;
  typedef   Sink    sink_type;
};

namespace detail
{

template<typename Source,
         typename Sink,
         typename Handler,
         typename Traits = async_copy_traits<Source, Sink>,
         // this is so we can later on value/typed streams like channels or coros.
         // raw_data means it uses a buffer
         typename = typename enable_if<Traits::source_traits::is_raw_data &&
                                       Traits::  sink_traits::is_raw_data>::type>
struct async_copy_some_impl
{
  typedef copy_traits<Source, Sink> traits;
  typedef typename traits::source_traits        source_traits;
  typedef typename traits::  sink_traits          sink_traits;
  typedef typename source_traits::buffer_traits buffer_traits;

  Source & source;
  Sink & sink;
  Handler handler;

  typename prefer_result<typename Source::executor_type, execution::outstanding_work_t::tracked_t>::type source_tracked{source.get_executor()};
  typename prefer_result<typename   Sink::executor_type, execution::outstanding_work_t::tracked_t>::type   sink_tracked{  sink.get_executor()};

  typedef typename associated_executor<Handler, typename Source::executor_type>::type executor_type;
  executor_type executor{get_associated_executor(handler, source.get_executor())};
  executor_type get_executor() {return executor;};

  typedef typename associated_allocator<executor_type>::type allocator_type;
  allocator_type get_allocator() const {return get_associated_allocator(executor);}

  asio::cancellation_state cancel_state{get_associated_cancellation_slot(handler)};
  asio::cancellation_signal cancel_read, cancel_write;

  typedef typename buffer_traits::template buffer_type<allocator_type> buffer_type;

  buffer_type read_buffer{buffer_traits::make_buffer(get_allocator())},
             write_buffer{buffer_traits::make_buffer(get_allocator())};

  std::atomic_bool is_reading{false},
                   is_writing{false};

  std::size_t read_size{0};
  std::size_t written{0};
  error_code err;

  struct handle_cancel
  {
    async_copy_some_impl *ptr;
    handle_cancel(async_copy_some_impl * ptr) : ptr(ptr) {}

    void operator()(cancellation_type t) const
    {
      ptr->cancel_read.emit(t);
      ptr->cancel_write.emit(t);
    }

  };

  async_copy_some_impl(Source & source, Sink & sink, Handler && handler)
          : source(source), sink(sink), handler(std::move(handler))
  {
    cancel_state.slot().emplace<handle_cancel>(this);
  }



  struct bind_handle_read
  {
    std::shared_ptr<async_copy_some_impl> ptr;

    using executor_type = executor_type;
    executor_type get_executor() const {return ptr->get_executor(); }

    typedef asio::cancellation_slot cancellation_slot_type;
    cancellation_slot_type get_cancellation_slot() const noexcept
    {
      return ptr->cancel_read.slot();
    }

    bind_handle_read(std::shared_ptr<async_copy_some_impl> ptr) : ptr(std::move(ptr)) {}

    void operator()(error_code ec, std::size_t sz)
    {
      handle_read(std::move(ptr), ec, sz);
    }
  };

  struct bind_handle_write
  {
    std::shared_ptr<async_copy_some_impl> ptr;

    using executor_type = executor_type;
    executor_type get_executor() const {return ptr->get_executor(); }

    typedef asio::cancellation_slot cancellation_slot_type;
    cancellation_slot_type get_cancellation_slot() const noexcept
    {
      return ptr->cancel_write.slot();
    }

    bind_handle_write(std::shared_ptr<async_copy_some_impl> ptr) : ptr(std::move(ptr)) {}

    void operator()(error_code ec, std::size_t sz)
    {
      handle_write(std::move(ptr), ec, sz);
    }
  };

  static void start(std::shared_ptr<async_copy_some_impl> this_)
  {
    this_->is_reading.store(true);
    ASIO_HANDLER_LOCATION((__FILE__, __LINE__, "async_copy"));
    source_traits::async_read(this_->source,
                              buffer_traits::read_buffer(this_->read_buffer),
                              bind_handle_read(std::move(this_)));
  }

  static void handle_read (std::shared_ptr<async_copy_some_impl> this_, error_code ec, std::size_t sz)
  {
    if (ec && !this_->err)
      this_->err = ec;

    //if writing, we wait for the write to finish before rereading
    if (this_->is_writing)
    {
      this_->is_reading.store(false);
      return;
    }

    if (ec && (sz == 0)) // error and NO pending write
    {
      // we're already on a post
      std::move(this_->handler)(this_->err == error::eof ? error_code() : this_->err, this_->written);
      return ;
    }

    // we read something, so switch the buffers and start a write:
    bool write_f{false};
    if (this_->is_writing.compare_exchange_strong(write_f, true))
    {
      std::swap(this_->read_buffer, this_->write_buffer);
      ASIO_HANDLER_LOCATION((__FILE__, __LINE__, "async_copy"));
      sink_traits::async_write(this_->sink,
                               buffer_traits::write_buffer(this_->write_buffer, sz),
                               bind_handle_write(this_));

      source_traits::async_read(this_->source,
                                buffer_traits::read_buffer(this_->read_buffer),
                                bind_handle_read(std::move(this_)));

    }
    else
      this_->read_size = sz;
  }
  static void handle_write(std::shared_ptr<async_copy_some_impl> this_, error_code ec, std::size_t sz)
  {
    if ((!this_->err || this_->err == error::eof ) && ec)
      this_->err = ec;

    this_->written += sz;
    bool is_r{false};
    if (!this_->is_reading.compare_exchange_strong(is_r, true)) // no pending reads.
    {
      if (this_->err)
      {
        // we're already on a post
        std::move(this_->handler)(this_->err == error::eof ? error_code() : this_->err, this_->written);
        return ;
      }

      // check we got more to write
      auto writable = std::exchange(this_->read_size, 0u);

      if (writable)
      {
        std::swap(this_->read_buffer, this_->write_buffer);
        ASIO_HANDLER_LOCATION((__FILE__, __LINE__, "async_copy"));
        sink_traits::async_write(this_->sink,
                                 buffer_traits::write_buffer(this_->write_buffer, writable),
                                 bind_handle_write(this_));
      }
      else
        this_->is_writing.store(false);

      if (!this_->is_reading.exchange(true))
      {
        ASIO_HANDLER_LOCATION((__FILE__, __LINE__, "async_copy"));
        source_traits::async_read(this_->source,
                                  buffer_traits::read_buffer(this_->read_buffer),
                                  bind_handle_read(std::move(this_)));
      }

    }
  }
  // ok reading is going on, so we need to wait for it to finish

};


struct initiate_async_copy
{

  template<typename Handler, typename Source, typename Sink>
  void operator()(Handler && handler, Source * source, Sink * sink)
  {
    typedef async_copy_some_impl<Source, Sink, typename std::decay<Handler>::type> impl_type;
    auto exec = get_associated_executor(handler, source->get_executor());
    auto p = std::allocate_shared<impl_type>(get_associated_allocator(exec),
                                             *source, *sink, std::forward<Handler>(handler));
    dispatch(exec, [p] {impl_type::start(std::move(p));});

  }
};

}


template<
        typename Source,
        typename Sink,
        ASIO_COMPLETION_TOKEN_FOR(void (asio::error_code, std::size_t)) CompletionToken,
        typename Traits = copy_traits<Source, Sink>>
ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
void (boost::system::error_code, std::size_t))
async_copy(
        Source & source,
        Sink & sink,
        CompletionToken && completion_token)
{
  return asio::async_initiate<CompletionToken, void(error_code, std::size_t)>
          (
           detail::initiate_async_copy{},
           completion_token,
           &source, &sink
          );
}

}

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_EXPERIMENTAL_COPY_HPP
