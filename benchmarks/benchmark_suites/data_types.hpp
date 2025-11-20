#pragma once

#include <array>
#include <cstdint>

namespace mcds::bench
{
    // Payload types
    using SmallPayload = int;

    struct MediumPayload
    {
        std::array<std::uint64_t, 8> data; // 64 bytes
    };

    struct LargePayload
    {
        std::array<std::uint64_t, 64> data; // 512 bytes
    };

    template <typename T>
    inline T make_payload(std::uint64_t seed)
    {
        T v{};
        if constexpr (std::is_same_v<T, SmallPayload>)
        {
            v = static_cast<int>(seed);
        }
        else
        {
            v.data[0] = seed;
        }
        return v;
    }

} // namespace mcds::bench
