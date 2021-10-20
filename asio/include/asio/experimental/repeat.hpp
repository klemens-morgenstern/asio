//
// experimental/repeat.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_EXPERIMENTAL_REPEAT_HPP
#define ASIO_EXPERIMENTAL_REPEAT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include <utility>
#include "asio/detail/array.hpp"
#include "asio/experimental/cancellation_condition.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace experimental {

template<typename Operation>
struct repeater
{
  repeater(Operation op) : op(std::move(op)) {}

  template<typename CompletionToken>
  auto async_repeat(CompletionToken && completion_token)
  {
    return op(std::forward<CompletionToken>(completion_token));
  }

 private:
  Operation op;
};

template<typename Operation>
repeater<std::decay_t<Operation>> repeat(Operation && operation)
{
  return repeater(std::forward<Operation>(operation));
}


} // namespace experimental
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_EXPERIMENTAL_PARALLEL_GROUP_HPP
