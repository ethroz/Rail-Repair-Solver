#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "State.hpp"

class Grid {
public:
    constexpr Grid() = default;

    constexpr const Cell& at(const Position& p) const { return m_data[p.y()][p.x()]; }
    constexpr Cell& at(const Position& p) { return m_data[p.y()][p.x()]; }
    constexpr const Cell& at(uint8_t x, uint8_t y) const { return m_data[y][x]; }
    constexpr Cell& at(uint8_t x, uint8_t y) { return m_data[y][x]; }

    template<typename S>
    constexpr auto at(const S& state, const Position& p) const {
        auto res = state.at(p, objectCount);
        if (res.cell == NOTHING) {
            res.cell = at(p);
        }
        return res;
    }

    constexpr Position find(const Cell& cell) const {
        for (uint8_t y = 0; y < height; ++y) {
            for (uint8_t x = 0; x < width; ++x) {
                if (at(x, y) == cell) {
                    return {x, y};
                }
            }
        }
        return INVALID_POS;
    }

    constexpr bool exits(const Vector& v) const {
        return (v.pos.y() == 0 && v.dir == UP) ||
               (v.pos.x() == width - 1 && v.dir == RIGHT) ||
               (v.pos.y() == height - 1 && v.dir == DOWN) ||
               (v.pos.x() == 0 && v.dir == LEFT);
    }

    template<typename S>
    constexpr std::string toString(const S& state) const {
        const uint8_t lineWidth = width + 1;
        std::string out(lineWidth * height, '\0');
        for (uint8_t y = 0; y < height; ++y) {
            uint8_t x = 0;
            for (; x < width; ++x) {
                out[y * lineWidth + x] = char(at(state, {x, y}).cell);
            }
            out[y * lineWidth + x] = '\n';
        }
        return out;
    }

    uint8_t width = 0;
    uint8_t height = 0;
    uint8_t objectCount = 0;
private:
    std::array<std::array<Cell, X_MAX>, Y_MAX> m_data{};
};
