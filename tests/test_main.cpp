#include <exception>
#include <iostream>

#include "test_support.h"

namespace test {

int RunAll() {
  int failed = 0;
  for (const auto& tc : Registry()) {
    try {
      tc.fn();
      std::cout << "[PASS] " << tc.name << "\n";
    } catch (const std::exception& ex) {
      ++failed;
      std::cout << "[FAIL] " << tc.name << " -> " << ex.what() << "\n";
    }
  }

  std::cout << "Total: " << Registry().size() << ", Failed: " << failed << "\n";
  return failed == 0 ? 0 : 1;
}

}  // namespace test

int main() { return test::RunAll(); }
