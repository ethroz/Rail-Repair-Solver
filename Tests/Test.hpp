#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "../CppProgramming/Utils.hpp"

// Test Things.

namespace {

class TestException : public std::exception {
public:
    TestException(std::string_view msg) : m_msg(msg) {}

    const char* what() const override { return m_msg.c_str(); }

private:
    std::string m_msg;
};

struct TestFixture;

static std::vector<TestFixture*> tests;

struct TestFixture {
public:
    TestFixture(std::string_view name) : func_name(name) {
        tests.push_back(this);
    }

    [[nodiscard]] std::string_view name() const { return func_name; }
    [[nodiscard]] bool passed() const { return !failed; }

    virtual void runTest() = 0;

protected:
    const std::string_view func_name;
    bool failed = false;
};

}

// Defines

#define TEST(x) struct Fixture_##x : public TestFixture { \
public: Fixture_##x(std::string_view name) : TestFixture(name) {} \
void runTest() override; \
}; \
static Fixture_##x fix_##x(#x); \
void Fixture_##x::runTest()

#define EXPECT_EQ(x, y) if (x != y) { failed = true; std::cerr << std::format("Failed expect on line {}\n{} != {}", __LINE__, toString(x), toString(y)) << std::endl; }
#define EXPECT_TRUE(x) if (!x) { failed = true; std::cerr << std::format("Failed expect on line {}\n{} is false", __LINE__, toString(x)) << std::endl; }
#define EXPECT_FALSE(x) if (x) { failed = true; std::cerr << std::format("Failed expect on line {}\n{} is true", __LINE__, toString(x)) << std::endl; }
#define ASSERT_EQ(x, y) if (x != y) { failed = true; throw TestException(std::format("Failed assert on line {}\n{} != {}", __LINE__, toString(x), toString(y))); }

// Main function.

int main() {
    std::cout << "Running All Tests\n" << std::endl;

    std::vector<std::string_view> failedTests;
    for (const auto& test : tests) {
        std::cout << std::format("Running {}", test->name()) << std::endl;

        try {
            test->runTest();
        }
        catch (const TestException& e) {
            std::cerr << e.what() << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Test threw exception: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Test threw unknown exception" << std::endl;
        }

        if (test->passed()) {
            std::cout << std::format("Done {}\n", test->name()) << std::endl;
        }
        else {
            failedTests.push_back(test->name());
            std::cout << std::format("Failed {}\n", test->name()) << std::endl;
        }
    }

    if (failedTests.empty()) {
        std::cout << "All Tests Passed" << std::endl;
    }
    else {
        std::cout << std::format("{} Test{} Failed:", failedTests.size(), failedTests.size() == 1 ? "" : "s") << std::endl;
        for (const auto& failedTest : failedTests) {
            std::cerr << failedTest << std::endl;
        }
    }

    return 0;
}
