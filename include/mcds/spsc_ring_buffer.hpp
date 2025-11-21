#pragma once

#include <atomic>
#include <type_traits>
#include <optional>

namespace mcds
{

    /**
     * @brief A lock-free, single-producer single-consumer (SPSC) ring buffer.
     *
     * @tparam T         Element type stored in the buffer.
     * @tparam Capacity  Maximum number of slots in the buffer. One slot
     *                   is always unused to distinguish full vs empty.
     *
     * @note Not thread safe for multiple producers or multiple consumers. Designed
     * for high throughput SPSC scenarios. Uses acquire/release atomics to guarantee
     * correct ordering.
     */
    template <typename T, std::size_t Capacity>
    class spsc_ring_buffer
    {
        static_assert(Capacity > 1, "Capacity must be greater than one!");
        /// Raw storage type for one element of type T
        using storage_t = std::aligned_storage_t<sizeof(T), alignof(T)>;

    public:
        /**
         * @brief Construct an empty buffer.
         */
        spsc_ring_buffer() = default;

        /**
         * @brief Destroy the buffer, calling destructors on any elements still present.
         */
        ~spsc_ring_buffer()
        {
            auto tail = tail_.load(std::memory_order_relaxed);
            auto head = head_.load(std::memory_order_acquire);

            while (tail != head)
            {
                T *elem = reinterpret_cast<T *>(&storage_[tail]);
                elem->~T(); // destroy directly
                tail = (tail + 1) % Capacity;
            }
        }

        // Non copyable, non movable for now
        spsc_ring_buffer(const spsc_ring_buffer &) = delete;
        spsc_ring_buffer &operator=(const spsc_ring_buffer &) = delete;

        /**
         * @brief Construct a new element in place at the head of the buffer.
         *
         * Perfectly forwards arguments to T's constructor.
         *
         * @tparam Args  Constructor argument types for T.
         * @param args   Constructor arguments for T.
         * @return true  If the element was successfully inserted.
         * @return false If the buffer was full.
         */
        template <typename... Args>
        bool emplace(Args &&...args)
        {
            static_assert(std::is_constructible_v<T, Args...>,
                          "Emplace arguments do not match T's constructor!");

            auto head = head_.load(std::memory_order_relaxed);
            auto next_head = (head + 1) % Capacity;

            if (next_head == tail_.load(std::memory_order_acquire))
                return false; // buffer full

            ::new (static_cast<void *>(&storage_[head]))
                T(std::forward<Args>(args)...);

            head_.store(next_head, std::memory_order_release);
            return true;
        }

        /**
         * @brief Pop an element from the tail of the buffer.
         *
         * The element is moved to optional to avoid copies by consumers.
         *
         * @return Optional return if buffer is available.
         */
        std::optional<T> pop()
        {
            auto tail = tail_.load(std::memory_order_relaxed);
            if (tail == head_.load(std::memory_order_acquire))
                return std::nullopt; // buffer empty

            T *elem = reinterpret_cast<T *>(&storage_[tail]);

            // Construct directly from *elem
            std::optional<T> out{std::move(*elem)};
            elem->~T(); // destroy the old one

            tail_.store((tail + 1) % Capacity, std::memory_order_release);
            return out; // RVO ensures no extra construction
        }

        /**
         * @brief Get the current number of elements in the buffer.
         */
        size_t size() const noexcept
        {
            auto head = head_.load(std::memory_order_acquire);
            auto tail = tail_.load(std::memory_order_acquire);
            return (head + Capacity - tail) % Capacity;
        }

        /**
         * @brief Get the maximum capacity of the buffer (not counting the one wasted slot).
         */
        size_t capacity() const noexcept
        {
            return Capacity;
        }

        /**
         * @brief Clears buffer.
         */
        void clear()
        {
            head_.store(0, std::memory_order_relaxed);
            tail_.store(0, std::memory_order_relaxed);
        }

    private:
        storage_t storage_[Capacity]; ///< Raw element storage
        alignas(64) std::atomic<size_t> head_{0}; ///< Next write index (producer-owned)
        alignas(64) std::atomic<size_t> tail_{0}; ///< Next read index (consumer-owned)
    };

} // namespace mcds
