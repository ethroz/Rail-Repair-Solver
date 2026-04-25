#pragma once

#include <stdexcept>
#include <vector>

template<typename T>
class StableQueue {
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

    constexpr void push(T&& item) {
        m_data.push_back(std::move(item));
    }

    constexpr void push(const T& item) {
        m_data.push_back(item);
    }

    [[nodiscard]] constexpr const T& at(size_t index) const {
        if (m_front > m_data.size()) {
            throw std::runtime_error("Index out of range");
        }
        return m_data.at(index);
    }

    [[nodiscard]] constexpr size_t index() const {
        return m_front;
    }

    [[nodiscard]] constexpr const T& peek() const {
        if (empty()) {
            throw std::runtime_error("Cannot peek an empty queue");
        }
        return m_data.at(m_front);
    }

    [[nodiscard]] constexpr const T& pop() {
        if (empty()) {
            throw std::runtime_error("Cannot pop from an empty queue");
        }
        return m_data.at(m_front++);
    }

private:
    std::vector<T> m_data;
    size_t m_front = 0;
};
