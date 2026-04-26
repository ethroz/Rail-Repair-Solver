#pragma once

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

static constexpr size_t NO_INDEX = std::numeric_limits<size_t>::max();

template<typename T>
class StableQueue {
private:
    struct Item {
        size_t prevIndex;
        T data;
    };

public:
    constexpr StableQueue(size_t capacity = 1) {
        m_data.reserve(std::max(capacity, size_t(1)));
    }

    [[nodiscard]] constexpr size_t size() const { return m_data.size() - m_front; }
    [[nodiscard]] constexpr bool empty() const { return size() == 0; }

    constexpr void clear() {
        m_front = 0;
        m_data.clear();
    }

    constexpr size_t push(T&& item, size_t prevIndex = NO_INDEX) {
        prevIndex = pruneBeforeGrowth(prevIndex);
        m_data.push_back({prevIndex, std::move(item)});
        return m_data.size() - 1;
    }

    constexpr size_t push(const T& item, size_t prevIndex = NO_INDEX) {
        prevIndex = pruneBeforeGrowth(prevIndex);
        m_data.push_back({prevIndex, item});
        return m_data.size() - 1;
    }

    [[nodiscard]] constexpr const T& at(size_t index) const {
        if (index >= m_data.size()) {
            throw std::runtime_error("Index out of range");
        }
        return m_data[index].data;
    }

    [[nodiscard]] constexpr size_t getPrevIndex(size_t index) const {
        if (index >= m_data.size()) {
            throw std::runtime_error("Index out of range");
        }
        return m_data[index].prevIndex;
    }

    [[nodiscard]] constexpr size_t index() const {
        return m_front;
    }

    [[nodiscard]] constexpr const T& peek() const {
        if (empty()) {
            throw std::runtime_error("Cannot peek an empty queue");
        }
        return m_data[m_front].data;
    }

    [[nodiscard]] constexpr const T& pop() {
        if (empty()) {
            throw std::runtime_error("Cannot pop from an empty queue");
        }
        return m_data[m_front++].data;
    }

    constexpr void removeFront() {
        if (empty()) {
            throw std::runtime_error("Cannot remove front from an empty queue");
        }
        ++m_front;
    }

private:
    constexpr size_t pruneBeforeGrowth(size_t pendingPrevIndex) {
        if (pendingPrevIndex != NO_INDEX && pendingPrevIndex >= m_data.size()) {
            throw std::runtime_error("prevIndex out of range");
        }

        if (m_data.size() < m_data.capacity()) {
            return pendingPrevIndex;
        }

#ifdef VERBOSE_LOGS
        const size_t oldSize = m_data.size();
        const size_t oldFront = m_front;
#endif
        const size_t oldCapacity = m_data.capacity();

        pendingPrevIndex = pruneDeadNodes(pendingPrevIndex);

#ifdef VERBOSE_LOGS
        const size_t newSize = m_data.size();
        const size_t newCapacity = m_data.capacity();
        const size_t removed = oldSize - newSize;
        const size_t reclaimedSlots = newCapacity - newSize;

        // Add your logging here.
        //
        // Example:
        std::cout
            << "StableQueue prune: "
            << "oldSize=" << oldSize
            << ", newSize=" << newSize
            << ", removed=" << removed
            << ", oldCapacity=" << oldCapacity
            << ", newCapacity=" << newCapacity
            << ", reclaimedSlots=" << reclaimedSlots
            << ", oldFront=" << oldFront
            << ", newFront=" << m_front
            << '\n';
#endif

        const size_t freeSlots = m_data.capacity() - m_data.size();
        const size_t minFreeSlots = std::max<size_t>(8, m_data.capacity() / 4);

        if (freeSlots >= minFreeSlots) {
#ifdef VERBOSE_LOGS
            std::cout << "StableQueue prune avoided reserve\n";
#endif
            return pendingPrevIndex;
        }

#ifdef VERBOSE_LOGS
        std::cout << "StableQueue prune insufficient; reserving more capacity\n";
#endif

        const size_t newReservedCapacity = std::max<size_t>(
            oldCapacity * 2,
            m_data.size() + minFreeSlots
        );

        m_data.reserve(newReservedCapacity);

        return pendingPrevIndex;
    }

    constexpr size_t pruneDeadNodes(size_t pendingPrevIndex) {
        const size_t oldSize = m_data.size();

        if (oldSize == 0) {
            m_front = 0;
            return NO_INDEX;
        }

        std::vector<unsigned char> live(oldSize, 0);

        auto markLiveChain = [&](size_t startIndex) {
            size_t index = startIndex;

            while (index != NO_INDEX) {
                assert(index < oldSize);

                if (live[index]) {
                    break;
                }

                live[index] = 1;
                index = m_data[index].prevIndex;
            }
        };

        // Keep all root nodes, even if they are not active.
        for (size_t i = 0; i < oldSize; ++i) {
            if (m_data[i].prevIndex == NO_INDEX) {
                live[i] = 1;
            }
        }

        // Active queue entries are roots. Their prevIndex chains are live recursively.
        for (size_t i = m_front; i < oldSize; ++i) {
            markLiveChain(i);
        }

        std::vector<size_t> remap(oldSize, NO_INDEX);

        size_t writeIndex = 0;

        for (size_t readIndex = 0; readIndex < oldSize; ++readIndex) {
            if (!live[readIndex]) {
                continue;
            }

            remap[readIndex] = writeIndex;

            if (writeIndex != readIndex) {
                m_data[writeIndex] = std::move(m_data[readIndex]);
            }

            ++writeIndex;
        }

        m_data.erase(
            m_data.begin() + static_cast<std::ptrdiff_t>(writeIndex),
            m_data.end()
        );

        for (size_t i = 0; i < m_data.size(); ++i) {
            size_t oldPrevIndex = m_data[i].prevIndex;

            if (oldPrevIndex != NO_INDEX) {
                size_t newPrevIndex = remap[oldPrevIndex];
                assert(newPrevIndex != NO_INDEX);
                m_data[i].prevIndex = newPrevIndex;
            }
        }

        if (m_data.empty()) {
            m_front = 0;
        } else if (m_front < oldSize) {
            m_front = remap[m_front];
            assert(m_front != NO_INDEX);
        } else {
            m_front = m_data.size();
        }

        if (pendingPrevIndex == NO_INDEX) {
            return NO_INDEX;
        }

        size_t newPendingPrevIndex = remap[pendingPrevIndex];
        assert(newPendingPrevIndex != NO_INDEX);

        return newPendingPrevIndex;
    }

private:
    std::vector<Item> m_data;
    size_t m_front = 0;
};