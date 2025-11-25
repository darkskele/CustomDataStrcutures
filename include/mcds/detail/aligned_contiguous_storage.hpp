#pragma once

#include <vector>
#include <cstddef>
#include <new>

namespace mcds::detail
{
    /**
     * @brief One properly aligned raw slot for a single @p Value.
     *
     * Provides @c sizeof(Value) bytes with alignment suitable for @p Value.
     * No construction or destruction of @p Value is performed.
     */
    template <typename Value>
    struct value_slot
    {
        alignas(Value) std::byte data[sizeof(Value)];
    };

    /**
     * @brief Primary template for aligned contiguous storage.
     *
     * @tparam Value Element type that will be stored.
     * @tparam CAPACITY Number of slots in the storage.
     * @tparam USE_STACK If true, uses stack storage; otherwise heap storage.
     *
     * This class only manages raw memory. It does not construct or destroy @p Value objects,
     * callers are expected to use placement new and explicit destruction as needed.
     */
    template <typename Value, std::size_t CAPACITY, bool USE_STACK>
    struct aligned_contiguous_storage;

    /**
     * @brief Stack based specialization for aligned contiguous storage.
     *
     * Stores @p CAPACITY * sizeof(Value) bytes on the stack, aligned for @p Value. Intended for
     * small capacities where stack usage is acceptable.
     */
    template <typename Value, std::size_t CAPACITY>
    struct aligned_contiguous_storage<Value, CAPACITY, true>
    {
        /// @brief One contiguous block of raw stack storage.
        alignas(Value) std::byte buf_[sizeof(Value) * CAPACITY];

        aligned_contiguous_storage() = default;
        ~aligned_contiguous_storage() = default;

        /**
         * @brief Get a pointer to the ith slot as @c Value*.
         *
         * @param i Index.
         * @return Pointer to the @p i th slot interpreted as @c Value*.
         *
         * @note The caller is responsible for ensuring that a @c Value object is actually constructed at
         * this location before use.
         */
        Value *ptr(std::size_t i) noexcept
        {
            return std::launder(reinterpret_cast<Value *>(buf_ + sizeof(Value) * i));
        }

        /**
         * @brief Get a const pointer to the ith slot as @c const Value*.
         *
         * @param i Index.
         * @return Pointer to the @p i th slot interpreted as @c const Value*.
         */
        const Value *ptr(std::size_t i) const noexcept
        {
            return std::launder(reinterpret_cast<const Value *>(buf_ + sizeof(Value) * i));
        }
    };

    /**
     * @brief Heap based specialization for aligned contiguous storage.
     *
     * Uses a @c std::vector of @c value_slot<Value> to provide @p CAPACITY aligned slots on the heap.
     */
    template <typename Value, std::size_t CAPACITY>
    struct aligned_contiguous_storage<Value, CAPACITY, false>
    {
        using slot_type = value_slot<Value>;

        /// @brief Underlying heap backed buffer of aligned slots.
        std::vector<slot_type> buf_;

        /**
         * @brief Construct storage with @p CAPACITY slots.
         *
         * Allocates @p CAPACITY aligned slots on the heap.
         */
        aligned_contiguous_storage() : buf_(CAPACITY) {}

        ~aligned_contiguous_storage() = default;

        /**
         * @brief Get a pointer to the ith slot as @c Value*.
         *
         * @param i Index.
         * @return Pointer to the @p i th slot interpreted as @c Value*.
         *
         * @note The caller is responsible for construction/destruction of any @c Value objects stored
         * at this location.
         */
        Value *ptr(std::size_t i) noexcept
        {
            return std::launder(reinterpret_cast<Value *>(&buf_[i]));
        }

        /**
         * @brief Get a const pointer to the ith slot as @c const Value*.
         *
         * @param i Index.
         * @return Pointer to the @p i th slot interpreted as @c const Value*.
         */
        const Value *ptr(std::size_t i) const noexcept
        {
            return std::launder(reinterpret_cast<const Value *>(&buf_[i]));
        }
    };

} // namespace mcds::detail
