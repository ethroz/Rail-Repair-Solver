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

#include "Queue.hpp"

template<class Q, class T>
concept StableQueueStorage =
    std::constructible_from<Q, size_t> &&
    requires(Q queue, const Q& constQueue, T item, std::span<T> range) {
        { constQueue.size() } -> std::convertible_to<size_t>;
        { constQueue.capacity() } -> std::convertible_to<size_t>;
        { constQueue.empty() } -> std::convertible_to<bool>;
        queue.clear();
        queue.push(item);
        queue.push(std::move(item));
        queue.pushRange(range);
        { constQueue.peek() } -> std::same_as<const T&>;
        { queue.pop() } -> std::convertible_to<T>;
    };
    
template<template<class...> class Q, class A, class D, typename I>
concept ValidStableQueue = 
    StableQueueStorage<Q<A>, A> &&
    std::constructible_from<D, A> &&
    std::unsigned_integral<I>;

template<class Q, class F>
concept AcceptsSwap =
    std::constructible_from<Q, size_t, F>;

template<
    template<class...> class MyQueue,
    class Alive,
    class Dead = Alive,
    typename Index = size_t,
    bool AutoPrune = true
> requires ValidStableQueue<MyQueue, Alive, Dead, Index>
class StableQueue {
private:
    struct Swapper;
    static constexpr bool Swappable = AcceptsSwap<MyQueue<Alive>, Swapper>;
    
    using IndexType = Index;
    static constexpr IndexType NO_INDEX = std::numeric_limits<IndexType>::max();

    static inline MyQueue<Alive> makeAlive(size_t aliveCap, StableQueue& self)
        requires (Swappable)
    {
        return MyQueue<Alive>(aliveCap, Swapper{self});
    }

    static inline MyQueue<Alive> makeAlive(size_t aliveCap, StableQueue&)
        requires (!Swappable)
    {
        return MyQueue<Alive>(aliveCap);
    }

public:
    inline StableQueue(size_t capacity = 2) : StableQueue(capacity / 2, capacity / 2) {}

    inline StableQueue(size_t aliveCap, size_t deadCap, size_t extraCap = 0) :
        m_alive(makeAlive(std::max<size_t>(aliveCap, 1), *this))
    {
        m_dead.reserve(std::max<size_t>(deadCap, 1));
        m_deadPrevIndex.reserve(m_dead.capacity());
        m_alivePrevIndex.reserve(m_alive.capacity());
        m_extraPrevIndex.reserve(extraCap);
    }

    StableQueue(const StableQueue&) = delete;
    StableQueue& operator=(const StableQueue&) = delete;

    StableQueue(StableQueue&&) = delete;
    StableQueue& operator=(StableQueue&&) = delete;

    [[nodiscard]] constexpr size_t size() const { return m_alive.size(); }
    [[nodiscard]] constexpr size_t deadSize() const { return m_dead.size(); }
    [[nodiscard]] constexpr bool empty() const { return m_alive.empty(); }
    [[nodiscard]] constexpr bool isDead() const { return !m_dead.empty() && m_alive.empty(); }

    constexpr void reset() {
        m_alive.clear();
        m_dead.clear();
        m_deadPrevIndex.clear();
        m_alivePrevIndex.clear();
        m_extraPrevIndex.clear();
        m_rejectFrontSwap = false;
    }

    constexpr void push(Alive&& item) {
        if (isDead()) {
            throw std::invalid_argument("Cannot push onto a dead queue");
        }
        if constexpr (AutoPrune) {
            pruneBeforeGrowth(1);
        }
        const IndexType prevIndex = peekIndex();
        m_alivePrevIndex.push(prevIndex);
        const PushSwapGuard guard(*this);
        m_alive.push(std::forward<Alive>(item));
        checkInvariants();
    }

    constexpr void push(const Alive& item) requires std::copy_constructible<Alive> {
        if (isDead()) {
            throw std::invalid_argument("Cannot push onto a dead queue");
        }
        if constexpr (AutoPrune) {
            pruneBeforeGrowth(1);
        }
        const IndexType prevIndex = peekIndex();
        m_alivePrevIndex.push(prevIndex);
        const PushSwapGuard guard(*this);
        m_alive.push(item);
        checkInvariants();
    }

    constexpr void addRefForFront() {
        if (empty()) {
            throw std::runtime_error("Cannot ref the front of an empty queue");
        }
        if constexpr (AutoPrune) {
            pruneBeforeGrowth(1);
        }
        const IndexType prevIndex = peekIndex();
        m_extraPrevIndex.push_back(prevIndex);
        checkInvariants();
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
        m_dead.push_back(Dead(m_alive.pop()));
        assert(!m_alivePrevIndex.empty());
        if constexpr (Swappable) {
            m_deadPrevIndex.push_back(m_alivePrevIndex.popBack());
        }
        else {
            m_deadPrevIndex.push_back(m_alivePrevIndex.pop());
        }
        checkInvariants();
    }

    template<
        template<class...> class SubQueue,
        typename SubAlive,
        typename SubDead,
        typename SubIndex,
        bool SubAutoPrune,
        std::ranges::input_range R
    > requires
        std::convertible_to<SubDead, Dead> &&
        (sizeof(SubIndex) <= sizeof(Index)) &&
        std::ranges::sized_range<R> &&
        std::constructible_from<
            Alive,
            std::ranges::range_reference_t<R>
        >
    constexpr void removeFrontWithDeadSubqueue(
        const StableQueue<SubQueue, SubAlive, SubDead, SubIndex, SubAutoPrune>& subqueue,
        R&& items
    ) {
        using SubQueueType = std::remove_cvref_t<decltype(subqueue)>;
        const size_t itemsSize = size_t(std::ranges::size(items));

        if (itemsSize != subqueue.m_extraPrevIndex.size()) {
            throw std::invalid_argument("There should be as many extra refs as items in the subqueue");
        }

        if (itemsSize == 0) {
            removeFront();
            return;
        }

        if (!subqueue.isDead()) {
            throw std::invalid_argument("Cannot push an alive subqueue");
        }
        assert(subqueue.deadSize() > 0);

        const size_t numDeadToKeep = subqueue.deadSize() - 1;

        if constexpr (AutoPrune) {
            pruneBeforeGrowth(numDeadToKeep + itemsSize);
        }

        removeFront();
        assert(!m_dead.empty());

        const IndexType rootIndex = indexCast(m_dead.size()) - 1;
        auto rebaseSubqueueIndex = [&](auto&& i) {
            assert(i != SubQueueType::NO_INDEX);
            return IndexType(i) + rootIndex;
        };
        m_deadPrevIndex.append_range(
            subqueue.m_deadPrevIndex |
            std::views::drop(1) |
            std::views::transform(rebaseSubqueueIndex)
        );
        m_dead.append_range(subqueue.m_dead | std::views::drop(1));

        assert(subqueue.size() == 0);
        m_alivePrevIndex.pushRange(
            subqueue.m_extraPrevIndex |
            std::views::transform(rebaseSubqueueIndex)
        );
        m_alive.pushRange(std::forward<R>(items));

        checkInvariants();
    }

    constexpr void pruneDead() {
        const IndexType oldDeadSize = indexCast(m_dead.size());
        const IndexType aliveAndExtraSize = indexCast(m_alive.size() + m_extraPrevIndex.size());
        const IndexType oldSize = indexCast(prevIndexSize());

        checkInvariants();

        if (oldSize == 0) {
            m_alive.clear(); // Allows the queue to reset itself.
            assert(m_dead.empty());
            assert(m_deadPrevIndex.empty());
            assert(m_alivePrevIndex.empty());
            assert(m_extraPrevIndex.empty());
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
                index = prevIndexAt(index);
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
                m_deadPrevIndex[writeDeadIndex] = m_deadPrevIndex[readDeadIndex];
            }

            ++writeDeadIndex;
        }

        const IndexType newDeadSize = writeDeadIndex;

        for (IndexType i = 0; i < aliveAndExtraSize; ++i) {
            const IndexType oldIndex = oldDeadSize + i;
            const IndexType newIndex = newDeadSize + i;
            remap[oldIndex] = newIndex;
        }

        m_dead.erase(
            m_dead.begin() + newDeadSize,
            m_dead.end()
        );

        m_deadPrevIndex.erase(
            m_deadPrevIndex.begin() + newDeadSize,
            m_deadPrevIndex.end()
        );

        // Remap all retained prevIndex links.
        remapPrevIndices(m_deadPrevIndex, oldSize, remap);
        remapPrevIndices(m_alivePrevIndex, oldSize, remap);
        remapPrevIndices(m_extraPrevIndex, oldSize, remap);

        checkInvariants();
    }

    struct chain_iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Dead;
        using reference = value_type;
        using pointer = void;
        using const_reference = const reference;
        using const_pointer = const pointer;

        constexpr chain_iterator(const StableQueue& owner, StableQueue::IndexType index) :
            m_owner(owner),
            m_index(index)
        {}

        constexpr reference operator*() const { return m_owner.at(m_index); }

        constexpr chain_iterator& operator++() { m_index = m_owner.getPrevIndex(m_index); return *this; }
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
        const StableQueue& m_owner;
        StableQueue::IndexType m_index;
    };

    constexpr chain_iterator begin() const { return chain_iterator(*this, peekIndex()); }
    constexpr chain_iterator end() const { return chain_iterator(*this, NO_INDEX); }

private:
    [[nodiscard]] static constexpr IndexType indexCast(size_t value) {
        if (value >= size_t(NO_INDEX)) {
            throw std::overflow_error("StableQueue index overflow");
        }
        return IndexType(value);
    }

    [[nodiscard]] constexpr size_t prevIndexSize() const {
        return m_deadPrevIndex.size() + m_alivePrevIndex.size() + m_extraPrevIndex.size();
    }

    [[nodiscard]] constexpr size_t prevIndexCapacity() const {
        return m_deadPrevIndex.capacity() + m_alivePrevIndex.capacity() + m_extraPrevIndex.capacity();
    }

    [[nodiscard]] constexpr IndexType prevIndexAt(IndexType index) const {
        const size_t rawIndex = size_t(index);
        const size_t deadSize = m_deadPrevIndex.size();
        const size_t aliveSize = m_alivePrevIndex.size();

        if (rawIndex < deadSize) {
            return m_deadPrevIndex[rawIndex];
        }
        if (rawIndex < deadSize + aliveSize) {
            return m_alivePrevIndex[rawIndex - deadSize];
        }
        if (rawIndex < deadSize + aliveSize + m_extraPrevIndex.size()) {
            return m_extraPrevIndex[rawIndex - deadSize - aliveSize];
        }

        throw std::invalid_argument("Index out of range");
    }

    template<class PrevIndexContainer>
    constexpr void remapPrevIndices(
        PrevIndexContainer& prevIndices,
        IndexType oldSize,
        const std::vector<IndexType>& remap
    ) {
        for (IndexType& prevIndex : prevIndices) {
            if (prevIndex == NO_INDEX) {
                continue;
            }
            assert(prevIndex < oldSize); // StableQueue contains invalid prevIndex

            const IndexType newPrevIndex = remap[prevIndex];
            assert(newPrevIndex != NO_INDEX); // StableQueue pruning removed a required parent

            prevIndex = newPrevIndex;
        }
    }

    [[nodiscard]] constexpr IndexType peekIndex() const {
        return empty() ? NO_INDEX : indexCast(m_dead.size());
    }

    [[nodiscard]] constexpr IndexType getPrevIndex(IndexType index) const {
        if (index >= prevIndexSize()) {
            throw std::invalid_argument("Index out of range");
        }
        return prevIndexAt(index);
    }

    [[nodiscard]] constexpr Dead at(IndexType index) const {
        if (index == NO_INDEX) {
            throw std::invalid_argument("Index out of range");
        }

        if (size_t(index) < m_dead.size()) {
            return m_dead[index];
        }

        if (size_t(index) == m_dead.size()) {
            if (m_alive.empty()) {
                throw std::invalid_argument("There are no alive items available");
            }
            return Dead(m_alive.peek());
        }

        throw std::invalid_argument("Index out of range");
    }

    constexpr void pruneBeforeGrowth(size_t newElems) {
        const size_t free = prevIndexCapacity() - prevIndexSize();
        if (newElems <= free) {
            return;
        }

#ifdef VERBOSE_LOGS
        const size_t oldSize = prevIndexSize();
        const size_t oldDeadSize = m_dead.size();
        const size_t aliveSize = m_alive.size();
#endif

        const size_t oldCapacity = prevIndexCapacity();

        pruneDead();

        const size_t freeSlots = prevIndexCapacity() - prevIndexSize();
        const size_t minFreeSlots = std::max<size_t>(newElems, prevIndexCapacity() / 4);

#ifdef VERBOSE_LOGS
        const size_t newSize = prevIndexSize();
        const size_t removed = oldSize - newSize;

        std::cout
            << "StableQueue prune: "
            << "oldSize=" << oldSize
            << ", newSize=" << newSize
            << ", removed=" << removed
            << ", oldDead=" << oldDeadSize
            << ", newDead=" << m_dead.size()
            << ", oldAlive=" << aliveSize
            << ", newAlive=" << m_alive.size()
            << ", capacity=" << prevIndexCapacity()
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
            prevIndexSize() + minFreeSlots
        );

#ifdef VERBOSE_LOGS
        std::cout
            << "StableQueue prune insufficient; reserving "
            << newCapacity
            << '\n';
#endif

        const size_t minDeadCapacity = m_deadPrevIndex.size();
        const size_t minAliveCapacity = m_alivePrevIndex.size();
        const size_t minExtraCapacity = m_extraPrevIndex.size();
        const size_t remainingCapacity = newCapacity - prevIndexSize();

        m_deadPrevIndex.reserve(minDeadCapacity + remainingCapacity);
        m_alivePrevIndex.reserve(minAliveCapacity + remainingCapacity);
        m_extraPrevIndex.reserve(minExtraCapacity + remainingCapacity);
    }

    constexpr void checkInvariants() const {
#ifndef NDEBUG
        assert(m_dead.size() == m_deadPrevIndex.size());
        assert(m_alive.size() == m_alivePrevIndex.size());

        const size_t totalSize = prevIndexSize();
        const size_t deadSize = m_deadPrevIndex.size();
        const size_t aliveSize = m_alivePrevIndex.size();

        auto checkPrevIndex = [&](size_t logicalIndex, IndexType prevIndex) {
            if (logicalIndex == 0) {
                assert(prevIndex == NO_INDEX);
                return;
            }

            // Revived queue error
            assert(prevIndex != NO_INDEX);

            // Pruning remap error
            assert(size_t(prevIndex) < totalSize);

            // Circular
            assert(size_t(prevIndex) != logicalIndex);

            // Backwards
            assert(size_t(prevIndex) < logicalIndex);

            // Pointing to the middle of the active queue
            if (logicalIndex >= deadSize) {
                assert(size_t(prevIndex) <= deadSize);
            }
        };

        for (size_t i = 0; i < m_deadPrevIndex.size(); ++i) {
            checkPrevIndex(i, m_deadPrevIndex[i]);
        }

        for (size_t i = 0; i < m_alivePrevIndex.size(); ++i) {
            checkPrevIndex(deadSize + i, m_alivePrevIndex[i]);
        }

        for (size_t i = 0; i < m_extraPrevIndex.size(); ++i) {
            checkPrevIndex(deadSize + aliveSize + i, m_extraPrevIndex[i]);
        }
#endif
    }

    template<
        template<class...> class OtherQueue,
        class OtherAlive,
        class OtherDead,
        typename OtherIndex,
        bool OtherAutoPrune
    > requires ValidStableQueue<OtherQueue, OtherAlive, OtherDead, OtherIndex>
    friend class StableQueue;

    struct PushSwapGuard {
        StableQueue& q;
        constexpr explicit PushSwapGuard(StableQueue& q) : q(q) {
            q.m_rejectFrontSwap = true;
        }
        constexpr ~PushSwapGuard() {
            q.m_rejectFrontSwap = false;
        }
    };

    struct Swapper {
        StableQueue& queue;

        // Called by MyQueue when it swaps alive slots lhs/rhs.
        // Keeps m_alivePrevIndex aligned with the alive storage order.
        constexpr void operator()(size_t lhs, size_t rhs) {
            assert(lhs != rhs);
            if (queue.m_rejectFrontSwap && (lhs == 0 || rhs == 0)) {
                throw std::invalid_argument("Cannot push an item that would move the StableQueue front node");
            }
            std::swap(queue.m_alivePrevIndex[lhs], queue.m_alivePrevIndex[rhs]);
        }
    };

    MyQueue<Alive> m_alive;
    std::vector<Dead> m_dead;
    std::vector<IndexType> m_deadPrevIndex;
    Queue<IndexType> m_alivePrevIndex;
    std::vector<IndexType> m_extraPrevIndex;
    bool m_rejectFrontSwap = false;
};
