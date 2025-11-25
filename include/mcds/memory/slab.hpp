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
     * @brief Fixed capacity slab allocator for POD and non POD types.
     *
     * The allocator does not perform double free checks or lifetime  validation. It is intended for
     * use inside higher- level memory pools that guarantee correctness.
     *
     * @tparam ValueType   The stored object type.
     * @tparam CAPACITY    Explicit capacity of slab.
     * @tparam FORCE_HEAP  If true, always allocates storage on heap.
     * @tparam STACK_BUDGET Maximum bytes allowed for stack allocation.
     */
    template <typename ValueType, size_t CAPACITY, bool FORCE_HEAP = false, size_t STACK_BUDGET = 64 * 1024>
    class slab
    {
        /// @brief Total byte size needed for all ValueType objects.
        static constexpr size_t STORAGE_SIZE = sizeof(ValueType) * CAPACITY;

        /// @brief Whether to store data on stack.
        static constexpr bool USE_STACK = !FORCE_HEAP && (STORAGE_SIZE <= STACK_BUDGET);

        static_assert(CAPACITY > 0, "Capacity must be non zero.");

        using StorageBuffer = detail::aligned_contiguous_storage<ValueType, CAPACITY, USE_STACK>;

    public:
        using value_type = ValueType;

        /**
         * @brief Constructs an empty slab with all slots free.
         */
        slab() : storage_(), free_top_(CAPACITY), allocated_()
        {
            for (size_t i = 0; i < CAPACITY; ++i)
            {
                free_list_[i] = i;
            }
        }

        /**
         * @brief Destroys all live objects.
         */
        ~slab()
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
        slab(const slab &) = delete;
        slab &operator=(const slab &) = delete;
        slab(slab &&) noexcept = delete;
        slab &operator=(slab &&) noexcept = delete;

        /**
         * @brief Allocates space for a new object and constructs it in place.
         *
         * @tparam Args Constructor argument types.
         * @param args Arguments forwarded to ValueType constructor.
         * @return Index of the allocated slot.
         *
         * @note Undefined behaviour if slab is full.
         */
        template <typename... Args>
        size_t allocate(Args &&...args) noexcept(std::is_nothrow_constructible_v<ValueType, Args...>)
        {
            size_t slot = allocate_slot();
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
        void deallocate(const size_t slot) noexcept
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
        ValueType &operator[](const size_t slot) noexcept
        {
            assert(slot < CAPACITY);
            return *storage_.ptr(slot);
        }

        /**
         * @brief Provides read-only access to an allocated object.
         */
        const ValueType &operator[](const size_t slot) const noexcept
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
        size_t allocate_slot() noexcept
        {
            assert(free_top_ > 0 && "slab exhausted!");

            const size_t slot = free_list_[--free_top_];
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
