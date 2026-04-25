#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

#include "CoordSystem.hpp"
#include "MoveList.hpp"

constexpr uint8_t IMMOVABLE = 0b10000000;
constexpr uint8_t TRACK = 0b01000000;
constexpr uint8_t LEVER = 0b00100000;
constexpr uint8_t START = 0b00010000;
constexpr uint8_t LEVER_ON = 0b00001000;
constexpr uint8_t INDEX = 0b00000111;
constexpr size_t MAX_LEVERS = 3;
constexpr size_t MAX_OBJECTS = 11;

enum CELL : uint8_t {
    PLAYER       = 0,
    FLOOR        = 1,
    WALL         = IMMOVABLE | 0,
    HOLE         = IMMOVABLE | 1,
    LEVER1_OFF   = IMMOVABLE | LEVER | 0,
    LEVER2_OFF   = IMMOVABLE | LEVER | 1,
    LEVER3_OFF   = IMMOVABLE | LEVER | 2,
    LEVER1_ON    = IMMOVABLE | LEVER | LEVER_ON | 0,
    LEVER2_ON    = IMMOVABLE | LEVER | LEVER_ON | 1,
    LEVER3_ON    = IMMOVABLE | LEVER | LEVER_ON | 2,
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
    constexpr Cell(CELL cell) : m_cell(cell) {}

    constexpr bool isMovable() const { return (m_cell & IMMOVABLE) == 0; }
    constexpr bool isTrack() const { return (m_cell & TRACK) > 0; }
    constexpr bool isLever() const { return (m_cell & LEVER) > 0; }
    constexpr bool isStart() const { return (m_cell & START) > 0; }
    constexpr bool leverState() const { return (m_cell & LEVER_ON) > 0; }
    constexpr bool isEmpty() const { return (m_cell == FLOOR) || (m_cell == HOLE); }
    constexpr uint8_t index() const { return m_cell & INDEX; }
    constexpr TrackType trackType() const { return TrackType(m_cell & INDEX); }
    constexpr void toggleLever() { m_cell = CELL(m_cell ^ LEVER_ON); }

    friend constexpr bool operator==(Cell a, Cell b) { return a.m_cell == b.m_cell; }
    friend constexpr bool operator>(Cell a, Cell b) { return a.m_cell > b.m_cell; }

private:
    CELL m_cell;
};

static constexpr uint8_t X_MAX = 10;
static constexpr uint8_t Y_MAX = 10;

struct Grid {
public:
    constexpr Grid() : m_data{} {}

    constexpr const Cell& at(Position p) const { return m_data[p.x][p.y]; }
    constexpr Cell& at(Position p) { return m_data[p.x][p.y]; }
    constexpr const Cell& at(int8_t x, int8_t y) const { return m_data[x][y]; }
    constexpr Cell& at(int8_t x, int8_t y) { return m_data[x][y]; }

private:
    std::array<std::array<Cell, Y_MAX>, X_MAX> m_data;
};

static_assert(sizeof(Grid) == X_MAX * Y_MAX);

bool operator==(const Grid& a, const Grid& b) { return memcmp(&a, &b, sizeof(a)) == 0; }

struct State {
    Grid grid{};
    Position player{};
    uint8_t toggledLevers = 0;
    MoveList moves{};
};

constexpr Direction trackToDirection(Cell c, Direction inDir) {
    const auto d = c.trackType();
    switch (d) {
    case H:
    case V:
        return (inDir % 2 == d - H) ? inDir : NONE;
    case NW:
    case NE:
    case SE:
    case SW:
        if (d == inDir) {
            return Direction((inDir + (MAX_DIR - 1)) % MAX_DIR);
        }
        else if ((d + 1) % MAX_DIR == inDir) {
            return Direction((inDir + 1) % MAX_DIR);
        }
        else {
            return NONE;
        }
    default: throw std::invalid_argument(std::format("unexpected direction: {}", std::to_underlying(inDir)));
    }
}
