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
constexpr uint8_t TRACK     = 0b01000000;
constexpr uint8_t LEVER     = 0b00100000;
constexpr uint8_t START     = 0b00010000;
constexpr uint8_t INDEX     = 0b00000111;
constexpr size_t MAX_LEVERS = 3;
constexpr size_t MAX_OBJECTS = 10;

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
        default: throw std::invalid_argument(std::format("Invalid cell value for char: {}", uint8_t(m_cell)));
        }
    }

    friend constexpr bool operator==(Cell a, Cell b) { return a.m_cell == b.m_cell; }
    friend constexpr bool operator>(Cell a, Cell b) { return a.m_cell > b.m_cell; }
    friend constexpr bool operator<(Cell a, Cell b) { return a.m_cell < b.m_cell; }

private:
    CELL m_cell;
};

static constexpr uint8_t ENCODING_BYTES = 10;
using StateEncoding = std::array<uint8_t, ENCODING_BYTES>;
using Rank = uint16_t;

struct State {
public:
    std::array<Cell, MAX_OBJECTS> objects = {};
    std::array<Position, MAX_OBJECTS> objectPositions = {};
    Rank rank = 0;
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

    constexpr size_t numToggledLevers() const {
        return std::popcount(leverBits);
    }

    constexpr StateEncoding encode(uint8_t objectCount) const {
        StateEncoding encoding{};

        uint64_t positionalEncoding = 0;
        constexpr size_t POS_ENC_BYTES = sizeof(positionalEncoding);
        static_assert(POS_ENC_BYTES <= ENCODING_BYTES);

        uint64_t multiplier = 1;
        for (uint8_t i = 0; i < objectCount; ++i) {
            positionalEncoding += uint64_t(objectPositions[i].rank()) * multiplier;
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
        blockTypeEncoding |= player.rank() << MAX_OBJECTS;
        static_assert(MAX_OBJECTS + std::bit_width(BASE - 1) <= TYPE_ENC_BYTES * 8);

        for (size_t i = 0; i < TYPE_ENC_BYTES; ++i) {
            encoding[i + POS_ENC_BYTES] = uint8_t(blockTypeEncoding >> (i * 8));
        }

        return encoding;
    }

    [[nodiscard]] friend constexpr bool operator<(const State& a, const State& b) { return a.rank < b.rank; }
};

struct DeadState {
    Position player = {};

    constexpr DeadState() = default;

    constexpr DeadState(const State& state)
#ifdef NDEBUG
        : player{state.player} {}
#else
        : player{state.player}, leverBits{state.leverBits} {}
    
    uint8_t leverBits = 0;

    constexpr bool leverToggled(uint8_t index) const {
        assert(index < MAX_LEVERS);
        return ((leverBits >> index) & 1) > 0;
    }

    constexpr void toggleLever(uint8_t index) {
        assert(index < MAX_LEVERS);
        leverBits ^= 1 << index;
    }

    constexpr size_t numToggledLevers() const {
        return std::popcount(leverBits);
    }
#endif
};

static_assert(std::is_default_constructible_v<DeadState>);
static_assert(std::constructible_from<DeadState, State>);

struct RankedDeadState : DeadState {
    Rank rank = 0;

    constexpr RankedDeadState() = default;

    constexpr RankedDeadState(const State& state) :
        DeadState(state),
        rank{state.rank}
    {}
};

static_assert(std::is_default_constructible_v<RankedDeadState>);
static_assert(std::constructible_from<RankedDeadState, State>);
static_assert(std::constructible_from<DeadState, RankedDeadState>);

struct RankedPosition {
    Position pos;
    uint8_t rank;

    [[nodiscard]] constexpr operator Position() const { return pos; }

    [[nodiscard]] friend constexpr bool operator<(const RankedPosition& a, const RankedPosition& b) { return a.rank < b.rank; }
};

static_assert(std::constructible_from<Position, RankedPosition>);

class Grid {
public:
    struct LookupResult {
        Cell cell;
        uint8_t index;
    };

    constexpr Grid() = default;

    constexpr const Cell& at(const Position& p) const { return m_data[p.y()][p.x()]; }
    constexpr Cell& at(const Position& p) { return m_data[p.y()][p.x()]; }
    constexpr const Cell& at(uint8_t x, uint8_t y) const { return m_data[y][x]; }
    constexpr Cell& at(uint8_t x, uint8_t y) { return m_data[y][x]; }

    constexpr LookupResult at(const State& state, const Position& p) const {
        uint8_t floorIndex = 0xFF;

        for (uint8_t i = 0; i < objectCount; ++i) {
            if (state.objectPositions[i] != p) {
                continue;
            }

            if (state.objects[i] != FLOOR) {
                return {state.objects[i], i};
            }

            floorIndex = i;
        }

        if (floorIndex != 0xFF) {
            return {FLOOR, floorIndex};
        }

        if (state.player == p) {
            return {PLAYER, 0xFF};
        }

        return {at(p), 0xFF};
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

    constexpr std::string toString(const State& state) const {
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

template<typename T, typename EmptyFn>
class LeverList {
public:
    constexpr size_t size() const { return m_size; }
    constexpr bool empty() const { return m_size == 0; }

    constexpr bool has(uint8_t index) const {
        return index < MAX_LEVERS && !m_emptyFn(m_data.at(index));
    }
    
    constexpr const T& at(uint8_t index) const {
        if (!has(index)) {
            throw std::invalid_argument(std::format("Invalid railroad index: {}", index));
        }
        return m_data[index];
    }

    constexpr void insert(uint8_t index, T&& value) {
        if (has(index)) {
            throw std::invalid_argument("Cannot have two starting railroads with the same index");
        }
        if (m_emptyFn(value)) {
            throw std::invalid_argument("Cannot insert an empty value");
        }
        m_data[index] = std::move(value);
        ++m_size;
    }

    struct pair_iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<uint8_t, const T&>;
        using reference = value_type;
        using pointer = void;
        using const_reference = const reference;
        using const_pointer = const pointer;

        constexpr pair_iterator(const LeverList& owner, uint8_t index) :
            m_owner(owner),
            m_index(index)
        {
            findNext();
        }

        constexpr reference operator*() const { return {m_index, m_owner.at(m_index)}; }

        constexpr pair_iterator& operator++() {
            m_index = std::min<uint8_t>(MAX_LEVERS, m_index + 1);
            findNext();
            return *this;
        }
        constexpr pair_iterator operator++(int) {
            pair_iterator temp = *this;
            ++(*this);
            return temp;
        }

        constexpr friend bool operator==(const pair_iterator& a, const pair_iterator& b) {
            assert(&a.m_owner == &b.m_owner);
            return a.m_index == b.m_index;
        }

        constexpr friend bool operator!=(const pair_iterator& a, const pair_iterator& b) {
            assert(&a.m_owner == &b.m_owner);
            return a.m_index != b.m_index;
        }

    private:
        constexpr void findNext() {
            while (m_index < MAX_LEVERS && !m_owner.has(m_index)) {
                ++m_index;
            }
        }

        const LeverList& m_owner;
        uint8_t m_index;
    };

    constexpr pair_iterator begin() const { return pair_iterator(*this, 0); }
    constexpr pair_iterator end() const { return pair_iterator(*this, MAX_LEVERS); }

private:
    std::array<T, MAX_LEVERS> m_data = {};
    size_t m_size = 0;
    EmptyFn m_emptyFn{};
};
