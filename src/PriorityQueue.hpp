#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

template<typename T, typename Compare = std::less<T>>
requires std::move_constructible<T>
class PriorityQueue {
public:
    static constexpr size_t Arity = 4;

    using SwapFn = std::function<void(size_t, size_t)>;
    inline PriorityQueue(size_t capacity = 1, SwapFn&& swapFn = [](auto&&...){}) :
        m_swapFn(std::move(swapFn))
    {
        if (!m_swapFn) {
            throw std::invalid_argument("The swap function cannot be null");
        }
        m_data.reserve(capacity);
    }

    PriorityQueue(const PriorityQueue&) = delete;
    PriorityQueue& operator=(const PriorityQueue&) = delete;

    [[nodiscard]] constexpr bool empty() const noexcept {
        return m_data.empty();
    }

    [[nodiscard]] constexpr size_t size() const noexcept {
        return m_data.size();
    }

    [[nodiscard]] constexpr size_t capacity() const {
        return m_data.capacity();
    }

    constexpr void clear() noexcept {
        m_data.clear();
    }

    [[nodiscard]] constexpr const T& peek() const {
        if (empty()) {
            throw std::runtime_error("The queue is empty");
        }
        return m_data.front();
    }

    constexpr void push(T&& item) {
        m_data.push_back(std::move(item));
        bubbleUp(m_data.size() - 1);
    }

    constexpr void push(const T& item) requires std::copy_constructible<T> {
        m_data.push_back(item);
        bubbleUp(m_data.size() - 1);
    }

    template <std::ranges::input_range R>
    requires std::constructible_from<T, std::ranges::range_reference_t<R>> &&
             std::ranges::sized_range<R>
    constexpr void pushRange(R&& rng) {
        const size_t addSize = size_t(std::ranges::size(rng));
        const size_t totalSize = m_data.size() + addSize;

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

        const size_t log2Total = ceilLog2(totalSize);
        const size_t height = (log2Total + Log2Arity - 1) / Log2Arity;

        // Equivalent to: addSize * height < totalSize
        if (height == 0 || addSize <= (totalSize - 1) / height) {
            for (auto&& item : rng) {
                push(std::forward<decltype(item)>(item));
            }
        }
        else {
            m_data.append_range(std::forward<R>(rng));
            heapify();
        }
    }

    [[nodiscard]] constexpr T pop() {
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
    [[nodiscard]] static constexpr size_t parentIndex(size_t index) noexcept {
        return (index - 1) / Arity;
    }

    [[nodiscard]] static constexpr size_t firstChildIndex(size_t index) noexcept {
        return Arity * index + 1;
    }

    constexpr void bubbleUp(size_t index) {
        T value = std::move(m_data[index]);
        
        while (index > 0) {
            const size_t parent = parentIndex(index);

            // Stop when parent <= value.
            if (!m_compare(value, m_data[parent])) {
                break;
            }

            m_data[index] = std::move(m_data[parent]);
            m_swapFn(index, parent);
            index = parent;
        }

        m_data[index] = std::move(value);
    }

    constexpr void bubbleDown(size_t index) {
        const size_t count = m_data.size();
        T value = std::move(m_data[index]);

        while (true) {
            const size_t firstChild = firstChildIndex(index);

            if (firstChild >= count) {
                break;
            }

            size_t smallestChild = firstChild;
            const size_t lastChild = std::min(firstChild + Arity, count);

            for (size_t child = firstChild + 1; child < lastChild; ++child) {
                if (m_compare(m_data[child], m_data[smallestChild])) {
                    smallestChild = child;
                }
            }

            // Stop when value <= smallest child.
            if (!m_compare(m_data[smallestChild], value)) {
                break;
            }

            m_data[index] = std::move(m_data[smallestChild]);
            m_swapFn(index, smallestChild);
            index = smallestChild;
        }

        m_data[index] = std::move(value);
    }

    constexpr void heapify() {
        if (m_data.size() < 2) {
            return;
        }

        for (size_t index = parentIndex(m_data.size() - 1) + 1; index > 0; --index) {
            bubbleDown(index - 1);
        }
    }

    const SwapFn m_swapFn;
    std::vector<T> m_data;
    Compare m_compare{};
};