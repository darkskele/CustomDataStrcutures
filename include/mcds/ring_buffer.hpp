#pragma once

#include <array>
#include <vector>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "detail/aligned_contiguous_storage.hpp"

namespace mcds
{

    /**
     * @brief Simple ring buffer for lightweight fifo and overwrite semantics.
     *
     * @tparam Value Element type stored in the buffer.
     * @tparam CAPACITY Maximum number of elements.
     * @tparam FORCE_HEAP Force dynamic allocation for the underlying storage.
     * @tparam STACK_BUDGET Maximum stack bytes allowed before forcing heap storage.
     *
     * @note When the buffer is full, pushing a new element overwrites the oldest one.
     *       Storage is provided by @c aligned_contiguous_storage, which may use stack
     *       or heap allocation depending on @p FORCE_HEAP and @p STACK_BUDGET.
     */
    template <typename Value, size_t CAPACITY, bool FORCE_HEAP = false, size_t STACK_BUDGET = 1 * 1024 * 1024>
    class ring_buffer
    {
        // Storage calculations
        static constexpr size_t STORAGE_SIZE = sizeof(Value) * CAPACITY;
        static constexpr bool USE_STACK = !FORCE_HEAP && (STORAGE_SIZE <= STACK_BUDGET);

        // Static asserts
        static_assert(CAPACITY > 0, "Capacity can't be 0!");

        // Storage type deduction
        using StorageBuffer = detail::aligned_contiguous_storage<Value, CAPACITY, USE_STACK>;

        // Power of 2 constant for wrap method deduction
        static constexpr bool POW2 = (CAPACITY & (CAPACITY - 1)) == 0;

    public:
        /**
         * @brief Construct an empty ring buffer.
         */
        ring_buffer() : head_(0), tail_(0), size_(0)
        {
        }

        /**
         * @brief Destroy the ring buffer and its elements.
         */
        ~ring_buffer()
        {
            clear();
        }

        /**
         * @brief Deleted copy constructor.
         *
         * The buffer is non copyable to avoid accidental expensive copies and double
         * ownership of manually-managed storage.
         */
        ring_buffer(const ring_buffer &) = delete;

        /**
         * @brief Deleted copy assignment.
         *
         * The buffer is non copyable to avoid accidental expensive copies and double
         * ownership of manually-managed storage.
         */
        ring_buffer &operator=(const ring_buffer &) = delete;

        /**
         * @brief Move constructor.
         *
         * Transfers ownership of the underlying storage and indices from @p other to this instance.
         * The moved from buffer is left empty.
         */
        ring_buffer(ring_buffer &&other) noexcept(std::is_nothrow_move_constructible_v<StorageBuffer>)
            : storage_(std::move(other.storage_)),
              head_(other.head_),
              tail_(other.tail_),
              size_(other.size_)
        {
            other.head_ = 0;
            other.tail_ = 0;
            other.size_ = 0;
        }

        /**
         * @brief Move assignment operator.
         *
         * Destroys the current contents, then transfers ownership of the underlying storage and indices from @p other.
         * The moved from buffer is left empty.
         */
        ring_buffer &operator=(ring_buffer &&other) noexcept(std::is_nothrow_move_assignable_v<StorageBuffer>)
        {
            if (this != &other)
            {
                // Destroy current elements before taking over new storage
                clear();

                storage_ = std::move(other.storage_);
                head_ = other.head_;
                tail_ = other.tail_;
                size_ = other.size_;

                other.head_ = 0;
                other.tail_ = 0;
                other.size_ = 0;
            }
            return *this;
        }

        /**
         * @brief Emplace-construct an element at the back of the buffer.
         *
         * If the buffer is full, the oldest element is destroyed (if needed) and overwritten by the new element.
         *
         * @tparam Args Argument types forwarded to @c Value 's constructor.
         * @param args Constructor arguments for the new element.
         */
        template <typename... Args>
        void emplace(Args &&...args) noexcept(std::is_nothrow_constructible_v<Value, Args &&...>)
        {
            const bool is_full = full();

            // Overwrite on full
            if (is_full)
            {
                // Resource managing value types must be destroyed
                if constexpr (!std::is_trivially_destructible_v<Value>)
                {
                    // Destroy before overwriting
                    storage_.ptr(head_)->~Value();
                }

                // Now overwrite by moving head
                increment_head();
            }

            // Construct new element in place
            new (storage_.ptr(tail_)) Value(std::forward<Args>(args)...);

            // Now move tail across for new values
            increment_tail();

            // If not full, we increment size
            size_ += static_cast<size_t>(!is_full);
        }

        /**
         * @brief Push a copy constructed element at the back of the buffer.
         *
         * @param v Value to copy into the buffer.
         *
         * @note Overwrites the front element if the buffer is full.
         */
        void push(const Value &v)
        {
            emplace(v);
        }

        /**
         * @brief Push a move constructed element at the back of the buffer.
         *
         * @param v Value to move into the buffer.
         *
         * @note Overwrites the front element if the buffer is full.
         */
        void push(Value &&v)
        {
            emplace(std::move(v));
        }

        /**
         * @brief Access the front (oldest) element (const).
         *
         * @return Reference to the front element.
         *
         * @warning Behaviour is undefined if the buffer is empty.
         */
        const Value &front() const noexcept
        {
            return *storage_.ptr(head_);
        }

        /**
         * @brief Access the front (oldest) element.
         *
         * @return Reference to the front element.
         *
         * @warning Behaviour is undefined if the buffer is empty.
         */
        Value &front() noexcept
        {
            return *storage_.ptr(head_);
        }

        /**
         * @brief Access the back (newest) element (const).
         *
         * @return Reference to the back element.
         *
         * @warning Behaviour is undefined if the buffer is empty.
         */
        const Value &back() const noexcept
        {
            const size_t idx = wrap(tail_ == 0 ? CAPACITY - 1 : tail_ - 1);
            return *storage_.ptr(idx);
        }

        /**
         * @brief Access the back (newest) element.
         *
         * @return Reference to the back element.
         *
         * @warning Behaviour is undefined if the buffer is empty.
         */
        Value &back() noexcept
        {
            const size_t idx = wrap(tail_ == 0 ? CAPACITY - 1 : tail_ - 1);
            return *storage_.ptr(idx);
        }

        /**
         * @brief Remove the front (oldest) element.
         *
         * If the buffer is empty, this is a no op.
         */
        void pop_front() noexcept(std::is_nothrow_destructible_v<Value>)
        {
            // Early return for empty buffer
            if (empty())
            {
                return;
            }

            // Destroy if needed
            if constexpr (!std::is_trivially_destructible_v<Value>)
            {
                Value *p = storage_.ptr(head_);
                p->~Value();
            }

            // Overwrite head and reduce size
            increment_head();

            --size_;
        }

        /**
         * @brief Remove the back (newest) element.
         *
         * If the buffer is empty, this is a no op.
         */
        void pop_back() noexcept(std::is_nothrow_destructible_v<Value>)
        {
            // Early return for empty buffer
            if (empty())
            {
                return;
            }

            // Destroy if needed
            if constexpr (!std::is_trivially_destructible_v<Value>)
            {
                // Calculate the actual back index
                const size_t back_idx = wrap(tail_ == 0 ? CAPACITY - 1 : tail_ - 1);
                Value *p = storage_.ptr(back_idx);
                p->~Value();
            }

            // Move tail backward
            decrement_tail();

            --size_;
        }

        /**
         * @brief Random access by logical index.
         *
         * @param i Index relative to the front element.
         * @return Reference to the element at logical position @p i.
         *
         * @warning No bounds checking is performed.
         */
        Value &operator[](const size_t i) noexcept
        {
            // Determine wrapped index
            const size_t idx = wrap(head_ + i);

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
        const Value &operator[](const size_t i) const noexcept
        {
            // Determine wrapped index
            const size_t idx = wrap(head_ + i);

            return *storage_.ptr(idx);
        }

        /**
         * @brief Destroy all elements and reset indices.
         *
         * Destroys all non trivially destructible elements currently stored
         * and resets @c head_ , @c tail_ and @c size_ to zero.
         */
        void clear() noexcept
        {

            // Destroy all elements
            if constexpr (!std::is_trivially_destructible_v<Value>)
            {
                size_t idx = head_;

                for (int i = 0; i < static_cast<int>(size_); ++i)
                {
                    storage_.ptr(idx)->~Value();
                    idx = wrap(idx + 1);
                }
            }

            // Reset pointers
            head_ = 0;
            tail_ = 0;
            size_ = 0;
        }

        /**
         * @brief Check whether the buffer is full.
         *
         * @return true if @c size() == @c capacity(), false otherwise.
         */
        bool full() const noexcept
        {
            return size_ == CAPACITY;
        }

        /**
         * @brief Check whether the buffer is empty.
         *
         * @return true if @c size() == 0, false otherwise.
         */
        bool empty() const noexcept { return size_ == 0; }

        /**
         * @brief Get the maximum number of elements the buffer can hold.
         *
         * @return Constant capacity of the buffer.
         */
        size_t capacity() const noexcept { return CAPACITY; }

        /**
         * @brief Get the current number of elements in the buffer.
         *
         * @return Number of valid elements.
         */
        size_t size() const noexcept { return size_; }

    private:
        /**
         * @brief Wrap an index into.
         *
         * Uses bitwise AND when @c CAPACITY is a power of two, otherwise falls back to modulo.
         *
         * @param x Raw index value.
         * @return Wrapped index.
         */
        size_t wrap(const size_t x) const noexcept
        {
            // Use AND for cheap modulo if power of 2 capcity
            if constexpr (POW2)
            {
                return x & (CAPACITY - 1);
            }
            else
            {
                return x % CAPACITY;
            }
        }

        /**
         * @brief Advance the head index by one with wrapping.
         */
        void increment_head() noexcept
        {
            // Use AND for cheap modulo if power of 2 capcity
            if constexpr (POW2)
            {
                head_ = (head_ + 1) & (CAPACITY - 1);
            }
            else
            {
                // Reset head_ when it reaches capacity
                if (++head_ == CAPACITY)
                {
                    head_ = 0;
                }
            }
        }

        /**
         * @brief Advance the tail index by one with wrapping.
         */
        void increment_tail() noexcept
        {
            // Use AND for cheap modulo if power of 2 capcity
            if constexpr (POW2)
            {
                tail_ = (tail_ + 1) & (CAPACITY - 1);
            }
            else
            {
                // Reset tail_ when it reaches capacity
                if (++tail_ == CAPACITY)
                {
                    tail_ = 0;
                }
            }
        }

        /**
         * @brief Move the tail index backward by one with wrapping.
         */
        void decrement_tail() noexcept
        {
            // Use AND for cheap modulo if power of 2 capacity
            if constexpr (POW2)
            {
                tail_ = (tail_ - 1) & (CAPACITY - 1);
            }
            else
            {
                // Wrap tail_ when it reaches 0
                if (tail_ == 0)
                {
                    tail_ = CAPACITY - 1;
                }
                else
                {
                    --tail_;
                }
            }
        }

        StorageBuffer storage_; ///< Underlying contiguous storage.
        size_t head_;           ///< Index of the front (oldest) element.
        size_t tail_;           ///< Index one past the back (newest) element.
        size_t size_;           ///< Number of valid elements currently stored.
    };

} // namespace mcds
