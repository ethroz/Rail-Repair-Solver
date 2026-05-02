#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <concepts>
#include <utility>

template<typename T, std::unsigned_integral I, size_t N>
requires std::move_constructible<T>
class FixedIndexedQueue {
public:
    constexpr FixedIndexedQueue() = default;

    [[nodiscard]] constexpr size_t size() const { return m_size; }
    [[nodiscard]] constexpr size_t capacity() const { return N; }
    [[nodiscard]] constexpr bool empty() const { return m_size == 0; }

    constexpr void clear() {
        m_front = 0;
        m_back = 0;
        m_size = 0;
    }

    [[nodiscard]] constexpr I& index(size_t i) {
        assert(i < m_size);
        return m_indices[(m_front + i) % N];
    }
    [[nodiscard]] constexpr const I& index(size_t i) const {
        assert(i < m_size);
        return m_indices[(m_front + i) % N];
    }

    [[nodiscard]] constexpr const T& peek() const {
        assert(!empty());
        return m_data[m_front];
    }

    template<typename U, typename V>
    requires std::constructible_from<T, U&&> && std::constructible_from<I, V&&>
    constexpr void push(U&& value, V&& index) {
        assert(m_size < N);
        m_data[m_back] = std::forward<U>(value);
        m_indices[m_back] = std::forward<V>(index);
        ++m_size;
        ++m_back;
        if (m_back == N) {
            m_back = 0;
        }
    }

    [[nodiscard]] constexpr std::pair<T, I> pop() {
        assert(!empty());
        T&& value = std::move(m_data[m_front]);
        I&& index = std::move(m_indices[m_front]);
        --m_size;
        ++m_front;
        if (m_front == N) {
            m_front = 0;
        }
        return {std::move(value), std::move(index)};
    }

private:
    std::array<T, N> m_data;
    std::array<I, N> m_indices;
    size_t m_size = 0;
    size_t m_front = 0;
    size_t m_back = 0;
};
