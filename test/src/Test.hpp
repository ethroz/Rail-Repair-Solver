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
    TestFixture(std::string_view name, std::string file) :
        m_name{name},
        m_file{std::move(file)}
    {
        tests.push_back(this);
    }

    [[nodiscard]] std::string_view name() const { return m_name; }
    [[nodiscard]] bool passed() const { return !m_failed; }

    virtual void run() final {
        try {
            runImpl();
        }
        catch (const TestException& e) {
            std::cerr << e.what() << std::endl;
        }
        catch (const std::exception& e) {
            m_failed = true;
            std::cerr << "Test threw exception: " << e.what() << std::endl;
            std::cerr << m_file << std::endl;
        }
        catch (...) {
            m_failed = true;
            std::cerr << "Test threw unknown exception" << std::endl;
            std::cerr << m_file << std::endl;
        }
    }

protected:
    virtual void runImpl() = 0;
    
    bool m_failed = false;

private:
    const std::string_view m_name;
    const std::string m_file;
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
public: Fixture_##x() : TestFixture(#x, std::format("{}:{}", __FILE__, __LINE__)) {} \
void runImpl() override; \
}; \
static Fixture_##x fix_##x{}; \
void Fixture_##x::runImpl()

#define EXPECT_EQ(x, y) if (x != y) { m_failed = true; std::cerr << std::format("Failed at {}:{}\n'{}' != '{}'\n{} != {}", __FILE__, __LINE__, #x, #y, toString(x), toString(y)) << std::endl; }
#define EXPECT_NE(x, y) if (x == y) { m_failed = true; std::cerr << std::format("Failed at {}:{}\n'{}' == '{}'\n{} == {}", __FILE__, __LINE__, #x, #y, toString(x), toString(y)) << std::endl; }
#define EXPECT_LT(x, y) if (x >= y) { m_failed = true; std::cerr << std::format("Failed at {}:{}\n'{}' >= '{}'\n{} >= {}", __FILE__, __LINE__, #x, #y, toString(x), toString(y)) << std::endl; }
#define EXPECT_LE(x, y) if (x >  y) { m_failed = true; std::cerr << std::format("Failed at {}:{}\n'{}' > '{}'\n{} > {}",   __FILE__, __LINE__, #x, #y, toString(x), toString(y)) << std::endl; }
#define EXPECT_GT(x, y) if (x <= y) { m_failed = true; std::cerr << std::format("Failed at {}:{}\n'{}' <= '{}'\n{} <= {}", __FILE__, __LINE__, #x, #y, toString(x), toString(y)) << std::endl; }
#define EXPECT_GE(x, y) if (x <  y) { m_failed = true; std::cerr << std::format("Failed at {}:{}\n'{}' < '{}'\n{} < {}",   __FILE__, __LINE__, #x, #y, toString(x), toString(y)) << std::endl; }
#define EXPECT_TRUE(x)  if (!x) { m_failed = true; std::cerr << std::format("Failed at {}:{}\n'{}' is false", __FILE__, __LINE__, #x) << std::endl; }
#define EXPECT_FALSE(x) if (x) { m_failed = true; std::cerr << std::format("Failed at {}:{}\n'{}' is true",   __FILE__, __LINE__, #x) << std::endl; }
#define ASSERT_EQ(x, y) if (x != y) { m_failed = true; throw TestException(std::format("Failed assert at {}:{}\n'{}' != '{}'\n{} != {}", __FILE__, __LINE__, #x, #y, toString(x), toString(y))); }
#define ASSERT_NE(x, y) if (x == y) { m_failed = true; throw TestException(std::format("Failed assert at {}:{}\n'{}' == '{}'\n{} == {}", __FILE__, __LINE__, #x, #y, toString(x), toString(y))); }
#define ASSERT_LT(x, y) if (x >= y) { m_failed = true; throw TestException(std::format("Failed assert at {}:{}\n'{}' >= '{}'\n{} >= {}", __FILE__, __LINE__, #x, #y, toString(x), toString(y))); }
#define ASSERT_LE(x, y) if (x >  y) { m_failed = true; throw TestException(std::format("Failed assert at {}:{}\n'{}' > '{}'\n{} > {}",   __FILE__, __LINE__, #x, #y, toString(x), toString(y))); }
#define ASSERT_GT(x, y) if (x <= y) { m_failed = true; throw TestException(std::format("Failed assert at {}:{}\n'{}' <= '{}'\n{} <= {}", __FILE__, __LINE__, #x, #y, toString(x), toString(y))); }
#define ASSERT_GE(x, y) if (x <  y) { m_failed = true; throw TestException(std::format("Failed assert at {}:{}\n'{}' < '{}'\n{} < {}",   __FILE__, __LINE__, #x, #y, toString(x), toString(y))); }
#define ASSERT_TRUE(x)  if (!x) { m_failed = true; throw TestException(std::format("Failed assert at {}:{}\n'{}' is false", __FILE__, __LINE__, #x)); }
#define ASSERT_FALSE(x) if (x) { m_failed = true; throw TestException(std::format("Failed assert at {}:{}\n'{}' is true",   __FILE__, __LINE__, #x)); }
#define FAIL() { m_failed = true; throw TestException(std::format("Failed at {}:{}", __FILE__, __LINE__)); }
#define EXPECT_THROW(x, ex) try { (void)x; m_failed = true; std::cerr << std::format("Failed at {}:{}\n'{}' did not throw '{}'", __FILE__, __LINE__, #x, #ex) << std::endl; } catch (const ex&) {}

// Main function.

int main(int argc, char* argv[]) {
    rlutil::saveDefaultColor();

    const int numArgs = argc - 1;
    std::vector<std::string_view> enabled(numArgs);
    for (int i = 0; i < numArgs; ++i) {
        auto testName = std::string_view(argv[i + 1]);
        if (std::find_if(tests.begin(), tests.end(), [&](auto&& test){
                return test->name() == testName;
            }) == tests.end()) {
            std::cerr << "Test does not exist: " << testName << std::endl;
                return 1;
        }
        enabled[i] = std::move(testName);
    }

    if (enabled.empty() || enabled.size() == tests.size()) {
        std::cout << "Running All Tests\n" << std::endl;
    }

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
        if (!enabled.empty() && std::find(enabled.begin(), enabled.end(), test->name()) == enabled.end()) {
            continue;
        }

        std::cout << std::format("Running {}", test->name()) << std::endl;
        test->run();

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
