#pragma once

#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace test {
struct Case final {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<Case>& Registry() {
  static std::vector<Case> cases;
  return cases;
}

inline void Register(const std::string& name, std::function<void()> fn) {
  Registry().push_back(Case{name, std::move(fn)});
}

inline void Expect(bool cond, const std::string& message) {
  if (!cond) {
    throw std::runtime_error(message);
  }
}
}  // namespace test

#define TEST_CASE(name)                                                  \
  static void name();                                                    \
  namespace {                                                            \
  struct name##_registrar final {                                        \
    name##_registrar() { test::Register(#name, name); }                  \
  } name##_registrar_instance;                                           \
  }                                                                      \
  static void name()

#define EXPECT_TRUE(cond) test::Expect((cond), "EXPECT_TRUE failed: " #cond)
#define EXPECT_EQ(a, b) test::Expect(((a) == (b)), "EXPECT_EQ failed: " #a " vs " #b)
