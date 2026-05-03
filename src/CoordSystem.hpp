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
public:
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

enum TRACKTYPE : uint8_t {
    NW = 0,
    NE = 1,
    SE = 2,
    SW = 3,
    H = 4,
    V = 5,
    NUM_TRACK = 6,
};

struct TrackType {
public:
    constexpr TrackType(const TRACKTYPE& d) : m_type{d} {}

    [[nodiscard]] constexpr Direction ride(Direction inDir) const {
        switch (m_type) {
        case H:
        case V:
            return (inDir % 2 != m_type - H) ? inDir : NONE;
        case NW:
        case NE:
        case SE:
        case SW:
            if (m_type == inDir - 1) {
                return DIRECTION(((inDir + MAX_DIR - 2) % MAX_DIR) + 1);
            }
            else if ((m_type + 1) % MAX_DIR == inDir - 1) {
                return DIRECTION((inDir % MAX_DIR) + 1);
            }
            else {
                return NONE;
            }
        default: throw std::invalid_argument(std::format("Invalid track value: {}", std::to_underlying(m_type)));
        }
    }

    [[nodiscard]] constexpr operator uint8_t() const { return uint8_t(m_type); }

private:
    TRACKTYPE m_type;
};

struct Position {
    constexpr Position() = default;
    constexpr Position(uint8_t _x, uint8_t _y) : m_bits((_y << 4) | (_x & 0xF)) { assert(_x <= 0XF && _y <= 0XF); }
    
    [[nodiscard]] friend inline constexpr Position operator+(Position a, Position b) { return Position(a.x() + b.x(), a.y() + b.y()); }
    constexpr Position& operator+=(Position o) {
        Position p = *this + o;
        m_bits = p.m_bits;
        return *this;
    }
    [[nodiscard]] friend inline constexpr Position operator+(Position p, Direction d) {
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
    [[nodiscard]] friend inline constexpr Position operator-(Position p, Direction d) {
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

    [[nodiscard]] friend inline constexpr bool operator==(Position a, Position b) { return a.m_bits == b.m_bits; }
    [[nodiscard]] friend inline constexpr bool operator!=(Position a, Position b) { return a.m_bits != b.m_bits; }
    
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

struct Vector {
    Position pos{};
    Direction dir{};
};
