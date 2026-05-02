#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "IndexedPriorityQueue.hpp"
#include "IndexedVector.hpp"

#include "StableFixedQueue.hpp"

template<
    class Alive,
    class Dead,
    std::unsigned_integral Index = size_t,
    bool AutoPrune = true
> requires std::constructible_from<Dead, Alive>
class StablePriorityQueue {
private:
    static constexpr Index NO_INDEX = std::numeric_limits<Index>::max();
public:
    inline StablePriorityQueue(size_t capacity = 2) : StablePriorityQueue(capacity / 2, capacity / 2) {}

    inline StablePriorityQueue(size_t aliveCap, size_t deadCap) :
        m_alive(aliveCap),
        m_dead(deadCap)
    {}

    StablePriorityQueue(const StablePriorityQueue&) = delete;
    StablePriorityQueue& operator=(const StablePriorityQueue&) = delete;

    StablePriorityQueue(StablePriorityQueue&&) = delete;
    StablePriorityQueue& operator=(StablePriorityQueue&&) = delete;

    [[nodiscard]] constexpr size_t size() const { return m_alive.size(); }
    [[nodiscard]] constexpr size_t deadSize() const { return m_dead.size(); }
    [[nodiscard]] constexpr bool empty() const { return m_alive.empty(); }
    [[nodiscard]] constexpr bool isDead() const { return !m_dead.empty() && m_alive.empty(); }

    constexpr void reset() {
        m_alive.clear();
        m_dead.clear();
    }

    template<typename U>
    constexpr void push(U&& item) {
        assert(!isDead());
        if constexpr (AutoPrune) {
            pruneBeforeGrowth(1);
        }
        Index prevIndex = peekIndex();
        m_alive.push(std::forward<U>(item), std::move(prevIndex));
        checkInvariants();
    }

    [[nodiscard]] constexpr const Alive& peek() const {
        assert(!empty());
        return m_alive.peek();
    }

    constexpr void removeFront() {
        assert(!empty());
        auto&& [item, index] = m_alive.pop();
        m_dead.push_back(Dead(std::move(item)), std::move(index));
        checkInvariants();
    }

    template<
        class SubAlive,
        size_t AliveSize,
        size_t DeadSize,
        size_t ExternSize,
        std::unsigned_integral SubIndex = size_t
    > requires (sizeof(SubIndex) <= sizeof(Index))
    constexpr void removeFrontWithDeadSubqueue(
        const StableFixedQueue<SubAlive, Dead, Alive, AliveSize, DeadSize, ExternSize, SubIndex>& subqueue
    ) {
        using SubQueueType = std::remove_cvref_t<decltype(subqueue)>;
        const size_t size = subqueue.m_extern.size();

        if (size == 0) {
            removeFront();
            return;
        }
        
        assert(subqueue.isDead());
        assert(subqueue.deadSize() > 0);

        const size_t numDeadToKeep = subqueue.deadSize() - 1;

        if constexpr (AutoPrune) {
            pruneBeforeGrowth(numDeadToKeep + size);
        }

        removeFront();
        assert(!m_dead.empty());

        const Index rootIndex = indexCast(m_dead.size()) - 1;
        auto rebaseSubqueueIndex = [&](auto&& i) {
            assert(i != SubQueueType::NO_INDEX);
            return Index(i) + rootIndex;
        };
        m_dead.append_range(
            subqueue.m_dead.values() |
            std::views::drop(1),
            subqueue.m_dead.indices() |
            std::views::drop(1) |
            std::views::transform(rebaseSubqueueIndex)
        );

        assert(subqueue.m_alive.size() == 0);
        m_alive.pushRange(
            subqueue.m_extern.values(),
            subqueue.m_extern.indices() |
            std::views::transform(rebaseSubqueueIndex)
        );

        checkInvariants();
    }

    constexpr void pruneDead() {
        const Index totalSize = indexCast(m_dead.size() + m_alive.size());
        const Index oldDeadSize = Index(m_dead.size());
        const Index maxIndex = Index(m_dead.size() + (m_alive.empty() ? 0 : 1));

        checkInvariants();

        if (totalSize == 0) {
            assert(m_dead.empty());
            assert(m_alive.empty());
            return;
        }

        std::vector<uint8_t> live(oldDeadSize, 0);
        for (Index i = oldDeadSize; i < totalSize; ++i) {
            Index index = i;
            while (index >= oldDeadSize) {
                assert(index < maxIndex);
                index = prevIndexAt(index);
            }

            while (index != NO_INDEX) {
                assert(index < oldDeadSize);
                if (live[index]) {
                    break;
                }

                live[index] = 1;
                index = prevIndexAt(index);
            }
        }

        std::vector<Index> remap(maxIndex, NO_INDEX);

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

        checkInvariants();
    }

    struct chain_iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Dead;
        using reference = value_type;
        using pointer = void;
        using const_reference = const reference;
        using const_pointer = const pointer;

        constexpr chain_iterator(const StablePriorityQueue& owner, Index index) :
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
        const StablePriorityQueue& m_owner;
        Index m_index;
    };

    constexpr chain_iterator begin() const { return chain_iterator(*this, peekIndex()); }
    constexpr chain_iterator end() const { return chain_iterator(*this, NO_INDEX); }

private:
    [[nodiscard]] static constexpr Index indexCast(size_t value) {
        if (value >= size_t(NO_INDEX)) {
            throw std::overflow_error("StablePriorityQueue index overflow");
        }
        return Index(value);
    }

    [[nodiscard]] constexpr size_t totalSize() const {
        return m_dead.size() + m_alive.size();
    }

    [[nodiscard]] constexpr size_t totalCapacity() const {
        return m_dead.capacity() + m_alive.capacity();
    }

    [[nodiscard]] constexpr Index prevIndexAt(Index index) const {
        const size_t deadIndex = size_t(index);
        if (deadIndex < m_dead.size()) {
            return m_dead.index(deadIndex);
        }
        const size_t aliveIndex = deadIndex - m_dead.size();
        assert(aliveIndex < m_alive.size());
        return m_alive.index(aliveIndex);
    }

    template<class Container>
    constexpr void remapPrevIndices(
        Container& container,
        Index maxIndex,
        const std::vector<Index>& remap
    ) {
        for (size_t i = 0; i < container.size(); ++i) {
            Index& prevIndex = container.index(i);
            if (prevIndex == NO_INDEX) {
                continue;
            }
            assert(prevIndex < maxIndex); // StablePriorityQueue contains invalid prevIndex

            const Index newPrevIndex = remap[prevIndex];
            assert(newPrevIndex != NO_INDEX); // StablePriorityQueue pruning removed a required parent

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

    constexpr void pruneBeforeGrowth(size_t newElems) {
        const size_t free = totalCapacity() - totalSize();
        if (newElems <= free) {
            return;
        }

#ifdef VERBOSE_LOGS
        const size_t oldSize = totalSize();
        const size_t oldDeadSize = m_dead.size();
        const size_t aliveSize = m_alive.size();
#endif

        const size_t oldCapacity = totalCapacity();

        pruneDead();

        const size_t newSize = totalSize();
        const size_t freeSlots = oldCapacity - newSize;
        const size_t minFreeSlots = std::max<size_t>(newElems, oldCapacity / 4);

#ifdef VERBOSE_LOGS
        const size_t removed = oldSize - newSize;

        std::cout
            << "StablePriorityQueue prune: "
            << "oldSize=" << oldSize
            << ", newSize=" << newSize
            << ", removed=" << removed
            << ", oldDead=" << oldDeadSize
            << ", newDead=" << m_dead.size()
            << ", oldAlive=" << aliveSize
            << ", newAlive=" << m_alive.size()
            << ", capacity=" << totalCapacity()
            << ", freeSlots=" << freeSlots
            << '\n';
#endif

        if (freeSlots >= minFreeSlots) {
#ifdef VERBOSE_LOGS
            std::cout << "StablePriorityQueue prune avoided reserve\n";
#endif
            return;
        }

        const size_t growthCapacity = std::max<size_t>(
            oldCapacity + 1,
            oldCapacity * 21 / 13
        );

        const size_t newCapacity = std::max<size_t>(
            growthCapacity,
            newSize + minFreeSlots
        );

#ifdef VERBOSE_LOGS
        std::cout
            << "StablePriorityQueue prune insufficient; reserving "
            << newCapacity
            << '\n';
#endif

        const size_t remainingCapacity = newCapacity - newSize;

        const size_t minDeadCapacity = m_dead.size();
        m_dead.reserve(minDeadCapacity + remainingCapacity);

        const size_t minAliveCapacity = m_alive.size();
        m_alive.reserve(minAliveCapacity + remainingCapacity);
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
#endif
    }

    IndexedVector<Dead, Index> m_dead;
    IndexedPriorityQueue<Alive, Index> m_alive;
};
