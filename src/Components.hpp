#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <utility>

#include "CoordSystem.hpp"

constexpr uint8_t IMMOVABLE = 0b10000000; // A cell in which the player cannot move to.
constexpr uint8_t TRACK = 0b01000000;
constexpr uint8_t LEVER = 0b00100000;
constexpr uint8_t START = 0b00010000;
constexpr uint8_t INDEX = 0b00000111;
constexpr size_t MAX_LEVERS = 3;
constexpr size_t MAX_OBJECTS = 10;
constexpr uint8_t X_MAX = 10;
constexpr uint8_t Y_MAX = 10;

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
    constexpr bool isTrack() const { return (m_cell & TRACK) > 0; }
    constexpr bool isLever() const { return (m_cell & LEVER) > 0; }
    constexpr bool isStart() const { return (m_cell & START) > 0; }
    constexpr bool isEmpty() const { return (m_cell == FLOOR) || (m_cell == HOLE); }
    constexpr uint8_t index() const { return m_cell & INDEX; }
    constexpr TrackType trackType() const {
        assert(isTrack());
        return TRACKTYPE(m_cell & INDEX);
    }

    constexpr uint8_t value() const { return uint8_t(m_cell); }

    friend constexpr bool operator==(Cell a, Cell b) { return a.m_cell == b.m_cell; }
    friend constexpr bool operator>(Cell a, Cell b) { return a.m_cell > b.m_cell; }
    friend constexpr bool operator<(Cell a, Cell b) { return a.m_cell < b.m_cell; }

private:
    CELL m_cell;
};

static constexpr uint8_t ENCODING_BYTES = 10;
using StateEncoding = std::array<uint8_t, ENCODING_BYTES>;

struct State {
public:
    std::array<Cell, MAX_OBJECTS> objects = {};
    std::array<Position, MAX_OBJECTS> objectPositions = {};
    uint8_t objectCount = 0;
    Position player = {};
    uint8_t leverBits = 0;

    constexpr bool leverToggled(uint8_t index) const {
        assert(index < MAX_LEVERS);
        return ((leverBits >> index) & 1) > 0;
    }

    constexpr void toggleLever(uint8_t index) {
        assert(index < MAX_LEVERS);
        leverBits ^= 1 << index;
    }

    constexpr int numToggledLevers() const {
        return std::popcount(leverBits);
    }

    constexpr StateEncoding encode() const {
        StateEncoding encoding{};

        constexpr uint8_t X_RANGE = X_MAX - 2;
        constexpr uint8_t Y_RANGE = Y_MAX - 2;
        constexpr uint64_t BASE = X_RANGE * Y_RANGE;
        const auto rank = [](const Position& pos) -> uint8_t {
            assert(pos.x() > 0 && pos.x() < X_MAX - 1 && pos.y() > 0 && pos.y() < Y_MAX - 1);
            return (pos.x() - 1) * X_RANGE + (pos.y() - 1);
        };

        uint64_t positionalEncoding = 0;
        constexpr size_t POS_ENC_BYTES = sizeof(positionalEncoding);
        static_assert(POS_ENC_BYTES <= ENCODING_BYTES);

        uint64_t multiplier = 1;
        for (uint8_t i = 0; i < objectCount; ++i) {
            positionalEncoding += uint64_t(rank(objectPositions[i])) * multiplier;
            multiplier *= BASE;
        }

        constexpr auto POS_BITS = std::bit_width([](uint64_t base, int exp) constexpr {
            uint64_t result = 1;
            while (exp > 0) { result *= base; --exp; }
            return result;
        }(BASE, MAX_OBJECTS) - 1);
        positionalEncoding |= uint64_t(leverBits) << POS_BITS;
        static_assert(POS_BITS + MAX_LEVERS <= POS_ENC_BYTES * 8);

        for (size_t i = 0; i < POS_ENC_BYTES; ++i) {
            encoding[i] = uint8_t(positionalEncoding >> (i * 8));
        }

        uint16_t blockTypeEncoding = 0;
        constexpr size_t TYPE_ENC_BYTES = sizeof(blockTypeEncoding);
        static_assert(POS_ENC_BYTES + TYPE_ENC_BYTES <= ENCODING_BYTES);

        for (uint8_t i = 0; i < objectCount; ++i) {
            blockTypeEncoding |= (objects[i].isTrack() ? 1 : 0) << i;
        }
        blockTypeEncoding |= rank(player) << MAX_OBJECTS;
        static_assert(MAX_OBJECTS + std::bit_width(BASE - 1) <= TYPE_ENC_BYTES * 8);

        for (size_t i = 0; i < TYPE_ENC_BYTES; ++i) {
            encoding[i + POS_ENC_BYTES] = uint8_t(blockTypeEncoding >> (i * 8));
        }

        return encoding;
    }
};

#ifdef NDEBUG
struct DeadState {
    Position player = {};

    constexpr DeadState() = default;

    constexpr DeadState(const State& state) : player{state.player} {}
};
#else
struct DeadState {
    Position player = {};
    uint8_t leverBits = 0;

    constexpr DeadState() = default;

    constexpr DeadState(const State& state) : player{state.player}, leverBits{state.leverBits} {}

    constexpr bool leverToggled(uint8_t index) const {
        assert(index < MAX_LEVERS);
        return ((leverBits >> index) & 1) > 0;
    }

    constexpr void toggleLever(uint8_t index) {
        assert(index < MAX_LEVERS);
        leverBits ^= 1 << index;
    }

    constexpr int numToggledLevers() const {
        return std::popcount(leverBits);
    }
};
#endif

static_assert(std::constructible_from<DeadState, State>);

struct Grid {
public:
    constexpr Grid() = default;

    constexpr struct { Cell cell; uint8_t index; } at(const State& state, Position p) const {
        for (uint8_t i = 0; i < state.objectCount; ++i) {
            if (state.objects[i] == FLOOR) {
                continue;
            }
            if (state.objectPositions[i] == p) {
                return {state.objects[i], i};
            }
        }
        for (uint8_t i = 0; i < state.objectCount; ++i) {
            if (state.objects[i] != FLOOR) {
                continue;
            }
            if (state.objectPositions[i] == p) {
                return {state.objects[i], i};
            }
        }
        if (state.player == p) {
            return {PLAYER, 0xFF};
        }
        return {at(p), 0xFF};
    }

    constexpr const Cell& at(Position p) const { return m_data[p.x()][p.y()]; }
    constexpr Cell& at(Position p) { return m_data[p.x()][p.y()]; }
    constexpr const Cell& at(uint8_t x, uint8_t y) const { return m_data[x][y]; }
    constexpr Cell& at(uint8_t x, uint8_t y) { return m_data[x][y]; }

private:
    std::array<std::array<Cell, Y_MAX>, X_MAX> m_data{};
};

bool operator==(const Grid& a, const Grid& b) { return memcmp(&a, &b, sizeof(a)) == 0; }
