#pragma once

#include <cstdint>
#include <format>
#include <iomanip>
#include <sstream>
#include <string>

template<typename T>
concept formattable = requires (T & v, std::format_context ctx) {
    std::formatter<std::remove_cvref_t<T>>().format(v, ctx);
};

template<typename T>
std::string toString(const T& val) {
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
