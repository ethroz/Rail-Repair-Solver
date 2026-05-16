#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "Components.hpp"
#include "CoordSystem.hpp"

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
