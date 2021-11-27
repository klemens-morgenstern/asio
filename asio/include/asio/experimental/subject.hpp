//
// experimental/subject.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_EXPERIMENTAL_SUBJECT_HPP
#define ASIO_EXPERIMENTAL_SUBJECT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include "asio/any_io_executor.hpp"
#include "asio/detail/type_traits.hpp"
#include "asio/execution/executor.hpp"
#include "asio/is_executor.hpp"
#include "asio/experimental/basic_subject.hpp"
#include "asio/experimental/channel_traits.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace experimental {
namespace detail {

template <typename ExecutorOrSignature, typename = void>
struct subject_type
{
  template <typename... Signatures>
  struct inner
  {
    typedef basic_subject<any_io_executor, channel_traits<>,
        ExecutorOrSignature, Signatures...> type;
  };

  template <typename... Signatures>
  struct inner_behaviour
  {
    typedef basic_behaviour_subject<any_io_executor, channel_traits<>,
            ExecutorOrSignature, Signatures...> type;
  };

  template <typename... Signatures>
  struct inner_replay
  {
    typedef basic_replay_subject<any_io_executor, channel_traits<>,
            ExecutorOrSignature, Signatures...> type;
  };
};

template <typename ExecutorOrSignature>
struct subject_type<ExecutorOrSignature,
    typename enable_if<
      is_executor<ExecutorOrSignature>::value
        || execution::is_executor<ExecutorOrSignature>::value
    >::type>
{
  template <typename... Signatures>
  struct inner
  {
    typedef basic_subject<ExecutorOrSignature,
        channel_traits<>, Signatures...> type;
  };

  template <typename... Signatures>
  struct inner_behaviour
  {
    typedef basic_behaviour_subject<ExecutorOrSignature, channel_traits<>,
            Signatures...> type;
  };

  template <typename... Signatures>
  struct inner_replay
  {
    typedef basic_replay_subject<ExecutorOrSignature, channel_traits<>,
            Signatures...> type;
  };
};

} // namespace detail

/// Template type alias for common use of subject.
template <typename ExecutorOrSignature, typename... Signatures>
using subject = typename detail::subject_type<
    ExecutorOrSignature>::template inner<Signatures...>::type;

/// Template type alias for common use of behaviour_subject.
template <typename ExecutorOrSignature, typename... Signatures>
using behaviour_subject = typename detail::subject_type<
        ExecutorOrSignature>::template inner_behaviour<Signatures...>::type;

/// Template type alias for common use of replay_subjectt.
template <typename ExecutorOrSignature, typename... Signatures>
using replay_subject = typename detail::subject_type<
        ExecutorOrSignature>::template inner_replay<Signatures...>::type;

} // namespace experimental
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_EXPERIMENTAL_SUBJECT_HPP
