//
// memory.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include "asio/experimental/memory.hpp"
#include "asio/awaitable.hpp"
#include "asio/co_spawn.hpp"
#include "asio/random_access_file.hpp"
#include "asio/use_awaitable.hpp"

#include <filesystem>
#include "../unit_test.hpp"

#include <iostream>
#include <asio/detached.hpp>
#include <asio/steady_timer.hpp>
#include <asio/read_at.hpp>
#include <asio/write_at.hpp>

asio::awaitable<void> memory_mapping()
{
  ASIO_CHECK(asio::experimental::memory_mapping::large_page_size() != 0);
  ASIO_CHECK(asio::experimental::memory_mapping::page_size() != 0);
  const auto ps = asio::experimental::memory_mapping::page_size();

  std::filesystem::remove("./memory-test-file-1");
  asio::random_access_file r{co_await asio::this_coro::executor, "./memory-test-file-1",
                             asio::file_base::read_write | asio::file_base::create};

  co_await r.async_write_some_at(2 * ps, asio::buffer("test-text"), asio::use_awaitable);

  asio::experimental::memory_mapping mm(r, asio::experimental::memory_mapping::read_write, ps, 3 * ps);

  auto p = reinterpret_cast<const char*>(mm.get()) + ps;

  std::string_view sv{p, 9};
  ASIO_CHECK(sv == "test-text");
}

asio::awaitable<void> memory_mapping_large()
{
  ASIO_CHECK(asio::experimental::memory_mapping::large_page_size() != 0);
  ASIO_CHECK(asio::experimental::memory_mapping::page_size() != 0);
  const auto ps = asio::experimental::memory_mapping::large_page_size();

  std::filesystem::remove("./memory-test-file-2");
  asio::random_access_file r{co_await asio::this_coro::executor, "./memory-test-file-2",
                             asio::file_base::read_write | asio::file_base::create};

  co_await r.async_write_some_at(2 * ps, asio::buffer("test-text"), asio::use_awaitable);

  asio::experimental::memory_mapping mm(r, asio::experimental::memory_mapping::read_write, ps, 3 * ps);

  auto p = reinterpret_cast<const char*>(mm.get()) + ps;

  std::string_view sv{p, 9};
  ASIO_CHECK(sv == "test-text");
}

asio::awaitable<void> memory_private_file()
{
  ASIO_CHECK(asio::experimental::memory_mapping::large_page_size() != 0);
  ASIO_CHECK(asio::experimental::memory_mapping::page_size() != 0);
  const auto ps = asio::experimental::memory_mapping::large_page_size();

  asio::random_access_file r{co_await asio::this_coro::executor};

  asio::experimental::open_memory(r, ps, true);
  co_await r.async_write_some_at(100, asio::buffer("test-text"), asio::use_awaitable);

  std::string buf;
  buf.resize(4);
  auto sz = co_await r.async_read_some_at(105, asio::buffer(buf), asio::use_awaitable);
  ASIO_CHECK(sz == 4);
  ASIO_CHECK(buf == "text");
}

asio::awaitable<void> memory_shared_file()
{

  ASIO_CHECK(asio::experimental::memory_mapping::large_page_size() != 0);
  ASIO_CHECK(asio::experimental::memory_mapping::page_size() != 0);
  const auto ps = asio::experimental::memory_mapping::large_page_size();

  asio::random_access_file r{co_await asio::this_coro::executor};

  asio::experimental::open_shared_memory(r, "asio-test-file",
                                         asio::experimental::memory_mapping::read_write
                                       | asio::experimental::memory_mapping::create, ps);
  co_await asio::async_write_at(r, 100, asio::buffer("test-text"), asio::use_awaitable);

  std::string buf;
  buf.resize(4);
  auto sz = co_await asio::async_read_at(r, 105, asio::buffer(buf), asio::use_awaitable);
  ASIO_CHECK(sz == 4);
  ASIO_CHECK(buf == "text");
}

#define COMPLETE_CO_SPAWN() [](std::exception_ptr ex) { try { if (ex) std::rethrow_exception(ex); } catch (std::exception & e) { ASIO_CHECK_MESSAGE(false, e.what()); }}

void memory_test()
{
  asio::io_context ctx;

  asio::co_spawn(ctx, memory_mapping, COMPLETE_CO_SPAWN());
  asio::co_spawn(ctx, memory_mapping_large, COMPLETE_CO_SPAWN());
  asio::co_spawn(ctx, memory_private_file, COMPLETE_CO_SPAWN());
  asio::co_spawn(ctx, memory_shared_file, COMPLETE_CO_SPAWN());

  ctx.run();
}

ASIO_TEST_SUITE
(
  "memory",
  ASIO_TEST_CASE(memory_test)
)