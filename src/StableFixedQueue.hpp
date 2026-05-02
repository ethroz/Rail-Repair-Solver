#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "FixedIndexedQueue.hpp"
#include "FixedIndexedVector.hpp"

template<
    class Alive,
    class Dead,
    class Extern,
    size_t AliveSize,
    size_t DeadSize,
    size_t ExternSize,
    std::unsigned_integral Index = size_t
> requires std::constructible_from<Dead, Alive> &&
    (DeadSize < std::numeric_limits<Index>::max())
class StableFixedQueue {
private:
    static constexpr Index NO_INDEX = std::numeric_limits<Index>::max();
public:
    constexpr StableFixedQueue() = default;

    StableFixedQueue(const StableFixedQueue&) = delete;
    StableFixedQueue& operator=(const StableFixedQueue&) = delete;

    StableFixedQueue(StableFixedQueue&&) = delete;
    StableFixedQueue& operator=(StableFixedQueue&&) = delete;

    [[nodiscard]] constexpr size_t size() const { return m_alive.size(); }
    [[nodiscard]] constexpr size_t deadSize() const { return m_dead.size(); }
    [[nodiscard]] constexpr bool empty() const { return m_alive.empty(); }
    [[nodiscard]] constexpr bool isDead() const { return !m_dead.empty() && m_alive.empty(); }

    constexpr void reset() {
        m_dead.clear();
        m_alive.clear();
        m_extern.clear();
        m_frontHasRef = false;
    }

    template<typename U>
    constexpr void push(U&& item) {
        assert(!isDead());
        m_alive.push(std::forward<U>(item), peekIndex());
        m_frontHasRef = true;
        checkInvariants();
    }

    template<typename U>
    constexpr void pushExtern(U&& ext) {
        assert(!isDead());
        m_extern.push_back(std::forward<U>(ext), peekIndex());
        m_frontHasRef = true;
        checkInvariants();
    }

    [[nodiscard]] constexpr const Alive& peek() const {
        assert(!m_alive.empty());
        return m_alive.peek();
    }

    constexpr void removeFront() {
        assert(!m_alive.empty());
        auto&& [item, index] = m_alive.pop();
        if (m_frontHasRef) {
            m_dead.push_back(Dead(std::move(item)), std::move(index));
            m_frontHasRef = false;
        }
        checkInvariants();
    }

    constexpr void pruneDead() {
        const Index totalSize = indexCast(m_dead.size() + m_alive.size() + m_extern.size());
        const Index oldDeadSize = Index(m_dead.size());
        const Index maxIndex = Index(m_dead.size() + (m_alive.empty() ? 0 : 1));

        checkInvariants();

        if (oldDeadSize == 0) {
            assert(m_dead.empty());
            assert(m_alive.empty());
            assert(m_extern.empty());
            return;
        }

        std::array<bool, DeadSize> live{};
        std::array<Index, DeadSize + 1> remap{};
        remap.fill(NO_INDEX);

        for (Index i = oldDeadSize; i < totalSize; ++i) {
            Index index = i;
            while (index >= oldDeadSize) {
                index = prevIndexAt(index);
            }

            while (index != NO_INDEX) {
                assert(index < oldDeadSize);
                if (live[index]) {
                    break;
                }

                live[index] = true;
                index = prevIndexAt(index);
            }
        }

        Index writeDeadIndex = 0;
        for (Index readDeadIndex = 0; readDeadIndex < oldDeadSize; ++readDeadIndex) {
            if (!live[readDeadIndex]) {
                continue;
            }

            remap[readDeadIndex] = writeDeadIndex;

            if (writeDeadIndex != readDeadIndex) {
                m_dead.move_assign_from(writeDeadIndex, readDeadIndex);
            }

            ++writeDeadIndex;
        }

        remap[oldDeadSize] = writeDeadIndex;
        m_dead.pop_back(m_dead.size() - writeDeadIndex);

        // Remap all retained prevIndex links.
        remapPrevIndices(m_dead, maxIndex, remap);
        remapPrevIndices(m_alive, maxIndex, remap);
        remapPrevIndices(m_extern, maxIndex, remap);

        checkInvariants();
    }

    struct chain_iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Dead;
        using reference = value_type;
        using pointer = void;
        using const_reference = const reference;
        using const_pointer = const pointer;

        constexpr chain_iterator(const StableFixedQueue& owner, Index index) :
            m_owner(owner),
            m_index(index)
        {}

        constexpr reference operator*() const { return m_owner.at(m_index); }

        constexpr chain_iterator& operator++() { m_index = m_owner.prevIndexAt(m_index); return *this; }
        constexpr chain_iterator operator++(int) { chain_iterator temp = *this; ++(*this); return temp; }

        constexpr friend bool operator==(const chain_iterator& a, const chain_iterator& b) {
            assert(&a.m_owner == &b.m_owner);
            return a.m_index == b.m_index;
        }

        constexpr friend bool operator!=(const chain_iterator& a, const chain_iterator& b) {
            assert(&a.m_owner == &b.m_owner);
            return a.m_index != b.m_index;
        }

    private:
        const StableFixedQueue& m_owner;
        Index m_index;
    };

    constexpr chain_iterator begin() const { return chain_iterator(*this, peekIndex()); }
    constexpr chain_iterator end() const { return chain_iterator(*this, NO_INDEX); }

private:
    [[nodiscard]] static constexpr Index indexCast(size_t value) {
        if (value >= size_t(NO_INDEX)) {
            throw std::overflow_error("StableFixedQueue index overflow");
        }
        return Index(value);
    }

    [[nodiscard]] constexpr size_t totalSize() const {
        return m_dead.size() + m_alive.size() + m_extern.size();
    }

    [[nodiscard]] constexpr Index prevIndexAt(Index index) const {
        const size_t deadIndex = size_t(index);
        if (deadIndex < m_dead.size()) {
            return m_dead.index(deadIndex);
        }
        const size_t aliveIndex = deadIndex - m_dead.size();
        if (aliveIndex < m_alive.size()) {
            return m_alive.index(aliveIndex);
        }
        const size_t externIndex = aliveIndex - m_alive.size();
        assert(externIndex < m_extern.size());
        return m_extern.index(externIndex);
    }

    template<class Container, class Remap>
    constexpr void remapPrevIndices(
        Container& container,
        Index maxIndex,
        const Remap& remap
    ) {
        for (size_t i = 0; i < container.size(); ++i) {
            Index& prevIndex = container.index(i);
            if (prevIndex == NO_INDEX) {
                continue;
            }
            assert(prevIndex < maxIndex); // StableFixedQueue contains invalid prevIndex

            const Index newPrevIndex = remap[prevIndex];
            assert(newPrevIndex != NO_INDEX); // StableFixedQueue pruning removed a required parent

            prevIndex = newPrevIndex;
        }
    }

    [[nodiscard]] constexpr Index peekIndex() const {
        return empty() ? NO_INDEX : indexCast(m_dead.size());
    }

    [[nodiscard]] constexpr Dead at(Index index) const {
        assert(index != NO_INDEX);
        const size_t rawIndex = size_t(index);

        if (rawIndex < m_dead.size()) {
            return m_dead.value(index);
        }
        assert(rawIndex == m_dead.size() && !m_alive.empty());
        return Dead(m_alive.peek());
    }

    constexpr void checkInvariants() const {
#ifndef NDEBUG
        const size_t size = totalSize();
        const size_t deadSize = m_dead.size();
        const size_t aliveSize = m_alive.size();

        auto checkPrevIndex = [&](size_t logicalIndex, Index prevIndex) {
            if (logicalIndex == 0) {
                assert(prevIndex == NO_INDEX);
                return;
            }

            // Revived queue error
            assert(prevIndex != NO_INDEX);

            // Pruning remap error
            assert(size_t(prevIndex) < size);

            // Circular
            assert(size_t(prevIndex) != logicalIndex);

            // Backwards
            assert(size_t(prevIndex) < logicalIndex);

            // Pointing to the middle of the active queue
            if (logicalIndex >= deadSize) {
                assert(size_t(prevIndex) <= deadSize);
            }
        };

        for (size_t i = 0; i < deadSize; ++i) {
            checkPrevIndex(i, m_dead.index(i));
        }

        for (size_t i = 0; i < aliveSize; ++i) {
            checkPrevIndex(deadSize + i, m_alive.index(i));
        }

        for (size_t i = 0; i < m_extern.size(); ++i) {
            checkPrevIndex(deadSize + aliveSize + i, m_extern.index(i));
        }
#endif
    }

    template<
        class OtherAlive,
        class OtherDead,
        std::unsigned_integral OtherIndex,
        bool AutoPrune
    > requires std::constructible_from<OtherDead, OtherAlive>
    friend class StablePriorityQueue;

    FixedIndexedVector<Dead, Index, DeadSize> m_dead;
    FixedIndexedQueue<Alive, Index, AliveSize> m_alive;
    FixedIndexedVector<Extern, Index, ExternSize> m_extern;
    bool m_frontHasRef = false;
};
