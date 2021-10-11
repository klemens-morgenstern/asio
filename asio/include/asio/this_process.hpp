//
// process/this_process.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ASIO_THIS_PROCESS_HPP
#define ASIO_THIS_PROCESS_HPP

#include <compare>
#include <iomanip>
#include <locale>
#include <string_view>
#include <optional>

#include "asio/detail/config.hpp"

#if defined(ASIO_WINDOWS)
#include "asio/detail/windows_env.hpp"
#else
#include "asio/detail/posix_this_process.hpp"
#endif


#include "asio/detail/push_options.hpp"


namespace asio
{

namespace detail
{
namespace this_process
{

#if defined(ASIO_WINDOWS)
namespace platform = windows;
#else
namespace platform = posix;
#endif

namespace env
{

template< class Traits, class Alloc, class CharT>
inline std::basic_string<CharT,Traits,Alloc> make_char_conv_out(const char * begin, const char * end, const Alloc & alloc, const std::locale &loc, CharT)
{
    std::mbstate_t mb = std::mbstate_t();

    auto& f = std::use_facet<std::codecvt<CharT, char, std::mbstate_t>>(loc);
    const std::size_t len = begin - end;
    std::basic_string<CharT,Traits,Alloc> res(len, char{}, alloc);
    auto itr = begin;
    auto out_itr = res.data();
    f.in(mb, begin, end, itr, res.data(), res.data() + res.size(), out_itr);
    res.resize(out_itr - res.data());
    return res;
}

template< class Traits, class Alloc>
inline std::basic_string<char,Traits,Alloc> make_char_conv_out(const char * begin, const char * end, const Alloc & alloc, const std::locale &loc, char)
{
    return {begin, end, alloc};
}

template<class Traits, class Alloc>
inline std::basic_string<char8_t,Traits,Alloc> make_char_conv_out(const char * begin, const char * end, const Alloc & alloc, const std::locale &loc, char8_t)
{
    return {reinterpret_cast<const char8_t*>(begin), reinterpret_cast<const char8_t*>(end), alloc};
}


template< class Traits, class Alloc, class CharT>
inline std::basic_string<CharT,Traits,Alloc> make_char_conv_out(const wchar_t * begin, const wchar_t * end, const Alloc & alloc, const std::locale &loc, CharT)
{
    std::mbstate_t mb = std::mbstate_t();

    auto& f = std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t>>(loc);
    const std::size_t len = (begin - end) * 2;
    std::string tmp(len, char{}, alloc);
    auto itr = begin;
    auto out_itr = tmp.data();
    f.out(mb, begin, end, itr, tmp.data(), tmp.data() + tmp.size(), out_itr);
    tmp.resize(out_itr - tmp.data());
    return make_char_conv_out(tmp.data(), tmp.data() + tmp.size(), alloc, loc);
}

template< class Traits, class Alloc>
inline std::basic_string<wchar_t,Traits,Alloc> make_char_conv_out(const wchar_t * begin, const wchar_t * end, const Alloc & alloc, const std::locale &loc, wchar_t)
{
    return {begin, end, alloc};
}


template< class Traits, class Alloc, typename CharT>
inline std::string make_char_conv_in(const CharT * begin, const CharT * end, const Alloc & alloc, const std::locale &loc, char)
{
    std::mbstate_t mb = std::mbstate_t();

    auto& f = std::use_facet<std::codecvt<CharT, char, std::mbstate_t>>(loc);
    const std::size_t len = f.length(mb, begin, end, std::numeric_limits<std::size_t>::max());
    std::basic_string<wchar_t, Traits,Alloc> res(len, char{}, alloc);
    auto itr = begin;
    auto out_itr = res.data();
    f.in(mb, begin, end, itr, res.data(), res.data() + res.size(), out_itr);
    res.resize(out_itr - res.data());
    return res;
}

template< class Traits, class Alloc>
inline std::basic_string<char,Traits,Alloc> make_char_conv_in(const char * begin, const char * end, const Alloc & alloc, const std::locale &loc, char)
{
    return {begin, end, alloc};
}

template<class Traits, class Alloc>
inline std::basic_string<char,Traits,Alloc> make_char_conv_in(const char8_t * begin, const char8_t * end,
                                                              const Alloc & alloc, const std::locale &loc, char)
{
    return {reinterpret_cast<const char8_t*>(begin), reinterpret_cast<const char8_t*>(end), alloc};
}

template< class Traits, class Alloc, typename Char, typename TargetChar>
inline std::string make_char_conv_in(const Char* source, const Alloc & alloc, const std::locale &loc, TargetChar tc)
{
    auto end = source;
    for (;*end; end++)
        return make_char_conv_in(source, end, alloc, loc, tc);
}

template< class Traits, class Alloc, typename SourceChar, typename SourceTraits, typename TargetChar>
inline std::string make_char_conv_in(std::basic_string_view<SourceChar, SourceTraits> source,
                                     const Alloc & alloc, const std::locale &loc, TargetChar tc)
{
    return make_char_conv_in(source.data(), source.data() + source.size(), alloc, loc, tc);
}


template< class Traits, class Alloc, typename CharT>
inline std::basic_string<wchar_t,Traits,Alloc> make_char_conv_in(const CharT * begin, const CharT * end, const Alloc & alloc, const std::locale &loc, wchar_t)
{
    auto tmp = make_char_conv_in(begin, end, alloc, loc, char{});

    std::mbstate_t mb = std::mbstate_t();
    auto& f = std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t>>(loc);
    const std::size_t len = f.length(mb, tmp.data(), tmp.data() + tmp.size(), std::numeric_limits<std::size_t>::max());
    std::basic_string<wchar_t,Traits,Alloc> res(len, char{}, alloc);
    auto itr = begin;
    auto out_itr = res.data();
    f.in(mb, begin, end, itr, res.data(), res.data() + res.size(), out_itr);
    res.resize(out_itr - res.data());
    return res;
}

template< class Traits, class Alloc>
inline std::basic_string<wchar_t,Traits,Alloc> make_char_conv_in(const wchar_t * begin, const wchar_t * end, const Alloc & alloc, const std::locale &loc, wchar_t)
{
    return {begin, end, alloc};
}


}
}
}


namespace this_process
{

/// Get the process id of the current process.
#if defined(ASIO_WINDOWS)
#else
using asio::detail::this_process::posix::get_id;
#endif

namespace env
{

#if defined(ASIO_WINDOWS)

using asio::detail::this_process::posix::env::key_char_traits; // needs to be case insensitive
using asio::detail::this_process::posix::env::value_char_traits;
using asio::detail::this_process::posix::env::char_type;

using asio::detail::this_process::posix::env::equality_sign;
using asio::detail::this_process::posix::env::separator;

using asio::detail::this_process::posix::env::native_handle; // this needs to be owning on windows.

#else

using asio::detail::this_process::posix::env::key_char_traits;
using asio::detail::this_process::posix::env::value_char_traits;
using asio::detail::this_process::posix::env::char_type;

using asio::detail::this_process::posix::env::equality_sign;
using asio::detail::this_process::posix::env::separator;

using asio::detail::this_process::posix::env::native_handle; // this needs to be owning on windows.

#endif

struct value_iterator
{
    using string_view_type = std::basic_string_view<char_type, value_char_traits<char_type>>;
    using difference_type   = std::size_t;
    using value_type        = string_view_type ;
    using pointer           = const string_view_type *;
    using reference         = const string_view_type & ;
    using iterator_category = std::forward_iterator_tag;


    value_iterator & operator++()
    {
        const auto init_pos = (current_.data() - view_.data()) + current_.size();
        if (init_pos == view_.size())
        {
            current_ = view_.substr(view_.size());
            return *this;
        }
        const auto val = view_.find( separator, init_pos + 1);
        if (val == string_view_type::npos)
            current_ = view_.substr(init_pos + 1, view_.size() - init_pos);
        else
            current_ = view_.substr(init_pos + 1, (val - init_pos) - 1);
        return *this;
    }

    value_iterator operator++(int)
    {
        auto last = *this;
        ++(*this);
        return last;
    }
    const string_view_type & operator*() const
    {
        return current_;
    }
    const string_view_type * operator->() const
    {
        return &current_;
    }


    value_iterator() = default;
    value_iterator(const value_iterator & ) = default;
    value_iterator(string_view_type view, std::size_t offset = 0u) :
                view_(view)
    {
        auto offst = std::min(offset, view.size());
        auto p = view.find(separator, offst);
        if (p == string_view_type::npos)
            current_ = string_view_type(view.data() + offst);
        else
            current_ = string_view_type(view.data() + offst, p);
    }

    friend bool operator==(const value_iterator & l, const value_iterator & r)
    {
        return l.current_.data() == r.current_.data()
            && l.current_.size() == r.current_.size();
    }
    friend auto operator<=>(const value_iterator & l, const value_iterator & r) -> std::strong_ordering
    {
        auto cmp =  l.current_.compare(r.current_);
        if (cmp)
            return std::strong_ordering::equal;
        else if (cmp > 0 )
            return std::strong_ordering::greater;
        else
            return std::strong_ordering::less;
    }
  private:
    string_view_type view_, current_;
};

struct key_view
{
    using value_type       = char_type;
    using string_view_type = std::basic_string_view<char_type, key_char_traits<char_type>>;

    key_view() noexcept = default;
    key_view( const key_view& p ) = default;
    key_view( key_view&& p ) noexcept = default;
    key_view( string_view_type source ) : value_(source) {}
    key_view( const char_type * p) : value_(p) {}
    key_view(       char_type * p) : value_(p) {}

    ~key_view() = default;

    key_view& operator=( const key_view& p ) = default;
    key_view& operator=( key_view&& p ) noexcept = default;
    key_view& operator=( string_view_type source )
    {
        value_ = source;
        return *this;
    }


    void swap( key_view& other ) noexcept
    {
        std::swap(value_, other.value_);
    }

    string_view_type native() const noexcept {return value_;}

    operator string_view_type() const {return native();}

    int compare( const key_view& p ) const noexcept {return value_.compare(p.value_);}
    int compare( string_view_type str ) const {return value_.compare(str);}
    int compare( const value_type* s ) const {return value_.compare(s);}

    template< class CharT, class Traits = std::char_traits<CharT>,
            class Alloc = std::allocator<CharT> >
    std::basic_string<CharT,Traits,Alloc>
    string( const Alloc& a = Alloc() ) const
    {
        return detail::this_process::env::make_char_conv_out<Traits>(&value_[0], &value_[value_.size()], a, std::locale(), CharT{});
    }

    std::string string() const       {return string<char>();}
    std::wstring wstring() const     {return string<wchar_t>();}
    std::u16string u16string() const {return string<char16_t>();}
    std::u32string u32string() const {return string<char32_t>();}
    std::u8string u8string() const   {return string<char8_t>();}

    bool empty() const {return value_.empty(); }

    friend std::size_t hash_value( const key_view& p ) noexcept
    {
        return std::hash<string_view_type>()(p.value_);
    }
    friend bool operator== (const key_view & l, const key_view & r) = default;
    friend auto operator<=>(const key_view & l, const key_view & r) -> std::strong_ordering
    {
        auto cmp =  l.value_.compare(r.value_);
        if (cmp)
            return std::strong_ordering::equal;
        else if (cmp > 0 )
            return std::strong_ordering::greater;
        else
            return std::strong_ordering::less;
    }

    template< class CharT, class Traits >
    friend std::basic_ostream<CharT,Traits>&
    operator<<( std::basic_ostream<CharT,Traits>& os, const key_view& p )
    {
        os << std::quoted(p.string<CharT,Traits>());
        return os;
    }

    template< class CharT, class Traits >
    friend std::basic_istream<CharT,Traits>&
    operator>>( std::basic_istream<CharT,Traits>& is, key_view& p )
    {
        std::basic_string<CharT, Traits> t;
        is >> std::quoted(t);
        p = t;
        return is;
    }
  private:
    string_view_type value_;
};


struct value_view
{
    using value_type       = char_type;
    using string_view_type = std::basic_string_view<char_type, value_char_traits<char_type>>;

    value_view() noexcept = default;
    value_view( const value_view& p ) = default;
    value_view( value_view&& p ) noexcept = default;
    value_view( string_view_type source ) : value_(source) {}
    value_view( const char_type * p) : value_(p) {}
    value_view(       char_type * p) : value_(p) {}

    ~value_view() = default;

    value_view& operator=( const value_view& p ) = default;
    value_view& operator=( value_view&& p ) noexcept = default;
    value_view& operator=( string_view_type source )
    {
        value_ = source;
        return *this;
    }


    void swap( value_view& other ) noexcept
    {
        std::swap(value_, other.value_);
    }

    string_view_type native() const noexcept {return value_;}

    operator string_view_type() const {return native();}

    int compare( const value_view& p ) const noexcept {return value_.compare(p.value_);}
    int compare( string_view_type str ) const {return value_.compare(str);}
    int compare( const value_type* s ) const {return value_.compare(s);}

    template< class CharT, class Traits = std::char_traits<CharT>,
            class Alloc = std::allocator<CharT> >
    std::basic_string<CharT,Traits,Alloc>
    string( const Alloc& a = Alloc() ) const
    {
        return detail::this_process::env::make_char_conv_out<Traits>(&value_[0], &value_[value_.size()], a, std::locale(), CharT{});
    }

    std::string string() const       {return string<char>();}
    std::wstring wstring() const     {return string<wchar_t>();}
    std::u16string u16string() const {return string<char16_t>();}
    std::u32string u32string() const {return string<char32_t>();}
    std::u8string u8string() const   {return string<char8_t>();}

    bool empty() const {return value_.empty(); }

    friend std::size_t hash_value( const value_view& p ) noexcept
    {
        return std::hash<string_view_type>()(p.value_);
    }

    friend bool operator==(const value_view & l, const value_view & r) = default;
    friend auto operator<=>(const value_view & l, const value_view & r) -> std::strong_ordering
    {
        auto cmp =  l.value_.compare(r.value_);
        if (cmp)
            return std::strong_ordering::equal;
        else if (cmp > 0 )
            return std::strong_ordering::greater;
        else
            return std::strong_ordering::less;
    }

    template< class CharT, class Traits >
    friend std::basic_ostream<CharT,Traits>&
    operator<<( std::basic_ostream<CharT,Traits>& os, const value_view& p )
    {
        os << std::quoted(p.string<CharT,Traits>());
        return os;
    }

    template< class CharT, class Traits >
    friend std::basic_istream<CharT,Traits>&
    operator>>( std::basic_istream<CharT,Traits>& is, value_view& p )
    {
        std::basic_string<CharT, Traits> t;
        is >> std::quoted(t);
        p = t;
        return is;
    }
    value_iterator begin() const {return value_iterator(value_.data());}
    value_iterator   end() const {return value_iterator(value_.data() , value_.size());}

  private:
    string_view_type value_;
};


struct key_value_pair_view
{
    using value_type       = char_type;
    using string_type      = std::basic_string<char_type>;
    using string_view_type = std::basic_string_view<char_type>;

    key_value_pair_view() noexcept = default;
    key_value_pair_view( const key_value_pair_view& p ) = default;
    key_value_pair_view( key_value_pair_view&& p ) noexcept = default;
    //key_value_pair_view( string_view_type source ) : value_(source) {}

    key_value_pair_view( const char_type * p) : value_(p) {}
    key_value_pair_view(       char_type * p) : value_(p) {}


    ~key_value_pair_view() = default;

    key_value_pair_view& operator=( const key_value_pair_view& p ) = default;
    key_value_pair_view& operator=( key_value_pair_view&& p ) noexcept = default;

    void swap( key_value_pair_view& other ) noexcept
    {
        std::swap(value_, other.value_);
    }

    string_view_type native() const noexcept {return value_;}

    operator string_view_type() const {return native();}

    int compare( const key_value_pair_view& p ) const noexcept {return value_.compare(p.value_);}
    int compare( const string_type& str ) const {return value_.compare(str);}
    int compare( string_view_type str ) const {return value_.compare(str);}
    int compare( const value_type* s ) const {return value_.compare(s);}

    template< class CharT, class Traits = std::char_traits<CharT>, class Alloc = std::allocator<CharT> >
    std::basic_string<CharT,Traits,Alloc> string( const Alloc& a = Alloc() ) const
    {
        return detail::this_process::env::make_char_conv_out<Traits>(&value_[0], &value_[value_.size()], a, std::locale(), CharT{});
    }

    std::string string() const       {return string<char>();}
    std::wstring wstring() const     {return string<wchar_t>();}
    std::u16string u16string() const {return string<char16_t>();}
    std::u32string u32string() const {return string<char32_t>();}
    std::u8string u8string() const   {return string<char8_t>();}

    bool empty() const {return value_.empty(); }

    key_view key_view() const
    {
        auto res = native().substr(0, value_.find(equality_sign) );
        return key_view::string_view_type(res.data(), res.size());
    }
    value_view value_view() const
    {
        auto res = native().substr(value_.find(equality_sign)  + 1, string_view_type::npos);
        return value_view::string_view_type(res.data(), res.size());
    }

    friend std::size_t hash_value( const key_value_pair_view& p ) noexcept
    {
        return std::hash<string_view_type>()(p.value_);
    }
    friend bool operator== (const key_value_pair_view & l, const key_value_pair_view & r) = default;
    friend auto operator<=>(const key_value_pair_view & l, const key_value_pair_view & r) -> std::strong_ordering
    {
        auto cmp =  l.value_.compare(r.value_);
        if (cmp)
            return std::strong_ordering::equal;
        else if (cmp > 0 )
            return std::strong_ordering::greater;
        else
            return std::strong_ordering::less;
    }

    template< class CharT, class Traits >
    friend std::basic_ostream<CharT,Traits>&
    operator<<( std::basic_ostream<CharT,Traits>& os, const key_value_pair_view& p )
    {
        os << std::quoted(p.string<CharT,Traits>());
        return os;
    }

    template< class CharT, class Traits >
    friend std::basic_istream<CharT,Traits>&
    operator>>( std::basic_istream<CharT,Traits>& is, key_value_pair_view& p )
    {
        std::basic_string<CharT, Traits> t;
        is >> std::quoted(t);
        p = t;
        return is;
    }

    template<std::size_t Idx>
    auto get() const
    {
        if constexpr (Idx == 0u)
            return key_view();
        else
            return value_view();
    }

  private:
    string_view_type value_;
};


struct key
{
    using value_type       = char_type;
    using string_type      = std::basic_string<char_type, key_char_traits<char_type>>;
    using string_view_type = std::basic_string_view<char_type, key_char_traits<char_type>>;

    key() noexcept = default;
    key( const key& p ) = default;
    key( key&& p ) noexcept = default;
    key( const string_type& source ) : value_(source) {}
    key( string_type&& source ) : value_(std::move(source)) {}
    key( const value_type * raw ) : value_(raw) {}
    key(       value_type * raw ) : value_(raw) {}

    key(key_view kv) : value_(kv) {}

    template< class Source >
    key( const Source& source, const std::locale& loc = std::locale()) : value_(
            detail::this_process::env::make_char_conv_in<std::char_traits<value_type>>(source, std::allocator<void>{}, loc, char_type{}))
    {
    }
    template< class InputIt >
    key( InputIt first, InputIt last, const std::locale& loc = std::locale())
    : key(std::basic_string(first, last), loc)
    {
    }

    ~key() = default;

    key& operator=( const key& p ) = default;
    key& operator=( key&& p ) noexcept = default;
    key& operator=( string_type&& source )
    {
        value_ = std::move(source);
        return *this;
    }
    template< class Source >
    key& operator=( const Source& source )
    {
        value_ = detail::this_process::env::make_char_conv_in<std::char_traits<value_type>>(source, std::locale(), char_type{});
        return *this;
    }

    key& assign( string_type&& source )
    {
        value_ = std::move(source);
        return *this;
    }
    template< class Source >
    key& assign( const Source& source , const std::locale & loc)
    {
        value_ = detail::this_process::env::make_char_conv_in<std::char_traits<value_type>>(source, std::allocator<void>{}, loc, char_type{});
        return *this;
    }

    template< class InputIt >
    key& assign( InputIt first, InputIt last )
    {
        return assign(std::string(first, last));
    }

    void clear() {value_.clear();}

    void swap( key& other ) noexcept
    {
        std::swap(value_, other.value_);
    }

    const value_type* c_str() const noexcept {return value_.c_str();}
    const string_type& native() const noexcept {return value_;}
    string_view_type native_view() const noexcept {return value_;}

    operator string_type() const {return native();}
    operator string_view_type() const {return native_view();}

    int compare( const key& p ) const noexcept {return value_.compare(p.value_);}
    int compare( const string_type& str ) const {return value_.compare(str);}
    int compare( string_view_type str ) const {return value_.compare(str);}
    int compare( const value_type* s ) const {return value_.compare(s);}

    template< class CharT, class Traits = std::char_traits<CharT>,
            class Alloc = std::allocator<CharT> >
    std::basic_string<CharT,Traits,Alloc>
    string( const Alloc& a = Alloc() ) const
    {
        return detail::this_process::env::make_char_conv_out<Traits>(&value_[0], &value_[value_.size()], a, std::locale(), CharT{});
    }

    std::string string() const       {return string<char>();}
    std::wstring wstring() const     {return string<wchar_t>();}
    std::u16string u16string() const {return string<char16_t>();}
    std::u32string u32string() const {return string<char32_t>();}
    std::u8string u8string() const   {return string<char8_t>();}

    bool empty() const {return value_.empty(); }

    friend std::size_t hash_value( const key& p ) noexcept
    {
        return std::hash<string_type>()(p.value_);
    }

    friend bool operator== (const key & l, const key & r) = default;
    friend auto operator<=>(const key & l, const key & r) -> std::strong_ordering
    {
        auto cmp =  l.value_.compare(r.value_);
        if (cmp)
            return std::strong_ordering::equal;
        else if (cmp > 0 )
            return std::strong_ordering::greater;
        else
            return std::strong_ordering::less;
    }

    template< class CharT, class Traits >
    friend std::basic_ostream<CharT,Traits>&
    operator<<( std::basic_ostream<CharT,Traits>& os, const key& p )
    {
        os << std::quoted(p.string<CharT,Traits>());
        return os;
    }

    template< class CharT, class Traits >
    friend std::basic_istream<CharT,Traits>&
    operator>>( std::basic_istream<CharT,Traits>& is, key& p )
    {
        std::basic_string<CharT, Traits> t;
        is >> std::quoted(t);
        p = t;
        return is;
    }

  private:
    string_type value_;
};

struct value
{
    using value_type       = char_type;
    using string_type      = std::basic_string<char_type, value_char_traits<char_type>>;
    using string_view_type = std::basic_string_view<char_type, value_char_traits<char_type>>;

    value() noexcept = default;
    value( const value& p ) = default;
    value( value&& p ) noexcept = default;
    value( const string_type& source ) : value_(source) {}
    value( string_type&& source ) : value_(std::move(source)) {}
    value( const value_type * raw ) : value_(raw) {}
    value(       value_type * raw ) : value_(raw) {}


    value(value_view kv) : value_(kv) {}

    template< class Source >
    value( const Source& source, const std::locale& loc = std::locale()) : value_(detail::this_process::env::make_char_conv_in<std::char_traits<value_type>>(source, std::allocator<void>{}, loc, char_type{}))
    {
    }
    template< class InputIt >
    value( InputIt first, InputIt last, const std::locale& loc = std::locale())
            : value(std::basic_string(first, last), loc)
    {
    }

    ~value() = default;

    value& operator=( const value& p ) = default;
    value& operator=( value&& p ) noexcept = default;
    value& operator=( string_type&& source )
    {
        value_ = std::move(source);
        return *this;
    }
    template< class Source >
    value& operator=( const Source& source )
    {
        value_ = detail::this_process::env::make_char_conv_in<std::char_traits<value_type>>(source, std::locale(), char_type{});
        return *this;
    }

    value& assign( string_type&& source )
    {
        value_ = std::move(source);
        return *this;
    }
    template< class Source >
    value& assign( const Source& source, const std::locale & loc = std::locale() )
    {
        value_ = detail::this_process::env::make_char_conv_in<std::char_traits<value_type>>(source, std::allocator<void>{}, loc, char_type{});
        return *this;
    }

    template< class InputIt >
    value& assign( InputIt first, InputIt last )
    {
        return assign(std::string(first, last));
    }

    void clear() {value_.clear();}

    void swap( value& other ) noexcept
    {
        std::swap(value_, other.value_);
    }

    const value_type* c_str() const noexcept {return value_.c_str();}
    const string_type& native() const noexcept {return value_;}
    string_view_type native_view() const noexcept {return value_;}

    operator string_type() const {return native();}
    operator string_view_type() const {return native_view();}

    int compare( const value& p ) const noexcept {return value_.compare(p.value_);}
    int compare( const string_type& str ) const {return value_.compare(str);}
    int compare( string_view_type str ) const {return value_.compare(str);}
    int compare( const value_type* s ) const {return value_.compare(s);}

    template< class CharT, class Traits = std::char_traits<CharT>,
            class Alloc = std::allocator<CharT> >
    std::basic_string<CharT,Traits,Alloc>
    string( const Alloc& a = Alloc() ) const
    {
        return detail::this_process::env::make_char_conv_out<Traits>(&value_[0], &value_[value_.size()], a, std::locale(), CharT{});
    }

    std::string string() const       {return string<char>();}
    std::wstring wstring() const     {return string<wchar_t>();}
    std::u16string u16string() const {return string<char16_t>();}
    std::u32string u32string() const {return string<char32_t>();}
    std::u8string u8string() const   {return string<char8_t>();}

    bool empty() const {return value_.empty(); }

    friend std::size_t hash_value( const value& p ) noexcept
    {
        return std::hash<string_type>()(p.value_);
    }

    friend bool operator== (const value & l, const value & r) = default;
    friend auto operator<=>(const value & l, const value & r) -> std::strong_ordering
    {
        auto cmp =  l.value_.compare(r.value_);
        if (cmp)
            return std::strong_ordering::equal;
        else if (cmp > 0 )
            return std::strong_ordering::greater;
        else
            return std::strong_ordering::less;
    }

    template< class CharT, class Traits >
    friend std::basic_ostream<CharT,Traits>&
    operator<<( std::basic_ostream<CharT,Traits>& os, const value& p )
    {
        os << std::quoted(p.string<CharT,Traits>());
        return os;
    }

    template< class CharT, class Traits >
    friend std::basic_istream<CharT,Traits>&
    operator>>( std::basic_istream<CharT,Traits>& is, value& p )
    {
        std::basic_string<CharT, Traits> t;
        is >> std::quoted(t);
        p = t;
        return is;
    }

    value_iterator begin() const {return value_iterator(value_.data());}
    value_iterator   end() const {return value_iterator(value_.data(), value_.size());}

  private:
    string_type value_;
};

struct key_value_pair
{
    using value_type       = char_type;
    using string_type      = std::basic_string<char_type>;
    using string_view_type = std::basic_string_view<char_type>;

    key_value_pair() noexcept = default;
    key_value_pair( const key_value_pair& p ) = default;
    key_value_pair( key_value_pair&& p ) noexcept = default;
    key_value_pair(key_view key, value_view value) : value_(key.string() + '=' + value.string()) {}
    key_value_pair( const string_type& source ) : value_(source) {}
    key_value_pair( string_type&& source ) : value_(std::move(source)) {}
    key_value_pair( const value_type * raw ) : value_(raw) {}
    key_value_pair(       value_type * raw ) : value_(raw) {}


    key_value_pair(key_value_pair_view kv) : value_(kv) {}

    template< class Source >
    key_value_pair( const Source& source, const std::locale& loc = std::locale()) :
        value_(detail::this_process::env::make_char_conv_in<std::char_traits<value_type>>(source, std::allocator<void>{}, loc, char_type{}))
    {
    }
    template< class InputIt >
    key_value_pair( InputIt first, InputIt last, const std::locale& loc = std::locale())
            : key_value_pair(std::basic_string(first, last), loc)
    {
    }

    ~key_value_pair() = default;

    key_value_pair& operator=( const key_value_pair& p ) = default;
    key_value_pair& operator=( key_value_pair&& p ) noexcept = default;
    key_value_pair& operator=( string_type&& source )
    {
        value_ = std::move(source);
        return *this;
    }
    template< class Source >
    key_value_pair& operator=( const Source& source )
    {
        value_ = detail::this_process::env::make_char_conv_in<std::char_traits<value_type>>(source, std::locale(), char_type{});
        return *this;
    }

    key_value_pair& assign( string_type&& source )
    {
        value_ = std::move(source);
        return *this;
    }
    template< class Source >
    key_value_pair& assign( const Source& source, const std::locale & loc = std::locale() )
    {
        value_ = detail::this_process::env::make_char_conv_in<std::char_traits<value_type>>(source, std::allocator<void>{}, loc, char_type{});
        return *this;
    }

    template< class InputIt >
    key_value_pair& assign( InputIt first, InputIt last )
    {
        return assign(std::string(first, last));
    }

    void clear() {value_.clear();}

    void swap( key_value_pair& other ) noexcept
    {
        std::swap(value_, other.value_);
    }

    const value_type* c_str() const noexcept {return value_.c_str();}
    const string_type& native() const noexcept {return value_;}
    string_view_type native_view() const noexcept {return value_;}

    operator string_type() const {return native();}
    operator string_view_type() const {return native_view();}

    int compare( const key_value_pair& p ) const noexcept {return value_.compare(p.value_);}
    int compare( const string_type& str ) const {return value_.compare(str);}
    int compare( string_view_type str ) const {return value_.compare(str);}
    int compare( const value_type* s ) const {return value_.compare(s);}

    template< class CharT, class Traits = std::char_traits<CharT>, class Alloc = std::allocator<CharT> >
    std::basic_string<CharT,Traits,Alloc> string( const Alloc& a = Alloc() ) const
    {
        return detail::this_process::env::make_char_conv_out<Traits>(&value_[0], &value_[value_.size()], a, std::locale(), CharT{});
    }

    std::string string() const       {return string<char>();}
    std::wstring wstring() const     {return string<wchar_t>();}
    std::u16string u16string() const {return string<char16_t>();}
    std::u32string u32string() const {return string<char32_t>();}
    std::u8string u8string() const   {return string<char8_t>();}

    bool empty() const {return value_.empty(); }

    key     key() const {return value_.substr(0, value_.find(equality_sign));}
    value value() const {return value_.substr(value_.find(equality_sign) + 1, string_type::npos);}

    key_view     key_view() const {return native_view().substr(0, value_.find(equality_sign));}
    value_view value_view() const {return native_view().substr(value_.find(equality_sign)  + 1, string_view_type::npos);}


    friend std::size_t hash_value( const key_value_pair& p ) noexcept
    {
        return std::hash<string_type>()(p.value_);
    }

    friend bool operator== (const key_value_pair & l, const key_value_pair & r) = default;
    friend auto operator<=>(const key_value_pair & l, const key_value_pair & r) -> std::strong_ordering
    {
        auto cmp =  l.value_.compare(r.value_);
        if (cmp)
            return std::strong_ordering::equal;
        else if (cmp > 0 )
            return std::strong_ordering::greater;
        else
            return std::strong_ordering::less;
    }

    template< class CharT, class Traits >
    friend std::basic_ostream<CharT,Traits>&
    operator<<( std::basic_ostream<CharT,Traits>& os, const key_value_pair& p )
    {
        os << std::quoted(p.string<CharT,Traits>());
        return os;
    }

    template< class CharT, class Traits >
    friend std::basic_istream<CharT,Traits>&
    operator>>( std::basic_istream<CharT,Traits>& is, key_value_pair& p )
    {
        std::basic_string<CharT, Traits> t;
        is >> std::quoted(t);
        p = t;
        return is;
    }

    template<std::size_t Idx>
    auto get() const
    {
        if constexpr (Idx == 0u)
            return key_view();
        else
            return value_view();
    }
  private:

    string_type value_;
};


struct view
{
    using native_handle_type = detail::this_process::platform::env::native_handle;

    view() = default;
    view(view && nt) = default;
    view(native_handle_type native_handle) : handle_(native_handle) {}

    const native_handle_type  & native_handle() { return handle_; }

    struct iterator
    {
        using value_type = key_value_pair_view;
        using iterator_category = std::bidirectional_iterator_tag;

        iterator() = default;
        iterator(const iterator & ) = default;
        iterator(const native_handle_type &native_handle) : iterator_(native_handle) {}

        iterator & operator++()
        {
            ++iterator_;
            return *this;
        }

        iterator operator++(int)
        {
            auto last = *this;
            ++iterator_;
            return last;
        }

        iterator & operator--()
        {
            --iterator_;
            return *this;
        }

        iterator operator--(int)
        {
            auto last = *this;
            --iterator_;
            return last;
        }

        const key_value_pair_view operator*() const
        {
            return key_value_pair_view(*iterator_);
        }

        std::optional<key_value_pair_view> operator->() const
        {
            return key_value_pair_view(*iterator_);
        }

        friend auto operator<=>(const iterator & l, const iterator & r) = default;
      private:
        detail::this_process::platform::env::native_iterator iterator_;
    };

    iterator begin() const {return iterator(handle_);}
    iterator   end() const {return iterator(detail::this_process::platform::env::find_end(handle_));}

  private:
    detail::this_process::platform::env::native_handle handle_{detail::this_process::platform::env::load()};
};

template<typename Environment>
inline std::filesystem::path find_executable(std::basic_string_view<std::filesystem::path::value_type> name,
                                             Environment && env = view())
{
    auto find_key = [&](key_view ky)
                    {
                        return std::find_if(std::begin(env), std::end(env),
                                            [&](key_value_pair vp) {return vp.key_view() == ky;});
                    };

    auto path = find_key("PATH");
#if defined(ASIO_WINDOWS)
    auto pathext = find_key("PATHEXT");
    for (std::filesystem::path pp : path)
        for (std::string st : pathext)
        {
            auto p = st + name;
            error_code ec;
            bool file = std::filesystem::is_regular_file(p, ec);
            if (!ec && file && SHGetFileInfoW(file.native().c_str(), 0,0,0, SHGFI_EXETYPE))
                return p;
        }
#else
    for (std::filesystem::path pp : path)
    {
        auto p = pp / name;
        error_code ec;
        bool file = std::filesystem::is_regular_file(p, ec);
        if (!ec && file && ::access(p.c_str(), X_OK) == 0)
            return p;
    }
#endif
    return {};
}


inline value get(const key & key, error_code & ec)
{
    return ::asio::detail::this_process::platform::env::get(key, ec);
}

inline value get(const key & key)
{
    error_code ec;
    auto res = get(key, ec);
    if (ec)
        detail::throw_error(ec, "env::get");
    return res;
}

inline void set(const key &  key, const value & value, error_code & ec)
{
    detail::this_process::platform::env::set(key.native(), value.native(), ec);
}

inline void set(const key & key, value value)
{
    error_code ec;
    set(key, value, ec);
    if (ec)
        detail::throw_error(ec, "env::set");
}


inline void unset(const key & key, error_code & ec)
{
    detail::this_process::platform::env::unset(std::string(key), ec);
}

inline void unset(const key & key)
{
    error_code ec;
    unset(key, ec);
    if (ec)
        detail::throw_error(ec, "env::unset");
}



}
}
}

namespace std
{

template<>
struct tuple_size<asio::this_process::env::key_value_pair> : integral_constant<std::size_t, 2u> {};

template<>
struct tuple_element<0u, asio::this_process::env::key_value_pair> {using type = asio::this_process::env::key_view;};

template<>
struct tuple_element<1u, asio::this_process::env::key_value_pair> {using type = asio::this_process::env::value_view;};

template<>
struct tuple_size<asio::this_process::env::key_value_pair_view> : integral_constant<std::size_t, 2u> {};

template<>
struct tuple_element<0u, asio::this_process::env::key_value_pair_view> {using type = asio::this_process::env::key_view;};

template<>
struct tuple_element<1u, asio::this_process::env::key_value_pair_view> {using type = asio::this_process::env::value_view;};

template<>
struct hash<asio::this_process::env::key_value_pair>
{
    size_t operator()(const asio::this_process::env::key_value_pair & vp)
    {
        return std::hash<asio::this_process::env::key_value_pair::string_type>()(vp.string());
    }
};

}


#include "asio/detail/pop_options.hpp"

#endif //ASIO_THIS_PROCESS_HPP
