#pragma once

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

template<typename T, typename Compare = std::less<T>>
class PriorityQueue {
public:
    constexpr PriorityQueue() = default;

    constexpr PriorityQueue(size_t capacity) {
        m_data.reserve(std::max(capacity, size_t(1)));
    }

    [[nodiscard]] constexpr bool empty() const { return m_data.empty(); }

    [[nodiscard]] constexpr size_t size() const { return m_data.size(); }

    constexpr void clear() { m_data.clear(); }

    [[nodiscard]] constexpr const T& minimum() const {
        if (empty()) {
            throw std::runtime_error("The queue is empty");
        }
        return m_data[0];
    }

    constexpr void insert(T&& item) {
        m_data.push_back(std::move(item));
        bubbleUp();
    }

    constexpr void insert(const T& item) {
        m_data.push_back(item);
        bubbleUp();
    }

    [[nodiscard]] constexpr T extract_min() {
        if (empty()) {
            throw std::runtime_error("Cannot pop from an empty queue");
        }

        T item{};
        std::swap(item, m_data[0]);
        std::swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();
        bubbleDown();
        return item;
    }
    
private:    
    constexpr void bubbleUp() {
        size_t currentIndex = m_data.size() - 1;
        while (currentIndex > 0) {
            const size_t parentIndex = (currentIndex - 1) / 2;
            if (lessThan(m_data[parentIndex], m_data[currentIndex])) {
                break;
            }
            std::swap(m_data[currentIndex], m_data[parentIndex]);
            currentIndex = parentIndex;
        }
    }

    constexpr void bubbleDown() {
        size_t currentIndex = 0;
        while (true) {
            const size_t leftIndex = 2 * currentIndex + 1;
            const size_t rightIndex = 2 * currentIndex + 2;
            size_t smallestIndex = currentIndex;

            if (leftIndex < m_data.size() && lessThan(m_data[leftIndex], m_data[smallestIndex])) {
                smallestIndex = leftIndex;
            }

            if (rightIndex < m_data.size() && lessThan(m_data[rightIndex], m_data[smallestIndex])) {
                smallestIndex = rightIndex;
            }

            if (smallestIndex == currentIndex) {
                break;
            }

            std::swap(m_data[currentIndex], m_data[smallestIndex]);
            currentIndex = smallestIndex;
        }
    }

    std::vector<T> m_data;
    Compare lessThan;
};
