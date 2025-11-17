#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <iostream>

namespace mcds::tests
{

    // Global tracking for leak detection
    inline std::atomic<int> constructions{0};
    inline std::atomic<int> destructions{0};

    inline void reset_tracking()
    {
        constructions = 0;
        destructions = 0;
    }

    inline bool has_leaks()
    {
        return constructions.load(std::memory_order_relaxed) != destructions.load(std::memory_order_relaxed);
    }

    inline int leaked_objects()
    {
        return constructions.load(std::memory_order_relaxed) - destructions.load(std::memory_order_relaxed);
    }

    // Complex test type. Multiple members for validation and correctness tests.
    struct ComplexType
    {
        int primary_id;
        int secondary_id;
        std::string label;

        // Default constructor
        ComplexType()
            : primary_id(0), secondary_id(0), label("")
        {
        }

        // Value constructor
        ComplexType(int pid, int sid, std::string lbl = "")
            : primary_id(pid), secondary_id(sid), label(std::move(lbl))
        {
        }

        // Comparison operators for use as key
        bool operator<(const ComplexType &other) const
        {
            if (primary_id != other.primary_id)
                return primary_id < other.primary_id;
            return secondary_id < other.secondary_id;
        }

        bool operator==(const ComplexType &other) const
        {
            return primary_id == other.primary_id &&
                   secondary_id == other.secondary_id;
        }

        bool operator!=(const ComplexType &other) const
        {
            return !(*this == other);
        }

        bool operator>(const ComplexType &other) const
        {
            return other < *this;
        }

        bool operator<=(const ComplexType &other) const
        {
            return !(other < *this);
        }

        bool operator>=(const ComplexType &other) const
        {
            return !(*this < other);
        }

        // For debugging output
        friend std::ostream &operator<<(std::ostream &os, const ComplexType &ct)
        {
            return os << "ComplexType(" << ct.primary_id << ", " << ct.secondary_id << ", \"" << ct.label << "\")";
        }
    };

    // Helper struct that actually tracks construction/destruction
    struct TrackedResource
    {
        int value;

        explicit TrackedResource(int v) : value(v)
        {
            ++constructions;
        }

        ~TrackedResource()
        {
            ++destructions;
        }

        // Disable copy/move to ensure tracking accuracy
        TrackedResource(const TrackedResource &) = delete;
        TrackedResource &operator=(const TrackedResource &) = delete;
        TrackedResource(TrackedResource &&) = delete;
        TrackedResource &operator=(TrackedResource &&) = delete;
    };

    // Resource managing type with tracking.
    struct TrackedType
    {
        int id;
        std::unique_ptr<TrackedResource> resource;

        // Default constructor
        TrackedType() : id(0), resource(std::make_unique<TrackedResource>(0))
        {
        }

        // Value constructor
        explicit TrackedType(int i) : id(i), resource(std::make_unique<TrackedResource>(i * 10))
        {
        }

        // Copy constructor
        TrackedType(const TrackedType &other)
            : id(other.id),
              resource(other.resource ? std::make_unique<TrackedResource>(other.resource->value) : nullptr)
        {
        }

        // Move constructor
        TrackedType(TrackedType &&other) noexcept
            : id(other.id),
              resource(std::move(other.resource))
        {
            other.id = -1; // Mark as moved-from
        }

        // Copy assignment
        TrackedType &operator=(const TrackedType &other)
        {
            if (this != &other)
            {
                id = other.id;
                resource = other.resource ? std::make_unique<TrackedResource>(other.resource->value) : nullptr;
            }
            return *this;
        }

        // Move assignment
        TrackedType &operator=(TrackedType &&other) noexcept
        {
            if (this != &other)
            {
                id = other.id;
                resource = std::move(other.resource);
                other.id = -1;
            }
            return *this;
        }

        // Destructor
        ~TrackedType() = default;

        // Comparison operators for use as key
        bool operator<(const TrackedType &other) const
        {
            return id < other.id;
        }

        bool operator==(const TrackedType &other) const
        {
            return id == other.id;
        }

        bool operator!=(const TrackedType &other) const
        {
            return !(*this == other);
        }

        bool operator>(const TrackedType &other) const
        {
            return other < *this;
        }

        bool operator<=(const TrackedType &other) const
        {
            return !(other < *this);
        }

        bool operator>=(const TrackedType &other) const
        {
            return !(*this < other);
        }

        // Helper to get resource value
        int get_resource_value() const
        {
            return resource ? resource->value : -999;
        }

        // For debugging output
        friend std::ostream &operator<<(std::ostream &os, const TrackedType &tt)
        {
            return os << "TrackedType(id=" << tt.id << ", resource=" << tt.get_resource_value() << ")";
        }
    };

} // namespace mcds::tests
