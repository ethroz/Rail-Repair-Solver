#pragma once

#include <algorithm>
#include <stdexcept>

#include "Container.hpp"

template<typename T>
class Queue {
public:
    constexpr Queue(size_t capacity = 1) : m_container(std::max(capacity, size_t(1))) {}
    constexpr ~Queue() {}

    static constexpr size_t GROWTH_FACTOR = 2;

    constexpr bool empty() const { return m_size == 0; }

    constexpr void reserve(size_t size) {
        if (size < m_container.capacity()) {
            throw std::runtime_error("Cannot shrink a queue allocation");
        }

        auto newContainer = Container<T>(size);
        if (m_back <= m_front) {
            const auto firstSize = m_container.capacity() - m_front;
            for (size_t i = 0; i < firstSize; i++) {
                std::swap(newContainer.data()[i], m_container.data()[m_front + i]);
            }
            for (size_t i = 0; i < m_back; i++) {
                std::swap(newContainer.data()[firstSize + i], m_container.data()[i]);
            }
        }
        else {
            for (size_t i = 0; i < m_back; i++) {
                std::swap(newContainer.data()[i], m_container.data()[m_front + i]);
            }
        }

        m_container = std::move(newContainer);
        m_front = 0;
        m_back = m_size;
    }

    constexpr T& at(size_t index) {
        if (index >= m_size) {
            throw std::runtime_error("Index out of range of queue");
        }

        index = (m_front + index) % m_container.capacity();
        return m_container.data()[index];
    }

    constexpr const T& at(size_t index) const {
        if (index >= m_size) {
            throw std::runtime_error("Index out of range of queue");
        }

        index = (m_front + index) % m_container.capacity();
        return m_container.data()[index];
    }

    constexpr const T& peek() const {
        if (empty()) {
            throw std::runtime_error("Cannot peek an empty queue");
        }

        return m_container.data()[m_front];
    }

    constexpr void push(T&& item) {
        if (m_size == m_container.capacity()) {
            reserve(m_size * GROWTH_FACTOR);
        }

        m_container.data()[m_back++] = std::move(item);
        m_size++;
        if (m_back == m_container.capacity()) {
            m_back = 0;
        }
    }

    constexpr void push(const T& item) {
        T copy = item;
        push(std::move(copy));
    }

    constexpr T pop() {
        if (empty()) {
            throw std::runtime_error("Cannot pop from an empty queue");
        }

        T item = std::move(m_container.data()[m_front++]);
        m_size--;
        if (m_front == m_container.capacity()) {
            m_front = 0;
        }

        return item;
    }

    constexpr void linearize() {
        if (m_size > 0 && m_front != 0) {
            for (size_t i = 0, j = m_front; i < m_size; i++, j = (j + 1) % m_container.capacity()) {
                std::swap(m_container.data()[i], m_container.data()[j]);
            }
        }

        m_front = 0;
        m_back = 0;
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
            assert(a.m_owner.m_container.data() == b.m_owner.m_container.data());
            return a.m_index == b.m_index;
        }

        constexpr friend bool operator!=(const iterator& a, const iterator& b) {
            assert(a.m_owner.m_container.data() == b.m_owner.m_container.data());
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
            assert(a.m_owner.m_container.data() == b.m_owner.m_container.data());
            return a.m_index == b.m_index;
        }

        constexpr friend bool operator!=(const const_iterator& a, const const_iterator& b) {
            assert(a.m_owner.m_container.data() == b.m_owner.m_container.data());
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
    size_t m_size = 0;
    size_t m_front = 0;
    size_t m_back = 0;
    Container<T> m_container;
};
