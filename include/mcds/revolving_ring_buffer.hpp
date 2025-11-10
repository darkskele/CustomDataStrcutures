#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <cassert>

namespace btc_stream::streamer::buffers
{

    /**
     * @brief A ring buffer with revolving overwrites. Defined with max capacity,
     * any pushes after capacity reached will overwrite oldest entry, FIFO style.
     * Uses a reversed ring buffer style for cache friendly forward iteration (
     * newest to oldest).
     *
     * @tparam T Datatype to store.
     * @tparam Capacity Max capacity, of size_t.
     *
     * @note Is not thread safe.
     */
    template <typename T, size_t Capacity = 1024>
    class revolving_recency_buffer
    {
        // Safety static asserts
        static_assert(Capacity > 0, "Capacity can't be 0!");
        /// @brief Raw storage type for T.
        using storage_t = std::aligned_storage_t<sizeof(T), alignof(T)>;

    public:
        /**
         * @brief Constructor for ring buffer.
         */
        revolving_recency_buffer() noexcept : head_(Capacity - 1), size_(0) {}

        /**
         * @brief Destructor for ring buffer. Destroys all objects in buffer,
         */
        ~revolving_recency_buffer() noexcept
        {
            // Clear
            clear();
        }

        ///@brief Rule of five No copy but allow move.
        revolving_recency_buffer(const revolving_recency_buffer &) = delete;
        revolving_recency_buffer &operator=(const revolving_recency_buffer &) = delete;
        revolving_recency_buffer(revolving_recency_buffer &&) noexcept = delete;
        revolving_recency_buffer &operator=(revolving_recency_buffer &&) noexcept = delete;

        /**
         * @brief Forward iterator for range based loops.
         *
         * @tparam IsConst Flag for const and non const iterators.
         */
        template <bool IsConst>
        class iterator_impl
        {
        public:
            /// @brief T specific type iterator types.
            using iterator_category = std::forward_iterator_tag;           ///< Iterator tag.
            using difference_type = std::ptrdiff_t;                        ///< Difference type.
            using pointer = std::conditional_t<IsConst, const T *, T *>;   ///< Pointer type, conditional on is const.
            using reference = std::conditional_t<IsConst, const T &, T &>; ///< Reference type, conditional on is const.

            /**
             * @brief Constructor for revolving_recency_buffer forward iterator.
             */
            iterator_impl(std::conditional_t<IsConst, const revolving_recency_buffer *, revolving_recency_buffer *> buff,
                          size_t logical_idx) noexcept
                : buff_(buff), logical_idx_(logical_idx) {}

            /// @brief Defreference overloads.
            reference operator*() const noexcept { return buff_->get(logical_idx_); }
            pointer operator->() const noexcept { return &buff_->get(logical_idx_); }

            /// @brief Pre increment operator overload.
            iterator_impl &operator++() noexcept
            {
                ++logical_idx_;
                return *this;
            }

            /// @brief Post increment operator overload.
            iterator_impl operator++(int) noexcept
            {
                iterator_impl tmp = *this; // current value
                ++(*this);                 // increment
                return tmp;                // return old value
            }

            /// @brief Equality overloads.
            friend bool operator==(const iterator_impl &a, const iterator_impl &b) noexcept
            {
                return a.buff_ == b.buff_ && a.logical_idx_ == b.logical_idx_;
            }

            friend bool operator!=(const iterator_impl &a, const iterator_impl &b) noexcept
            {
                return !(a == b);
            }

            /// @brief  Cross template friend access
            template <bool OtherConst>
            friend class iterator_impl;

            /// @brief Cross-type comparison (only need one direction due to symmetry)
            template <bool OtherConst>
            friend bool operator==(const iterator_impl<IsConst> &a, const iterator_impl<OtherConst> &b) noexcept
            {
                return a.buff_ == b.buff_ && a.logical_idx_ == b.logical_idx_;
            }

            template <bool OtherConst>
            friend bool operator!=(const iterator_impl<IsConst> &a, const iterator_impl<OtherConst> &b) noexcept
            {
                return !(a == b);
            }

        private:
            std::conditional_t<IsConst, const revolving_recency_buffer *,
                               revolving_recency_buffer *>
                buff_;           ///< Buffer to iterate.
            size_t logical_idx_; // Current index wrapped by iterator.
        };

        /// @brief Iterator types.
        using iterator = iterator_impl<false>;
        using const_iterator = iterator_impl<true>;

        /**
         * @brief Begin iterator for range based loops.
         */
        iterator begin() noexcept
        {
            return iterator(this, 0);
        }

        /**
         * @brief End iterator for range based loops.
         */
        iterator end() noexcept
        {
            return iterator(this, size_);
        }

        /**
         * @brief Const specific iterator getters.
         */
        const_iterator begin() const noexcept
        {
            return const_iterator(this, 0);
        }
        const_iterator end() const noexcept
        {
            return const_iterator(this, size_);
        }
        const_iterator cbegin() const noexcept
        {
            return const_iterator(this, 0);
        }
        const_iterator cend() const noexcept
        {
            return const_iterator(this, size_);
        }

        /**
         * @brief Emplace from arguments of T in place into buffer.
         * @param args Argument pack.
         */
        template <typename... Args>
        void emplace(Args &&...args) noexcept(std::is_nothrow_constructible_v<T, Args &&...>)
        {
            if (size_ == Capacity)
            {
                // Destroy oldest element at max capacity.
                std::destroy_at(std::launder(reinterpret_cast<T *>(&buff_[head_])));
            }
            else
            {
                // Increment size.
                ++size_;
            }

            // Placement new
            ::new (static_cast<void *>(&buff_[head_])) T(std::forward<Args>(args)...);

            // Increment head pointer
            head_ = (head_ + Capacity - 1) % Capacity;
        }

        /**
         * @brief Push wrappers for copy insertion.
         * @param value Value of type T.
         */
        void push(const T &value) noexcept
        {
            emplace(value);
        }

        /**
         * @brief Push wrappers for move insertion.
         * @param value Value of type T.
         */
        void push(T &&value) noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            emplace(std::move(value));
        }

        /**
         * @brief Clears the buffer for clean reset.
         */
        void clear() noexcept
        {
            // Destroy
            for (size_t i = 0; i < size_; ++i)
            {
                std::destroy_at(&get(i));
            }
            size_ = 0;
            head_ = Capacity - 1;
        }

        /**
         * @brief Access operator overload. Indexes backwards from head with head being 0.
         * @param idx Index backwards from head (most recent).
         */
        T &operator[](const size_t idx) noexcept
        {
            return get(idx);
        }
        const T &operator[](const size_t idx) const noexcept
        {
            return get(idx);
        }

        /**
         * @brief Safe access methods. Throws on out of bounds.
         * @param idx Index backwards from head (most recent).
         */
        T &at(const size_t idx)
        {
            if (idx >= size_)
                throw std::out_of_range("revolving_recency_buffer::at");
            return get(idx);
        }
        const T &at(const size_t idx) const
        {
            if (idx >= size_)
                throw std::out_of_range("revolving_recency_buffer::at");
            return get(idx);
        }

        /**
         * @brief Front access for most recent addition.
         */
        T &front() noexcept
        {
            assert(size_ > 0 && "Front called on empty buffer!");
            return get(0);
        }
        const T &front() const noexcept
        {
            assert(size_ > 0 && "Front called on empty buffer!");
            return get(0);
        }

        /**
         * @brief Back access for oldest addition.
         */
        T &back() noexcept
        {
            assert(size_ > 0 && "Back called on empty buffer!");
            return get(size_ - 1);
        }
        const T &back() const noexcept
        {
            assert(size_ > 0 && "Back called on empty buffer!");
            return get(size_ - 1);
        }

        /// @brief State getters.
        size_t size() const noexcept { return size_; }
        constexpr size_t capicity() const noexcept { return Capacity; }
        bool empty() const noexcept { return size_ == 0; }
        bool full() const noexcept { return size_ == Capacity; }

    private:
        /// @brief Access helper. Gets from behind head.
        T &get(const size_t idx) noexcept
        {
            assert(idx < size_ && "revolving_recency_buffer out of range!");
            size_t pos = (head_ + 1 + idx) % Capacity;
            return *std::launder(reinterpret_cast<T *>(&buff_[pos]));
        }
        const T &get(const size_t idx) const noexcept
        {
            assert(idx < size_ && "revolving_recency_buffer out of range!");
            size_t pos = (head_ + 1 + idx) % Capacity;
            return *std::launder(reinterpret_cast<const T *>(&buff_[pos]));
        }

        storage_t buff_[Capacity]; ///< Raw data buffer.
        size_t head_;              ///< Pointer to head of buffer.
        size_t size_;              ///< Size of buffer.
    };

} // namespace btc_stream::streamer::buffers
