#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <rlutil.h>

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

template<typename T>
concept formattable = requires (T & v, std::format_context ctx) {
    std::formatter<std::remove_cvref_t<T>>().format(v, ctx);
};

template<typename T>
inline std::string toString(const T& val) {
    if constexpr (formattable<T>) {
        return std::format("{}", val);
    }
    else {
        std::stringstream ss;
        ss << "0x" << std::hex;
        for (int i = 0; i < sizeof(T); ++i) {
            ss << std::setw(2) << std::setfill('0') << (int)reinterpret_cast<const uint8_t*>(&val)[i];
        }
        return ss.str();
    }
}

using ColorType = decltype(rlutil::WHITE);

struct ColorScope {
    ColorScope(ColorType color) { rlutil::setColor(color); }
    ~ColorScope() { rlutil::resetColor(); }
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

    rlutil::saveDefaultColor();

    const auto pass = []() -> std::ostream & {
        const auto scope = ColorScope(rlutil::GREEN);
        return std::cout << "Pass";
    };

    const auto fail = []() -> std::ostream& {
        const auto scope = ColorScope(rlutil::RED);
        return std::cout << "Fail";
    };

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

        (test->passed() ? pass() : fail()) << std::format(" {}\n", test->name()) << std::endl;
        if (!test->passed()) {
            failedTests.push_back(test->name());
        }
    }

    if (failedTests.empty()) {
        const auto scope = ColorScope(rlutil::GREEN);
        std::cout << "All Tests Passed" << std::endl;
    }
    else {
        const auto scope = ColorScope(rlutil::RED);
        std::cout << std::format("{} Test{} Failed:", failedTests.size(), failedTests.size() == 1 ? "" : "s") << std::endl;
        for (const auto& failedTest : failedTests) {
            std::cerr << failedTest << std::endl;
        }
    }

    return 0;
}
