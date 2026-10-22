#include "test_utils.hpp"

int main() {
  int failed_tests = 0;
  for (auto& test : testRegistry()) {
    int failures_before = testFailures();
    test.fn();
    bool passed = testFailures() == failures_before;
    if (!passed) failed_tests++;
    std::cout << (passed ? "[ OK ] " : "[FAIL] ") << test.name << std::endl;
  }
  std::cout << std::endl
            << testRegistry().size() - failed_tests << "/"
            << testRegistry().size() << " tests passed" << std::endl;
  return failed_tests == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
