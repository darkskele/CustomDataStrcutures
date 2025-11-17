#pragma once

#include <cstddef>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace mcds
{

    /**
     * @brief Stack stored B+Tree.
     *
     * @tparam Key       Key type (must support comparison via < and ==).
     * @tparam Value     Mapped value type.
     * @tparam Order     Maximum number of children per node.
     * @tparam MAX_NODES Maximum number of nodes in the tree.
     * @tparam STACK_NODE_BUDGET Maximum number of bytes to store on the stack, otherwise puts on heap.
     */
    template <typename Key, typename Value, size_t Order = 16, size_t MAX_NODES = 15'000, size_t STACK_NODE_BUDGET = 1 * 1024 * 1024>
    class BTree
    {
        // Static assertions
        static_assert(Order >= 3, "Order must be at least 3");
        static_assert((Order - 1) / 2 >= 1, "MIN_KEYS must be at least 1");
        static_assert(2 * ((Order - 1) / 2) <= (Order - 1), "Nodes cannot be merged - Order too small");
        static_assert(MAX_NODES > 0, "MAX_NODES must be positive");
        static_assert(MAX_NODES <= 0xFFFFFFFF, "MAX_NODES exceeds uint32_t range");

    private:
        // Constants
        /// @brief Minimum number of keys allowed in non root nodes.
        static constexpr size_t MIN_KEYS = (Order - 1) / 2;
        /// @brief Maximum number of keys per node.
        static constexpr size_t MAX_KEYS = Order - 1;
        /// @brief Maximum number of children per node.
        static constexpr size_t MAX_CHILDREN = Order;
        /// @brief Sentinel index used for invalid child pointers.
        static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;

        /**
         * @brief B tree node stored in an array.
         */
        struct alignas(64) Node
        {
            std::array<Key, MAX_KEYS> keys_;              ///< Stored keys.
            std::array<Value, MAX_KEYS> values_;          ///< Stored values.
            std::array<uint32_t, MAX_CHILDREN> children_; ///< Child node indices.
            uint16_t num_keys_;                           ///< Number of valid keys.
            bool is_leaf_;                                ///< True if this node is a leaf.

            /// @brief Construct an empty leaf node.
            Node() : num_keys_(0), is_leaf_(true)
            {
                children_.fill(INVALID_INDEX);
            }
        };

        /// @brief Node size.
        static constexpr size_t NODE_BYTES = sizeof(Node);
        /// @brief Total node bytes.
        static constexpr size_t TOTAL_NODE_BYTES = NODE_BYTES * MAX_NODES;
        /// @brief Conditional value for stack vs. heap storage.
        static constexpr bool USE_STACK_NODES = (TOTAL_NODE_BYTES <= STACK_NODE_BUDGET);

        /// @brief Expression to switch between heap and stack storage for large types.
        using NodeStorage = std::conditional_t<
            USE_STACK_NODES,
            std::array<Node, MAX_NODES>,
            std::vector<Node>>;

        alignas(64) NodeStorage nodes_;                         ///< Node storage pool.
        alignas(64) std::array<uint32_t, MAX_NODES> free_list_; ///< Free node indices.
        uint32_t root_index_;                                   ///< Index of root node.
        uint32_t next_free_node_;                               ///< Next unused node index.
        uint32_t free_list_size_;                               ///< Free list size.

    public:

        /// @brief Public key and value types for testing and info.
        using key_type = Key;
        using value_type = Value;

        /// @brief Construct an empty B tree with a single root node.
        BTree() : root_index_(0), next_free_node_(1), free_list_size_(0)
        {
            if constexpr (!USE_STACK_NODES)
            {
                nodes_.resize(MAX_NODES);
            }
            nodes_[0] = Node();
        }

        /// @brief No extra cleanup needed – all members are RAII types.
        ~BTree() = default;

        /// @brief Copying a giant fixed pool tree is not ideal.
        BTree(const BTree &) = delete;
        BTree &operator=(const BTree &) = delete;

        /// @brief Allow moves.
        BTree(BTree &&) noexcept = default;
        BTree &operator=(BTree &&) noexcept = default;

        /**
         * @brief Find value associated with a given key.
         *
         * @param key Key to search for.
         * @return Pointer to value if found, otherwise nullptr.
         */
        Value *find(const Key &key) noexcept
        {
            // Start search from root
            uint32_t node_idx = root_index_;

            while (true)
            {
                Node &node = nodes_[node_idx];

                // Binary search in node
                int32_t left = 0;
                int32_t right = static_cast<int32_t>(node.num_keys_) - 1;

                while (left <= right)
                {
                    const int32_t mid = left + ((right - left) / 2);
                    const Key &mid_key = node.keys_[mid];

                    if (mid_key == key)
                    {
                        return &node.values_[mid];
                    }
                    else if (mid_key < key)
                    {
                        left = mid + 1;
                    }
                    else
                    {
                        right = mid - 1;
                    }
                }

                // If leaf, not found
                if (node.is_leaf_)
                {
                    return nullptr;
                }

                // Otherwise descend to child at index `left`
                node_idx = node.children_[left];
            }
        }

        /**
         * @brief Insert a key/value pair into the tree.
         *
         * If the key already exists, its value is overwritten.
         *
         * @param key Key to insert or update.
         * @param value Value to associate with the key.
         * @return true if insertion/update succeeded.
         * @throws std::runtime_error if the node pool is exhausted.
         */
        bool insert(const Key &key, const Value &value)
        {
            return insert_internal(key, &value) != nullptr;
        }

        /**
         * @brief Erase a node from the tree.
         *
         * @param key Key to remove.
         * @return true if the key existed and was removed, false otherwise.
         */
        bool erase(const Key &key)
        {
            bool found = erase_from_node(root_index_, key);

            // If root is now empty and has a child, make that child the new root
            if (nodes_[root_index_].num_keys_ == 0)
            {
                if (!nodes_[root_index_].is_leaf_)
                {
                    uint32_t old_root = root_index_;
                    root_index_ = nodes_[root_index_].children_[0];
                    free_node(old_root); // Return old root to free list
                }
            }

            return found;
        }

        /**
         * @brief Find the minimum value in the tree.
         *
         * @return Pointer to minimum value, or nullptr if the tree is empty.
         */
        std::pair<const Key *, Value *> find_min() noexcept
        {
            // Find minimum key
            if (nodes_[root_index_].num_keys_ == 0)
            {
                return {nullptr, nullptr};
            }

            return find_min_in_subtree(root_index_);
        }

        /**
         * @brief Find the maximum value in the tree.
         *
         * @return Pointer to maximum value, or nullptr if the tree is empty.
         */
        std::pair<const Key *, Value *> find_max() noexcept
        {
            // Find maximum key
            if (nodes_[root_index_].num_keys_ == 0)
            {
                return {nullptr, nullptr};
            }

            return find_max_in_subtree(root_index_);
        }

        /**
         * @brief Find first value whose key is not less than @p search_key.
         *
         * @param search_key Lower bound key.
         * @return Pointer to the key >= search_key, or nullptr if none.
         */
        const Key *lower_bound(const Key &search_key)
        {
            // Find first key >= search_key
            return lower_bound_in_subtree(root_index_, search_key);
        }

        /**
         * @brief Find first value whose key is greater than @p search_key.
         *
         * @param search_key Upper bound key.
         * @return Pointer to the key > search_key, or nullptr if none.
         */
        const Key *upper_bound(const Key &search_key)
        {
            // Find first key > search_key
            return upper_bound_in_subtree(root_index_, search_key);
        }

        /**
         * @brief Count total number of keys stored in the tree.
         *
         * @return Number of keys currently stored.
         */
        size_t size() const noexcept
        {
            return count_keys(root_index_);
        }

        /**
         * @brief Check whether the tree is empty.
         *
         * @return true if the tree has no keys, false otherwise.
         */
        bool empty() const noexcept
        {
            return nodes_[root_index_].num_keys_ == 0;
        }

        /**
         * @brief Access or insert value associated with @p key.
         *
         * If the key does not exist, it is inserted with a default constructed
         * Value and a reference to that value is returned.
         *
         * @param key Key to look up.
         * @return Reference to the associated value.
         */
        Value &operator[](const Key &key)
        {
            if (Value *v = find(key))
            {
                return *v;
            }

            return *insert_internal(key, nullptr); // default insert, one walk
        }

    private:
        /**
         * @brief Allocate a new node from the free list or node pool.
         *
         * @return Index of the allocated node.
         * @throws std::runtime_error if MAX_NODES is exceeded.
         */
        uint32_t allocate_node()
        {
            uint32_t idx;

            // Try free list first
            if (free_list_size_ > 0)
            {
                idx = free_list_[--free_list_size_];
                nodes_[idx] = Node(); // Reinitialize
                return idx;
            }

            // Fall back to allocating new node
            if (next_free_node_ >= MAX_NODES)
            {
                throw std::runtime_error("Ran out of memory in BTREE!");
            }

            idx = next_free_node_++;
            nodes_[idx] = Node();
            return idx;
        }

        /**
         * @brief Return a node index to the free list.
         *
         * @param node_idx Index of node to release.
         * @throws std::runtime_error if the free list overflows.
         */
        void free_node(uint32_t node_idx)
        {
            if (free_list_size_ >= MAX_NODES)
            {
                throw std::runtime_error("Free list overflow - this should never happen!");
            }
            // Add new node to free list
            free_list_[free_list_size_++] = node_idx;
        }

        /**
         * @brief Internal insert helper, wrapped by public functions.
         *
         * Splits first if root is full before proceeding to insert.
         *
         * @param key Key to insert into.
         * @param value Value to insert.
         * @return Pointer to inserted value.
         */
        Value *insert_internal(const Key &key, const Value *value_if_new)
        {
            // Ensure root not full
            if (nodes_[root_index_].num_keys_ == MAX_KEYS)
            {
                uint32_t new_root_idx = allocate_node();
                Node &new_root = nodes_[new_root_idx];
                new_root.is_leaf_ = false;
                new_root.children_[0] = root_index_;
                split_child(new_root_idx, 0);
                root_index_ = new_root_idx;
            }

            return insert_non_full(root_index_, key, value_if_new);
        }

        /**
         * @brief Insert key/value into a node that is guaranteed not full.
         *
         * @param node_idx Index of node to insert into.
         * @param key Key to insert.
         * @param value Value to insert.
         * @return Pointer to new value on success.
         */
        Value *insert_non_full(uint32_t node_idx, const Key &key, const Value *value_if_new)
        {
            // Iterative search through tree, preferred over recursion
            while (true)
            {
                Node &node = nodes_[node_idx];
                int32_t i = static_cast<int32_t>(node.num_keys_) - 1;

                if (node.is_leaf_)
                {
                    // Find insertion position and shift keys/values
                    while (i >= 0 && key < node.keys_[i])
                    {
                        node.keys_[i + 1] = node.keys_[i];
                        node.values_[i + 1] = node.values_[i];
                        --i;
                    }

                    // Check for duplicates
                    if (i >= 0 && node.keys_[i] == key)
                    {
                        // Only overwrite on insert(), not on operator[]
                        if (value_if_new)
                        {
                            node.values_[i] = *value_if_new;
                        }
                        return &node.values_[i];
                    }

                    const int32_t insert_pos = i + 1;
                    node.keys_[insert_pos] = key;
                    if (value_if_new)
                        node.values_[insert_pos] = *value_if_new;
                    else
                        node.values_[insert_pos] = Value{};

                    ++node.num_keys_;
                    return &node.values_[insert_pos];
                }

                // Internal node - find child
                while (i >= 0 && key < node.keys_[i])
                {
                    --i;
                }
                ++i;

                uint32_t child_idx = node.children_[i];

                // Split child if full
                if (nodes_[child_idx].num_keys_ == MAX_KEYS)
                {
                    split_child(node_idx, i);

                    if (key > nodes_[node_idx].keys_[i])
                    {
                        ++i;
                    }
                    child_idx = nodes_[node_idx].children_[i];
                }

                node_idx = child_idx;
            }
        }

        /**
         * @brief Split a full child of a given parent node.
         *
         * @param parent_idx Index of parent node.
         * @param child_position Child slot in parent to split.
         */
        void split_child(const uint32_t parent_idx, const uint32_t child_position)
        {
            Node &parent = nodes_[parent_idx];
            uint32_t full_child_idx = parent.children_[child_position];
            Node &full_child = nodes_[full_child_idx];

            // Allocate new node for right half
            uint32_t new_child_idx = allocate_node();
            Node &new_child = nodes_[new_child_idx];
            new_child.is_leaf_ = full_child.is_leaf_;

            // Middle key goes up to new parent
            constexpr size_t mid = MAX_KEYS / 2;

            // Copy right half to new node
            new_child.num_keys_ = MAX_KEYS - mid - 1;
            for (size_t j = 0; j < new_child.num_keys_; ++j)
            {
                new_child.keys_[j] = full_child.keys_[mid + 1 + j];
                new_child.values_[j] = full_child.values_[mid + 1 + j];
            }

            // Copy children pointers if not leaf
            if (!full_child.is_leaf_)
            {
                for (size_t j = 0; j <= new_child.num_keys_; ++j)
                {
                    new_child.children_[j] = full_child.children_[mid + 1 + j];
                }
            }

            full_child.num_keys_ = mid;

            // Shift parents children to make room
            for (int32_t j = static_cast<int32_t>(parent.num_keys_); j > static_cast<int32_t>(child_position); j--)
            {
                parent.children_[j + 1] = parent.children_[j];
            }
            parent.children_[child_position + 1] = new_child_idx;

            // Shift parents keys and insert middle key
            for (int32_t j = static_cast<int32_t>(parent.num_keys_) - 1; j >= static_cast<int32_t>(child_position); j--)
            {
                parent.keys_[j + 1] = parent.keys_[j];
                parent.values_[j + 1] = parent.values_[j];
            }
            parent.keys_[child_position] = full_child.keys_[mid];
            parent.values_[child_position] = full_child.values_[mid];
            ++parent.num_keys_;
        }

        /**
         * @brief Erase a key starting from a given node.
         *
         * @param node_idx Index of node to erase from.
         * @param key Key to remove.
         * @return true if the key existed and was erased.
         */
        bool erase_from_node(uint32_t node_idx, const Key &key)
        {
            while (true)
            {
                Node &node = nodes_[node_idx];

                // Binary search for key in this node
                int32_t idx = find_key_index(node, key);

                // Key found in this node
                if (idx < static_cast<int32_t>(node.num_keys_) && node.keys_[idx] == key)
                {
                    if (node.is_leaf_)
                    {
                        return erase_from_leaf(node_idx, idx);
                    }
                    else
                    {
                        return erase_from_internal(node_idx, idx);
                    }
                }

                // Key not in this node
                if (node.is_leaf_)
                {
                    return false; // Key doesn't exist in tree
                }

                // Key must be in subtree
                // idx now points to the child that should contain the key
                bool is_in_last_child = (idx == static_cast<int32_t>(node.num_keys_));

                // If child has minimum keys, we need to ensure it won't underflow
                if (nodes_[node.children_[idx]].num_keys_ == MIN_KEYS)
                {
                    fill_child(node_idx, idx);

                    // Reload node reference after fill_child
                    Node &node_after_fill = nodes_[node_idx];

                    // Re search for the correct child index since keys may have moved
                    idx = find_key_index(node_after_fill, key);

                    // After fill, the key might have moved to this node
                    if (idx < static_cast<int32_t>(node_after_fill.num_keys_) && node_after_fill.keys_[idx] == key)
                    {
                        break;
                    }

                    // Determine which child to recurse into after fill
                    if (is_in_last_child && idx > static_cast<int32_t>(node_after_fill.num_keys_))
                    {
                        idx = static_cast<int32_t>(node_after_fill.num_keys_);
                    }
                }

                // Access children from nodes_ array directly, not stale reference
                node_idx = nodes_[node_idx].children_[idx];
            }

            return false; // shouldn't happen
        }

        /**
         * @brief Find index where key is or should be within a node.
         *
         * @param node Node to search.
         * @param key Key to locate.
         * @return Index of first key >= key.
         */
        int32_t find_key_index(const Node &node, const Key &key) const noexcept
        {
            // Find index where key is or should be
            int32_t left = 0;
            int32_t right = static_cast<int32_t>(node.num_keys_);

            while (left < right)
            {
                int32_t mid = left + ((right - left) / 2);

                if (node.keys_[mid] < key)
                {
                    left = mid + 1; // key is to the right of mid
                }
                else
                {
                    right = mid; // mid might be the first >= key, keep it in range
                }
            }

            // left == right, and is the first index where !(keys[idx] < key),
            return left;
        }

        /**
         * @brief Erase key at position @p idx from a leaf node.
         *
         * @param node_idx Index of leaf node.
         * @param idx Position of key to erase.
         * @return true on success.
         */
        bool erase_from_leaf(uint32_t node_idx, int32_t idx)
        {
            // Remove key from leaf node
            Node &node = nodes_[node_idx];

            // Shift keys and values left
            for (int32_t i = idx; i < static_cast<int32_t>(node.num_keys_) - 1; ++i)
            {
                node.keys_[i] = node.keys_[i + 1];
                node.values_[i] = node.values_[i + 1];
            }
            --node.num_keys_;

            return true;
        }

        /**
         * @brief Erase key at position @p idx from an internal node.
         *
         * @param node_idx Index of internal node.
         * @param idx Position of key to erase.
         * @return true on success.
         */
        bool erase_from_internal(uint32_t node_idx, int32_t idx)
        {
            // Remove key from internal node
            Node &node = nodes_[node_idx];
            Key key = node.keys_[idx];

            uint32_t left_child_idx = node.children_[idx];
            uint32_t right_child_idx = node.children_[idx + 1];

            // Left child has at least MIN_KEYS + 1 keys
            if (nodes_[left_child_idx].num_keys_ > MIN_KEYS)
            {
                auto [pred_key, pred_value] = get_predecessor(node_idx, idx);
                node.keys_[idx] = pred_key;
                node.values_[idx] = pred_value;
                return erase_from_node(left_child_idx, pred_key);
            }
            // Right child has at least MIN_KEYS + 1 keys
            else if (nodes_[right_child_idx].num_keys_ > MIN_KEYS)
            {
                auto [succ_key, succ_value] = get_successor(node_idx, idx);
                node.keys_[idx] = succ_key;
                node.values_[idx] = succ_value;
                return erase_from_node(right_child_idx, succ_key);
            }
            // Both children have MIN_KEYS, merge them
            else
            {
                merge_children(node_idx, idx);
                return erase_from_node(left_child_idx, key);
            }
        }

        /**
         * @brief Get predecessor key (largest in left subtree) for key at position @p idx.
         *
         * @param node_idx Index of node containing the key.
         * @param idx Position of key in node.
         * @return Predecessor key and value.
         */
        std::pair<Key, Value> get_predecessor(uint32_t node_idx, int32_t idx)
        {
            // Get predecessor (largest key in left subtree) with its value
            uint32_t cur = nodes_[node_idx].children_[idx];
            while (!nodes_[cur].is_leaf_)
            {
                cur = nodes_[cur].children_[nodes_[cur].num_keys_];
            }
            int32_t key_idx = static_cast<int32_t>(nodes_[cur].num_keys_) - 1;
            return {nodes_[cur].keys_[key_idx], nodes_[cur].values_[key_idx]};
        }

        /**
         * @brief Get successor key (smallest in right subtree) for key at position @p idx.
         *
         * @param node_idx Index of node containing the key.
         * @param idx Position of key in node.
         * @return Successor key.
         */
        std::pair<Key, Value> get_successor(uint32_t node_idx, int32_t idx)
        {
            // Get successor (smallest key in right subtree) with its value
            uint32_t cur = nodes_[node_idx].children_[idx + 1];
            while (!nodes_[cur].is_leaf_)
            {
                cur = nodes_[cur].children_[0];
            }
            return {nodes_[cur].keys_[0], nodes_[cur].values_[0]};
        }

        /**
         * @brief Ensure child at @p idx has at least MIN_KEYS + 1 keys.
         *
         * Borrows from siblings or merges if needed.
         *
         * @param node_idx Index of parent node.
         * @param idx Child position in parent.
         */
        void fill_child(uint32_t node_idx, int32_t idx)
        {
            // Ensure child at idx has at least MIN_KEYS + 1 keys
            Node &node = nodes_[node_idx];

            // Try to borrow from left sibling
            if (idx != 0 && nodes_[node.children_[idx - 1]].num_keys_ > MIN_KEYS)
            {
                borrow_from_left(node_idx, idx);
            }
            // Try to borrow from right sibling
            else if (idx != static_cast<int32_t>(node.num_keys_) && nodes_[node.children_[idx + 1]].num_keys_ > MIN_KEYS)
            {
                borrow_from_right(node_idx, idx);
            }
            // Merge with sibling - prefer merging with right if both are under full
            else
            {
                // If we have a right sibling, merge with it
                if (idx != static_cast<int32_t>(node.num_keys_))
                {
                    merge_children(node_idx, idx);
                }
                // Otherwise merge with left sibling
                else if (idx > 0)
                {
                    merge_children(node_idx, idx - 1);
                }
            }
        }

        /**
         * @brief Borrow a key from left sibling into child at @p child_idx.
         *
         * @param node_idx  Index of parent node.
         * @param child_idx Index of child in parent.
         */
        void borrow_from_left(uint32_t node_idx, int32_t child_idx)
        {
            // Borrow a key from left sibling
            Node &parent = nodes_[node_idx];
            Node &child = nodes_[parent.children_[child_idx]];
            Node &left_sibling = nodes_[parent.children_[child_idx - 1]];

            // Shift child's keys/values right to make room
            for (int32_t i = static_cast<int32_t>(child.num_keys_) - 1; i >= 0; --i)
            {
                child.keys_[i + 1] = child.keys_[i];
                child.values_[i + 1] = child.values_[i];
            }

            // Shift child's children right if not leaf
            if (!child.is_leaf_)
            {
                for (int32_t i = static_cast<int32_t>(child.num_keys_); i >= 0; --i)
                {
                    child.children_[i + 1] = child.children_[i];
                }
            }

            // Move parent's key down to child
            child.keys_[0] = parent.keys_[child_idx - 1];
            child.values_[0] = parent.values_[child_idx - 1];

            // Move left sibling's last child to child's first child
            if (!child.is_leaf_)
            {
                child.children_[0] = left_sibling.children_[left_sibling.num_keys_];
            }

            // Move left sibling's last key up to parent
            parent.keys_[child_idx - 1] = left_sibling.keys_[left_sibling.num_keys_ - 1];
            parent.values_[child_idx - 1] = left_sibling.values_[left_sibling.num_keys_ - 1];

            ++child.num_keys_;
            --left_sibling.num_keys_;
        }

        /**
         * @brief Borrow a key from right sibling into child at @p child_idx.
         *
         * @param node_idx  Index of parent node.
         * @param child_idx Index of child in parent.
         */
        void borrow_from_right(uint32_t node_idx, int32_t child_idx)
        {
            // Borrow a key from right sibling
            Node &parent = nodes_[node_idx];
            Node &child = nodes_[parent.children_[child_idx]];
            Node &right_sibling = nodes_[parent.children_[child_idx + 1]];

            // Move parent's key down to child
            child.keys_[child.num_keys_] = parent.keys_[child_idx];
            child.values_[child.num_keys_] = parent.values_[child_idx];

            // Move right sibling's first child to child's last child
            if (!child.is_leaf_)
            {
                child.children_[child.num_keys_ + 1] = right_sibling.children_[0];
            }

            // Move right sibling's first key up to parent
            parent.keys_[child_idx] = right_sibling.keys_[0];
            parent.values_[child_idx] = right_sibling.values_[0];

            // Shift right sibling's keys/values left
            for (size_t i = 0; i < right_sibling.num_keys_ - 1; ++i)
            {
                right_sibling.keys_[i] = right_sibling.keys_[i + 1];
                right_sibling.values_[i] = right_sibling.values_[i + 1];
            }

            // Shift right sibling's children left if not leaf
            if (!right_sibling.is_leaf_)
            {
                for (size_t i = 0; i < right_sibling.num_keys_; ++i)
                {
                    right_sibling.children_[i] = right_sibling.children_[i + 1];
                }
            }

            ++child.num_keys_;
            --right_sibling.num_keys_;
        }

        /**
         * @brief Merge child at @p idx with its right sibling.
         *
         * @param node_idx Index of parent node.
         * @param idx Index of left child in parent.
         */
        void merge_children(uint32_t node_idx, int32_t idx)
        {
            // Merge child with its right sibling
            Node &parent = nodes_[node_idx];
            uint32_t left_child_idx = parent.children_[idx];
            uint32_t right_child_idx = parent.children_[idx + 1];
            Node &left_child = nodes_[left_child_idx];
            Node &right_child = nodes_[right_child_idx];

            // Pull key from parent down to left child
            left_child.keys_[left_child.num_keys_] = parent.keys_[idx];
            left_child.values_[left_child.num_keys_] = parent.values_[idx];

            // Copy keys and values from right child to left child
            for (size_t i = 0; i < right_child.num_keys_; ++i)
            {
                left_child.keys_[left_child.num_keys_ + 1 + i] = right_child.keys_[i];
                left_child.values_[left_child.num_keys_ + 1 + i] = right_child.values_[i];
            }

            // Copy children pointers if not leaf
            if (!left_child.is_leaf_)
            {
                for (size_t i = 0; i <= right_child.num_keys_; ++i)
                {
                    left_child.children_[left_child.num_keys_ + 1 + i] = right_child.children_[i];
                }
            }

            left_child.num_keys_ += right_child.num_keys_ + 1;

            // Shift parent's keys and values left
            for (int32_t i = idx; i < static_cast<int32_t>(parent.num_keys_) - 1; ++i)
            {
                parent.keys_[i] = parent.keys_[i + 1];
                parent.values_[i] = parent.values_[i + 1];
            }

            // Shift parent's children left (starting from idx+1, since we're removing child at idx+1)
            for (int32_t i = idx + 1; i <= static_cast<int32_t>(parent.num_keys_); ++i)
            {
                parent.children_[i] = parent.children_[i + 1];
            }

            --parent.num_keys_;

            // Return the merged node to free list
            free_node(right_child_idx);
        }

        /**
         * @brief Find minimum value in subtree rooted at @p node_idx.
         *
         * @param node_idx Root of subtree.
         * @return Pointer to minimum value, or nullptr if subtree is empty.
         */
        std::pair<const Key *, Value *> find_min_in_subtree(uint32_t node_idx)
        {
            while (true)
            {
                // Find minimum value in subtree
                Node &node = nodes_[node_idx];

                // If leaf, first key is minimum
                if (node.is_leaf_)
                {
                    return (node.num_keys_ > 0) ? std::make_pair(&node.keys_[0], &node.values_[0]) : std::make_pair(nullptr, nullptr);
                }

                // Otherwise retry on leftmost child
                node_idx = node.children_[0];
            }
        }

        /**
         * @brief Find maximum value in subtree rooted at @p node_idx.
         *
         * @param node_idx Root of subtree.
         * @return Pointer to maximum value, or nullptr if subtree is empty.
         */
        std::pair<const Key *, Value *> find_max_in_subtree(uint32_t node_idx)
        {
            while (true)
            {
                // Find maximum value in subtree
                Node &node = nodes_[node_idx];

                // If leaf, last key is maximum
                if (node.is_leaf_)
                {
                    return (node.num_keys_ > 0) ? std::make_pair(&node.keys_[node.num_keys_ - 1], &node.values_[node.num_keys_ - 1]) : std::make_pair(nullptr, nullptr);
                }

                // Otherwise, try the rightmost child
                node_idx = node.children_[node.num_keys_];
            }
        }

        /**
         * @brief Find first value with key >= @p search_key in subtree.
         *
         * @param node_idx    Root of subtree.
         * @param search_key  Lower-bound key.
         * @return Pointer to key, or nullptr if none.
         */
        Key *lower_bound_in_subtree(uint32_t node_idx, const Key &search_key)
        {
            // Find first key >= search_key
            if (node_idx == INVALID_INDEX)
            {
                return nullptr;
            }

            Node &node = nodes_[node_idx];

            // Binary search in current node for first key >= search_key
            int32_t left = 0;
            int32_t right = static_cast<int32_t>(node.num_keys_) - 1;
            int32_t result_idx = -1;

            while (left <= right)
            {
                int32_t mid = left + (right - left) / 2;

                if (node.keys_[mid] >= search_key)
                {
                    result_idx = mid;
                    right = mid - 1; // Keep searching left for smaller valid key
                }
                else
                {
                    left = mid + 1;
                }
            }

            // If we found a key >= search_key in this node
            if (result_idx != -1)
            {
                // Check if there's a smaller valid key in left child
                if (!node.is_leaf_)
                {
                    Key *left_result = lower_bound_in_subtree(node.children_[result_idx], search_key);
                    if (left_result != nullptr)
                    {
                        return left_result;
                    }
                }

                return &node.keys_[result_idx];
            }

            // No key >= search_key in this node, check rightmost child
            if (!node.is_leaf_)
            {
                return lower_bound_in_subtree(node.children_[node.num_keys_], search_key);
            }

            return nullptr;
        }

        /**
         * @brief Find first value with key > @p search_key in subtree.
         *
         * @param node_idx Root of subtree.
         * @param search_key Upper-bound key.
         * @return Pointer to the upper bound key.
         */
        Key *upper_bound_in_subtree(uint32_t node_idx, const Key &search_key)
        {
            // Find first key > search_key
            if (node_idx == INVALID_INDEX)
            {
                return nullptr;
            }

            Node &node = nodes_[node_idx];

            // Binary search in current node for first key > search_key
            int32_t left = 0;
            int32_t right = static_cast<int32_t>(node.num_keys_) - 1;
            int32_t result_idx = -1;

            while (left <= right)
            {
                int32_t mid = left + (right - left) / 2;

                if (node.keys_[mid] > search_key)
                {
                    result_idx = mid;
                    right = mid - 1; // Keep searching left
                }
                else
                {
                    left = mid + 1;
                }
            }

            // If we found a key > search_key in this node
            if (result_idx != -1)
            {
                // Check if there's a smaller valid key in left child
                if (!node.is_leaf_)
                {
                    Key *left_result = upper_bound_in_subtree(node.children_[result_idx], search_key);
                    if (left_result != nullptr)
                    {
                        return left_result;
                    }
                }

                return &node.keys_[result_idx];
            }

            // No key > search_key in this node, check rightmost child
            if (!node.is_leaf_)
            {
                return upper_bound_in_subtree(node.children_[node.num_keys_], search_key);
            }

            return nullptr;
        }

        /**
         * @brief Count total number of keys in subtree.
         *
         * @param node_idx Root of subtree.
         * @return Number of keys in subtree.
         */
        size_t count_keys(uint32_t node_idx) const
        {
            // Count total keys in tree
            if (node_idx == INVALID_INDEX)
                return 0;

            const Node &node = nodes_[node_idx];
            size_t count = node.num_keys_;

            if (!node.is_leaf_)
            {
                for (size_t i = 0; i <= node.num_keys_; ++i)
                {
                    count += count_keys(node.children_[i]);
                }
            }

            return count;
        }
    };

} // namespace mcds
