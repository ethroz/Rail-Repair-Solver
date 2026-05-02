#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "IndexedVector.hpp"

template<typename T, std::unsigned_integral I, typename Compare = std::less<T>>
requires std::move_constructible<T> && std::assignable_from<T&, T>
class IndexedPriorityQueue {
public:
    static constexpr size_t Arity = 4;

    constexpr IndexedPriorityQueue(size_t capacity = 1) : m_data(capacity) {}

    IndexedPriorityQueue(const IndexedPriorityQueue&) = delete;
    IndexedPriorityQueue& operator=(const IndexedPriorityQueue&) = delete;

    [[nodiscard]] constexpr bool empty() const noexcept {
        return m_data.empty();
    }

    [[nodiscard]] constexpr size_t size() const noexcept {
        return m_data.size();
    }

    [[nodiscard]] constexpr size_t capacity() const {
        return m_data.capacity();
    }

    constexpr void reserve(size_t capacity) {
        m_data.reserve(capacity);
    }

    constexpr void clear() noexcept {
        m_data.clear();
    }

    [[nodiscard]] constexpr I& index(size_t i) {
        assert(!empty());
        return m_data.index(i);
    }

    [[nodiscard]] constexpr const I& index(size_t i) const {
        assert(!empty());
        return m_data.index(i);
    }

    [[nodiscard]] constexpr const T& peek() const {
        assert(!empty());
        return m_data.frontValue();
    }

    [[nodiscard]] constexpr const I& peekIndex() const {
        assert(!empty());
        return m_data.frontIndex();
    }

    template<typename U, typename V>
    constexpr void push(U&& item, V&& index) {
        m_data.push_back(std::forward<U>(item), std::forward<V>(index));
        bubbleUp(m_data.size() - 1);
    }

    template <std::ranges::input_range R, std::ranges::input_range S>
    requires std::ranges::sized_range<R> && std::ranges::sized_range<S>
    constexpr void pushRange(R&& values, S&& indices) {
        const size_t addSize = size_t(std::ranges::size(values));
        assert(addSize == size_t(std::ranges::size(indices)));
        
        if (addSize == 0) {
            return;
        }
        
        constexpr auto ceilLog2 = [](size_t x) {
            if (x == 0) {
                return 0;
            }
            return std::bit_width(x - 1);
        };
        
        static_assert(std::has_single_bit(Arity) && Arity > 1);
        constexpr size_t Log2Arity = ceilLog2(Arity);
        
        const size_t totalSize = m_data.size() + addSize;
        const size_t log2Total = ceilLog2(totalSize);
        const size_t height = (log2Total + Log2Arity - 1) / Log2Arity;

        // Equivalent to: addSize * height < totalSize
        if (height == 0 || addSize <= (totalSize - 1) / height) {
            for (auto&& [item, index] : std::views::zip(values, indices)) {
                push(std::forward<decltype(item)>(item), std::forward<decltype(index)>(index));
            }
        }
        else {
            m_data.append_range(std::forward<R>(values), std::forward<S>(indices));
            heapify();
        }
    }

    [[nodiscard]] constexpr std::pair<T, I> pop() {
        assert(!empty());

        T value = std::move(m_data.frontValue());
        I index = std::move(m_data.frontIndex());

        if (m_data.size() > 1) {
            m_data.move_assign_from(0, m_data.size() - 1);
        }
        
        m_data.pop_back();

        if (!m_data.empty()) {
            bubbleDown(0);
        }

        return {std::move(value), std::move(index)};
    }

private:
    [[nodiscard]] static constexpr size_t parentIndex(size_t index) noexcept {
        return (index - 1) / Arity;
    }

    [[nodiscard]] static constexpr size_t firstChildIndex(size_t index) noexcept {
        return Arity * index + 1;
    }

    constexpr void bubbleUp(size_t i) {
        T value = std::move(m_data.value(i));
        I index = std::move(m_data.index(i));
        
        while (i > 0) {
            const size_t parent = parentIndex(i);

            // Stop when parent <= value.
            if (!m_compare(value, m_data.value(parent))) {
                break;
            }

            m_data.move_assign_from(i, parent);
            i = parent;
        }

        m_data.assign(i, std::move(value), std::move(index));
    }

    constexpr void bubbleDown(size_t i) {
        const size_t count = m_data.size();
        T value = std::move(m_data.value(i));
        I index = std::move(m_data.index(i));

        while (true) {
            const size_t firstChild = firstChildIndex(i);

            if (firstChild >= count) {
                break;
            }

            size_t smallestChild = firstChild;
            const size_t lastChild = std::min(firstChild + Arity, count);

            for (size_t child = firstChild + 1; child < lastChild; ++child) {
                if (m_compare(m_data.value(child), m_data.value(smallestChild))) {
                    smallestChild = child;
                }
            }

            // Stop when value <= smallest child.
            if (!m_compare(m_data.value(smallestChild), value)) {
                break;
            }

            m_data.move_assign_from(i, smallestChild);
            i = smallestChild;
        }

        m_data.assign(i, std::move(value), std::move(index));
    }

    constexpr void heapify() {
        if (m_data.size() < 2) {
            return;
        }

        for (size_t i = parentIndex(m_data.size() - 1) + 1; i > 0; --i) {
            bubbleDown(i - 1);
        }
    }

    IndexedVector<T, I> m_data;
    Compare m_compare{};
};