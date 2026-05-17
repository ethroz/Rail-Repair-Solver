#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>

#include "Components.hpp"
#include "CoordSystem.hpp"

template<typename T, typename EmptyFn>
class LeverList {
public:
    constexpr LeverList() noexcept = default;
    constexpr ~LeverList() noexcept = default;

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

    template<typename U>
    constexpr void insert(uint8_t index, U&& value) {
        if (has(index)) {
            throw std::invalid_argument("Cannot have two starting railroads with the same index");
        }
        if (m_emptyFn(value)) {
            throw std::invalid_argument("Cannot insert an empty value");
        }
        m_data[index] = std::forward<U>(value);
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

protected:
    template<typename U>
    using NodeContainer = std::array<U, MAX_LEVERS>;

    NodeContainer<T> m_data = {};
    size_t m_size = 0;
    EmptyFn m_emptyFn{};
};

struct IsZeroVector {
    constexpr bool operator()(const Vector& v) const {
        return v.dir == NONE;
    }
};
using StartList = LeverList<Vector, IsZeroVector>;

StartList createStartList(const Grid& grid) {
    StartList list;

    for (uint8_t x = 0; x < grid.width; x++) {
        for (uint8_t y = 0; y < grid.height; y++) {
            const auto cell = grid.at(x, y);
            if (cell.isStart()) {
                Direction dir;

                const bool top = y == 0;
                const bool right = x == grid.width - 1;
                const bool bottom = y == grid.height - 1;
                const bool left = x == 0;
                uint8_t used = (top ? 0b1000 : 0) | (right ? 0b0100 : 0) | (bottom ? 0b0010 : 0) | (left ? 0b0001 : 0);
                switch (used) {
                case 0b1000: dir = DOWN;  break;
                case 0b0100: dir = LEFT;  break;
                case 0b0010: dir = UP;    break;
                case 0b0001: dir = RIGHT; break;
                default: throw std::invalid_argument("Cannot have a starting railroad on a corner");
                }

                list.insert(cell.index(), Vector{{x, y}, dir});
            }
        }
    }

    return list;
}
