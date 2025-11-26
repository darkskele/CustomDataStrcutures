#pragma once

#include <algorithm>
#include <array>
#include <vector>
#include <stdexcept>
#include <type_traits>

#include "memory/slab.hpp"

namespace mcds
{

    /**
     * @brief Fixed capacity sorted flat map with optional stack/heap storage.
     *
     * A FlatMap stores key value pairs in sorted contiguous arrays and performs lookups via binary search.
     *  Insert/erase are O(n) due to shifting, but iteration and lookups are cache friendly.
     *
     * @tparam Key Key type, must be LessThanComparable.
     * @tparam Value Mapped value type.
     * @tparam CAPACITY Maximum number of elements.
     * @tparam FORCE_HEAP If true, always use heap (vector) storage.
     * @tparam STACK_BUDGET Maximum total bytes allowed for stack storage.
     */
    template <typename Key, typename Value, size_t CAPACITY = 1024, bool FORCE_HEAP = false, size_t STACK_BUDGET = 2 * 1024 * 1024>
    class FlatMap
    {
        // Constants for type decision
        /// @brief Total value bytes.
        static constexpr size_t TOTAL_VAL_BYTES = sizeof(Value) * CAPACITY;
        /// @brief Total key bytes.
        static constexpr size_t TOTAL_KEY_BYTES = sizeof(Key) * CAPACITY;

        /// @brief Select stack vs. heap storage for key/value arrays.
        static constexpr bool USE_STACK = !FORCE_HEAP && ((TOTAL_KEY_BYTES + TOTAL_VAL_BYTES) <= STACK_BUDGET);

        static_assert(CAPACITY > 0, "FlatMap capacity must be greater than zero");

        // Stack storage requires default constructible types.
        static_assert(!USE_STACK || std::is_default_constructible_v<Key>, "Key must be default constructible for stack storage");
        static_assert(!USE_STACK || std::is_default_constructible_v<Value>, "Value must be default constructible for stack storage");

        // Both storage types need move or copy semantics.
        static_assert(std::is_copy_constructible_v<Key> || std::is_move_constructible_v<Key>, "Key must be copy or move constructible");
        static_assert(std::is_copy_assignable_v<Key> || std::is_move_assignable_v<Key>, "Key must be copy or move assignable");
        static_assert(std::is_copy_constructible_v<Value> || std::is_move_constructible_v<Value>, "Value must be copy or move constructible");
        static_assert(std::is_copy_assignable_v<Value> || std::is_move_assignable_v<Value>, "Value must be copy or move assignable");

        /**
         * @brief Storage adapter: fixed-size array on stack or vector on heap.
         */
        template <typename T>
        using StorageType = std::conditional_t<
            USE_STACK,
            std::array<T, CAPACITY>,
            std::vector<T>>;

    public:
        /// @brief Key type.
        using key_type = Key;
        /// @brief Mapped value type.
        using value_type = Value;

        /**
         * @brief Constructs an empty FlatMap.
         *
         * For heap backed storage, reserves @p CAPACITY elements.
         */
        FlatMap() : size_(0)
        {
            if constexpr (!USE_STACK)
            {
                keys_.resize(CAPACITY);
                values_.resize(CAPACITY);
            }
        }

        /**
         * @brief Finds a value by key.
         *
         * @param key Key to search for.
         * @return Pointer to value if found, otherwise @c nullptr.
         */
        Value *find(const Key &key) noexcept
        {
            const size_t i = find_index(key);
            if (i != size_ && keys_[i] == key)
            {
                return &values_[i];
            }
            return nullptr;
        }

        /**
         * @brief Inserts or updates a key–value pair.
         *
         * If the key already exists, its value is overwritten. Otherwise, a new key value pair is inserted at the
         * correct sorted position.
         *
         * @param key Key to insert/update.
         * @param value Value to associate with the key.
         * @return @c true on success, @c false only for heap-backed storage when capacity is exhausted. For stack backed storage,
         * overflow throws @c std::runtime_error.
         */
        bool insert(const Key &key, const Value &value)
        {
            // Find insertion point
            size_t i = find_index(key);

            const bool key_exists = (i < size_ && keys_[i] == key);

            if (!key_exists)
            {
                if (size_ >= CAPACITY)
                {
                    if constexpr (!USE_STACK)
                    {
                        // Heap-backed: report failure to caller.
                        return false;
                    }
                    else
                    {
                        throw std::runtime_error("Stack overflow in FlatMap!");
                    }
                }

                // Shift existing elements to make room.
                std::move_backward(keys_.begin() + i,
                                   keys_.begin() + size_,
                                   keys_.begin() + size_ + 1);
                std::move_backward(values_.begin() + i,
                                   values_.begin() + size_,
                                   values_.begin() + size_ + 1);

                keys_[i] = std::move(key);
                ++size_;
            }

            // Insert or overwrite value.
            values_[i] = std::move(value);
            return true;
        }

        /**
         * @brief Erases an element by key.
         *
         * @param key Key to erase.
         * @return @c true if an element was erased, @c false if key not found.
         */
        bool erase(const Key &key) noexcept
        {
            const size_t i = find_index(key);

            if (i >= size_ || keys_[i] != key)
            {
                return false;
            }

            // Shift elements left to fill the gap.
            std::move(keys_.begin() + i + 1,
                      keys_.begin() + size_,
                      keys_.begin() + i);
            std::move(values_.begin() + i + 1,
                      values_.begin() + size_,
                      values_.begin() + i);
            --size_;

            return true;
        }

        /**
         * @brief Returns the smallest key and its value.
         *
         * @return Pair of (key pointer, value pointer), or (nullptr, nullptr) if the map is empty.
         */
        std::pair<const Key *, Value *> find_min() noexcept
        {
            if (size_ == 0)
                return {nullptr, nullptr};
            return {&keys_[0], &values_[0]};
        }

        /**
         * @brief Returns the largest key and its value.
         *
         * @return Pair of (key pointer, value pointer), or (nullptr, nullptr) if the map is empty.
         */
        std::pair<const Key *, Value *> find_max() noexcept
        {
            if (size_ == 0)
                return {nullptr, nullptr};
            return {&keys_[size_ - 1], &values_[size_ - 1]};
        }

        /**
         * @brief Returns pointer to the first key not less than @p key.
         *
         * @param key Search key.
         * @return Pointer to key, or @c nullptr if no such key exists.
         */
        const Key *lower_bound(const Key &key) const noexcept
        {
            auto it = std::lower_bound(keys_.begin(),
                                       keys_.begin() + size_,
                                       key);
            return (it != keys_.begin() + size_) ? &(*it) : nullptr;
        }

        /**
         * @brief Returns pointer to the first key greater than @p key.
         *
         * @param key Search key.
         * @return Pointer to key, or @c nullptr if no such key exists.
         */
        const Key *upper_bound(const Key &key) const noexcept
        {
            auto it = std::upper_bound(keys_.begin(),
                                       keys_.begin() + size_,
                                       key);
            return (it != keys_.begin() + size_) ? &(*it) : nullptr;
        }

        /**
         * @brief Returns current number of stored elements.
         */
        size_t size() const noexcept
        {
            return size_;
        }

        /**
         * @brief Returns @c true if the map contains no elements.
         */
        bool empty() const noexcept
        {
            return size_ == 0;
        }

        /**
         * @brief Returns @c true if the map is at @p CAPACITY.
         */
        bool full() const noexcept
        {
            return size_ >= CAPACITY;
        }

        /**
         * @brief Accesses the value associated with @p key, inserting if needed.
         *
         * If the key does not exist and capacity permits, a new entry is inserted with a default-constructed value, which is then returned.
         * If capacity is exhausted, throws @c std::runtime_error.
         *
         * @param key Key to look up or insert.
         * @return Reference to the mapped value.
         */
        Value &operator[](const Key &key)
        {
            size_t i = find_index(key);

            if (i >= size_ || keys_[i] != key)
            {
                if (size_ >= CAPACITY)
                {
                    throw std::runtime_error(
                        "Overflow in FlatMap! Can't default construct!");
                }

                std::move_backward(keys_.begin() + i,
                                   keys_.begin() + size_,
                                   keys_.begin() + size_ + 1);
                std::move_backward(values_.begin() + i,
                                   values_.begin() + size_,
                                   values_.begin() + size_ + 1);

                keys_[i] = std::move(key);
                values_[i] = std::move(Value{});
                ++size_;
            }

            return values_[i];
        }

    private:
        /**
         * @brief Finds the index where @p key should be inserted.
         *
         * Uses @c std::lower_bound on the sorted key array.
         *
         * @param key Search key.
         * @return Index in [0, size_], where the key is or should be.
         */
        size_t find_index(const Key &key) const noexcept
        {
            auto it = std::lower_bound(keys_.begin(),
                                       keys_.begin() + size_,
                                       key);
            return static_cast<size_t>(it - keys_.begin());
        }

        StorageType<Key> keys_;     ///< Sorted keys.
        StorageType<Value> values_; ///< Mapped values, aligned with @p keys_.
        size_t size_;               ///< Current number of stored elements.
    };

} // namespace mcds
