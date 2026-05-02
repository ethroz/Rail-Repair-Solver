#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <span>
#include <utility>

template <typename T, std::unsigned_integral I, size_t N>
class FixedIndexedVector {
public:
    constexpr FixedIndexedVector() = default;

    [[nodiscard]] constexpr size_t size() const { return m_size; }
    [[nodiscard]] constexpr size_t capacity() const { return N; }
    [[nodiscard]] constexpr bool full() const { return m_size == N; }
    [[nodiscard]] constexpr bool empty() const { return m_size == 0; }

    [[nodiscard]] constexpr T& value(size_t i) {
        assert(i < m_size);
        return m_data[i];
    }
    [[nodiscard]] constexpr const T& value(size_t i) const {
        assert(i < m_size);
        return m_data[i];
    }
    [[nodiscard]] constexpr std::span<const T> values() const {
        return std::span(m_data).subspan(0, m_size);
    }
    
    [[nodiscard]] constexpr I& index(size_t i) {
        assert(i < m_size);
        return m_indices[i];
    }
    [[nodiscard]] constexpr const I& index(size_t i) const {
        assert(i < m_size);
        return m_indices[i];
    }
    [[nodiscard]] constexpr std::span<const I> indices() const {
        return std::span(m_indices).subspan(0, m_size);
    }

    constexpr void clear() {
        m_size = 0;
    }

    template<typename U, typename V>
    requires std::constructible_from<T, U&&> && std::constructible_from<I, V&&>
    constexpr void push_back(U&& value, V&& index) {
        assert(m_size < N);
        m_data[m_size] = std::forward<U>(value);
        m_indices[m_size] = std::forward<V>(index);
        ++m_size;
    }

    constexpr void pop_back(size_t num = 1) {
        assert(m_size >= num);
        m_size -= num;
    }

    constexpr void move_assign_from(size_t dst, size_t src)
    requires std::assignable_from<T&, T>
    {
        m_data[dst] = std::move(m_data[src]);
        m_indices[dst] = std::move(m_indices[src]);
    }

private:
    std::array<T, N> m_data;
    std::array<I, N> m_indices;
    size_t m_size = 0;
};
