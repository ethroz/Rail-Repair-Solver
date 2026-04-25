#pragma once

#include <stdexcept>

#include <plf_list.h>

template<typename T>
class StableQueue {
public:
    constexpr StableQueue(size_t capacity = 1) {
        m_data.reserve(std::max(capacity, size_t(1)));
    }

    [[nodiscard]] constexpr size_t size() const { return m_size; }
    [[nodiscard]] constexpr bool empty() const { return m_size == 0; }

    constexpr void clear() {
        m_front = {};
        m_size = 0;
        m_data.clear();
    }

    constexpr void push(T&& item) {
        m_data.push_back(std::move(item));
        if (empty()) {
            m_front = --m_data.end();
        }
        ++m_size;
    }

    constexpr void push(const T& item) {
        T copy = item;
        push(std::move(copy));
    }

    [[nodiscard]] constexpr const T& peek() const {
        if (empty()) {
            throw std::runtime_error("Cannot peek an empty queue");
        }
        return *m_front;
    }

    [[nodiscard]] constexpr const T& pop() {
        if (empty()) {
            throw std::runtime_error("Cannot pop from an empty queue");
        }
        const T& item = *m_front;
        ++m_front;
        --m_size;
        return item;
    }

private:
    plf::list<T> m_data;
    plf::list<T>::iterator m_front;
    size_t m_size = 0;
};
