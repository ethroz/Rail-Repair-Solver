#pragma once

#include <cassert>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

#include "CoordSystem.hpp"

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

constexpr uint8_t IMMOVABLE = 0b10000000; // A cell in which the player cannot move to.
constexpr uint8_t TRACK     = 0b01000000;
constexpr uint8_t LEVER     = 0b00100000;
constexpr uint8_t START     = 0b00010000;
constexpr uint8_t INDEX     = 0b00000111;
constexpr size_t MAX_LEVERS = 3;
constexpr size_t MAX_OBJECTS = 10;
constexpr size_t MAX_HOLES = 8;

enum CELL : uint8_t {
    PLAYER       = 0,
    FLOOR        = 1,
    WALL         = IMMOVABLE | 0,
    HOLE         = IMMOVABLE | 1,
    LEVER1       = IMMOVABLE | LEVER | 0,
    LEVER2       = IMMOVABLE | LEVER | 1,
    LEVER3       = IMMOVABLE | LEVER | 2,
    TRACK1       = IMMOVABLE | TRACK | START | 0,
    TRACK2       = IMMOVABLE | TRACK | START | 1,
    TRACK3       = IMMOVABLE | TRACK | START | 2,
    IMMOVABLE_NW = IMMOVABLE | TRACK | NW,
    IMMOVABLE_NE = IMMOVABLE | TRACK | NE,
    IMMOVABLE_SE = IMMOVABLE | TRACK | SE,
    IMMOVABLE_SW = IMMOVABLE | TRACK | SW,
    IMMOVABLE_H  = IMMOVABLE | TRACK | H,
    IMMOVABLE_V  = IMMOVABLE | TRACK | V,
    MOVABLE_NW   = TRACK | NW,
    MOVABLE_NE   = TRACK | NE,
    MOVABLE_SE   = TRACK | SE,
    MOVABLE_SW   = TRACK | SW,
    MOVABLE_H    = TRACK | H,
    MOVABLE_V    = TRACK | V,
    NOTHING      = 255,
};

struct Cell {
public:
    constexpr Cell() : m_cell(NOTHING) {}
    constexpr Cell(const CELL& cell) : m_cell(cell) {}

    constexpr bool isMovable() const { return (m_cell & IMMOVABLE) == 0; }
    constexpr bool isWalkable() const { return isMovable() || isLever() || m_cell == HOLE; }
    constexpr bool isTrack() const { return (m_cell & TRACK) > 0; }
    constexpr bool isLever() const { return (m_cell & LEVER) > 0; }
    constexpr bool isStart() const { return (m_cell & START) > 0; }
    constexpr bool isEmpty() const { return (m_cell == FLOOR) || (m_cell == HOLE) || (m_cell == PLAYER); }
    constexpr uint8_t index() const { return m_cell & INDEX; }
    constexpr TrackType trackType() const {
        assert(isTrack());
        return TRACKTYPE(m_cell & INDEX);
    }

    constexpr uint8_t value() const { return uint8_t(m_cell); }

    constexpr explicit operator char() const {
        switch (m_cell) {
        case PLAYER:       return '@';
        case FLOOR:        return ' ';
        case WALL:         return '#';
        case HOLE:         return '*';
        case LEVER1:       return '1';
        case LEVER2:       return '2';
        case LEVER3:       return '3';
        case TRACK1:       return '1';
        case TRACK2:       return '2';
        case TRACK3:       return '3';
        case IMMOVABLE_NW: return 'U';
        case IMMOVABLE_NE: return 'R';
        case IMMOVABLE_SE: return 'D';
        case IMMOVABLE_SW: return 'L';
        case IMMOVABLE_H:  return 'H';
        case IMMOVABLE_V:  return 'V';
        case MOVABLE_NW:   return 'u';
        case MOVABLE_NE:   return 'r';
        case MOVABLE_SE:   return 'd';
        case MOVABLE_SW:   return 'l';
        case MOVABLE_H:    return 'h';
        case MOVABLE_V:    return 'v';
        case NOTHING:      return '\0';
        default: throw std::invalid_argument(std::format("Invalid cell value for char: {}", std::to_underlying(m_cell)));
        }
    }

    friend constexpr bool operator==(Cell a, Cell b) { return a.m_cell == b.m_cell; }
    friend constexpr bool operator>(Cell a, Cell b) { return a.m_cell > b.m_cell; }
    friend constexpr bool operator<(Cell a, Cell b) { return a.m_cell < b.m_cell; }

private:
    CELL m_cell;
};
