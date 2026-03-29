#pragma once

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(assume)
#define WP_ASSUME(expr) [[assume(expr)]]
#endif
#endif

#ifndef WP_ASSUME
#if defined(_MSC_VER)
#define WP_ASSUME(expr) __assume(expr)
#elif defined(__clang__) || defined(__GNUC__)
#define WP_ASSUME(expr)                 \
  do {                                  \
    if (!(expr)) {                      \
      __builtin_unreachable();          \
    }                                   \
  } while (false)
#else
#define WP_ASSUME(expr) ((void)0)
#endif
#endif
