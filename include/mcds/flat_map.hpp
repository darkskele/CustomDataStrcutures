#pragma once

#include <algorithm>
#include <array>
#include <vector>
#include <stdexcept>

#include "memory/slab.hpp"

namespace mcds
{

    template <typename Key, typename Value, size_t CAPACITY = 1024, bool FORCE_HEAP = false, size_t STACK_BUDGET = 2 * 1024 * 1024>
    class FlatMap
    {
        /// @brief Unsigned integer type for slot indices within a slab.
        using SlotIndexType = std::conditional_t<CAPACITY <= (1ULL << 8), uint8_t, std::conditional_t<CAPACITY <= (1ULL << 16), uint16_t, std::conditional_t<CAPACITY <= (1ULL << 32), uint32_t, uint64_t>>>;

        // Constants for type decision
        /// @brief Total value bytes.
        static constexpr size_t TOTAL_VAL_BYTES = sizeof(Value) * CAPACITY;
        /// @brief Total key bytes.
        static constexpr size_t TOTAL_KEY_BYTES = sizeof(Key) * CAPACITY;
        /// @brief Total slot index bytes.
        static constexpr size_t TOTAL_SLOT_INDEX_BYTES = sizeof(SlotIndexType) * CAPACITY;

        /// @brief Conditional value for stack vs. heap storage.
        static constexpr bool USE_STACK = !FORCE_HEAP && ((TOTAL_KEY_BYTES + TOTAL_VAL_BYTES + TOTAL_SLOT_INDEX_BYTES) <= STACK_BUDGET);

        static_assert(CAPACITY > 0, "FlatMap capacity must be greater than zero");

        // Stack storage requires default constructible
        static_assert(!USE_STACK || std::is_default_constructible_v<Key>, "Key must be default constructible for stack storage");
        static_assert(!USE_STACK || std::is_default_constructible_v<Value>, "Value must be default constructible for stack storage");

        // Both storage types need move or copy
        static_assert(std::is_copy_constructible_v<Key> || std::is_move_constructible_v<Key>, "Key must be copy or move constructible");
        static_assert(std::is_copy_assignable_v<Key> || std::is_move_assignable_v<Key>, "Key must be copy or move assignable");
        static_assert(std::is_copy_constructible_v<Value> || std::is_move_constructible_v<Value>, "Value must be copy or move constructible");
        static_assert(std::is_copy_assignable_v<Value> || std::is_move_assignable_v<Value>, "Value must be copy or move assignable");

        /// @brief Expression to switch between heap and stack storage for large types.
        template <typename T>
        using StorageType = std::conditional_t<
            USE_STACK,
            std::array<T, CAPACITY>,
            std::vector<T>>;

        /// @brief Slab type for storing values.
        using SlabType = memory::slab<Value, CAPACITY, !USE_STACK>;

    public:
        /// @brief Public key, value and slab index types for testing and info.
        using key_type = Key;
        using value_type = Value;
        using slot_index_type = SlotIndexType;

        FlatMap() : size_(0)
        {
            if constexpr (!USE_STACK)
            {
                keys_.resize(CAPACITY);
                values_ptrs_.resize(CAPACITY);
            }
        }

        Value *find(const Key &key) noexcept
        {
            // Binary search for key
            size_t i = find_index(key);
            // If not end of keys, return pointer to value
            if (i != size_ && keys_[i] == key)
            {
                // Retrieve ptr then get from slab
                const slot_index_type ptr = values_ptrs_[i];

                return &values_[ptr];
            }

            // Else return nullptr
            return nullptr;
        }

        bool insert(const Key &key, const Value &value)
        {
            // Find insertion point
            size_t i = find_index(key);

            // Key exists if i < size_ AND keys match
            bool key_exists = (i < size_ && keys_[i] == key);

            // Check if it doesn't exist
            if (key_exists)
            {
                const slot_index_type ptr = values_ptrs_[i];

                // Overwrite data
                values_[ptr] = std::move(value);
            }
            else
            {
                if (size_ >= CAPACITY)
                {
                    // Overflow
                    throw std::runtime_error("Stack overflow in flat map!");
                }

                // Shift elements right
                std::move_backward(keys_.begin() + i, keys_.begin() + size_, keys_.begin() + size_ + 1);
                std::move_backward(values_ptrs_.begin() + i, values_ptrs_.begin() + size_, values_ptrs_.begin() + size_ + 1);

                // Insert key
                keys_[i] = std::move(key);
                ++size_;

                // Insert data into slab
                const slot_index_type ptr = values_.allocate(std::move(value));
                values_ptrs_[i] = ptr;
            }

            return true;
        }

        bool erase(const Key &key) noexcept
        {
            // Find erasure point
            size_t i = find_index(key);

            if (i >= size_ || keys_[i] != key)
            {
                // Doesn't exist
                return false;
            }

            // Deallocate
            const slot_index_type ptr = values_ptrs_[i];
            values_.deallocate(ptr);

            // Shift left to remove
            std::move(keys_.begin() + i + 1, keys_.begin() + size_, keys_.begin() + i); // from one post i, shifted to i, therfore overwriting i
            std::move(values_ptrs_.begin() + i + 1, values_ptrs_.begin() + size_, values_ptrs_.begin() + i);
            --size_;

            return true;
        }

        std::pair<const Key *, Value *> find_min() noexcept
        {
            if (size_ == 0)
                return {nullptr, nullptr};
            // Sorted array, smallest is first
            return {&keys_[0], &values_[values_ptrs_[0]]};
        }

        std::pair<const Key *, Value *> find_max() noexcept
        {
            if (size_ == 0)
                return {nullptr, nullptr};
            // Sorted array, largest is last
            return {&keys_[size_ - 1], &values_[values_ptrs_[size_ - 1]]};
        }

        const Key *lower_bound(const Key &key) const noexcept
        {
            // Find lower
            auto it = std::lower_bound(keys_.begin(), keys_.begin() + size_, key);
            return (it != keys_.begin() + size_) ? &(*it) : nullptr;
        }

        const Key *upper_bound(const Key &key) const noexcept
        {
            // Find upper
            auto it = std::upper_bound(keys_.begin(), keys_.begin() + size_, key);
            return (it != keys_.begin() + size_) ? &(*it) : nullptr;
        }

        size_t size() const noexcept
        {
            return size_;
        }

        bool empty() const noexcept
        {
            return size_ == 0;
        }

        bool full() const noexcept
        {
            return size_ >= CAPACITY;
        }

        Value &operator[](const Key &key)
        {
            // Search for key
            size_t i = find_index(key);

            // Check if it doesn't exist
            if (i >= size_ || keys_[i] != key)
            {
                if (size_ >= CAPACITY)
                {
                    // Overflow
                    throw std::runtime_error("Overflow in flat map! Can't default construct!");
                }

                // Shift elements right
                std::move_backward(keys_.begin() + i, keys_.begin() + size_, keys_.begin() + size_ + 1);
                std::move_backward(values_ptrs_.begin() + i, values_ptrs_.begin() + size_, values_ptrs_.begin() + size_ + 1);

                // Insert key
                keys_[i] = std::move(key);
                // Allocate in slab
                const slot_index_type ptr = values_.allocate(std::move(Value{}));
                // Add ptr
                values_ptrs_[i] = ptr;
                ++size_;
            }

            // Return reference to value
            return values_[values_ptrs_[i]];
        }

    private:
        size_t find_index(const Key &key) const noexcept
        {
            auto it = std::lower_bound(keys_.begin(), keys_.begin() + size_, key);
            return it - keys_.begin();
        }

        StorageType<Key> keys_;
        StorageType<slot_index_type> values_ptrs_;
        SlabType values_;
        size_t size_;
    };

} // namespace mcds
