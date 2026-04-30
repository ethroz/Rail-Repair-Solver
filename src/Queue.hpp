#pragma once

#include <algorithm>
#include <concepts>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "Allocator.hpp"

template<typename T>
requires std::move_constructible<T>
class Queue {
public:
    constexpr Queue(size_t capacity = 1) {
        m_capacity = std::max(capacity, size_t(1));
        m_data = m_allocator.allocate(m_capacity);
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    constexpr ~Queue() {
        clear();
        m_allocator.deallocate(&m_data, m_capacity);
    }

    static constexpr size_t GROWTH_FACTOR = 2;

    [[nodiscard]] constexpr bool empty() const { return m_size == 0; }
    
    [[nodiscard]] constexpr size_t size() const { return m_size; }

    [[nodiscard]] constexpr size_t capacity() const { return m_capacity; }

    constexpr void reserve(size_t size) {
        if (size < m_capacity) {
            throw std::runtime_error("Cannot shrink a queue allocation");
        }

        T* newData = m_allocator.allocate(size);
        size_t constructed = 0;

        try {
            for (; constructed < m_size; constructed++) {
                const size_t oldIndex = (m_front + constructed) % m_capacity;
                m_allocator.construct(newData + constructed, std::move(m_data[oldIndex]));
            }
        }
        catch (...) {
            m_allocator.destroy(newData, constructed);
            m_allocator.deallocate(&newData, size);
            throw;
        }

        const size_t oldSize = m_size;

        clear();

        m_allocator.deallocate(&m_data, m_capacity);
        m_data = newData;
        m_capacity = size;
        m_front = 0;
        m_back = oldSize;
        m_size = oldSize;
    }

    constexpr void clear() {
        if (!empty()) {
            if (m_back <= m_front) {
                m_allocator.destroy(m_data + m_front, m_capacity - m_front);
                m_allocator.destroy(m_data, m_back);
            }
            else {
                m_allocator.destroy(m_data + m_front, m_size);
            }
        }

        m_front = 0;
        m_back = 0;
        m_size = 0;
    }

    [[nodiscard]] constexpr T& at(size_t index) {
        if (index >= m_size) {
            throw std::runtime_error("Index out of range of queue");
        }

        index = (m_front + index) % m_capacity;
        return m_data[index];
    }

    [[nodiscard]] constexpr const T& at(size_t index) const {
        if (index >= m_size) {
            throw std::runtime_error("Index out of range of queue");
        }

        index = (m_front + index) % m_capacity;
        return m_data[index];
    }

    [[nodiscard]] constexpr const T& peek() const {
        if (empty()) {
            throw std::runtime_error("Cannot peek an empty queue");
        }

        return m_data[m_front];
    }

    [[nodiscard]] constexpr T& operator[](size_t index) {
        assert(index < m_size);
        index = (m_front + index) % m_capacity;
        return m_data[index];
    }

    [[nodiscard]] constexpr const T& operator[](size_t index) const {
        assert(index < m_size);
        index = (m_front + index) % m_capacity;
        return m_data[index];
    }

    constexpr void push(T&& item) {
        if (m_size == m_capacity) {
            reserve(m_capacity * GROWTH_FACTOR);
        }
        unchecked_push(std::move(item));
    }

    constexpr void push(const T& item) requires std::copy_constructible<T> {
        if (m_size == m_capacity) {
            reserve(m_capacity * GROWTH_FACTOR);
        }
        unchecked_push(item);
    }

    template <std::ranges::input_range R>
    requires std::constructible_from<T, std::ranges::range_reference_t<R>> &&
             std::ranges::sized_range<R>
    constexpr void pushRange(R&& rng) {
        const auto count = size_t(std::ranges::size(rng));

        if (count > m_capacity - m_size) {
            reserve(growCapacityFor(m_size + count));
        }

        for (auto&& item : rng) {
            unchecked_push(std::forward<decltype(item)>(item));
        }
    }

    [[nodiscard]] constexpr T pop() {
        if (empty()) {
            throw std::runtime_error("Cannot pop from an empty queue");
        }

        T item = std::move(m_data[m_front]);
        m_allocator.destroy(m_data + m_front++);
        --m_size;
        if (m_front == m_capacity) {
            m_front = 0;
        }

        return item;
    }

    [[nodiscard]] constexpr T popBack() {
        if (empty()) {
            throw std::runtime_error("Cannot pop back from an empty queue");
        }

        const size_t back = (m_back == 0) ? (m_capacity - 1) : (m_back - 1);
        T item = std::move(m_data[back]);
        m_allocator.destroy(m_data + back);
        m_back = back;
        --m_size;
        if (m_front == m_capacity) {
            m_front = 0;
        }

        return item;
    }

    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;

        constexpr iterator(Queue& owner, size_t index) : m_owner(owner), m_index(index) {}

        constexpr reference operator*() { return m_owner.at(m_index); }
        constexpr pointer operator->() { return &m_owner.at(m_index); }

        constexpr iterator& operator++() { assert(m_index != m_owner.m_size); m_index++; return *this; }
        constexpr iterator operator++(int) { iterator temp = *this; ++(*this); return temp; }

        constexpr friend bool operator==(const iterator& a, const iterator& b) {
            assert(&a.m_owner == &b.m_owner);
            return a.m_index == b.m_index;
        }

        constexpr friend bool operator!=(const iterator& a, const iterator& b) {
            assert(&a.m_owner == &b.m_owner);
            return a.m_index != b.m_index;
        }

    private:
        Queue& m_owner;
        size_t m_index;
    };

    struct const_iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;

        constexpr const_iterator(const Queue& owner, size_t index) : m_owner(owner), m_index(index) {}

        constexpr const_reference operator*() const { return m_owner.at(m_index); }
        constexpr const_pointer operator->() const { return &m_owner.at(m_index); }

        constexpr const_iterator& operator++() { assert(m_index != m_owner.m_size); m_index++; return *this; }
        constexpr const_iterator operator++(int) { const_iterator temp = *this; ++(*this); return temp; }

        constexpr friend bool operator==(const const_iterator& a, const const_iterator& b) {
            assert(a.m_owner.m_data == b.m_owner.m_data);
            return a.m_index == b.m_index;
        }

        constexpr friend bool operator!=(const const_iterator& a, const const_iterator& b) {
            assert(a.m_owner.m_data == b.m_owner.m_data);
            return a.m_index != b.m_index;
        }

    private:
        const Queue& m_owner;
        size_t m_index;
    };

    constexpr iterator begin() { return iterator(*this, 0); }
    constexpr iterator end() { return iterator(*this, m_size); }
    constexpr const_iterator begin() const { return const_iterator(*this, 0); }
    constexpr const_iterator end() const { return const_iterator(*this, m_size); }

    constexpr const_iterator cbegin() const { return const_iterator(*this, 0); }
    constexpr const_iterator cend() const { return const_iterator(*this, m_size); }

private:
    constexpr size_t growCapacityFor(size_t required) const {
        size_t newCapacity = m_capacity;

        if (newCapacity == 0) {
            newCapacity = 1;
        }

        while (newCapacity < required) {
            newCapacity *= GROWTH_FACTOR;
        }

        return newCapacity;
    }

    template<typename U>
    constexpr void unchecked_push(U&& item) {
        m_allocator.construct(m_data + m_back++, std::forward<U>(item));
        if (m_back == m_capacity) {
            m_back = 0;
        }
        ++m_size;
    }

    size_t m_front = 0;
    size_t m_back = 0;
    size_t m_size = 0;
    size_t m_capacity = 0;
    T* m_data = nullptr;
    Allocator<T> m_allocator;
};
