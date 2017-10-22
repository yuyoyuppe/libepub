#pragma once

#include <string>
#include <algorithm>
#include <string_view>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <queue>
#include <vector>
#include <tuple>

namespace libepub {
namespace detail {

template <class Iter, class T, class Compare>
inline Iter binary_find(Iter begin, Iter end, T val, Compare comp)
{
  Iter i = lower_bound(begin, end, val, comp);
  return (i != end && !(comp(val, *i))) ? i : end;
}

inline bool endsWith(std::string const & string, std::string const & pattern)
{
  return equal(crbegin(pattern), crend(pattern), crbegin(string));
}

inline std::string toLower(std::string s)
{
  transform(cbegin(s), cend(s), begin(s), ::tolower);
  return s;
}

inline std::string trim(std::string const & s, char const * const chars = "\n\t\r ")
{
  std::size_t const start = s.find_first_not_of(chars);
  return start == std::string::npos ? s : s.substr(start, s.find_last_not_of(chars) - start + 1);
}

template <unsigned idx>
inline auto tupleElementSorter()
{
  return [](auto const & lhs, auto const & rhs) {
    auto const & a = std::get<idx>(lhs);
    auto const & b = std::get<idx>(rhs);
    return lexicographical_compare(cbegin(a), cend(a), cbegin(b), cend(b));
  };
}

template <typename DelimT>
inline std::pair<std::string, std::string> splitToPair(std::string const & str, DelimT const && delim)
{
  std::size_t const equalPos = str.find(delim);
  return std::make_pair(str.substr(0, equalPos), equalPos == std::string::npos ? "" : str.substr(equalPos + 1));
}

static char const * const fatal_error_prefix = "[FATAL]: ";
inline void               fatal(std::string msg, ...)
{
  ::va_list args;
  va_start(args, msg);

  std::size_t constexpr max_buffer_size = 256;
  char buffer[max_buffer_size];
  ::strncpy(buffer, fatal_error_prefix, max_buffer_size);

  std::size_t const prefix_len = std::strlen(buffer);
  ::vsnprintf(buffer + prefix_len, max_buffer_size - prefix_len, msg.c_str(), args);

  va_end(args);

  throw std::runtime_error(buffer);
}
} // namespace detail
} // namespace libepub
