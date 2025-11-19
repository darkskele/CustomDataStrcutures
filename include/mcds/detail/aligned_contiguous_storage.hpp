#pragma once

#include <vector>
#include <cstddef>
#include <new>

namespace mcds::detail
{
    // One properly aligned slot for a Value
    template <typename Value>
    struct value_slot
    {
        alignas(Value) std::byte data[sizeof(Value)];
    };

    // Primary template
    template <typename Value, std::size_t CAPACITY, bool USE_STACK>
    struct aligned_contiguous_storage;

    // Stack specialization
    template <typename Value, std::size_t CAPACITY>
    struct aligned_contiguous_storage<Value, CAPACITY, true>
    {
        // One contiguous block of raw stack storage
        alignas(Value) std::byte buf_[sizeof(Value) * CAPACITY];

        aligned_contiguous_storage() = default;

        Value *ptr(std::size_t i) noexcept
        {
            return std::launder(reinterpret_cast<Value *>(buf_ + sizeof(Value) * i));
        }

        const Value *ptr(std::size_t i) const noexcept
        {
            return std::launder(reinterpret_cast<const Value *>(buf_ + sizeof(Value) * i));
        }
    };

    // Heap specialization
    template <typename Value, std::size_t CAPACITY>
    struct aligned_contiguous_storage<Value, CAPACITY, false>
    {
        using slot_type = value_slot<Value>;
        std::vector<slot_type> buf_;

        aligned_contiguous_storage() : buf_(CAPACITY) // CAPACITY aligned slots
        {
        }

        Value *ptr(std::size_t i) noexcept
        {
            return std::launder(reinterpret_cast<Value *>(&buf_[i]));
        }

        const Value *ptr(std::size_t i) const noexcept
        {
            return std::launder(reinterpret_cast<const Value *>(&buf_[i]));
        }
    };

} // namespace mcds::detail
