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

std::size_t memory_mapping::page_size()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwAllocationGranularity;
}
std::size_t memory_mapping::large_page_size()
{
    return ::GetLargePageMinimum();
}


void memory_mapping::open(native_file_type fd, flags map_flags,
                          std::size_t offset, std::size_t length, error_code & ec) ASIO_NOEXCEPT
{
    if (length == 0)
    {
        LARGE_INTEGER upper{0u};
        if (!GetFileSizeEx(fd, &upper))
        {
            ec.assign(static_cast<int>(::GetLastError()), system_category());
            return;
        }
        length = upper.QuadPart - offset;
    }

    DWORD mts = 0u;

    switch (map_flags & (read_only | write_only | execute | read_write))
    {
        case static_cast<flags>(0):
            break;
        case read_only:
            mts = PAGE_READONLY;
            break;
        case write_only:
            mts = PAGE_WRITECOPY;
            break;
        case execute:
            mts = PAGE_EXECUTE_READ;
            break;
        case (read_only | execute):
            mts = PAGE_EXECUTE_READ;
            break;
        case read_write:
            mts = PAGE_READWRITE;
            break;
        case (write_only | execute):
            mts = PAGE_EXECUTE_WRITECOPY;
            break;
        case read_write | execute:
            mts = PAGE_EXECUTE_READWRITE;
            break;
    }
    if (map_flags & large_pages)
        mts |= SEC_LARGE_PAGES;

    LARGE_INTEGER x={.QuadPart=offset+length};
    const auto mp = ::CreateFileMappingW(fd, nullptr, mts, x.HighPart, x.LowPart, nullptr);
    if (mp == nullptr)
    {
        ec.assign(static_cast<int>(::GetLastError()), system_category());
        return ;
    }

    DWORD prots = 0u;
    if (map_flags & read_only)
        prots = FILE_MAP_READ;
    if (map_flags & write_only)
        prots = FILE_MAP_WRITE;
    if (map_flags & read_write)
        prots = FILE_MAP_ALL_ACCESS;
    if (map_flags & execute)
        prots |= FILE_MAP_EXECUTE;

    if (map_flags & copy_on_write)
        prots = FILE_MAP_COPY;
    if (map_flags & large_pages)
        prots |= FILE_MAP_LARGE_PAGES;

    LARGE_INTEGER li{.QuadPart=offset};
    auto p = MapViewOfFile(mp, prots, li.HighPart, li.LowPart, length);

    if (p != nullptr)
    {
        memory_ = p;
        size_ = length;
    }
    else
        ec.assign(static_cast<int>(::GetLastError()), system_category());
}



void memory_mapping::close(error_code & ec) ASIO_NOEXCEPT
{
    if (!::UnmapViewOfFile(memory_))
        ec.assign(static_cast<int>(::GetLastError()), system_category());
}

template<typename Executor>
ASIO_DECL ASIO_SYNC_OP_VOID open_memory(basic_file<Executor> & file,
                                        std::size_t max_size_hint,
                                        bool large_pages,
                                        error_code & ec) ASIO_NOEXCEPT
{
    static long counter1 = 0;
    static long counter2 = 0;

    long n1 = ::InterlockedIncrement(&counter1);
    long n2 = (static_cast<unsigned long>(n1) % 0x10000000) == 0
              ? ::InterlockedIncrement(&counter2)
              : ::InterlockedExchangeAdd(&counter2, 0);
    wchar_t pipe_name[128];
#if defined(ASIO_HAS_SECURE_RTL)
    swprintf_s(
#else // defined(ASIO_HAS_SECURE_RTL)
            _snwprintf(
#endif // defined(ASIO_HAS_SECURE_RTL)
            pipe_name, 128,
            L"Local\\asio-A0812896-741A-484D-AF23-BE51BF620E22-%u-%ld-%ld",
            static_cast<unsigned int>(::GetCurrentProcessId()), n1, n2);


    DWORD mts = PAGE_READWRITE;
    if (large_pages)
        mts |= SEC_LARGE_PAGES;

    LARGE_INTEGER li{.QuadPart=max_size_hint};
    auto mp = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, mts,
                                   li.HighPart, li.LowPart, pipe_name);
    if (mp == nullptr)
    {
        ec.assign(errno, system_category());
        ASIO_SYNC_OP_VOID_RETURN(ec);
    }

    DWORD prots = FILE_MAP_ALL_ACCESS ; //| FILE_MAP_EXECUTE;
    if (large_pages)
        prots |= FILE_MAP_LARGE_PAGES;


    auto fd = ::OpenFileMappingW(prots, true, pipe_name);
    if (fd == nullptr)
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
    const auto pipe_name = std::string("Global\\") + name;


    DWORD mts = 0u;
    using f = asio::experimental::memory_mapping;

    switch (map_flags & (f::read_only | f::write_only | f::execute | f::read_write))
    {
        case f::read_only:
            mts = PAGE_READONLY;
            break;
        case f::write_only:
            mts = PAGE_WRITECOPY;
            break;
        case f:: execute:
            mts = PAGE_EXECUTE_READ;
            break;
        case (f::read_only | f::execute):
            mts = PAGE_EXECUTE_READ;
            break;
        case f::read_write:
            mts = PAGE_READWRITE;
            break;
        case (f::write_only | f::execute):
            mts = PAGE_EXECUTE_WRITECOPY;
            break;
        case f::read_write | f::execute:
            mts = PAGE_EXECUTE_READWRITE;
            break;
    }
    if (map_flags & f::large_pages)
        mts |= SEC_LARGE_PAGES;


    LARGE_INTEGER li{.QuadPart=max_size_hint};
    auto mp = ::CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, mts,
                                   li.HighPart, li.LowPart, pipe_name.c_str());
    if (mp == nullptr)
    {
        ec.assign(errno, system_category());
        ASIO_SYNC_OP_VOID_RETURN(ec);
    }

    DWORD prots = 0u;
    if (map_flags & f::read_only)
        prots = FILE_MAP_READ;
    if (map_flags & f::write_only)
        prots = FILE_MAP_WRITE;
    if (map_flags & f::read_write)
        prots = FILE_MAP_ALL_ACCESS;
    if (map_flags & f::execute)
        prots |= FILE_MAP_EXECUTE;

    if (map_flags & f::copy_on_write)
        prots = FILE_MAP_COPY;
    if (map_flags & f::large_pages)
        prots |= FILE_MAP_LARGE_PAGES;

    auto fd = ::OpenFileMappingA(prots, true, pipe_name.c_str());
    if (fd == nullptr)
    {
        ec.assign(errno, system_category());
        ASIO_SYNC_OP_VOID_RETURN(ec);
    }
    file.assign(fd, ec);
    ASIO_SYNC_OP_VOID_RETURN(ec);

}


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