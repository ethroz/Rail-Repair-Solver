#pragma once

#include <memory>
#include <utility>

template<typename T>
class Allocator {
public:
    constexpr Allocator() = default;

    [[nodiscard]] constexpr T* allocate(size_t size) {
        return m_allocator.allocate(size);
    }

    template<typename... Args>
    constexpr void construct(T* ptr, Args&&... args) {
        std::construct_at(ptr, std::forward<Args>(args)...);
    }

    constexpr void deallocate(T** ptr, size_t size) {
        m_allocator.deallocate(*ptr, size);
        *ptr = nullptr;
    }

    constexpr void destroy(T* ptr) {
        std::destroy_at(ptr);
    }

    constexpr void destroy(T** ptr, size_t size) {
        destroy(*ptr, size);
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