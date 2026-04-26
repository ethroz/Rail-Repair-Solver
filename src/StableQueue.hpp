#pragma once

#include <stdexcept>
#include <vector>
#include <limits>

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
        m_data.push_back({std::move(item), prevIndex});
        return m_data.size() - 1;
    }

    constexpr size_t push(const T& item, size_t prevIndex = NO_INDEX) {
        m_data.push_back({item, prevIndex});
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

private:
    std::vector<Item> m_data;
    size_t m_front = 0;
};
