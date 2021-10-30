//
// experimental/memory.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef ASIO_MEMORY_HPP
#define ASIO_MEMORY_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include "asio/buffer.hpp"
#include "asio/detail/throw_error.hpp"
#include "asio/error_code.hpp"
#include "asio/file_base.hpp"
#include <memory>
#include "asio/detail/push_options.hpp"

namespace asio
{

template <typename Executor>
class basic_file;

namespace experimental
{

struct memory_mapping
{

#if defined(GENERATING_DOCUMENTATION)
  /// A bitmask type (C++ Std [lib.bitmask.types]).
  typedef unspecified flags;

  /// Map as readable memory. Writing will cause a SIGSEV.
  static const flags read_only = implementation_defined;

  /// Map as writable memory. Reading is undefined behaviour.
  static const flags write_only = implementation_defined;

  /// Map as readable and writable memory.
  static const flags read_write = implementation_defined;

  /// Allow code in the mapping to be executoed.
  static const flags execute = implementation_defined;

  /// Make a private copy-on-write handle.
  static const flags copy_on_write = implementation_defined;

  /// Indicate the usage of large pages. The offset needs to be a multiple `large_page_size()`.
  static const flags large_pages = implementation_defined;

  /// Create file flag for the in-memory files. Unused by memory mappings.
  static const flags large_pages = implementation_defined;
#else
  enum flags
  {
    read_only = 1,
    write_only = 2,
    read_write = 4,
    execute = 8,
    copy_on_write = 16,
    large_pages = 32,
    create = 64
  };

  // Implement bitmask operations as shown in C++ Std [lib.bitmask.types].

  friend ASIO_CONSTEXPR flags operator&(flags x, flags y)
  {
    return static_cast<flags>(
            static_cast<unsigned int>(x) & static_cast<unsigned int>(y));
  }

  friend ASIO_CONSTEXPR flags operator|(flags x, flags y)
  {
    return static_cast<flags>(
            static_cast<unsigned int>(x) | static_cast<unsigned int>(y));
  }

  friend ASIO_CONSTEXPR flags operator^(flags x, flags y)
  {
    return static_cast<flags>(
            static_cast<unsigned int>(x) ^ static_cast<unsigned int>(y));
  }

  friend ASIO_CONSTEXPR flags operator~(flags x)
  {
    return static_cast<flags>(~static_cast<unsigned int>(x));
  }

  friend ASIO_CONSTEXPR flags& operator&=(flags& x, flags y)
  {
    x = x & y;
    return x;
  }

  friend ASIO_CONSTEXPR flags& operator|=(flags& x, flags y)
  {
    x = x | y;
    return x;
  }

  friend ASIO_CONSTEXPR flags& operator^=(flags& x, flags y)
  {
    x = x ^ y;
    return x;
  }

#endif

  /// The native representation of a file.
#if defined(GENERATING_DOCUMENTATION)
  typedef implementation_defined native_file_type;
#elif defined(ASIO_HAS_IOCP)
  typedef void* native_file_type;
#elif defined(ASIO_HAS_IO_URING)
  typedef int native_file_type;
#endif
  /// Get the memory page size of the system. Offset needs the be a multiple of this.
  ASIO_DECL static std::size_t page_size();

  /// Get the large memory page size of the system. Offset needs the be a multiple of this.
  ASIO_DECL static std::size_t large_page_size();

  /// Default constructor. Creates an empty mapping.
  ASIO_DECL memory_mapping() {}

#if ASIO_HAS_MOVE
  /// Move-construct a mapping from another.
  memory_mapping(memory_mapping && lhs) : memory_(lhs.memory_), size_(lhs.size_)
  {
    lhs.memory_ = nullptr;
    lhs.size_ = 0;
  }

  /// Move-assign a mapping from another.
  memory_mapping& operator=(memory_mapping && lhs)
  {
    std::swap(memory_, lhs.memory_);
    std::swap(size_, lhs.size_);
    return *this;
  }
#endif
  /// Construct a mapping from an open file.
  /**
   * This constructor create new mapping from a file.
   *
   * If the size is zero it will use the size of the opened file.
   *
   * @param file The basic_file object which shall be mapped into memory
   *
   * @param map_flags The falgs of the mapping
   *
   * @param offset The offset of the mapping. This must be a multiple of the page_size.
   *
   * @param length The length to be mapped after the offset. It does not need to be a multiple of page_size.
   */
  template<typename Executor>
  memory_mapping(basic_file<Executor> & file, flags map_flags = read_write,
                 std::size_t offset = 0u, std::size_t length = 0u)
  {
    open(file, map_flags, offset, length);
  }

  /// Construct a mapping from a file descriptor.
  /**
   * This constructor create new mapping from a file.
   *
   * If the size is zero it will use the size of the opened file.
   *
   * @param file The file descriptor of the file which shall be mapped into memory
   *
   * @param map_flags The falgs of the mapping
   *
   * @param offset The offset of the mapping. This must be a multiple of the page_size.
   *
   * @param length The length to be mapped after the offset. It does not need to be a multiple of page_size.
   */
  explicit memory_mapping(native_file_type file, flags map_flags = read_write,
                          std::size_t offset = 0u, std::size_t length = 0u)
  {
    open(file, map_flags, offset, length);
  }

  /// Open a new a mapping from an open file.
  /**
   * This constructor create new mapping from a file.
   *
   * If the size is zero it will use the size of the opened file.
   * @param file The basic_file object which shall be mapped into memory
   *
   * @param map_flags The falgs of the mapping
   *
   * @param offset The offset of the mapping. This must be a multiple of the page_size.
   *
   * @param length The length to be mapped after the offset. It does not need to be a multiple of page_size.
   */
  template<typename Executor>
  void open(basic_file<Executor> & file, flags map_flags, std::size_t offset = 0u, std::size_t length = 0u)
  {
    error_code ec;
    open(file, map_flags, offset, length, ec);
    asio::detail::throw_error(ec, "open");
  }

  /// Open a new a mapping from an open file.
  /**
   * This constructor create new mapping from a file.
   *
   * If the size is zero it will use the size of the opened file.
   *
   * @param file The file descriptor of the file which shall be mapped into memory
   *
   * @param map_flags The falgs of the mapping
   *
   * @param offset The offset of the mapping. This must be a multiple of the page_size.
   *
   * @param length The length to be mapped after the offset. It does not need to be a multiple of page_size.
   */
  void open(native_file_type file, flags map_flags, std::size_t offset = 0u, std::size_t length = 0u)
  {
    error_code ec;
    open(file, map_flags, offset, length, ec);
    asio::detail::throw_error(ec, "open");
  }

  /// Open a new a mapping from an open file.
  /**
   * This constructor create new mapping from a file.
   *
   * If the size is zero it will use the size of the opened file.
   *
   * @param file The basic_file object which shall be mapped into memory
   *
   * @param map_flags The falgs of the mapping
   *
   * @param offset The offset of the mapping. This must be a multiple of the page_size.
   *
   * @param length The length to be mapped after the offset. It does not need to be a multiple of page_size.\
   *
   * @param ec Error code.
   */
  template<typename Executor>
  void open(basic_file<Executor> & file, flags map_flags,
            std::size_t offset, std::size_t length, error_code & ec) ASIO_NOEXCEPT
  {
    open(file.native_handle(), map_flags, offset, length, ec);
  }

  /// Open a new a mapping from an open file.
  /**
   * This constructor create new mapping from a file.
   *
   * If the size is zero it will use the size of the opened file.
   *
   * @param file The file descriptor of the file which shall be mapped into memory
   *
   * @param map_flags The falgs of the mapping
   *
   * @param offset The offset of the mapping. This must be a multiple of the page_size.
   *
   * @param length The length to be mapped after the offset. It does not need to be a multiple of page_size.
   *
   * @param ec Error code.
   */
  ASIO_DECL void open(native_file_type file, flags map_flags,
            std::size_t offset, std::size_t length, error_code & ec) ASIO_NOEXCEPT;


  /// Close the file.
  /**
   * This function is used to close the mapping. This will free the underlying memory.
   *
   * @throws asio::system_error Thrown on failure.
   */
  ASIO_DECL void close()
  {
    error_code ec;
    close(ec);
    asio::detail::throw_error(ec, "close");
  }
  /// Close the file.
  /**
   * This function is used to close the mapping. This will free the underlying memory.
   *
   * @throws asio::system_error Thrown on failure.
   */
  ASIO_DECL void close(error_code & ec) ASIO_NOEXCEPT;

  /// Destruction of the memory mapping. Frees the memory.
  ~memory_mapping()
  {
    error_code ec;
    close(ec);
  }

  ///Check if the mapping has a mapping internally.
  ASIO_NODISCARD bool is_open() const ASIO_NOEXCEPT {return memory_ != nullptr;}
  ///The size of the mapping.
  ASIO_NODISCARD std::size_t size() const ASIO_NOEXCEPT {return size_;}

  /// Access to the underlying memory for reading & writing.
  ASIO_NODISCARD       void *get()       ASIO_NOEXCEPT {return memory_;}
  /// Access to the underlying memory for writing only..
  ASIO_NODISCARD const void *get() const ASIO_NOEXCEPT {return memory_;}

 private:

  memory_mapping(const memory_mapping&) ASIO_DELETED;
  memory_mapping& operator=(const memory_mapping&) ASIO_DELETED;

  void *      memory_{nullptr};
  std::size_t size_{};
};

/// Open an anonymous memory file.
/** This opens an anonymous file in memory. This file can be passed 
 * to child processes, but not be opened by processes other than that.
 *
 * @tparam Executor Executor of the file
 * @param file The file handle the opened file shall be assigned to.
 * @param max_size_hint The max size hint, which is required on windows.
 * @param large_pages Use large pages for the file.
 * @param ec The error of the operation
 */
template<typename Executor>
ASIO_DECL ASIO_SYNC_OP_VOID open_memory(basic_file<Executor> & file,
                                        std::size_t max_size_hint,
                                        bool large_pages,
                                        error_code & ec) ASIO_NOEXCEPT;

/// Open an anonymous memory file.
/** This opens an anonymous file in memory. This file can be passed 
 * to child processes, but not be opened by processes other than that.
 *
 * @tparam Executor Executor of the file
 * @param file The file handle the opened file shall be assigned to.
 * @param max_size_hint The max size hint, which is required on windows.
 * @param large_pages Use large pages for the file.
 */
template<typename Executor>
ASIO_DECL void open_memory(basic_file<Executor> & file,
                           std::size_t max_size_hint,
                           bool large_pages = false)
{
  error_code ec;
  open_memory(file, max_size_hint, large_pages, ec);
  asio::detail::throw_error(ec, "open");
}

/// Open a shared memory  file.
/** This opens an anonymous file in memory. This file can be passed
 * to child processes, and can be opened by other processes by it's name.
 *
 * @tparam Executor Executor of the file
 * @param file The file handle the opened file shall be assigned to.
 * @param name The name of the file
 * @param map_flags The flags used for the creation of the file
 * @param max_size_hint The max size hint, which is required on windows.
 * @param large_pages Use large pages for the file.
 * @param ec The error of the operation
 */
template<typename Executor>
ASIO_DECL ASIO_SYNC_OP_VOID open_shared_memory(basic_file<Executor> & file,
                                               const char * name,
                                               memory_mapping::flags map_flags,
                                               std::size_t max_size_hint,
                                               error_code & ec) ASIO_NOEXCEPT;

/// Open a shared memory  file.
/** This opens an anonymous file in memory. This file can be passed
 * to child processes, and can be opened by other processes by it's name.
 *
 * @tparam Executor Executor of the file
 * @param file The file handle the opened file shall be assigned to.
 * @param name The name of the file
 * @param map_flags The flags used for the creation of the file
 * @param max_size_hint The max size hint, which is required on windows.
 * @param large_pages Use large pages for the file.
 */
template<typename Executor>
ASIO_DECL void open_shared_memory(basic_file<Executor> & file,
                                  const char * name,
                                  memory_mapping::flags map_flags,
                                  std::size_t max_size_hint)
{
  error_code ec;
  open_shared_memory(file, name, map_flags, max_size_hint, ec);
  asio::detail::throw_error(ec, "open");
}

/// Open a shared memory  file.
/** This opens an anonymous file in memory. This file can be passed
 * to child processes, and can be opened by other processes by it's name.
 *
 * @tparam Executor Executor of the file
 * @param file The file handle the opened file shall be assigned to.
 * @param name The name of the file
 * @param map_flags The flags used for the creation of the file
 * @param max_size_hint The max size hint, which is required on windows.
 * @param large_pages Use large pages for the file.
 * @param ec The error of the operation
 */
template<typename Executor>
ASIO_DECL ASIO_SYNC_OP_VOID open_shared_memory(basic_file<Executor> & file,
                                               const std::string & name,
                                               memory_mapping::flags map_flags,
                                               std::size_t max_size_hint,
                                               error_code & ec) ASIO_NOEXCEPT
{
  open_shared_memory(file.c_str(), name, map_flags, max_size_hint, ec);
}

/// Open a shared memory  file.
/** This opens an anonymous file in memory. This file can be passed
 * to child processes, and can be opened by other processes by it's name.
 *
 * @tparam Executor Executor of the file
 * @param file The file handle the opened file shall be assigned to.
 * @param name The name of the file
 * @param map_flags The flags used for the creation of the file
 * @param max_size_hint The max size hint, which is required on windows.
 * @param large_pages Use large pages for the file.
 */
template<typename Executor>
ASIO_DECL void open_shared_memory(basic_file<Executor> & file,
                                  const std::string & name,
                                  memory_mapping::flags map_flags,
                                  std::size_t max_size_hint)
{
  error_code ec;
  open_shared_memory(file, name, map_flags, max_size_hint, ec);
  asio::detail::throw_error(ec, "open");
}




}
}

#include "asio/detail/pop_options.hpp"


#if defined(ASIO_HEADER_ONLY)
# include "asio/experimental/impl/memory.ipp"
#endif // defined(ASIO_HEADER_ONLY)


#endif //ASIO_MEMORY_HPP
