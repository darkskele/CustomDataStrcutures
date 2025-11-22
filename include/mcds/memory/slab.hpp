#pragma once

#include <stddef.h>
#include <type_traits>
#include <limits>
#include <cstddef>
#include <memory>
#include <cassert>
#include <bitset>

#include "mcds/detail/aligned_contiguous_storage.hpp"

namespace mcds::memory
{

    /**
     * @brief Fixed-capacity slab allocator for POD and non POD types.
     *
     * The allocator does not perform double-free checks or lifetime  validation. It is intended for
     * use inside higher-level memory pools that guarantee correctness.
     *
     * @note Size grows exponentially for the index type. Exercise caution for when providing @t IndexType.
     *
     * @tparam ValueType   The stored object type.
     * @tparam IndexType   Unsigned integral index type (defines capacity).
     * @tparam FORCE_HEAP  If true, always allocates storage on heap.
     * @tparam STACK_BUDGET Maximum bytes allowed for stack allocation.
     */
    template <typename ValueType, typename IndexType, bool FORCE_HEAP = false, size_t STACK_BUDGET = 64 * 1024>
        requires std::is_unsigned_v<IndexType> && std::is_integral_v<IndexType>
    class Slab
    {
        /// @brief Maximum number of values storable in this slab.
        static constexpr size_t CAPACITY = static_cast<size_t>(std::numeric_limits<IndexType>::max()) + 1ULL;

        /// @brief Total byte size needed for all ValueType objects.
        static constexpr size_t STORAGE_SIZE = sizeof(ValueType) * CAPACITY;

        /// @brief Whether to store data on stack.
        static constexpr bool USE_STACK = !FORCE_HEAP && (STORAGE_SIZE <= STACK_BUDGET);

        static_assert(CAPACITY > 0, "Capacity must be non-zero.");

        using StorageBuffer = detail::aligned_contiguous_storage<ValueType, CAPACITY, USE_STACK>;

    public:
        using value_type = ValueType;
        using index_type = IndexType;

        /**
         * @brief Constructs an empty slab with all slots free.
         */
        Slab() : storage_(), free_top_(CAPACITY), allocated_()
        {
            for (size_t i = 0; i < CAPACITY; ++i)
            {
                free_list_[i] = i;
            }
        }

        /**
         * @brief Destroys all live objects.
         */
        ~Slab()
        {
            if constexpr (!std::is_trivially_destructible_v<ValueType>)
            {
                for (size_t i = 0; i < CAPACITY; ++i)
                {
                    if (allocated_[i])
                    {
                        storage_.ptr(i)->~ValueType();
                    }
                }
            }
        }

        /// @brief All copy and move deleted to avoid nasty double frees. Memory ownership is explict and intransinet.
        Slab(const Slab &) = delete;
        Slab &operator=(const Slab &) = delete;
        Slab(Slab &&) noexcept = delete;
        Slab &operator=(Slab &&) noexcept = delete;

        /**
         * @brief Allocates space for a new object and constructs it in place.
         *
         * @tparam Args Constructor argument types.
         * @param args Arguments forwarded to ValueType constructor.
         * @return IndexType  Index of the allocated slot.
         *
         * @note Undefined behaviour if slab is full.
         */
        template <typename... Args>
        IndexType allocate(Args &&...args) noexcept(std::is_nothrow_constructible_v<ValueType, Args...>)
        {
            IndexType slot = allocate_slot();
            std::construct_at(storage_.ptr(slot), std::forward<Args>(args)...);
            return slot;
        }

        /**
         * @brief Destroys the object at the given slot and frees the slot.
         *
         * @param slot Index returned by allocate().
         *
         * @note No double free protection. Caller must ensure correctness.
         */
        void deallocate(const IndexType slot) noexcept
        {
            assert(slot < CAPACITY && "Out-of-bounds slot index!");

            if constexpr (!std::is_trivially_destructible_v<ValueType>)
            {
                std::destroy_at(storage_.ptr(slot));
                allocated_[slot] = false;
            }

            assert(free_top_ < CAPACITY && "Double free or corrupted free list!");
            free_list_[free_top_++] = slot;
        }

        /**
         * @brief Provides mutable access to an allocated object.
         */
        ValueType &operator[](const IndexType slot) noexcept
        {
            assert(slot < CAPACITY);
            return *storage_.ptr(slot);
        }

        /**
         * @brief Provides read-only access to an allocated object.
         */
        const ValueType &operator[](const IndexType slot) const noexcept
        {
            assert(slot < CAPACITY);
            return *storage_.ptr(slot);
        }

        /**
         * @brief Returns true if at least one slot is free.
         */
        bool has_free() const noexcept
        {
            return free_top_ > 0;
        }

        /**
         * @brief Returns total number of slots.
         */
        size_t capacity() const noexcept
        {
            return CAPACITY;
        }

        /**
         * @brief Returns number of currently allocated slots.
         */
        size_t size() const noexcept
        {
            return CAPACITY - free_top_;
        }

    private:
        /**
         * @brief Pops the next free slot from the free-stack.
         */
        IndexType allocate_slot() noexcept
        {
            assert(free_top_ > 0 && "Slab exhausted!");

            const IndexType slot = free_list_[--free_top_];
            if constexpr (!std::is_trivially_destructible_v<ValueType>)
            {
                allocated_[slot] = true;
            }

            return slot;
        }

        StorageBuffer storage_;           ///< Underlying raw storage.
        size_t free_list_[CAPACITY];      ///< Free-slot stack.
        size_t free_top_;                 ///< Stack pointer (free slots).
        std::bitset<CAPACITY> allocated_; ///< Tracks live objects.
    };

} // namespace mcds::memory
