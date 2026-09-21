#pragma once

#include <cstddef>

#if defined(ONITRO_ENABLE_AVX2)
#if defined(_MSC_VER)
#define ONITRO_SIMD_LOOP
#elif defined(__clang__) || defined(__GNUC__)
#define ONITRO_SIMD_LOOP _Pragma("omp simd")
#else
#define ONITRO_SIMD_LOOP
#endif
#else
#define ONITRO_SIMD_LOOP
#endif

namespace ONITRO::simd {

inline constexpr std::size_t alignment = 64;
inline constexpr std::size_t block_size = 1024;

template <typename T, std::size_t Size = block_size>
struct alignas(alignment) buffer {
    T values[Size];

    constexpr T& operator[](std::size_t index) noexcept { return values[index]; }
    constexpr const T& operator[](std::size_t index) const noexcept {
        return values[index];
    }
};

template <typename Function>
void for_each_block(std::size_t count, Function&& function) noexcept {
    for (std::size_t offset = 0; offset < count; offset += block_size) {
        function(offset, count - offset < block_size ? count - offset : block_size);
    }
}

}  // namespace ONITRO::simd
