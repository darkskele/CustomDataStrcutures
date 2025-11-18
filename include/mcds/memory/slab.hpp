#pragma once

#include <stddef.h>
#include <type_traits>
#include <limits>
#include <cstddef>
#include <array>

namespace mcds::memory
{

    template <typename ValueType, typename IndexType, bool FORCE_HEAP = false, size_t STACK_BUDGET = 64 * 1024>
        requires std::is_unsigned_v<IndexType> && std::is_integral_v<IndexType>
    class Slab
    {
        // Derive capacity from index type
        static constexpr size_t CAPACITY = static_cast<size_t>(std::numeric_limits<IndexType>::max() + 1);

        // Storage size calculations
        static constexpr size_t STORAGE_SIZE = sizeof(ValueType) * CAPACITY;
        static constexpr USE_STACK = !FORCE_HEAP && (STORAGE_SIZE <= STACK_BUDGET);

        // Assert slab has size
        static_assert(CAPACITY > 0, "Capacity can't be zero!");

        // Defining storage type
        struct alignas(ValueType) Storage
        {
            std::byte data[sizeof(ValueType)];
        };

        // Conditional StorageType
        using StorageBuffer = std::conditional_t<USE_STACK, std::array<Storage, CAPACITY>, Storage *>;

    public:
        // Accessors for types
        using value_type = ValueType;
        using index_type = IndexType;

        Slab()

    private:

        StorageBuffer storage_;

    };

} // namespace mcds::memory
