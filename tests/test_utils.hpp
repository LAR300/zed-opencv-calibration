#pragma once

// Minimal test harness: no external dependency (see .ai/specs/aprilgrid-target/decisions.md, D10)

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& testRegistry() {
  static std::vector<TestCase> registry;
  return registry;
}

inline int& testFailures() {
  static int failures = 0;
  return failures;
}

struct TestRegistrar {
  TestRegistrar(const std::string& name, std::function<void()> fn) {
    testRegistry().push_back({name, fn});
  }
};

#define TEST(name)                                       \
  static void name();                                    \
  static TestRegistrar name##_registrar(#name, name);    \
  static void name()

#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond)) {                                                      \
      std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": "     \
                << #cond << std::endl;                                  \
      testFailures()++;                                                 \
    }                                                                   \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                           \
  do {                                                                  \
    double va = (a), vb = (b);                                          \
    if (std::abs(va - vb) > (tol)) {                                    \
      std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": "     \
                << #a << " = " << va << ", expected " << vb << " +/- "  \
                << (tol) << std::endl;                                  \
      testFailures()++;                                                 \
    }                                                                   \
  } while (0)
