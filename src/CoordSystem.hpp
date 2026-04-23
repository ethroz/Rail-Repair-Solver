#pragma once

#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

enum Direction : uint8_t {
    MIN_DIR = 0,
    RIGHT = 0,
    DOWN = 1,
    LEFT = 2,
    UP = 3,
    MAX_DIR = 4,
    NONE = 5
};

char toChar(Direction dir) {
    switch (dir) {
    case UP:     return 'u';
    case RIGHT:  return 'r';
    case DOWN:   return 'd';
    case LEFT:   return 'l';
    default: throw std::invalid_argument(std::format("invalid dir: {}", std::to_underlying(dir)));
    }
}

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

    constexpr Position(Direction dir) {
        switch (dir) {
        case UP:    y = -1; break;
        case RIGHT: x = 1;  break;
        case DOWN:  y = 1;  break;
        case LEFT:  x = -1; break;
        case NONE:          break;
        default: throw std::invalid_argument(std::format("invalid dir: {}", std::to_underlying(dir)));
        }
    }

    constexpr Position& operator+=(Position o) { x += o.x; y += o.y; return *this; }
    friend inline constexpr [[nodiscard]] Position operator+(Position a, Position b) { return Position(a.x + b.x, a.y + b.y); }
    friend inline constexpr [[nodiscard]] bool operator==(Position a, Position b) { return a.x == b.x && a.y == b.y; }
    friend inline constexpr [[nodiscard]] bool operator!=(Position a, Position b) { return a.x != b.x || a.y != b.y; }

    int8_t x = 0;
    int8_t y = 0;
};
