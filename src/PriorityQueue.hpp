#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

template<typename T, typename Compare = std::less<T>>
class PriorityQueue {
public:
    static constexpr std::size_t Arity = 4;

    constexpr PriorityQueue() = default;

    explicit constexpr PriorityQueue(std::size_t capacity) {
        m_data.reserve(capacity);
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return m_data.empty();
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept {
        return m_data.size();
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return m_data.capacity();
    }

    constexpr void clear() noexcept {
        m_data.clear();
    }

    constexpr void reserve(std::size_t capacity) {
        m_data.reserve(capacity);
    }

    [[nodiscard]] constexpr const T& minimum() const {
        if (empty()) {
            throw std::runtime_error("The queue is empty");
        }

        return m_data.front();
    }

    constexpr void insert(const T& item) {
        m_data.push_back(item);
        bubbleUp(m_data.size() - 1);
    }

    constexpr void insert(T&& item) {
        m_data.push_back(std::move(item));
        bubbleUp(m_data.size() - 1);
    }

    [[nodiscard]] constexpr T extract_min() {
        if (empty()) {
            throw std::runtime_error("Cannot pop from an empty queue");
        }

        T result = std::move(m_data.front());

        if (m_data.size() == 1) {
            m_data.pop_back();
            return result;
        }

        m_data.front() = std::move(m_data.back());
        m_data.pop_back();

        bubbleDown(0);

        return result;
    }

private:
    [[nodiscard]] static constexpr std::size_t parentIndex(std::size_t index) noexcept {
        return (index - 1) / Arity;
    }

    [[nodiscard]] static constexpr std::size_t firstChildIndex(std::size_t index) noexcept {
        return Arity * index + 1;
    }

    constexpr void bubbleUp(std::size_t index) {
        T value = std::move(m_data[index]);

        while (index > 0) {
            const std::size_t parent = parentIndex(index);

            // Stop when parent <= value.
            if (!m_compare(value, m_data[parent])) {
                break;
            }

            m_data[index] = std::move(m_data[parent]);
            index = parent;
        }

        m_data[index] = std::move(value);
    }

    constexpr void bubbleDown(std::size_t index) {
        const std::size_t count = m_data.size();
        T value = std::move(m_data[index]);

        while (true) {
            const std::size_t firstChild = firstChildIndex(index);

            if (firstChild >= count) {
                break;
            }

            std::size_t smallestChild = firstChild;
            const std::size_t lastChild = std::min(firstChild + Arity, count);

            for (std::size_t child = firstChild + 1; child < lastChild; ++child) {
                if (m_compare(m_data[child], m_data[smallestChild])) {
                    smallestChild = child;
                }
            }

            // Stop when value <= smallest child.
            if (!m_compare(m_data[smallestChild], value)) {
                break;
            }

            m_data[index] = std::move(m_data[smallestChild]);
            index = smallestChild;
        }

        m_data[index] = std::move(value);
    }

    std::vector<T> m_data;
    Compare m_compare{};
};