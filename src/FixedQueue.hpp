#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <concepts>
#include <utility>

template<typename T, size_t N>
requires std::move_constructible<T>
class FixedQueue {
public:
    constexpr FixedQueue() = default;

    [[nodiscard]] constexpr size_t size() const { return m_size; }
    [[nodiscard]] constexpr size_t capacity() const { return N; }
    [[nodiscard]] constexpr bool empty() const { return m_size == 0; }

    constexpr void clear() {
        m_front = 0;
        m_back = 0;
        m_size = 0;
    }

    [[nodiscard]] constexpr const T& peek() const {
        assert(!empty());
        return m_data[m_front];
    }

    template<typename U>
    requires std::constructible_from<T, U&&>
    constexpr void push(U&& value) {
        assert(m_size < N);
        m_data[m_back] = std::forward<U>(value);
        ++m_size;
        ++m_back;
        if (m_back == N) {
            m_back = 0;
        }
    }

    [[nodiscard]] constexpr T pop() {
        assert(!empty());
        T value = std::move(m_data[m_front]);
        --m_size;
        ++m_front;
        if (m_front == N) {
            m_front = 0;
        }
        return value;
    }

private:
    std::array<T, N> m_data;
    size_t m_size = 0;
    size_t m_front = 0;
    size_t m_back = 0;
};
