#ifndef ASIO_WINDOW_TRAITS_HPP
#define ASIO_WINDOW_TRAITS_HPP


template <>
struct char_traits<char>
{
    typedef char      char_type;
    typedef int       int_type;
    typedef streamoff off_type;
    typedef streampos pos_type;
    typedef mbstate_t state_type;

    static inline
    void assign(char_type& __c1, const char_type& __c2) _NOEXCEPT {__c1 = __c2;}
    static inline  bool eq(char_type __c1, char_type __c2) _NOEXCEPT
    {return __c1 == __c2;}
    static inline  bool lt(char_type __c1, char_type __c2) _NOEXCEPT
    {return (unsigned char)__c1 < (unsigned char)__c2;}

    static
    int compare(const char_type* __s1, const char_type* __s2, size_t __n) _NOEXCEPT;
    static inline size_t
    length(const char_type* __s)  _NOEXCEPT {return __builtin_strlen(__s);}
    static
    const char_type* find(const char_type* __s, size_t __n, const char_type& __a) _NOEXCEPT;
    static inline 
    char_type* move(char_type* __s1, const char_type* __s2, size_t __n) _NOEXCEPT
    {
        return __libcpp_is_constant_evaluated()
               ? _VSTD::__move_constexpr(__s1, __s2, __n)
               : __n == 0 ? __s1 : (char_type*)_VSTD::memmove(__s1, __s2, __n);
    }
    static inline 
    char_type* copy(char_type* __s1, const char_type* __s2, size_t __n) _NOEXCEPT
    {
        _LIBCPP_ASSERT(__s2 < __s1 || __s2 >= __s1+__n, "char_traits::copy overlapped range");
        return __libcpp_is_constant_evaluated()
               ? _VSTD::__copy_constexpr(__s1, __s2, __n)
               : __n == 0 ? __s1 : (char_type*)_VSTD::memcpy(__s1, __s2, __n);
    }
    static inline 
    char_type* assign(char_type* __s, size_t __n, char_type __a) _NOEXCEPT
    {
        return __libcpp_is_constant_evaluated()
               ? _VSTD::__assign_constexpr(__s, __n, __a)
               : __n == 0 ? __s : (char_type*)_VSTD::memset(__s, to_int_type(__a), __n);
    }

    static inline  int_type  not_eof(int_type __c) _NOEXCEPT
    {return eq_int_type(__c, eof()) ? ~eof() : __c;}
    static inline  char_type to_char_type(int_type __c) _NOEXCEPT
    {return char_type(__c);}
    static inline  int_type to_int_type(char_type __c) _NOEXCEPT
    {return int_type((unsigned char)__c);}
    static inline  bool eq_int_type(int_type __c1, int_type __c2) _NOEXCEPT
    {return __c1 == __c2;}
    static inline  int_type  eof() _NOEXCEPT
    {return int_type(EOF);}
};

constexpr char equality_sign = ':';
constexpr char separator = ';';


#endif //ASIO_WINDOW_TRAITS_HPP
