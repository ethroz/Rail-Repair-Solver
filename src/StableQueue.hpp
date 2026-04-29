#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

template<class Q, class T>
concept StableQueueStorage =
    std::constructible_from<Q, size_t> &&
    requires(Q queue, const Q constQueue, T item, size_t index, std::span<T> range) {
        { constQueue.size() } -> std::convertible_to<size_t>;
        { constQueue.capacity() } -> std::convertible_to<size_t>;
        { constQueue.empty() } -> std::convertible_to<bool>;
        queue.clear();
        queue.push(std::move(item));
        queue.pushRange(range);
        { constQueue.at(index) } -> std::same_as<const T&>;
        { constQueue.peek() } -> std::same_as<const T&>;
        { queue.pop() } -> std::convertible_to<T>;
    };

template<template<class> class Q, class A, class D, typename I>
concept ValidStableQueue = 
    StableQueueStorage<Q<A>, A> &&
    std::constructible_from<D, const A&> &&
    std::unsigned_integral<I>;

template<class Q>
concept AcceptsSwap =
    std::constructible_from<Q, size_t, std::function<void(size_t, size_t)>>;

template<
    template<class> class Queue,
    class Alive,
    class Dead = Alive,
    typename Index = size_t,
    bool AutoPrune = true
> requires ValidStableQueue<Queue, Alive, Dead, Index>
class StableQueue {
private:
    static constexpr Queue<Alive> makeAlive(size_t aliveCap, StableQueue& self)
        requires AcceptsSwap<Queue<Alive>>
    {
        return Queue<Alive>(aliveCap, Swapper{self});
    }

    static constexpr Queue<Alive> makeAlive(size_t aliveCap, StableQueue&)
        requires (!AcceptsSwap<Queue<Alive>>)
    {
        return Queue<Alive>(aliveCap);
    }

public:
    using IndexType = Index;
    static constexpr IndexType NO_INDEX = std::numeric_limits<IndexType>::max();

    constexpr StableQueue(size_t capacity = 2) : StableQueue(capacity / 2, capacity / 2) {}

    constexpr StableQueue(size_t aliveCap, size_t deadCap) :
        m_alive(makeAlive(std::max(aliveCap, size_t(1)), *this))
    {
        m_dead.reserve(std::max(deadCap, size_t(1)));
        m_prevIndex.reserve(m_alive.capacity() + m_dead.capacity());
    }

    StableQueue(const StableQueue&) = delete;
    StableQueue& operator=(const StableQueue&) = delete;

    StableQueue(StableQueue&&) = delete;
    StableQueue& operator=(StableQueue&&) = delete;

    [[nodiscard]] constexpr size_t totalSize() const { return m_prevIndex.size(); }
    [[nodiscard]] constexpr size_t aliveSize() const { return m_alive.size(); }
    [[nodiscard]] constexpr size_t deadSize() const { return m_dead.size(); }
    [[nodiscard]] constexpr bool empty() const { return m_alive.empty(); }

    constexpr void clear() {
        m_alive.clear();
        m_dead.clear();
        m_prevIndex.clear();
    }

    [[nodiscard]] constexpr IndexType peekIndex() const {
        return empty() ? NO_INDEX : indexCast(m_dead.size());
    }

    constexpr void push(Alive&& item) {
        if constexpr (AutoPrune) {
            pruneBeforeGrowth(1);
        }
        const IndexType prevIndex = peekIndex();
        m_prevIndex.push_back(prevIndex);
        m_alive.push(std::move(item));
        checkInvariants();
    }

    constexpr void push(const Alive& item) {
        Alive copy = item;
        push(std::move(copy));
    }

    template <std::ranges::input_range R>
    requires std::constructible_from<
                 std::pair<Alive, Index>,
                 std::ranges::range_reference_t<R>
             > && std::ranges::sized_range<R>
    constexpr void pushIndexedRange(R&& rng) {
        if constexpr (AutoPrune) {
            pruneBeforeGrowth(size_t(std::ranges::size(rng)));
        }
        m_prevIndex.append_range(rng | std::views::values);
        m_alive.pushRange(rng | std::views::keys);
        checkInvariants();
    }

    [[nodiscard]] constexpr Dead at(IndexType index) const {
        if (index >= m_prevIndex.size()) {
            throw std::invalid_argument("Index out of range");
        }
        if (index < m_dead.size()) {
            return m_dead[index];
        }
        return Dead(m_alive.at(index));
    }

    [[nodiscard]] constexpr IndexType getPrevIndex(IndexType index) const {
        if (index >= m_prevIndex.size()) {
            throw std::invalid_argument("Index out of range");
        }
        return m_prevIndex[index];
    }

    [[nodiscard]] constexpr const Alive& peek() const {
        if (empty()) {
            throw std::runtime_error("Cannot peek an empty queue");
        }
        return m_alive.peek();
    }

    constexpr void removeFront() {
        if (empty()) {
            throw std::runtime_error("Cannot remove front from an empty queue");
        }
        auto it = m_dead.insert(m_dead.end(), Dead{});
        *it = Dead(m_alive.pop());
        checkInvariants();
    }

    template<
        template<class> class DeadQueue,
        typename DeadIndex,
        bool DeadAutoPrune,
        std::ranges::input_range R
    > requires (sizeof(DeadIndex) <= sizeof(Index)) && std::ranges::sized_range<R>
    constexpr void pushDeadQueue(
        const StableQueue<DeadQueue, Dead, Dead, DeadIndex, DeadAutoPrune>& deadQueue,
        R&& items
    ) {
        if (m_dead.empty() || deadQueue.deadSize()) {
            throw std::runtime_error("Cannot push a dead queue if either of them have no dead items");
        }

        const auto itemsSize = size_t(std::ranges::size(items));
        if (itemsSize != deadQueue.aliveSize()) {
            throw std::invalid_argument("The items must be alive in the dead queue");
        }

        if (deadQueue.empty()) {
            return;
        }

        if constexpr (AutoPrune) {
            pruneBeforeGrowth(deadQueue.deadSize() + itemsSize);
        }

        const IndexType deadRoot = indexCast(m_dead.size()) - 1;
        const IndexType offset = indexCast(deadQueue.deadSize()) - 1;

        m_prevIndex.insert(m_prevIndex.begin() + m_dead.size(), size_t(offset), {});
        m_dead.insert(m_dead.end(), size_t(offset), {});
        for (size_t i = 1; i < deadQueue.deadSize(); ++i) {
            m_dead[size_t(deadRoot) + i] = deadQueue.m_dead[i];
            m_prevIndex[size_t(deadRoot) + i] = IndexType(deadQueue.m_prevIndex[i]) + deadRoot;
        }

        const size_t oldIndexSize = m_prevIndex.size();
        for (size_t i = m_dead.size(); i < oldIndexSize; ++i) {
            if (m_prevIndex[i] > deadRoot) {
                m_prevIndex[i] += offset;
            }
        }

        m_prevIndex.insert(m_prevIndex.end(), itemsSize, {});
        for (size_t i = 0; i < itemsSize; ++i) {
            m_prevIndex[oldIndexSize + i] = IndexType(deadQueue.m_prevIndex[deadQueue.deadSize() + i]) + deadRoot;
        }
        m_alive.pushRange(items);

        checkInvariants();
    }

    constexpr void pruneDeadNodes() {
        const IndexType oldDeadSize = indexCast(m_dead.size());
        const IndexType oldAliveSize = indexCast(m_alive.size());
        const IndexType oldSize = indexCast(m_prevIndex.size());

        checkInvariants();

        if (oldSize == 0) {
            m_alive.clear();
            m_dead.clear();
            return;
        }

        std::vector<uint8_t> live(oldSize, 0);

        auto markLiveChain = [&](IndexType startIndex) {
            IndexType index = startIndex;

            while (index != NO_INDEX) {
                assert(index < oldSize);

                if (live[index]) {
                    break;
                }

                live[index] = 1;
                index = m_prevIndex[index];
            }
        };

        for (IndexType i = oldDeadSize; i < oldSize; ++i) {
            markLiveChain(i);
        }

        std::vector<IndexType> remap(oldSize, NO_INDEX);

        IndexType writeDeadIndex = 0;
        for (IndexType readDeadIndex = 0; readDeadIndex < oldDeadSize; ++readDeadIndex) {
            if (!live[readDeadIndex]) {
                continue;
            }

            remap[readDeadIndex] = writeDeadIndex;

            if (writeDeadIndex != readDeadIndex) {
                m_dead[writeDeadIndex] = std::move(m_dead[readDeadIndex]);
                m_prevIndex[writeDeadIndex] = m_prevIndex[readDeadIndex];
            }

            ++writeDeadIndex;
        }

        const IndexType newDeadSize = writeDeadIndex;
        const IndexType deadRemoved = oldDeadSize - newDeadSize;

        for (IndexType aliveOffset = 0; aliveOffset < oldAliveSize; ++aliveOffset) {
            const IndexType oldIndex = oldDeadSize + aliveOffset;
            const IndexType newIndex = newDeadSize + aliveOffset;

            remap[oldIndex] = newIndex;

            if (newIndex != oldIndex) {
                m_prevIndex[newIndex] = m_prevIndex[oldIndex];
            }
        }

        m_dead.erase(
            m_dead.begin() + newDeadSize,
            m_dead.end()
        );

        m_prevIndex.erase(
            m_prevIndex.begin() + newDeadSize + oldAliveSize,
            m_prevIndex.end()
        );

        // Remap all retained prevIndex links.
        for (size_t i = 0; i < m_prevIndex.size(); ++i) {
            const IndexType oldPrevIndex = m_prevIndex[i];

            if (oldPrevIndex == NO_INDEX) {
                continue;
            }
            assert(oldPrevIndex < oldSize); // StableQueue contains invalid prevIndex

            const IndexType newPrevIndex = remap[oldPrevIndex];
            assert(newPrevIndex != NO_INDEX); // StableQueue pruning removed a required parent

            m_prevIndex[i] = newPrevIndex;
        }
    }

private:
    static constexpr IndexType indexCast(size_t value) {
        if (value >= size_t(NO_INDEX)) {
            throw std::overflow_error("StableQueue index overflow");
        }
        return IndexType(value);
    }

    constexpr void pruneBeforeGrowth(size_t newElems) {
        const size_t free = m_prevIndex.capacity() - m_prevIndex.size();
        if (newElems <= free) {
            return;
        }

#ifdef VERBOSE_LOGS
        const size_t oldSize = m_prevIndex.size();
        const size_t oldDeadSize = m_dead.size();
        const size_t oldAliveSize = m_alive.size();
#endif

        const size_t oldCapacity = m_prevIndex.capacity();

        pruneDeadNodes();

        const size_t freeSlots = m_prevIndex.capacity() - m_prevIndex.size();
        const size_t minFreeSlots = std::max<size_t>(newElems, m_prevIndex.capacity() / 4);

#ifdef VERBOSE_LOGS
        const size_t newSize = m_prevIndex.size();
        const size_t removed = oldSize - newSize;

        std::cout
            << "StableQueue prune: "
            << "oldSize=" << oldSize
            << ", newSize=" << newSize
            << ", removed=" << removed
            << ", oldDead=" << oldDeadSize
            << ", newDead=" << m_dead.size()
            << ", oldAlive=" << oldAliveSize
            << ", newAlive=" << m_alive.size()
            << ", capacity=" << m_prevIndex.capacity()
            << ", freeSlots=" << freeSlots
            << '\n';
#endif

        if (freeSlots >= minFreeSlots) {
#ifdef VERBOSE_LOGS
            std::cout << "StableQueue prune avoided reserve\n";
#endif
            return;
        }

        const size_t growthCapacity = std::max<size_t>(
            oldCapacity + 1,
            oldCapacity * 21 / 13
        );

        const size_t newCapacity = std::max<size_t>(
            growthCapacity,
            m_prevIndex.size() + minFreeSlots
        );

#ifdef VERBOSE_LOGS
        std::cout
            << "StableQueue prune insufficient; reserving "
            << newCapacity
            << '\n';
#endif

        m_prevIndex.reserve(newCapacity);
    }

    constexpr void checkInvariants() const {
#ifndef NDEBUG
        assert(m_prevIndex.size() == m_dead.size() + m_alive.size());

        for (size_t i = 0; i < m_prevIndex.size(); ++i) {
            const IndexType prevIndex = m_prevIndex[i];

            if (prevIndex == NO_INDEX) {
                continue;
            }

            // Pruning remap error
            assert(size_t(prevIndex) < m_prevIndex.size());

            // Circular
            assert(size_t(prevIndex) != i);

            // Backwards
            assert(size_t(prevIndex) < i);

            // Pointing to the middle of the active queue
            assert(i < m_dead.size() || size_t(prevIndex) <= m_dead.size());
        }
#endif
    }

    template<
        template<class> class OtherQueue,
        class OtherAlive,
        class OtherDead,
        typename OtherIndex,
        bool OtherAutoPrune
    > requires ValidStableQueue<OtherQueue, OtherAlive, OtherDead, OtherIndex>
    friend class StableQueue;

    struct Swapper {
        StableQueue& queue;

        constexpr void operator()(size_t lhs, size_t rhs) {
            const size_t offset = queue.m_dead.size();
            std::swap(queue.m_prevIndex[offset + lhs], queue.m_prevIndex[offset + rhs]);
        }
    };

    Queue<Alive> m_alive;
    std::vector<Dead> m_dead;
    std::vector<IndexType> m_prevIndex;
};