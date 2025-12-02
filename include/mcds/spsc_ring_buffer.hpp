#pragma once

#include <atomic>
#include <type_traits>
#include <optional>

#include "detail/aligned_contiguous_storage.hpp"

namespace mcds
{
    /**
     * @brief A lock free, SPSC ring buffer.
     *
     * @tparam T Element type stored in the buffer.
     * @tparam CAPACITY Maximum number of elements (all usable).
     * @tparam FORCE_HEAP Force dynamic allocation for the underlying storage.
     * @tparam STACK_BUDGET Maximum stack bytes allowed before forcing heap storage.
     *
     * @note Not thread safe for multiple producers or multiple consumers.
     */
    template <typename T, std::size_t CAPACITY, bool FORCE_HEAP = false, std::size_t STACK_BUDGET = 1 * 1024 * 1024>
    class spsc_ring_buffer
    {
        static_assert(CAPACITY > 0, "Capacity must be greater than zero!");

        // Storage calculations
        static constexpr size_t STORAGE_SIZE = sizeof(T) * CAPACITY;
        static constexpr bool USE_STACK = !FORCE_HEAP && (STORAGE_SIZE <= STACK_BUDGET);

        // Storage type deduction
        using StorageBuffer = detail::aligned_contiguous_storage<T, CAPACITY, USE_STACK>;

        // Power of 2 constant for wrap method deduction
        static constexpr bool POW2 = (CAPACITY & (CAPACITY - 1)) == 0;

    public:
        using value_type = T;

        /**
         * @brief Construct an empty buffer.
         */
        spsc_ring_buffer() = default;

        /**
         * @brief Destroy the buffer, calling destructors on any elements still present.
         */
        ~spsc_ring_buffer()
        {
            clear();
        }

        /**
         * @brief Deleted copy constructor.
         *
         * The buffer is non copyable due to atomic members.
         */
        spsc_ring_buffer(const spsc_ring_buffer &) = delete;

        /**
         * @brief Deleted copy assignment.
         *
         * The buffer is non copyable due to atomic members.
         */
        spsc_ring_buffer &operator=(const spsc_ring_buffer &) = delete;

        /**
         * @brief Move constructor.
         *
         * Transfers ownership of elements from @p other to this instance.
         * The moved from buffer is left empty.
         *
         * @warning Not thread safe. Ensure no concurrent access during move.
         */
        spsc_ring_buffer(spsc_ring_buffer &&other) noexcept(std::is_nothrow_move_constructible_v<T>)
            : head_(other.head_.load(std::memory_order_relaxed)),
              tail_(other.tail_.load(std::memory_order_relaxed)),
              size_(other.size_.load(std::memory_order_relaxed))
        {
            auto sz = size_.load(std::memory_order_relaxed);
            auto head = head_.load(std::memory_order_relaxed);

            // Move construct each element in place
            for (size_t i = 0; i < sz; ++i)
            {
                size_t idx;
                if constexpr (POW2)
                {
                    idx = (head + i) & (CAPACITY - 1);
                }
                else
                {
                    idx = (head + i) % CAPACITY;
                }

                new (storage_.ptr(idx)) T(std::move(*other.storage_.ptr(idx)));
            }

            // Clear other
            other.clear();
        }

        /**
         * @brief Move assignment operator.
         *
         * Move constructs elements from @p other. The moved from buffer is left empty.
         *
         * @warning Not thread safe. Ensure no concurrent access during move.
         */
        spsc_ring_buffer &operator=(spsc_ring_buffer &&other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>)
        {
            if (this != &other)
            {
                // Clear current contents
                clear();
                
                // Copy indices from other
                head_.store(other.head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
                tail_.store(other.tail_.load(std::memory_order_relaxed), std::memory_order_relaxed);
                size_.store(other.size_.load(std::memory_order_relaxed), std::memory_order_relaxed);

                auto sz = size_.load(std::memory_order_relaxed);
                auto head = head_.load(std::memory_order_relaxed);

                // Move construct each element in place
                for (size_t i = 0; i < sz; ++i)
                {
                    size_t idx;
                    if constexpr (POW2)
                    {
                        idx = (head + i) & (CAPACITY - 1);
                    }
                    else
                    {
                        idx = (head + i) % CAPACITY;
                    }
                    new (storage_.ptr(idx)) T(std::move(*other.storage_.ptr(idx)));
                }

                // Clear other
                other.clear();
            }
            return *this;
        }

        /**
         * @brief Emplace construct an element at the back of the buffer.
         *
         * If the buffer is full, returns false without modifying the buffer.
         *
         * @tparam Args Argument types forwarded to @c T 's constructor.
         * @param args Constructor arguments for the new element.
         * @return true if element was successfully inserted, false if buffer was full.
         */
        template <typename... Args>
        bool emplace_back(Args &&...args)
        {
            static_assert(std::is_constructible_v<T, Args...>, "Emplace arguments do not match T's constructor!");

            if (size_.load(std::memory_order_acquire) >= CAPACITY)
            {
                return false; // buffer full
            }

            auto tail = tail_.load(std::memory_order_relaxed);
            new (storage_.ptr(tail)) T(std::forward<Args>(args)...);

            tail_.store(increment_index(tail), std::memory_order_release);
            size_.fetch_add(1, std::memory_order_release);
            return true;
        }

        /**
         * @brief Emplace construct an element at the front of the buffer.
         *
         * If the buffer is full, returns false without modifying the buffer.
         *
         * @tparam Args Argument types forwarded to @c T 's constructor.
         * @param args Constructor arguments for the new element.
         * @return true if element was successfully inserted, false if buffer was full.
         */
        template <typename... Args>
        bool emplace_front(Args &&...args)
        {
            static_assert(std::is_constructible_v<T, Args...>, "Emplace arguments do not match T's constructor!");

            if (size_.load(std::memory_order_acquire) >= CAPACITY)
            {
                return false; // buffer full
            }

            auto head = head_.load(std::memory_order_relaxed);
            auto prev_head = decrement_index(head);
            new (storage_.ptr(prev_head)) T(std::forward<Args>(args)...);

            head_.store(prev_head, std::memory_order_release);
            size_.fetch_add(1, std::memory_order_release);
            return true;
        }

        /**
         * @brief Push a copy constructed element at the back of the buffer.
         *
         * @param value Value to copy into the buffer.
         * @return true if element was successfully inserted, false if buffer was full.
         */
        bool push_back(const T &value)
        {
            return emplace_back(value);
        }

        /**
         * @brief Push a move constructed element at the back of the buffer.
         *
         * @param value Value to move into the buffer.
         * @return true if element was successfully inserted, false if buffer was full.
         */
        bool push_back(T &&value)
        {
            return emplace_back(std::move(value));
        }

        /**
         * @brief Push a copy constructed element at the front of the buffer.
         *
         * @param value Value to copy into the buffer.
         * @return true if element was successfully inserted, false if buffer was full.
         */
        bool push_front(const T &value)
        {
            return emplace_front(value);
        }

        /**
         * @brief Push a move constructed element at the front of the buffer.
         *
         * @param value Value to move into the buffer.
         * @return true if element was successfully inserted, false if buffer was full.
         */
        bool push_front(T &&value)
        {
            return emplace_front(std::move(value));
        }

        /**
         * @brief Access the front element (const).
         *
         * @return Reference to the front element.
         *
         * @warning Behaviour is undefined if the buffer is empty.
         * @warning In SPSC context, consumer should check empty() before calling.
         */
        const T &front() const noexcept
        {
            auto head = head_.load(std::memory_order_acquire);
            return *storage_.ptr(head);
        }

        /**
         * @brief Access the front element.
         *
         * @return Reference to the front element.
         *
         * @warning Behaviour is undefined if the buffer is empty.
         */
        T &front() noexcept
        {
            auto head = head_.load(std::memory_order_acquire);
            return *storage_.ptr(head);
        }

        /**
         * @brief Access the back (newest) element (const).
         *
         * @return Reference to the back element.
         *
         * @warning Behaviour is undefined if the buffer is empty.
         */
        const T &back() const noexcept
        {
            auto tail = tail_.load(std::memory_order_acquire);
            size_t back_idx = decrement_index(tail);
            return *storage_.ptr(back_idx);
        }

        /**
         * @brief Access the back (newest) element.
         *
         * @return Reference to the back element.
         *
         * @warning Behaviour is undefined if the buffer is empty.
         */
        T &back() noexcept
        {
            auto tail = tail_.load(std::memory_order_acquire);
            size_t back_idx = decrement_index(tail);
            return *storage_.ptr(back_idx);
        }

        /**
         * @brief Pop an element from the front of the buffer.
         *
         * The element is moved into an optional to avoid copies.
         *
         * @return std::optional<T> containing the value if available, std::nullopt if empty.
         */
        std::optional<T> pop_front()
        {
            if (size_.load(std::memory_order_acquire) == 0)
                return std::nullopt; // buffer empty

            auto head = head_.load(std::memory_order_relaxed);
            T *elem = storage_.ptr(head);
            std::optional<T> out{std::move(*elem)};
            elem->~T();

            head_.store(increment_index(head), std::memory_order_release);
            size_.fetch_sub(1, std::memory_order_release);
            return out;
        }

        /**
         * @brief Pop an element from the back of the buffer.
         *
         * The element is moved into an optional to avoid copies.
         *
         * @return std::optional<T> containing the value if available, std::nullopt if empty.
         *
         * @note This provides atomic check + value retrieval in one operation (thread safe).
         */
        std::optional<T> pop_back()
        {
            if (size_.load(std::memory_order_acquire) == 0)
            {
                return std::nullopt; // buffer empty
            }

            auto tail = tail_.load(std::memory_order_relaxed);
            size_t back_idx = decrement_index(tail);
            T *elem = storage_.ptr(back_idx);
            std::optional<T> out{std::move(*elem)};
            elem->~T();

            tail_.store(back_idx, std::memory_order_release);
            size_.fetch_sub(1, std::memory_order_release);
            return out;
        }

        /**
         * @brief Random access by logical index.
         *
         * @param i Index relative to the front element.
         * @return Reference to the element at logical position @p i.
         *
         * @warning No bounds checking is performed.
         */
        T &operator[](const size_t i) noexcept
        {
            auto head = head_.load(std::memory_order_acquire);
            size_t idx = (head + i) % CAPACITY;
            if constexpr (POW2)
            {
                idx = (head + i) & (CAPACITY - 1);
            }
            return *storage_.ptr(idx);
        }

        /**
         * @brief Random access by logical index (const).
         *
         * @param i Index relative to the front element.
         * @return Const reference to the element at logical position @p i.
         *
         * @warning No bounds checking is performed.
         */
        const T &operator[](const size_t i) const noexcept
        {
            auto head = head_.load(std::memory_order_acquire);
            size_t idx = (head + i) % CAPACITY;
            if constexpr (POW2)
            {
                idx = (head + i) & (CAPACITY - 1);
            }
            return *storage_.ptr(idx);
        }

        /**
         * @brief Get the current number of elements in the buffer.
         *
         * @return Number of valid elements.
         *
         * @note This is an approximate value in concurrent scenarios.
         */
        size_t size() const noexcept
        {
            return size_.load(std::memory_order_acquire);
        }

        /**
         * @brief Check whether the buffer is empty.
         *
         * @return true if @c size() == 0, false otherwise.
         *
         * @note This is an approximate check in concurrent scenarios.
         */
        bool empty() const noexcept
        {
            return size() == 0;
        }

        /**
         * @brief Check whether the buffer is full.
         *
         * @return true if @c size() == @c capacity(), false otherwise.
         *
         * @note This is an approximate check in concurrent scenarios.
         */
        bool full() const noexcept
        {
            return size() == CAPACITY;
        }

        /**
         * @brief Get the maximum number of elements the buffer can hold.
         *
         * @return Constant capacity of the buffer.
         */
        size_t capacity() const noexcept
        {
            return CAPACITY;
        }

        /**
         * @brief Destroy all elements and reset indices.
         *
         * Destroys all non trivially destructible elements currently stored and resets @c head_,
         * @c tail_, and @c size_ to zero.
         */
        void clear() noexcept
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                auto head = head_.load(std::memory_order_relaxed);
                auto current_size = size_.load(std::memory_order_relaxed);

                for (size_t i = 0; i < current_size; ++i)
                {
                    storage_.ptr(head)->~T();
                    head = increment_index(head);
                }
            }

            head_.store(0, std::memory_order_relaxed);
            tail_.store(0, std::memory_order_relaxed);
            size_.store(0, std::memory_order_relaxed);
        }

    private:
        /**
         * @brief Increment an index with wrapping.
         *
         * Uses bitwise AND when @c CAPACITY is a power of two, otherwise falls back to modulo.
         *
         * @param x Index value to increment.
         * @return Wrapped incremented index.
         */
        size_t increment_index(size_t x) const noexcept
        {
            if constexpr (POW2)
            {
                return (x + 1) & (CAPACITY - 1);
            }
            else
            {
                return (x + 1) % CAPACITY;
            }
        }

        /**
         * @brief Decrement an index with wrapping.
         *
         * Uses bitwise AND when @c CAPACITY is a power of two, otherwise handles wrap manually.
         *
         * @param x Index value to decrement.
         * @return Wrapped decremented index.
         */
        size_t decrement_index(size_t x) const noexcept
        {
            if constexpr (POW2)
            {
                return (x - 1) & (CAPACITY - 1);
            }
            else
            {
                return x == 0 ? CAPACITY - 1 : x - 1;
            }
        }

        StorageBuffer storage_;                   ///< Underlying contiguous storage.
        alignas(64) std::atomic<size_t> head_{0}; ///< Index of the front element.
        alignas(64) std::atomic<size_t> tail_{0}; ///< Index one past the back (newest) element.
        alignas(64) std::atomic<size_t> size_{0}; ///< Number of valid elements currently stored.
    };

} // namespace mcds
