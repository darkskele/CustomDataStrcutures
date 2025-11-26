#pragma once

#include <map>

namespace mcds::bench::baselines
{

    // Wrapper to match FlatMap and BTree interface
    template <typename Key, typename Value, size_t CAPACITY = 1024>
    class StdMapWrapper
    {
    public:
        Value *find(const Key &key) noexcept
        {
            auto it = map_.find(key);
            if (it != map_.end())
            {
                return &it->second;
            }
            return nullptr;
        }

        bool insert(const Key &key, const Value &value)
        {
            map_[key] = value;
            return true;
        }

        bool erase(const Key &key) noexcept
        {
            return map_.erase(key) > 0;
        }

        std::pair<const Key *, Value *> find_min() noexcept
        {
            if (map_.empty())
                return {nullptr, nullptr};
            auto it = map_.begin();
            return {&it->first, &it->second};
        }

        std::pair<const Key *, Value *> find_max() noexcept
        {
            if (map_.empty())
                return {nullptr, nullptr};
            auto it = map_.rbegin();
            return {&it->first, &it->second};
        }

        const Key *lower_bound(const Key &key) const noexcept
        {
            auto it = map_.lower_bound(key);
            return (it != map_.end()) ? &it->first : nullptr;
        }

        const Key *upper_bound(const Key &key) const noexcept
        {
            auto it = map_.upper_bound(key);
            return (it != map_.end()) ? &it->first : nullptr;
        }

        Value &operator[](const Key &key)
        {
            return map_[key];
        }

        size_t size() const noexcept
        {
            return map_.size();
        }

        bool empty() const noexcept
        {
            return map_.empty();
        }

        bool full() const noexcept
        {
            return map_.size() >= CAPACITY;
        }

    private:
        std::map<Key, Value> map_;
    };

} // namespace mcds::bench::baselines
