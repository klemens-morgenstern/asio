//
// experimental/memory.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASIO_MEMORY_IPP
#define ASIO_MEMORY_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#if defined(ASIO_WINDOWS) || defined(__CYGWIN__)

#else

#include <sys/mman.h>
#include <linux/memfd.h>

#endif

#include "asio/detail/push_options.hpp"

namespace asio
{

template<typename Executor>
class basic_file;

namespace experimental
{

#if defined(ASIO_WINDOWS) || defined(__CYGWIN__)

#else

std::size_t memory_mapping::page_size()       { return sysconf(_SC_PAGE_SIZE); }
std::size_t memory_mapping::large_page_size() { return 1024 * 1024 * 2; }


void memory_mapping::open(native_file_type fd, flags map_flags,
                                    std::size_t offset, std::size_t length, error_code & ec) ASIO_NOEXCEPT
{
  if (length == 0)
  {
    const off_t current = lseek(fd, static_cast<std::size_t>(0), SEEK_CUR);
    const auto size  = lseek(fd, static_cast<std::size_t>(0), SEEK_END);
    lseek(fd, current, SEEK_SET);
    length = (size - offset);
  }

  int prots = PROT_NONE;
  if (map_flags & read_only)
    prots |= PROT_READ;
  if (map_flags & write_only)
    prots |= PROT_WRITE;
  if (map_flags & read_write)
    prots |= PROT_READ | PROT_WRITE;
  if (map_flags & execute)
    prots |= PROT_EXEC;

  int flags = MAP_SHARED;
  if (map_flags & copy_on_write)
    flags = MAP_PRIVATE;
  if (map_flags & large_pages)
    flags |= MAP_HUGETLB | (21 << MAP_HUGE_SHIFT);


  auto p =  mmap(nullptr, length, prots, flags, fd, static_cast<off_t >(offset));

  if (p != MAP_FAILED)
  {
    memory_ = p;
    size_ = length;
  }
  else
    ec.assign(errno, system_category());
}

void memory_mapping::close(error_code & ec) ASIO_NOEXCEPT
{
  if (munmap(memory_, size_))
    ec.assign(errno, system_category());
}

template<typename Executor>
ASIO_DECL ASIO_SYNC_OP_VOID open_memory(basic_file<Executor> & file,
                                        std::size_t max_size_hint,
                                        bool large_pages,
                                        error_code & ec) ASIO_NOEXCEPT
{
  const auto fd = memfd_create("asio-memory-file", large_pages ? (MFD_HUGE_2MB | MFD_HUGETLB) : 0);
  if (fd == -1)
  {
    ec.assign(errno, system_category());
    ASIO_SYNC_OP_VOID_RETURN(ec);
  }

  file.assign(fd, ec);
  ASIO_SYNC_OP_VOID_RETURN(ec);
}

template<typename Executor>
 ASIO_SYNC_OP_VOID open_shared_memory(basic_file<Executor> & file,
                                      const char * name,
                                      memory_mapping::flags map_flags,
                                      std::size_t max_size_hint,
                                      error_code & ec) ASIO_NOEXCEPT
{

  using f = asio::experimental::memory_mapping;
  int oflag = 0;
  if (map_flags & f::read_only)
    oflag |= O_RDONLY;
  if (map_flags & f::write_only)
    oflag |= O_WRONLY;
  if (map_flags & f::read_write)
    oflag |= O_RDWR;
  if (map_flags & f::execute)
    oflag |= O_EXCL;
  if (map_flags & f::create)
    oflag |= O_CREAT;

  int flags = MAP_SHARED;
  if (map_flags & f::copy_on_write)
    flags = MAP_PRIVATE;
  if (map_flags & f::large_pages)
    flags |= MAP_HUGETLB | (21 << MAP_HUGE_SHIFT);

  auto fd = shm_open(name,oflag, 0666);

  if (fd == -1)
  {
    ec.assign(errno, system_category());
    ASIO_SYNC_OP_VOID_RETURN(ec);
  }

  file.assign(fd, ec);
  ASIO_SYNC_OP_VOID_RETURN(ec);

}


#endif

}
}

#include "asio/detail/pop_options.hpp"

#endif