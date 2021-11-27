//
// experimental/subject.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include "asio/io_context.hpp"
#include "asio/experimental/subject.hpp"

#include "../unit_test.hpp"

void subject_test()
{
  asio::io_context ctx;

  asio::experimental::subject<void(asio::error_code, int)> sub{ctx};

  bool sent = 0;
  int rec1 = 0, rec2 = 0;

  auto s1 = sub.subscribe(), s2 = sub.subscribe();

  s1.async_receive([&](asio::error_code ec, int i ) {ASIO_CHECK_MESSAGE(!ec, ec.message()); rec1 = i;});
  s2.async_receive([&](asio::error_code ec, int i ) {ASIO_CHECK_MESSAGE(!ec, ec.message()); rec2 = i;});


  sub.async_send(asio::error_code{}, 42, [&](asio::error_code ec) {ASIO_CHECK_MESSAGE(!ec, ec.message()); sent = true;});

  ctx.run();

  ASIO_CHECK(sent);
  ASIO_CHECK(rec1 == 42);
  ASIO_CHECK(rec2 == 42);
}

void behaviour_subject_test()
{
  asio::io_context ctx;

  asio::experimental::behaviour_subject<void(asio::error_code, int)> sub(ctx, {asio::error_code{}, 42});

  bool sent = 0;
  int rec1 = 0, rec2 = 0;

  auto s1 = sub.subscribe(), s2 = sub.subscribe();

  s1.async_receive([&](asio::error_code ec, int i ) {ASIO_CHECK_MESSAGE(!ec, ec.message()); rec1 = i;});
  s2.async_receive([&](asio::error_code ec, int i ) {ASIO_CHECK_MESSAGE(!ec, ec.message()); rec2 = i;});
  ctx.run();

  ASIO_CHECK(rec1 == 42);
  ASIO_CHECK(rec2 == 42);
}

void replay_subject_test()
{
  asio::io_context ctx;


  asio::experimental::replay_subject<void(asio::error_code, int)> sub(ctx, 1);

  bool sent = 0;
  int rec1 = 0, rec2 = 0;

  auto s1 = sub.subscribe(), s2 = sub.subscribe();

  s1.async_receive([&](asio::error_code ec, int i ) {ASIO_CHECK_MESSAGE(!ec, ec.message()); rec1 = i;});
  s2.async_receive([&](asio::error_code ec, int i ) {ASIO_CHECK_MESSAGE(!ec, ec.message()); rec2 = i;});
  sub.async_send(asio::error_code{}, 42, [&](asio::error_code ec) {ASIO_CHECK_MESSAGE(!ec, ec.message()); sent = true;});
  ctx.run();

  ASIO_CHECK(rec1 == 42);
  ASIO_CHECK(rec2 == 42);


  ctx.restart();
  s1 = sub.subscribe();
  s2 = sub.subscribe();
  rec1 = 0;
  rec2 = 0;
  s1.async_receive([&](asio::error_code ec, int i ) {ASIO_CHECK_MESSAGE(!ec, ec.message()); rec1 = i;});
  s2.async_receive([&](asio::error_code ec, int i ) {ASIO_CHECK_MESSAGE(!ec, ec.message()); rec2 = i;});
  ctx.run();

  ASIO_CHECK(rec1 == 42);
  ASIO_CHECK(rec2 == 42);
}

ASIO_TEST_SUITE
(
  "experimental/subject",
  ASIO_TEST_CASE(subject_test)
  ASIO_TEST_CASE(behaviour_subject_test)
  ASIO_TEST_CASE(replay_subject_test)
)
