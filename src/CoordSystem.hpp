#pragma once

#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

enum DIRECTION : uint8_t {
    NONE = 0,
    RIGHT = 1,
    DOWN = 2,
    LEFT = 3,
    UP = 4,
    MIN_DIR = 1,
    MAX_DIR = 4,
};

struct Direction {
public:
    constexpr Direction(const DIRECTION& d = NONE) : m_dir{d} {}
    constexpr Direction(uint8_t v) : Direction(DIRECTION(v)) {}

    [[nodiscard]] constexpr explicit operator char() const {
        switch (m_dir) {
        case 1: return 'r';
        case 2: return 'd';
        case 3: return 'l';
        case 4: return 'u';
        default: throw std::invalid_argument(std::format("Invalid dir value for char: {}", uint8_t(m_dir)));
        }
    }

    [[nodiscard]] constexpr operator uint8_t() const { return uint8_t(m_dir); }

private:
    DIRECTION m_dir;
};

enum TrackType : uint8_t {
    NW = 0,
    NE = 1,
    SE = 2,
    SW = 3,
    H = 4,
    V = 5,
};

struct Position {
    constexpr Position() {}
    constexpr Position(int8_t _x, int8_t _y) : x{ _x }, y{ _y } {}
    constexpr Position(const Direction& dir) {
        switch (dir) {
        case NONE:          break;
        case RIGHT: x =  1; break;
        case DOWN:  y =  1; break;
        case LEFT:  x = -1; break;
        case UP:    y = -1; break;
        default: throw std::invalid_argument(std::format("Invalid dir value: {}", uint8_t(dir)));
        }
    }

    constexpr operator Direction() const {
        switch (x) {
        case -1: return LEFT;
        case  1: return RIGHT;
        default: break;
        }
        switch (y) {
        case -1: return UP;
        case  1: return DOWN;
        default: return NONE;
        }
    }

    constexpr Position& operator+=(Position o) { x += o.x; y += o.y; return *this; }
    friend inline constexpr [[nodiscard]] Position operator+(Position a, Position b) { return Position(a.x + b.x, a.y + b.y); }
    friend inline constexpr [[nodiscard]] Position operator-(Position a, Position b) { return Position(a.x - b.x, a.y - b.y); }
    friend inline constexpr [[nodiscard]] bool operator==(Position a, Position b) { return a.x == b.x && a.y == b.y; }
    friend inline constexpr [[nodiscard]] bool operator!=(Position a, Position b) { return a.x != b.x || a.y != b.y; }

    int8_t x = 0;
    int8_t y = 0;
};
