#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <span>
#include <utility>

template <typename T, size_t N>
class FixedVector {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    constexpr FixedVector() = default;

    [[nodiscard]] constexpr size_type size() const noexcept { return m_size; }
    [[nodiscard]] constexpr size_type capacity() const noexcept { return N; }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }
    [[nodiscard]] constexpr bool full() const noexcept { return m_size == N; }

    constexpr reference operator[](size_type i) {
        assert(i < m_size);
        return m_data[i];
    }

    constexpr const_reference operator[](size_type i) const {
        assert(i < m_size);
        return m_data[i];
    }

    constexpr reference at(size_type i) {
        if (i >= m_size) {
            throw std::out_of_range("FixedVector::at");
        }
        return m_data[i];
    }

    constexpr const_reference at(size_type i) const {
        if (i >= m_size) {
            throw std::out_of_range("FixedVector::at");
        }
        return m_data[i];
    }

    constexpr reference front() {
        assert(m_size > 0);
        return m_data[0];
    }

    constexpr const_reference front() const {
        assert(m_size > 0);
        return m_data[0];
    }

    constexpr reference back() {
        assert(m_size > 0);
        return m_data[m_size - 1];
    }

    constexpr const_reference back() const {
        assert(m_size > 0);
        return m_data[m_size - 1];
    }

    [[nodiscard]] constexpr pointer data() noexcept { return m_data.data(); }
    [[nodiscard]] constexpr const_pointer data() const noexcept { return m_data.data(); }

    constexpr void clear() noexcept {
        m_size = 0;
    }

    template<typename U>
    requires std::constructible_from<T, U&&>
    constexpr void push_back(U&& value) {
        assert(m_size < N);
        m_data[m_size] = std::forward<U>(value);
        ++m_size;
    }

    template<typename... Args>
    requires std::constructible_from<T, Args&&...>
    constexpr T& emplace_back(Args&&... args) {
        assert(m_size < N);
        m_data[m_size] = T(std::forward<Args>(args)...);
        ++m_size;
        return m_data[m_size - 1];
    }

    constexpr void resize(size_type new_size) {
        assert(new_size <= N);
        m_size = new_size;
    }

    constexpr void resize(size_type new_size, const T& value) {
        assert(new_size <= N);
        if (new_size > m_size) {
            for (size_type i = m_size; i < new_size; ++i) {
                m_data[i] = value;
            }
        }
        m_size = new_size;
    }

    constexpr void pop_back() {
        assert(m_size > 0);
        --m_size;
    }

    constexpr iterator begin() noexcept { return data(); }
    constexpr const_iterator begin() const noexcept { return data(); }
    constexpr const_iterator cbegin() const noexcept { return data(); }

    constexpr iterator end() noexcept { return data() + m_size; }
    constexpr const_iterator end() const noexcept { return data() + m_size; }
    constexpr const_iterator cend() const noexcept { return data() + m_size; }

    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

    constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

private:
    std::array<T, N> m_data{};
    size_type m_size = 0;
};
