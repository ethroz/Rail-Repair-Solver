#pragma once

#include <cassert>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

constexpr uint8_t X_MAX = 10;
constexpr uint8_t Y_MAX = 10;
constexpr uint64_t TOTAL = X_MAX * Y_MAX;
constexpr uint8_t X_RANGE = X_MAX - 2;
constexpr uint8_t Y_RANGE = Y_MAX - 2;
constexpr uint64_t BASE = X_RANGE * Y_RANGE;

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
    constexpr Direction(const DIRECTION& d = NONE) : m_dir{d} {}

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

class Position {
public:
    constexpr Position() = default;
    constexpr Position(uint8_t _x, uint8_t _y) : m_bits((_y << 4) | (_x & 0xF)) { assert(_x <= 0XF && _y <= 0XF); }
    
    [[nodiscard]] friend constexpr Position operator+(Position a, Position b) { return Position(a.x() + b.x(), a.y() + b.y()); }
    constexpr Position& operator+=(Position o) {
        Position p = *this + o;
        m_bits = p.m_bits;
        return *this;
    }
    [[nodiscard]] friend constexpr Position operator+(Position p, Direction d) {
        switch(d) {
        case RIGHT: return Position(p.x() + 1, p.y()    );
        case DOWN:  return Position(p.x(),     p.y() + 1);
        case LEFT:  return Position(p.x() - 1, p.y()    );
        case UP:    return Position(p.x(),     p.y() - 1);
        default:    return p;
        }
    }
    constexpr Position& operator+=(Direction d) {
        Position p = *this + d;
        m_bits = p.m_bits;
        return *this;
    }
    [[nodiscard]] friend constexpr Position operator-(Position p, Direction d) {
        switch(d) {
        case RIGHT: return Position(p.x() - 1, p.y()    );
        case DOWN:  return Position(p.x(),     p.y() - 1);
        case LEFT:  return Position(p.x() + 1, p.y()    );
        case UP:    return Position(p.x(),     p.y() + 1);
        default:    return p;
        }
    }
    constexpr Position& operator-=(Direction d) {
        Position p = *this - d;
        m_bits = p.m_bits;
        return *this;
    }

    static constexpr Direction diffStep(const Position& from, const Position& to) {
        const int8_t dx = int8_t(to.x()) - int8_t(from.x());
        const int8_t dy = int8_t(to.y()) - int8_t(from.y());
        assert(((dx == -1 || dx == 1) && dy == 0) || ((dy == -1 || dy == 1) && dx == 0) || (dx == 0 && dy == 0));
        switch(dx) {
        case -1: return LEFT;
        case  1: return RIGHT;
        default: break;
        }
        switch(dy) {
        case -1: return UP;
        case  1: return DOWN;
        default: return NONE;
        }
    }

    static constexpr uint8_t distance(const Position& from, const Position& to) {
        const uint8_t fx = from.x();
        const uint8_t fy = from.y();
        const uint8_t tx = to.x();
        const uint8_t ty = to.y();
        const uint8_t dx = fx > tx ? fx - tx : tx - fx;
        const uint8_t dy = fy > ty ? fy - ty : ty - fy;
        return dx + dy;
    }

    [[nodiscard]] friend constexpr bool operator==(const Position& a, const Position& b) { return a.m_bits == b.m_bits; }
    [[nodiscard]] friend constexpr bool operator!=(const Position& a, const Position& b) { return a.m_bits != b.m_bits; }
    [[nodiscard]] friend constexpr bool operator<(const Position& a, const Position& b) { return a.m_bits < b.m_bits; }
    
    constexpr uint8_t x() const { return m_bits & 0xF; }
    constexpr uint8_t y() const { return m_bits >> 4; }
    constexpr void x(uint8_t _x) {
        assert(_x <= 0XF);
        m_bits = (m_bits & 0xF0) | (_x & 0xF);
    }
    constexpr void y(uint8_t _y) {
        assert(_y <= 0XF);
        m_bits = (_y << 4) | (m_bits & 0xF);
    }

    constexpr uint8_t value() const { return uint8_t(m_bits); }

    constexpr uint8_t rank() const {
        assert(x() > 0 && x() < X_MAX - 1 && y() > 0 && y() < Y_MAX - 1);
        return (y() - 1) * Y_RANGE + (x() - 1);
    };

private:
    uint8_t m_bits = 0;
};

static constexpr Position INVALID_POS = {X_MAX, Y_MAX};

struct Vector {
    Position pos{};
    Direction dir{};
};
