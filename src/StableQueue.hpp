#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Queue.hpp"

static constexpr size_t NO_INDEX = std::numeric_limits<size_t>::max();

template<typename Alive, typename Dead = Alive>
class StableQueue {
public:
    static_assert(std::constructible_from<Dead, Alive>);

    constexpr StableQueue(size_t capacity = 1) :
        m_alive(std::max(capacity / 2, size_t(1))) {
        m_dead.reserve(std::max(capacity / 2, size_t(1)));
        m_prevIndex.reserve(std::max(capacity, size_t(1)));
    }

    [[nodiscard]] constexpr size_t size() const { return m_alive.size(); }
    [[nodiscard]] constexpr bool empty() const { return m_alive.empty(); }

    constexpr void clear() {
        m_alive.clear();
        m_dead.clear();
        m_prevIndex.clear();
    }

    constexpr size_t push(Alive&& item) {
        pruneBeforeGrowth();
        const size_t prevIndex = empty() ? NO_INDEX : m_dead.size();
        m_alive.push(std::move(item));
        m_prevIndex.push_back(prevIndex);
        assert(checkInvariants());
        return m_prevIndex.size() - 1;
    }

    constexpr size_t push(const Alive& item) {
        Alive copy = item;
        return push(std::move(copy));
    }

    [[nodiscard]] constexpr Dead at(size_t index) const {
        if (index >= m_prevIndex.size()) {
            throw std::invalid_argument("Index out of range");
        }
        if (index < m_dead.size()) {
            return m_dead[index];
        }
        return Dead(m_alive.at(index - m_dead.size()));
    }

    [[nodiscard]] constexpr size_t getPrevIndex(size_t index) const {
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
        m_dead.push_back(Dead(m_alive.pop()));
        assert(checkInvariants());
    }

private:
    constexpr void pruneBeforeGrowth() {
        if (m_prevIndex.size() < m_prevIndex.capacity()) {
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
        const size_t minFreeSlots = std::max<size_t>(8, m_prevIndex.capacity() / 4);

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

    constexpr void pruneDeadNodes() {
        const size_t oldDeadSize = m_dead.size();
        const size_t oldAliveSize = m_alive.size();
        const size_t oldSize = m_prevIndex.size();

        assert(checkInvariants());

        if (oldSize == 0) {
            m_alive.clear();
            m_dead.clear();
            return;
        }

        std::vector<uint8_t> live(oldSize, 0);

        auto markLiveChain = [&](size_t startIndex) {
            size_t index = startIndex;

            while (index != NO_INDEX) {
                assert(index < oldSize);

                if (live[index]) {
                    break;
                }

                live[index] = 1;
                index = m_prevIndex[index];
            }
        };

        for (size_t i = oldDeadSize; i < oldSize; ++i) {
            markLiveChain(i);
        }

        std::vector<size_t> remap(oldSize, NO_INDEX);

        size_t writeDeadIndex = 0;
        for (size_t readDeadIndex = 0; readDeadIndex < oldDeadSize; ++readDeadIndex) {
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

        const size_t newDeadSize = writeDeadIndex;
        const size_t deadRemoved = oldDeadSize - newDeadSize;

        for (size_t aliveOffset = 0; aliveOffset < oldAliveSize; ++aliveOffset) {
            const size_t oldIndex = oldDeadSize + aliveOffset;
            const size_t newIndex = newDeadSize + aliveOffset;

            remap[oldIndex] = newIndex;

            if (newIndex != oldIndex) {
                m_prevIndex[newIndex] = m_prevIndex[oldIndex];
            }
        }

        m_dead.erase(
            m_dead.begin() + static_cast<std::ptrdiff_t>(newDeadSize),
            m_dead.end()
        );

        m_prevIndex.erase(
            m_prevIndex.begin() + static_cast<std::ptrdiff_t>(newDeadSize + oldAliveSize),
            m_prevIndex.end()
        );

        // Remap all retained prevIndex links.
        for (size_t i = 0; i < m_prevIndex.size(); ++i) {
            size_t oldPrevIndex = m_prevIndex[i];

            if (oldPrevIndex == NO_INDEX) {
                continue;
            }
            assert(oldPrevIndex < oldSize); // StableQueue contains invalid prevIndex

            size_t newPrevIndex = remap[oldPrevIndex];
            assert(newPrevIndex != NO_INDEX); // StableQueue pruning removed a required parent

            m_prevIndex[i] = newPrevIndex;
        }
    }

    [[nodiscard]] constexpr bool checkInvariants() const {
        if (m_prevIndex.size() != m_dead.size() + m_alive.size()) {
            return false;
        }

        for (size_t i = 0; i < m_prevIndex.size(); ++i) {
            const size_t prevIndex = m_prevIndex[i];

            if (prevIndex != NO_INDEX && prevIndex >= m_prevIndex.size()) {
                return false;
            }
        }

        return true;
    }

private:
    Queue<Alive> m_alive;
    std::vector<Dead> m_dead;
    std::vector<size_t> m_prevIndex;
};