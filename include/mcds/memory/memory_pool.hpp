#pragma once

#include <bit>
#include <array>
#include <type_traits>
#include <cassert>
#include <cstddef>

#include "mcds/memory/slab.hpp"

namespace
{
    /**
     * @brief Compile-time layout computation for a slab-based memory pool with fixed slab bit allocation.
     *
     * Uses a fixed 4 bit allocation for slab indices (max 16 slabs) and dynamically computes
     * the optimal number of slabs to minimize overcapacity while keeping pack/unpack operations
     * as simple bit shifts and masks.
     *
     * @tparam TOTAL_CAPACITY Total number of elements desired in the pool.
     */
    template <size_t TOTAL_CAPACITY>
    struct compute_pool_layout
    {
        static_assert(TOTAL_CAPACITY > 0, "Capacity must be non-zero");

        /// @brief Determine minimum index type needed to represent TOTAL_CAPACITY distinct values.
        static constexpr size_t BITS_AVAILABLE = TOTAL_CAPACITY <= (1ULL << 8) ? 8 : TOTAL_CAPACITY <= (1ULL << 16) ? 16
                                            : TOTAL_CAPACITY <= (1ULL << 32)   ? 32 : 64;

        /// @brief Fixed slab bit allocation (always 4 bits = max 16 slabs).
        static constexpr size_t SLAB_BITS = 4;
        static constexpr size_t MAX_POSSIBLE_SLABS = 1ULL << SLAB_BITS;

        /// @brief Remaining bits allocated to slot indices.
        static constexpr size_t SLOT_BITS = BITS_AVAILABLE - SLAB_BITS;
        static constexpr size_t MAX_SLOT_CAPACITY = 1ULL << SLOT_BITS;

        /**
         * @brief Find optimal number of slabs to minimize overcapacity.
         *
         * Iterates through possible slab counts [1, 16] and selects the configuration
         * that minimizes wasted capacity while respecting slot bit constraints.
         *
         * @return Optimal number of slabs.
         */
        static constexpr size_t compute_optimal_slabs()
        {
            size_t best_slabs = 1;
            size_t min_waste = SIZE_MAX;

            for (size_t s = 1; s <= MAX_POSSIBLE_SLABS; ++s)
            {
                // Calculate slots needed per slab (round up)
                size_t slots_needed = (TOTAL_CAPACITY + s - 1) / s;

                // Check if this configuration fits in available slot bits
                if (slots_needed <= MAX_SLOT_CAPACITY)
                {
                    size_t actual_capacity = s * slots_needed;
                    size_t waste = actual_capacity - TOTAL_CAPACITY;

                    if (waste < min_waste)
                    {
                        min_waste = waste;
                        best_slabs = s;
                    }
                }
            }

            return best_slabs;
        }

        /// @brief Optimal number of slabs computed to minimize waste.
        static constexpr size_t NUM_SLABS = compute_optimal_slabs();

        /// @brief Capacity per slab (rounded up to fit TOTAL_CAPACITY).
        static constexpr size_t SLAB_CAPACITY = (TOTAL_CAPACITY + NUM_SLABS - 1) / NUM_SLABS;

        /// @brief Actual total capacity (may slightly exceed requested).
        static constexpr size_t ACTUAL_CAPACITY = NUM_SLABS * SLAB_CAPACITY;

        static_assert(ACTUAL_CAPACITY >= TOTAL_CAPACITY, "Insufficient capacity");
        static_assert(NUM_SLABS <= MAX_POSSIBLE_SLABS, "Too many slabs");
        static_assert(SLAB_CAPACITY <= MAX_SLOT_CAPACITY, "Slab capacity exceeds slot bits");

        /// @brief Unsigned integer type that can hold any packed index.
        using IndexType = std::conditional_t<BITS_AVAILABLE <= 8, uint8_t, std::conditional_t<BITS_AVAILABLE <= 16, uint16_t,
                                                  std::conditional_t<BITS_AVAILABLE <= 32, uint32_t, uint64_t>>>;

        /// @brief Unsigned integer type for slab indices.
        using SlabIndexType = std::conditional_t<NUM_SLABS <= (1ULL << 8), uint8_t, std::conditional_t<NUM_SLABS <= (1ULL << 16), uint16_t,
                                                  std::conditional_t<NUM_SLABS <= (1ULL << 32), uint32_t, uint64_t>>>;

        /// @brief Unsigned integer type for slot indices within a slab.
        using SlotIndexType = std::conditional_t<SLAB_CAPACITY <= (1ULL << 8), uint8_t, std::conditional_t<SLAB_CAPACITY <= (1ULL << 16), uint16_t,
                                                  std::conditional_t<SLAB_CAPACITY <= (1ULL << 32), uint32_t, uint64_t>>>;

        /// @brief Mask for extracting slot index from packed index.
        static constexpr IndexType SLOT_MASK = (IndexType{1} << SLOT_BITS) - 1;
    };

} // anonymous namespace

namespace mcds::memory
{

    /**
     * @brief Fixed capacity memory pool built from multiple slabs.
     *
     * The pool splits the global capacity across @c NUM_SLABS individual slabs of capacity @c SLAB_CAPACITY each.
     * A single packed @c index_type encodes both slab index and slot index.
     *
     * @note The pool does not do double-free checks or lifetime validation, this is left to the caller.
     *
     * @tparam Value Stored value type.
     * @tparam CAPACITY Desired total capacity of the pool.
     * @tparam FORCE_HEAP If true, slabs always allocate their storage on the heap.
     * @tparam STACK_BUDGET Defines the maximum number of bytes before structure is forced onto the heap.
     */
    template <typename Value, size_t CAPACITY, bool FORCE_HEAP = false, size_t STACK_BUDGET = 64 * 1024>
    class memory_pool
    {
        static_assert(CAPACITY > 0, "Capacity must be non-zero");

        using Layout = compute_pool_layout<CAPACITY>;

        static constexpr size_t NUM_SLABS = Layout::NUM_SLABS;
        static constexpr size_t SLAB_CAPACITY = Layout::SLAB_CAPACITY;
        static constexpr size_t ACTUAL_CAPACITY = Layout::ACTUAL_CAPACITY;
        static constexpr size_t SLAB_BITS = Layout::SLAB_BITS;
        static constexpr size_t SLOT_BITS = Layout::SLOT_BITS;

    public:
        /// @brief Value type stored in the pool.
        using value_type = Value;
        /// @brief Packed index type (slab + slot).
        using index_type = typename Layout::IndexType;
        /// @brief Index type for addressing slabs.
        using slab_index_type = typename Layout::SlabIndexType;
        /// @brief Index type for addressing slots within a slab.
        using slot_index_type = typename Layout::SlotIndexType;

    private:
        /// @brief Total byte size needed for all Value objects.
        static constexpr size_t STORAGE_SIZE = sizeof(Value) * ACTUAL_CAPACITY;

        /// @brief Whether to store data on stack.
        static constexpr bool USE_STACK = !FORCE_HEAP && (STORAGE_SIZE <= STACK_BUDGET);

        /// @brief Slab type for storing values.
        using SlabType = slab<Value, Layout::SLAB_CAPACITY, !USE_STACK>;

        /// @brief Mask for extracting the slot bits from a packed index.
        static constexpr index_type SLOT_MASK = (index_type{1} << SLOT_BITS) - 1;

    public:
        /**
         * @brief Constructs an empty memory pool.
         *
         * All slabs start as empty and are placed in the available slab stack.
         */
        memory_pool() : slabs_(), available_slabs_(), available_top_(NUM_SLABS)
        {
            // Initialize stack of slab indices
            for (size_t i = 0; i < NUM_SLABS; ++i)
            {
                available_slabs_[i] = static_cast<slab_index_type>(i);
            }
        }

        /// @brief Slabs manage their own destruction.
        ~memory_pool() = default;

        /// @brief No copies or move, ownership is explict and not transient
        memory_pool(const memory_pool &) = delete;
        memory_pool &operator=(const memory_pool &) = delete;
        memory_pool(memory_pool &&) noexcept = delete;
        memory_pool &operator=(memory_pool &&) noexcept = delete;

        /**
         * @brief Allocates a new object in the pool and constructs it in place.
         *
         * @tparam Args Constructor argument types.
         * @param args Arguments forwarded to @p Value's constructor.
         * @return Packed @c index_type identifying the allocated object.
         *
         * @note Undefined behaviour if the pool is exhausted.
         */
        template <typename... Args>
        index_type allocate(Args &&...args) noexcept(std::is_nothrow_constructible_v<Value, Args...>)
        {
            assert(available_top_ > 0 && "Memory pool exhausted!");

            // Choose the top available slab.
            const slab_index_type slab_idx = available_slabs_[available_top_ - 1];

            // Allocate from that slab.
            const slot_index_type slot_idx = slabs_[slab_idx].allocate(std::forward<Args>(args)...);

            // If slab is now full, remove it from the available stack.
            if (!slabs_[slab_idx].has_free())
            {
                --available_top_;
            }

            return pack_index(slab_idx, slot_idx);
        }

        /**
         * @brief Deallocates the object at the given packed index.
         *
         * @param idx Packed @c index_type returned by allocate().
         *
         * @note Caller must guarantee the index is valid and not double freed.
         */
        void deallocate(const index_type idx) noexcept
        {
            const auto [slab_idx, slot_idx] = unpack_index(idx);

            // Check whether slab was full before deallocation.
            const bool was_full = !slabs_[slab_idx].has_free();

            // Deallocate from the underlying slab.
            slabs_[slab_idx].deallocate(slot_idx);

            if (was_full)
            {
                // Slab gained free space again, push it back onto the stack.
                available_slabs_[available_top_++] = slab_idx;
            }
        }

        /**
         * @brief Mutable access to an object by packed index.
         */
        Value &operator[](const index_type idx) noexcept
        {
            const auto [slab_idx, slot_idx] = unpack_index(idx);
            return slabs_[slab_idx][slot_idx];
        }

        /**
         * @brief Read only access to an object by packed index.
         */
        const Value &operator[](const index_type idx) const noexcept
        {
            const auto [slab_idx, slot_idx] = unpack_index(idx);
            return slabs_[slab_idx][slot_idx];
        }

        /**
         * @brief Returns true if there is at least one free slot in the pool.
         */
        bool has_free() const noexcept
        {
            return available_top_ > 0;
        }

        /**
         * @brief Returns the requested logical capacity of the pool.
         *
         * @note The internally allocated capacity (@c ACTUAL_CAPACITY) may be equal or greater than @c CAPACITY.
         */
        size_t capacity() const noexcept
        {
            return ACTUAL_CAPACITY;
        }

        /**
         * @brief Returns the number of currently allocated objects.
         *
         * @warning This is O(NUM_SLABS) due to the per-slab scan.
         */
        size_t size() const noexcept
        {
            size_t sz = 0;
            for (const auto &sl : slabs_)
            {
                sz += sl.size();
            }
            return sz;
        }

    private:
        /**
         * @brief Packs slab and slot indices into a single index_type.
         *
         * Layout: [ slab_bits | slot_bits ].
         */
        static constexpr index_type
        pack_index(const slab_index_type slab_idx, const slot_index_type slot) noexcept
        {
            return (static_cast<index_type>(slab_idx) << SLOT_BITS) | static_cast<index_type>(slot);
        }

        /**
         * @brief Unpacks a packed index into (slab_idx, slot_idx).
         */
        static constexpr std::pair<slab_index_type, slot_index_type>
        unpack_index(const index_type idx) noexcept
        {
            const slab_index_type slab_idx = static_cast<slab_index_type>(idx >> SLOT_BITS);
            const slot_index_type slot = static_cast<slot_index_type>(idx & SLOT_MASK);
            return {slab_idx, slot};
        }

        std::array<SlabType, NUM_SLABS> slabs_;                  ///<  Underlying slab array.
        std::array<slab_index_type, NUM_SLABS> available_slabs_; ///< Stack of slab indices that still have free space.
        size_t available_top_;                                   ///< Stack top (number of slabs with free space).
    };

} // namespace mcds::memory
