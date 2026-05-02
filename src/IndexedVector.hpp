#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <new>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

template<typename T, std::unsigned_integral I>
class IndexedVector {
public:
    constexpr IndexedVector(size_t capacity = 1) {
        reserve(capacity);
    }

    IndexedVector(const IndexedVector&) = delete;
    IndexedVector& operator=(const IndexedVector&) = delete;

    constexpr ~IndexedVector() {
        clear();
        deallocate();
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return m_size == 0;
    }

    [[nodiscard]] constexpr size_t size() const noexcept {
        return m_size;
    }

    [[nodiscard]] constexpr size_t capacity() const noexcept {
        return m_capacity;
    }

    constexpr void reserve(size_t capacity) {
        if (capacity <= m_capacity) {
            return;
        }

        reallocate(capacity);
    }

    constexpr void clear() noexcept {
        std::destroy_n(values(), m_size);
        std::destroy_n(indices(), m_size);
        m_size = 0;
    }

    [[nodiscard]] constexpr T& value(size_t i) noexcept {
        return values()[i];
    }

    [[nodiscard]] constexpr const T& value(size_t i) const noexcept {
        return values()[i];
    }

    [[nodiscard]] constexpr I& index(size_t i) noexcept {
        return indices()[i];
    }

    [[nodiscard]] constexpr const I& index(size_t i) const noexcept {
        return indices()[i];
    }

    [[nodiscard]] constexpr T& frontValue() noexcept {
        return value(0);
    }

    [[nodiscard]] constexpr const T& frontValue() const noexcept {
        return value(0);
    }

    [[nodiscard]] constexpr I& frontIndex() noexcept {
        return index(0);
    }

    [[nodiscard]] constexpr const I& frontIndex() const noexcept {
        return index(0);
    }

    [[nodiscard]] constexpr T& backValue() noexcept {
        return value(m_size - 1);
    }

    [[nodiscard]] constexpr const T& backValue() const noexcept {
        return value(m_size - 1);
    }

    [[nodiscard]] constexpr I& backIndex() noexcept {
        return index(m_size - 1);
    }

    [[nodiscard]] constexpr const I& backIndex() const noexcept {
        return index(m_size - 1);
    }

    template<typename U, typename V>
    requires std::constructible_from<T, U&&> && std::constructible_from<I, V&&>
    constexpr void push_back(U&& value, V&& index) {
        if (m_size == m_capacity) {
            reserve(growthCapacity());
        }

        T* valueSlot = values() + m_size;
        I* indexSlot = indices() + m_size;

        std::construct_at(valueSlot, std::forward<U>(value));

        try {
            std::construct_at(indexSlot, std::forward<V>(index));
        }
        catch (...) {
            std::destroy_at(valueSlot);
            throw;
        }

        ++m_size;
    }

    template<std::ranges::input_range Values, std::ranges::input_range Indices>
    requires std::ranges::sized_range<Values> &&
             std::ranges::sized_range<Indices> &&
             std::constructible_from<T, std::ranges::range_reference_t<Values>> &&
             std::constructible_from<I, std::ranges::range_reference_t<Indices>>
    constexpr void append_range(Values&& vals, Indices&& inds) {
        const size_t valueCount = size_t(std::ranges::size(vals));
        assert(valueCount == size_t(std::ranges::size(inds)));

        if (valueCount == 0) {
            return;
        }

        reserve(m_size + valueCount);

        const size_t oldSize = m_size;

        try {
            auto valueIt = std::ranges::begin(vals);
            auto indexIt = std::ranges::begin(inds);

            for (; valueIt != std::ranges::end(vals); ++valueIt, ++indexIt) {
                T* valueSlot = values() + m_size;
                I* indexSlot = indices() + m_size;

                std::construct_at(valueSlot, *valueIt);

                try {
                    std::construct_at(indexSlot, *indexIt);
                }
                catch (...) {
                    std::destroy_at(valueSlot);
                    throw;
                }

                ++m_size;
            }
        }
        catch (...) {
            while (m_size > oldSize) {
                pop_back();
            }

            throw;
        }
    }

    constexpr void pop_back(size_t num = 1) noexcept {
        assert(num <= m_size);
        m_size -= num;
        std::destroy_n(values() + m_size, num);
        std::destroy_n(indices() + m_size, num);
    }

    constexpr void move_assign_from(size_t dst, size_t src)
    requires std::assignable_from<T&, T>
    {
        value(dst) = std::move(value(src));
        index(dst) = std::move(index(src));
    }

    constexpr void assign(size_t dst, T&& newValue, I&& newIndex)
    requires std::assignable_from<T&, T>
    {
        value(dst) = std::move(newValue);
        index(dst) = std::move(newIndex);
    }

private:
    [[nodiscard]] constexpr T* values() noexcept {
        return m_values;
    }

    [[nodiscard]] constexpr const T* values() const noexcept {
        return m_values;
    }

    [[nodiscard]] constexpr I* indices() noexcept {
        return m_indices;
    }

    [[nodiscard]] constexpr const I* indices() const noexcept {
        return m_indices;
    }

    [[nodiscard]] constexpr size_t growthCapacity() const noexcept {
        return m_capacity == 0 ? 1 : m_capacity * 2;
    }

    constexpr void reallocate(size_t newCapacity) {
        T* newValues = allocateValues(newCapacity);
        I* newIndices = allocateIndices(newCapacity);

        size_t constructedValues = 0;
        size_t constructedIndices = 0;

        try {
            for (; constructedValues < m_size; ++constructedValues) {
                std::construct_at(
                    newValues + constructedValues,
                    std::move(m_values[constructedValues])
                );
            }

            for (; constructedIndices < m_size; ++constructedIndices) {
                std::construct_at(
                    newIndices + constructedIndices,
                    std::move(m_indices[constructedIndices])
                );
            }
        }
        catch (...) {
            std::destroy_n(newValues, constructedValues);
            std::destroy_n(newIndices, constructedIndices);

            deallocateValues(newValues);
            deallocateIndices(newIndices);

            throw;
        }

        clear();
        deallocate();

        m_values = newValues;
        m_indices = newIndices;
        m_capacity = newCapacity;
        m_size = constructedValues;
    }

    [[nodiscard]] static constexpr T* allocateValues(size_t capacity) {
        if (capacity == 0) {
            return nullptr;
        }

        return static_cast<T*>(
            ::operator new(sizeof(T) * capacity, std::align_val_t{alignof(T)})
        );
    }

    [[nodiscard]] static constexpr I* allocateIndices(size_t capacity) {
        if (capacity == 0) {
            return nullptr;
        }

        return static_cast<I*>(
            ::operator new(sizeof(I) * capacity, std::align_val_t{alignof(I)})
        );
    }

    static constexpr void deallocateValues(T* ptr) noexcept {
        if (ptr != nullptr) {
            ::operator delete(ptr, std::align_val_t{alignof(T)});
        }
    }

    static constexpr void deallocateIndices(I* ptr) noexcept {
        if (ptr != nullptr) {
            ::operator delete(ptr, std::align_val_t{alignof(I)});
        }
    }

    constexpr void deallocate() noexcept {
        deallocateValues(m_values);
        deallocateIndices(m_indices);

        m_values = nullptr;
        m_indices = nullptr;
        m_capacity = 0;
    }

    T* m_values = nullptr;
    I* m_indices = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;
};
