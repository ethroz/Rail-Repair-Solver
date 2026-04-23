#pragma once

#include <algorithm>
#include <stdexcept>

#include "Allocator.hpp"

template<typename T>
class Queue {
public:
    constexpr Queue(size_t capacity = 1) {
        m_capacity = std::max(capacity, size_t(1));
        m_data = m_allocator.allocate(m_capacity);
    }

    constexpr ~Queue() {
        clear();
        m_allocator.deallocate(&m_data, m_capacity);
    }

    static constexpr size_t GROWTH_FACTOR = 2;

    [[nodiscard]] constexpr bool empty() const { return m_size == 0; }

    constexpr void reserve(size_t size) {
        if (size < m_capacity) {
            throw std::runtime_error("Cannot shrink a queue allocation");
        }

        T* newData = m_allocator.allocate(size);

        if (!empty()) {
            m_allocator.construct(newData, m_size);
            if (m_back <= m_front) {
                const auto frontSize = m_capacity - m_front;
                for (size_t i = 0; i < frontSize; i++) {
                    std::swap(newData[i], m_data[m_front + i]);
                }
                for (size_t i = 0; i < m_back; i++) {
                    std::swap(newData[frontSize + i], m_data[i]);
                }
                m_allocator.destroy(m_data + m_front, frontSize);
                m_allocator.destroy(m_data, m_back);
            }
            else {
                for (size_t i = 0; i < m_size; i++) {
                    std::swap(newData[i], m_data[m_front + i]);
                }
                m_allocator.destroy(m_data + m_front, m_size);
            }
        }

        m_allocator.deallocate(&m_data, m_capacity);
        m_data = newData;
        m_capacity = size;
        m_front = 0;
        m_back = m_size;
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

    constexpr void push(T&& item) {
        if (m_size == m_capacity) {
            reserve(m_capacity * GROWTH_FACTOR);
        }

        m_allocator.construct(m_data + m_back++, std::move(item));
        m_size++;
        if (m_back == m_capacity) {
            m_back = 0;
        }
    }

    constexpr void push(const T& item) {
        T copy = item;
        push(std::move(copy));
    }

    [[nodiscard]] constexpr T pop() {
        if (empty()) {
            throw std::runtime_error("Cannot pop from an empty queue");
        }

        T item{};
        std::swap(item, m_data[m_front]);
        m_allocator.destroy(m_data + m_front++);
        m_size--;
        if (m_front == m_capacity) {
            m_front = 0;
        }

        return item;
    }

    constexpr void linearize() {
        if (m_size > 0 && m_front != 0) {
            if (m_back <= m_front) {
                // Construct between the two.
                m_allocator.construct(m_data + m_back, m_capacity - m_size);

                // Iterate until the dest ptr is greater than the src ptr.
                // Should only happen for odd array sizes. TODO: confirm this. ########################
                // Even sizes should end perfectly. TODO: confirm this. ########################
                const auto swapSize;
            }
            else if (m_front < m_back) {
                // Construct before the front.
                m_allocator.construct(m_data, m_front);

                for (size_t i = 0; i < m_size; i++) {
                    std::swap(m_data[i], m_data[m_front + i]);
                }

                // Destroy after the end.
                m_allocator.destroy(m_data, m_back - m_size);
            }
        }

        m_front = 0;
        m_back = m_size;
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
            assert(a.m_owner.m_data == b.m_owner.m_data);
            return a.m_index == b.m_index;
        }

        constexpr friend bool operator!=(const iterator& a, const iterator& b) {
            assert(a.m_owner.m_data == b.m_owner.m_data);
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

    constexpr const_iterator cbegin() const { return const_iterator(*this, 0); }
    constexpr const_iterator cend() const { return const_iterator(*this, m_size); }

private:
    size_t m_front = 0;
    size_t m_back = 0;
    size_t m_size = 0;
    size_t m_capacity;
    T* m_data;
    Allocator<T> m_allocator;
};
