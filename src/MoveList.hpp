#pragma once

#include <cassert>
#include <vector>

#include "CoordSystem.hpp"

class MoveList {
public:
    [[nodiscard]] constexpr bool empty() const {
        return m_data.empty();
    }

    [[nodiscard]] constexpr size_t size() const {
        assert(m_data.size() % 2 == 0);
        return m_data.size() / 2;
    }

    constexpr void push_back(Direction dir) {
        assert(dir >= MIN_DIR && dir < MAX_DIR);
        const auto data = std::to_underlying(dir);
        const bool bit1 = (data & 2) > 0;
        const bool bit0 = (data & 1) > 0;
        m_data.push_back(bit1);
        m_data.push_back(bit0);
    }

    constexpr Direction at(size_t index) const {
        bool bit1 = m_data.at(index * 2);
        bool bit0 = m_data.at(index * 2 + 1);
        return Direction((bit1 ? 2 : 0) | (bit0 ? 1 : 0));
    }

    struct const_iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Direction;

        constexpr const_iterator(const MoveList& owner, size_t index) : m_owner(owner), m_index(index) {}

        constexpr value_type operator*() const { return m_owner.at(m_index); }

        constexpr const_iterator& operator++() { assert(m_index != m_owner.size()); m_index++; return *this; }
        constexpr const_iterator operator++(int) { const_iterator temp = *this; ++(*this); return temp; }

        constexpr friend bool operator==(const const_iterator& a, const const_iterator& b) {
            return a.m_index == b.m_index;
        }

        constexpr friend bool operator!=(const const_iterator& a, const const_iterator& b) {
            return a.m_index != b.m_index;
        }

    private:
        const MoveList& m_owner;
        size_t m_index;
    };

    constexpr const_iterator begin() const { return const_iterator(*this, 0); }
    constexpr const_iterator end() const { return const_iterator(*this, size()); }

    constexpr const_iterator cbegin() const { return const_iterator(*this, 0); }
    constexpr const_iterator cend() const { return const_iterator(*this, size()); }

private:
    std::vector<bool> m_data;
};
