#pragma once

#include <expected>
#include <version>

namespace wallpaper::cpp26 {

#if defined(__cpp_if_consteval) && __cpp_if_consteval >= 202106L
inline constexpr bool kIfConsteval = true;
#else
inline constexpr bool kIfConsteval = false;
#endif

#if defined(__cpp_consteval) && __cpp_consteval >= 201811L
inline constexpr bool kConsteval = true;
#else
inline constexpr bool kConsteval = false;
#endif

#if defined(__cpp_explicit_this_parameter) && __cpp_explicit_this_parameter >= 202110L
inline constexpr bool kDeducingThis = true;
#else
inline constexpr bool kDeducingThis = false;
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
inline constexpr bool kExpected = true;
#else
inline constexpr bool kExpected = false;
#endif

#if defined(__cpp_lib_mdspan) && __cpp_lib_mdspan >= 202207L
inline constexpr bool kMdspan = true;
#else
inline constexpr bool kMdspan = false;
#endif

#if defined(__cpp_pattern_matching)
inline constexpr bool kPatternMatching = true;
#else
inline constexpr bool kPatternMatching = false;
#endif

#if defined(__cpp_static_reflection)
inline constexpr bool kStaticReflection = true;
#else
inline constexpr bool kStaticReflection = false;
#endif

}  // namespace wallpaper::cpp26
