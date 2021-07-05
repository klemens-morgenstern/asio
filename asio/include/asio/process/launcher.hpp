//
// process/launcher.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_PROCESS_HANDLE_HPP
#define ASIO_PROCESS_HANDLE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)


#include "asio/detail/config.hpp"

#if defined(ASIO_WINDOWS) || defined(__CYGWIN__)
#error Not implemented yet

#else
#include "asio/process/detail/posix_launcher.hpp"
#endif


#endif //ASIO_HANDLE_HPP
