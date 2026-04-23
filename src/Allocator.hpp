#pragma once

#include <memory>

template<typename T>
class Allocator {
public:
    constexpr Allocator() {}

    [[nodiscard]] constexpr T* allocate(size_t size) { return m_allocator.allocate(size); }
    constexpr void construct(T* ptr) { std::construct_at(ptr, T()); }
    constexpr void construct(T* ptr, T&& val) { std::construct_at(ptr, std::move(val)); }

    [[nodiscard]] constexpr T* construct(size_t size) {
        T* ptr = allocate(size);
        for (size_t i = 0; i < size; i++) {
            construct(ptr + i);
        }
        return ptr;
    }

    constexpr void construct(T* ptr, size_t size) {
        for (size_t i = 0; i < size; i++) {
            construct(ptr + i);
        }
    }

    constexpr void deallocate(T** ptr, size_t size) { m_allocator.deallocate(*ptr, size); *ptr = nullptr; }
    constexpr void destroy(T* ptr) { std::destroy_at(ptr); }

    constexpr void destroy(T** ptr, size_t size) {
        for (size_t i = 0; i < size; i++) {
            destroy(*ptr + i);
        }
        deallocate(ptr, size);
    }

    constexpr void destroy(T* ptr, size_t size) {
        for (size_t i = 0; i < size; i++) {
            destroy(ptr + i);
        }
    }

private:
    std::allocator<T> m_allocator;
};
