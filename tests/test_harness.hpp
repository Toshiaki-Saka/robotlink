#pragma once
// Minimal dependency-free test harness for RobotLink.
// Usage:
//   TEST("name") { CHECK(cond); CHECK_NEAR(a, b, tol); }
//   int main() { return robotlink_test::run_all(); }

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace robotlink_test {

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failure_count() {
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline void report_failure(const char* file, int line, const std::string& msg) {
    ++failure_count();
    std::printf("    FAIL %s:%d  %s\n", file, line, msg.c_str());
}

inline int run_all() {
    int failed_cases = 0;
    for (const auto& c : registry()) {
        const int before = failure_count();
        std::printf("[ RUN  ] %s\n", c.name.c_str());
        c.fn();
        if (failure_count() == before) {
            std::printf("[  OK  ] %s\n", c.name.c_str());
        } else {
            std::printf("[ FAIL ] %s\n", c.name.c_str());
            ++failed_cases;
        }
    }
    std::printf("\n%d/%zu test case(s) passed.\n",
                static_cast<int>(registry().size()) - failed_cases,
                registry().size());
    return failed_cases == 0 ? 0 : 1;
}

} // namespace robotlink_test

// ── Macros ───────────────────────────────────────────────────────────────────
#define ROBOTLINK_CONCAT_(a, b) a##b
#define ROBOTLINK_CONCAT(a, b)  ROBOTLINK_CONCAT_(a, b)

#define TEST(NAME)                                                            \
    static void ROBOTLINK_CONCAT(robotlink_test_fn_, __LINE__)();              \
    static ::robotlink_test::Registrar ROBOTLINK_CONCAT(robotlink_test_reg_,   \
        __LINE__){NAME, &ROBOTLINK_CONCAT(robotlink_test_fn_, __LINE__)};      \
    static void ROBOTLINK_CONCAT(robotlink_test_fn_, __LINE__)()

#define CHECK(COND)                                                           \
    do {                                                                      \
        if (!(COND))                                                          \
            ::robotlink_test::report_failure(__FILE__, __LINE__,              \
                "CHECK(" #COND ")");                                          \
    } while (0)

#define CHECK_NEAR(A, B, TOL)                                                 \
    do {                                                                      \
        const double a_ = (A), b_ = (B), tol_ = (TOL);                        \
        if (std::fabs(a_ - b_) > tol_) {                                      \
            char buf_[256];                                                   \
            std::snprintf(buf_, sizeof(buf_),                                 \
                "CHECK_NEAR(" #A ", " #B ", " #TOL "): %.3e vs %.3e (|d|=%.3e)",\
                a_, b_, std::fabs(a_ - b_));                                   \
            ::robotlink_test::report_failure(__FILE__, __LINE__, buf_);       \
        }                                                                     \
    } while (0)
