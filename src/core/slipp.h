#ifndef __SLIPP_H__
#define __SLIPP_H__

#include "linearmodel.h"
#include <stdint.h>
#include <math.h>
#include <limits>
#include <cstdio>
#include <stack>
#include <queue>
#include <map>
#include <vector>
#include <cstring>
#include <string>
#include <sstream>

typedef uint8_t bitmap_t;
#define BITMAP_WIDTH (sizeof(bitmap_t) * 8)
#define BITMAP_SIZE(num_items) (((num_items) + BITMAP_WIDTH - 1) / BITMAP_WIDTH)
#define BITMAP_GET(bitmap, pos) (((bitmap)[(pos) / BITMAP_WIDTH] >> ((pos) % BITMAP_WIDTH)) & 1)
#define BITMAP_SET(bitmap, pos) ((bitmap)[(pos) / BITMAP_WIDTH] |= 1 << ((pos) % BITMAP_WIDTH))
#define BITMAP_CLEAR(bitmap, pos) ((bitmap)[(pos) / BITMAP_WIDTH] &= ~bitmap_t(1 << ((pos) % BITMAP_WIDTH)))
#define BITMAP_NEXT_1(bitmap_item) __builtin_ctz((bitmap_item))

#define MAX_KEY 9187201950435737471 // 0x7F7F7F7F7F7F7F7F
#define MIN_KEY 2314885530818453536 // 0x2020202020202020 (0x20:(space))

// runtime assert
#define RT_ASSERT(expr)                                                                     \
    {                                                                                       \
        if (!(expr))                                                                        \
        {                                                                                   \
            fprintf(stderr, "RT_ASSERT Error at %s:%d, `%s`\n", __FILE__, __LINE__, #expr); \
            exit(0);                                                                        \
        }                                                                                   \
    }

#define COLLECT_TIME 0

#if COLLECT_TIME
#include <chrono>
#endif

template <class T, class P, bool USE_FMCD = true>
class SLIPP
{
    inline int compute_gap_count(int size)
    {
        if (size >= 1000000)
            return 1;
        if (size >= 100000)
            return 2;
        return 5;
    }

    struct Node;
    inline int PREDICT_POS(Node *node, T key) const
    {
        double v = node->model.predict_double(key);
        if (v > std::numeric_limits<int>::max() / 2)
        {
            return node->num_items - 1;
        }
        if (v < 0)
        {
            return 0;
        }
        return std::min(node->num_items - 1, static_cast<int>(v));
    }

    static void remove_last_bit(bitmap_t &bitmap_item)
    {
        bitmap_item -= 1 << BITMAP_NEXT_1(bitmap_item);
    }

    const double BUILD_LR_REMAIN;
    const bool QUIET;

    struct
    {
        long long fmcd_success_times = 0;
        long long fmcd_broken_times = 0;
#if COLLECT_TIME
        double time_scan_and_destory_tree = 0;
        double time_build_tree_bulk = 0;
#endif
    } stats;

public:
    typedef std::pair<T, P> V;

    SLIPP(double BUILD_LR_REMAIN = 0, bool QUIET = true)
        : BUILD_LR_REMAIN(BUILD_LR_REMAIN), QUIET(QUIET)
    {
        {
            std::vector<Node *> nodes;
            for (int _ = 0; _ < 1e6; _++)
            {
                Node *node = build_tree_two(T(0), P(), T(1), P());
                nodes.push_back(node);
            }
            for (auto node : nodes)
            {
                destroy_tree(node);
            }
            if (!QUIET)
            {
                printf("initial memory pool size = %lu\n", pending_two.size());
            }
        }
        if (USE_FMCD && !QUIET)
        {
            printf("enable FMCD\n");
        }

        root = build_tree_none();
    }
    ~SLIPP()
    {
        destroy_tree(root);
        root = NULL;
        destory_pending();
    }

    void insert(const V &v)
    {
        insert(v.first, v.second);
    }
    void insert(const T &key, const P &value)
    {
        root = insert_tree(root, key, value);
    }
    P at(const T &key, bool skip_existence_check = true) const
    {
        Node *node = root;

        while (true)
        {
            int pos = PREDICT_POS(node, key);
            if (BITMAP_GET(node->child_bitmap, pos) == 1)
            {
                // slipp
                node = node->items[pos].comp.child;
            }
            else
            {
                if (skip_existence_check)
                {
                    return node->items[pos].comp.data.value;
                }
                else
                {
                    if (BITMAP_GET(node->none_bitmap, pos) == 1)
                    {
                        RT_ASSERT(false);
                    }
                    else if (BITMAP_GET(node->child_bitmap, pos) == 0)
                    {
                        RT_ASSERT(node->items[pos].comp.data.key == key);
                        return node->items[pos].comp.data.value;
                    }
                }
            }
        }
    }
    bool exists(const T &key) const
    {
        Node *node = root;
        while (true)
        {
            int pos = PREDICT_POS(node, key);
            if (BITMAP_GET(node->none_bitmap, pos) == 1)
            {
                return false;
            }
            else if (BITMAP_GET(node->child_bitmap, pos) == 0)
            {
                return node->items[pos].comp.data.key == key;
            }
            else
            {
                node = node->items[pos].comp.child;
            }
        }
    }
    void bulk_load(const V *vs, int num_keys)
    {
        if (num_keys == 0)
        {
            destroy_tree(root);
            root = build_tree_none();
            return;
        }
        if (num_keys == 1)
        {
            destroy_tree(root);
            root = build_tree_none();
            insert(vs[0]);
            return;
        }
        if (num_keys == 2)
        {
            destroy_tree(root);
            root = build_tree_two(vs[0].first, vs[0].second, vs[1].first, vs[1].second);
            return;
        }

        RT_ASSERT(num_keys > 2);
        for (int i = 1; i < num_keys; i++)
        {
            RT_ASSERT(vs[i].first > vs[i - 1].first);
        }

        T *keys = new T[num_keys];
        P *values = new P[num_keys];
        for (int i = 0; i < num_keys; i++)
        {
            keys[i] = vs[i].first;
            values[i] = vs[i].second;
        }
        destroy_tree(root);
        root = build_tree_bulk(keys, values, num_keys);
        delete[] keys;
        delete[] values;
    }

    void show() const
    {
        printf("============= SHOW SLIPP ================\n");

        std::stack<Node *> s;
        s.push(root);
        while (!s.empty())
        {
            Node *node = s.top();
            s.pop();

            printf("Node(%p, a = %g, b = %Lf, num_items = %d, layer = %d)", node, node->model.a, node->model.b, node->num_items, node->layer);
            printf("[");
            int first = 1;
            for (int i = 0; i < node->num_items; i++)
            {
                if (!first)
                {
                    printf(", ");
                }
                first = 0;
                if (BITMAP_GET(node->none_bitmap, i) == 1)
                {
                    printf("None");
                }
                else if (BITMAP_GET(node->child_bitmap, i) == 0)
                {
                    printf("Key(%s)", node->items[i].comp.data.key.to_string().c_str());
                }
                else
                {
                    printf("Child(%p)", node->items[i].comp.child);
                    s.push(node->items[i].comp.child);
                }
            }
            printf("]\n");
        }
    }
    void print_depth() const
    {
        std::stack<Node *> s;
        std::stack<int> d;
        s.push(root);
        d.push(1);

        int max_depth = 1;
        int sum_depth = 0, sum_nodes = 0;
        while (!s.empty())
        {
            Node *node = s.top();
            s.pop();
            int depth = d.top();
            d.pop();
            for (int i = 0; i < node->num_items; i++)
            {
                if (BITMAP_GET(node->child_bitmap, i) == 1)
                {
                    s.push(node->items[i].comp.child);
                    d.push(depth + 1);
                }
                else if (BITMAP_GET(node->none_bitmap, i) != 1)
                {
                    max_depth = std::max(max_depth, depth);
                    sum_depth += depth;
                    sum_nodes++;
                }
            }
        }

        printf("max_depth = %d, avg_depth = %.2lf\n", max_depth, double(sum_depth) / double(sum_nodes));
    }
    void verify() const
    {
        std::stack<Node *> s;
        s.push(root);

        while (!s.empty())
        {
            Node *node = s.top();
            s.pop();
            int sum_size = 0;
            for (int i = 0; i < node->num_items; i++)
            {
                if (BITMAP_GET(node->child_bitmap, i) == 1)
                {
                    s.push(node->items[i].comp.child);
                    sum_size += node->items[i].comp.child->size;
                }
                else if (BITMAP_GET(node->none_bitmap, i) != 1)
                {
                    sum_size++;
                }
            }
            RT_ASSERT(sum_size == node->size);
        }
    }
    void print_stats() const
    {
        printf("======== Stats ===========\n");
        if (USE_FMCD)
        {
            printf("\t fmcd_success_times = %lld\n", stats.fmcd_success_times);
            printf("\t fmcd_broken_times = %lld\n", stats.fmcd_broken_times);
        }
#if COLLECT_TIME
        printf("\t time_scan_and_destory_tree = %lf\n", stats.time_scan_and_destory_tree);
        printf("\t time_build_tree_bulk = %lf\n", stats.time_build_tree_bulk);
#endif
    }
    size_t index_size(bool total = false, bool ignore_child = true) const
    {
        std::stack<Node *> s;
        s.push(root);

        size_t size = 0;
        while (!s.empty())
        {
            Node *node = s.top();
            s.pop();
            bool has_child = false;
            if (ignore_child == false)
            {
                size += sizeof(*node);
            }
            for (int i = 0; i < node->num_items; i++)
            {
                if (ignore_child == true)
                {
                    size += sizeof(Item);
                    has_child = true;
                }
                else
                {
                    if (total)
                        size += sizeof(Item);
                }
                if (BITMAP_GET(node->child_bitmap, i) == 1)
                {
                    if (!total)
                        size += sizeof(Item);
                    s.push(node->items[i].comp.child);
                }
            }
            if (ignore_child == true && has_child)
            {
                size += sizeof(*node);
            }
        }
        return size;
    }

private:
    struct Node;
    struct Item
    {
        struct
        {
            struct
            {
                T key;
                P value;
            } data;
            Node *child;
        } comp;
    };
    struct Node
    {
        // int is_one;
        int is_two;     // is special node for only two keys
        int build_size; // tree size (include sub nodes) when node created
        int size;       // current tree size (include sub nodes)
        int fixed;      // fixed node will not trigger rebuild
        int num_inserts, num_insert_to_data;
        int num_items; // size of items
        uint8_t layer; // the layer of key that the node hold (slipp)
        LinearModel<T> model;
        Item *items;
        bitmap_t *none_bitmap;  // 1 means None, 0 means Data or Child
        bitmap_t *child_bitmap; // 1 means Child. will always be 0 when none_bitmap is 1
        bitmap_t *cp_bitmap;    // 1 means child_cp.
    };

    Node *root;
    std::stack<Node *> pending_two;

    std::allocator<Node> node_allocator;
    Node *new_nodes(int n)
    {
        Node *p = node_allocator.allocate(n);
        RT_ASSERT(p != NULL && p != (Node *)(-1));
        return p;
    }
    void delete_nodes(Node *p, int n)
    {
        node_allocator.deallocate(p, n);
    }

    std::allocator<Item> item_allocator;
    Item *new_items(int n)
    {
        Item *p = item_allocator.allocate(n);
        RT_ASSERT(p != NULL && p != (Item *)(-1));
        return p;
    }
    void delete_items(Item *p, int n)
    {
        item_allocator.deallocate(p, n);
    }

    std::allocator<bitmap_t> bitmap_allocator;
    bitmap_t *new_bitmap(int n)
    {
        bitmap_t *p = bitmap_allocator.allocate(n);
        RT_ASSERT(p != NULL && p != (bitmap_t *)(-1));
        return p;
    }
    void delete_bitmap(bitmap_t *p, int n)
    {
        bitmap_allocator.deallocate(p, n);
    }

    /// build an empty tree
    Node *build_tree_none()
    {
        Node *node = new_nodes(1);
        node->is_two = 0;
        node->build_size = 0;
        node->size = 0;
        node->fixed = 0;
        node->num_inserts = node->num_insert_to_data = 0;
        node->num_items = 1;
        node->layer = 0; //(slipp)
        node->model.a = node->model.b = 0;
        node->model.layer = 0; //(slipp)
        node->items = new_items(1);
        node->none_bitmap = new_bitmap(1);
        node->none_bitmap[0] = 0;
        BITMAP_SET(node->none_bitmap, 0);
        node->child_bitmap = new_bitmap(1);
        node->child_bitmap[0] = 0;
        node->cp_bitmap = new_bitmap(1);
        node->cp_bitmap[0] = 0;

        return node;
    }

    /// build a tree with two keys
    Node *build_tree_two(T key1, P value1, T key2, P value2, uint8_t layer = 0)
    {
        // if (key1 > key2)
        // {
        //     std::swap(key1, key2);
        //     std::swap(value1, value2);
        // }
        // RT_ASSERT(key1 < key2);
        static_assert(BITMAP_WIDTH == 8);

        Node *node = NULL;
        if (pending_two.empty())
        {
            node = new_nodes(1);
            node->is_two = 1;
            node->build_size = 2;
            node->size = 2;
            node->fixed = 0;
            node->num_inserts = node->num_insert_to_data = 0;
            node->num_items = 8;
            node->layer = layer; //(slipp)
            node->items = new_items(node->num_items);
            node->none_bitmap = new_bitmap(1);
            node->child_bitmap = new_bitmap(1);
            node->cp_bitmap = new_bitmap(1);
            node->none_bitmap[0] = 0xff;
            node->child_bitmap[0] = 0;
            node->cp_bitmap[0] = 0;
        }
        else
        {
            node = pending_two.top();
            pending_two.pop();
            node->layer = layer;
        }
        node->model.layer = layer;
        long double mid1_key;
        long double mid2_key;
        if (key1.to_model_key(layer) > key2.to_model_key(layer))
        {
            mid1_key = key2.to_model_key(layer);
            mid2_key = key1.to_model_key(layer);
        }
        else
        {
            mid1_key = key1.to_model_key(layer);
            mid2_key = key2.to_model_key(layer);
        }

        const double mid1_target = node->num_items / 3;
        const double mid2_target = node->num_items * 2 / 3;

        node->model.a = (mid2_target - mid1_target) / (mid2_key - mid1_key);
        node->model.b = mid1_target - node->model.a * mid1_key;
        RT_ASSERT(isfinite(node->model.a));
        RT_ASSERT(isfinite(node->model.b));

        { // insert key1&value1
            int pos = PREDICT_POS(node, key1);
            RT_ASSERT(BITMAP_GET(node->none_bitmap, pos) == 1);
            BITMAP_CLEAR(node->none_bitmap, pos);
            node->items[pos].comp.data.key = key1;
            node->items[pos].comp.data.value = value1;
        }
        { // insert key2&value2
            int pos = PREDICT_POS(node, key2);
            RT_ASSERT(BITMAP_GET(node->none_bitmap, pos) == 1);
            BITMAP_CLEAR(node->none_bitmap, pos);
            node->items[pos].comp.data.key = key2;
            node->items[pos].comp.data.value = value2;
        }

        return node;
    }

    // build a tree with two keys in the next layer
    /*
    Node *build_tree_next_layer(T key1, P value1, T key2, P value2, uint8_t layer = 0)
    {
        static_assert(BITMAP_WIDTH == 8);
        Node *node = NULL;

        node = new_nodes(1);
        node->is_one = 1;
        node->is_two = 0;
        node->build_size = 2;
        node->size = 2;
        node->fixed = 0;
        node->num_inserts = node->num_insert_to_data = 0;
        node->num_items = 8;
        node->layer = layer; //(slipp)
        node->items = new_items(node->num_items);
        node->none_bitmap = new_bitmap(1);
        node->child_bitmap = new_bitmap(1);
        node->none_bitmap[0] = 0xff;
        node->child_bitmap[0] = 0;

        node->model.layer = layer;
        T key(key1, layer);
        node->model.a = (double)(node->num_items - 1) / (MAX_KEY - MIN_KEY);
        node->model.b = (long double)0 - (long double)(node->num_items - 1) * MIN_KEY / (MAX_KEY - MIN_KEY);
        RT_ASSERT(isfinite(node->model.a));
        RT_ASSERT(isfinite(node->model.b));

        int pos = PREDICT_POS(node, key);
        RT_ASSERT(BITMAP_GET(node->none_bitmap, pos) == 1);
        BITMAP_CLEAR(node->none_bitmap, pos);
        node->items[pos].comp.data.key = key;
        node->items[pos].comp.cp = true;
        BITMAP_SET(node->child_bitmap, pos);
        node->items[pos].comp.child = build_tree_two(key1, value1, key2, value2, layer);
        return node;
    }
    */
    Node *build_tree_cp(T key, P value, T key_cp, uint8_t layer, Node *subtree)
    {
        static_assert(BITMAP_WIDTH == 8);
        Node *node = NULL;
        if (pending_two.empty())
        {
            node = new_nodes(1);
            node->is_two = 1;
            node->build_size = 2;
            node->size = 1 + subtree->size;
            node->fixed = 0;
            node->num_inserts = 0;
            node->num_insert_to_data = 0;
            node->num_items = 8;
            node->layer = layer;
            node->items = new_items(node->num_items);
            node->none_bitmap = new_bitmap(1);
            node->child_bitmap = new_bitmap(1);
            node->cp_bitmap = new_bitmap(1);
            node->none_bitmap[0] = 0xff;
            node->child_bitmap[0] = 0;
            node->cp_bitmap[0] = 0;
        }
        else
        {
            node = pending_two.top();
            pending_two.pop();
            node->layer = layer;
        }
        node->model.layer = layer;
        long double mid1_key;
        long double mid2_key;
        if (key.to_model_key(layer) > key_cp.to_model_key(layer))
        {
            mid1_key = key_cp.to_model_key(layer);
            mid2_key = key.to_model_key(layer);
        }
        else
        {
            mid1_key = key.to_model_key(layer);
            mid2_key = key_cp.to_model_key(layer);
        }

        const double mid1_target = node->num_items / 3;
        const double mid2_target = node->num_items * 2 / 3;

        node->model.a = (mid2_target - mid1_target) / (mid2_key - mid1_key);
        node->model.b = mid1_target - node->model.a * mid1_key;
        RT_ASSERT(isfinite(node->model.a));
        RT_ASSERT(isfinite(node->model.b));

        {
            int pos = PREDICT_POS(node, key);
            RT_ASSERT(BITMAP_GET(node->none_bitmap, pos) == 1);
            BITMAP_CLEAR(node->none_bitmap, pos);
            node->items[pos].comp.data.key = key;
            node->items[pos].comp.data.value = value;
        }
        {
            int pos = PREDICT_POS(node, key_cp);
            RT_ASSERT(BITMAP_GET(node->none_bitmap, pos) == 1);
            BITMAP_CLEAR(node->none_bitmap, pos);
            BITMAP_SET(node->child_bitmap, pos);
            BITMAP_SET(node->cp_bitmap, pos);
            node->items[pos].comp.data.key = key_cp;
            node->items[pos].comp.child = subtree;
        }

        return node;
    }

    Node *build_tree_cp2(T key_cp1, T key_cp2, uint8_t layer, Node *subtree1, Node *subtree2)
    {
        static_assert(BITMAP_WIDTH == 8);
        Node *node = NULL;
        if (pending_two.empty())
        {
            node = new_nodes(1);
            node->is_two = 1;
            node->build_size = 2;
            node->size = subtree1->size + subtree2->size;
            node->fixed = 0;
            node->num_inserts = 0;
            node->num_insert_to_data = 0;
            node->num_items = 8;
            node->layer = layer;
            node->items = new_items(node->num_items);
            node->none_bitmap = new_bitmap(1);
            node->child_bitmap = new_bitmap(1);
            node->cp_bitmap = new_bitmap(1);
            node->none_bitmap[0] = 0xff;
            node->child_bitmap[0] = 0;
            node->cp_bitmap[0] = 0;
        }
        else
        {
            node = pending_two.top();
            pending_two.pop();
            node->layer = layer;
        }
        node->model.layer = layer;
        long double mid1_key;
        long double mid2_key;
        if (key_cp1.to_model_key(layer) > key_cp2.to_model_key(layer))
        {
            mid1_key = key_cp2.to_model_key(layer);
            mid2_key = key_cp1.to_model_key(layer);
        }
        else
        {
            mid1_key = key_cp1.to_model_key(layer);
            mid2_key = key_cp2.to_model_key(layer);
        }

        const double mid1_target = node->num_items / 3;
        const double mid2_target = node->num_items * 2 / 3;

        node->model.a = (mid2_target - mid1_target) / (mid2_key - mid1_key);
        node->model.b = mid1_target - node->model.a * mid1_key;
        RT_ASSERT(isfinite(node->model.a));
        RT_ASSERT(isfinite(node->model.b));

        {
            int pos = PREDICT_POS(node, key_cp1);
            RT_ASSERT(BITMAP_GET(node->none_bitmap, pos) == 1);
            BITMAP_CLEAR(node->none_bitmap, pos);
            BITMAP_SET(node->child_bitmap, pos);
            BITMAP_SET(node->cp_bitmap, pos);
            node->items[pos].comp.data.key = key_cp1;
            node->items[pos].comp.child = subtree1;
        }
        {
            int pos = PREDICT_POS(node, key_cp2);
            RT_ASSERT(BITMAP_GET(node->none_bitmap, pos) == 1);
            BITMAP_CLEAR(node->none_bitmap, pos);
            BITMAP_SET(node->child_bitmap, pos);
            BITMAP_SET(node->cp_bitmap, pos);
            node->items[pos].comp.data.key = key_cp2;
            node->items[pos].comp.child = subtree2;
        }

        return node;
    }

    /// bulk build, _keys must be sorted in asc order.
    Node *build_tree_bulk(T *_keys, P *_values, int _size, bitmap_t *is_cp, std::map<int, Node *> &nodes, uint8_t _layer, int _treesize)
    {
        if (USE_FMCD)
        {
            return build_tree_bulk_fmcd(_keys, _values, _size, is_cp, nodes, _layer, _treesize);
        }
        else
        {
            return build_tree_bulk_fast(_keys, _values, _size, is_cp, nodes, _layer, _treesize);
        }
    }
    /// bulk build, _keys must be sorted in asc order.
    /// split keys into three parts at each node.
    Node *build_tree_bulk_fast(T *_keys, P *_values, int _size, bitmap_t *is_cp, std::map<int, Node *> &nodes, uint8_t _layer, int _treesize)
    {
        RT_ASSERT(_size > 1);

        typedef struct
        {
            int begin;
            int end;
            int level; // top level = 1
            Node *node;
            int treesize;
        } Segment;
        std::stack<Segment> s;

        Node *ret = new_nodes(1);
        s.push((Segment){0, _size, 1, ret, _treesize});
        // int node_i = 0;
        while (!s.empty())
        {
            const int begin = s.top().begin;
            const int end = s.top().end;
            const int level = s.top().level;
            const int treesize = s.top().treesize;
            Node *node = s.top().node;
            s.pop();

            RT_ASSERT(end - begin >= 2);
            if (end - begin == 2)
            {
                if (BITMAP_GET(is_cp, begin) == 0)
                {
                    if (BITMAP_GET(is_cp, begin + 1) == 0)
                    {
                        Node *_ = build_tree_two(_keys[begin], _values[begin], _keys[begin + 1], _values[begin + 1], _layer);
                        memcpy(node, _, sizeof(Node));
                        delete_nodes(_, 1);
                    }
                    else
                    {
                        RT_ASSERT(nodes.find(begin + 1) != nodes.end());
                        Node *_ = build_tree_cp(_keys[begin], _values[begin], _keys[begin + 1], _layer, nodes[begin + 1]);
                        memcpy(node, _, sizeof(Node));
                        delete_nodes(_, 1);
                    }
                }
                else
                {
                    if (BITMAP_GET(is_cp, begin + 1) == 0)
                    {
                        RT_ASSERT(nodes.find(begin) != nodes.end());
                        Node *_ = build_tree_cp(_keys[begin + 1], _values[begin + 1], _keys[begin], _layer, nodes[begin]);
                        memcpy(node, _, sizeof(Node));
                        delete_nodes(_, 1);
                    }
                    else
                    {
                        RT_ASSERT(nodes.find(begin) != nodes.end() && nodes.find(begin + 1) != nodes.end());
                        Node *_ = build_tree_cp2(_keys[begin], _keys[begin + 1], _layer, nodes[begin], nodes[begin + 1]);
                        memcpy(node, _, sizeof(Node));
                        delete_nodes(_, 1);
                    }
                }
            }
            else
            {
                T *keys = _keys + begin;
                P *values = _values + begin;
                const int size = end - begin;
                const int BUILD_GAP_CNT = compute_gap_count(size);

                node->is_two = 0;
                node->build_size = size;
                node->size = treesize;
                node->fixed = 0;
                node->num_inserts = node->num_insert_to_data = 0;
                node->layer = _layer;

                int mid1_pos = (size - 1) / 3;
                int mid2_pos = (size - 1) * 2 / 3;

                RT_ASSERT(0 <= mid1_pos);
                RT_ASSERT(mid1_pos < mid2_pos);
                RT_ASSERT(mid2_pos < size - 1);

                node->model.layer = node->layer;
                const long double mid1_key =
                    (static_cast<long double>(keys[mid1_pos].to_model_key(node->layer)) + static_cast<long double>(keys[mid1_pos + 1].to_model_key(node->layer))) / 2;
                const long double mid2_key =
                    (static_cast<long double>(keys[mid2_pos].to_model_key(node->layer)) + static_cast<long double>(keys[mid2_pos + 1].to_model_key(node->layer))) / 2;

                node->num_items = size * static_cast<int>(BUILD_GAP_CNT + 1);
                const double mid1_target = mid1_pos * static_cast<int>(BUILD_GAP_CNT + 1) + static_cast<int>(BUILD_GAP_CNT + 1) / 2;
                const double mid2_target = mid2_pos * static_cast<int>(BUILD_GAP_CNT + 1) + static_cast<int>(BUILD_GAP_CNT + 1) / 2;

                node->model.a = (mid2_target - mid1_target) / (mid2_key - mid1_key);
                node->model.b = mid1_target - node->model.a * mid1_key;
                RT_ASSERT(isfinite(node->model.a));
                RT_ASSERT(isfinite(node->model.b));

                const int lr_remains = static_cast<int>(size * BUILD_LR_REMAIN);
                node->model.b += lr_remains;
                node->num_items += lr_remains * 2;

                if (size > 1e6)
                {
                    node->fixed = 1;
                }

                node->items = new_items(node->num_items);
                const int bitmap_size = BITMAP_SIZE(node->num_items);
                node->none_bitmap = new_bitmap(bitmap_size);
                node->child_bitmap = new_bitmap(bitmap_size);
                node->cp_bitmap = new_bitmap(bitmap_size);
                memset(node->none_bitmap, 0xff, sizeof(bitmap_t) * bitmap_size);
                memset(node->child_bitmap, 0, sizeof(bitmap_t) * bitmap_size);
                memset(node->cp_bitmap, 0, sizeof(bitmap_t) * bitmap_size);

                for (int item_i = PREDICT_POS(node, keys[0]), offset = 0; offset < size;)
                {
                    int next = offset + 1, next_i = -1;
                    while (next < size)
                    {
                        next_i = PREDICT_POS(node, keys[next]);
                        if (next_i == item_i)
                        {
                            next++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    if (next == offset + 1)
                    {
                        BITMAP_CLEAR(node->none_bitmap, item_i);
                        node->items[item_i].comp.data.key = keys[offset];

                        if (BITMAP_GET(is_cp, begin + offset) == 1)
                        {
                            RT_ASSERT(nodes.find(begin + offset) != nodes.end());
                            BITMAP_SET(node->child_bitmap, item_i);
                            BITMAP_SET(node->cp_bitmap, item_i);
                            node->items[item_i].comp.child = nodes[begin + offset];
                        }
                        else
                        {
                            node->items[item_i].comp.data.value = values[offset];
                        }
                    }
                    else
                    {
                        // ASSERT(next - offset <= (size+2) / 3);
                        BITMAP_CLEAR(node->none_bitmap, item_i);
                        BITMAP_SET(node->child_bitmap, item_i);
                        node->items[item_i].comp.child = new_nodes(1);
                        int ts = 0;
                        for (int i = begin + offset; i < begin + next; i++)
                        {
                            if (BITMAP_GET(is_cp, i) == 1)
                            {
                                RT_ASSERT(nodes.find(i) != nodes.end());
                                ts += nodes[i]->size;
                            }
                            else
                            {
                                ts++;
                            }
                        }
                        s.push((Segment){
                            begin + offset,
                            begin + next,
                            level + 1,
                            node->items[item_i].comp.child,
                            ts});
                    }
                    if (next >= size)
                    {
                        break;
                    }
                    else
                    {
                        item_i = next_i;
                        offset = next;
                    }
                }
            }
        }

        return ret;
    }
    /// bulk build, _keys must be sorted in asc order.
    /// FMCD method.
    Node *build_tree_bulk_fmcd(T *_keys, P *_values, int _size, bitmap_t *is_cp, std::map<int, Node *> &nodes, uint8_t _layer, int _treesize)
    {
        RT_ASSERT(_size > 1);

        typedef struct
        {
            int begin;
            int end;
            int level; // top level = 1
            Node *node;
            int treesize;
        } Segment;
        std::stack<Segment> s;

        Node *ret = new_nodes(1);
        s.push((Segment){0, _size, 1, ret, _treesize});
        // int node_i = 0;
        while (!s.empty())
        {
            const int begin = s.top().begin;
            const int end = s.top().end;
            const int level = s.top().level;
            const int treesize = s.top().treesize;
            Node *node = s.top().node;
            s.pop();

            RT_ASSERT(end - begin >= 2);
            if (end - begin == 2)
            {
                if (BITMAP_GET(is_cp, begin) == 0)
                {
                    if (BITMAP_GET(is_cp, begin + 1) == 0)
                    {
                        Node *_ = build_tree_two(_keys[begin], _values[begin], _keys[begin + 1], _values[begin + 1], _layer);
                        memcpy(node, _, sizeof(Node));
                        delete_nodes(_, 1);
                    }
                    else
                    {
                        RT_ASSERT(nodes.find(begin + 1) != nodes.end());
                        Node *_ = build_tree_cp(_keys[begin], _values[begin], _keys[begin + 1], _layer, nodes[begin + 1]);
                        memcpy(node, _, sizeof(Node));
                        delete_nodes(_, 1);
                    }
                }
                else
                {
                    if (BITMAP_GET(is_cp, begin + 1) == 0)
                    {
                        RT_ASSERT(nodes.find(begin) != nodes.end());
                        Node *_ = build_tree_cp(_keys[begin + 1], _values[begin + 1], _keys[begin], _layer, nodes[begin]);
                        memcpy(node, _, sizeof(Node));
                        delete_nodes(_, 1);
                    }
                    else
                    {
                        RT_ASSERT(nodes.find(begin) != nodes.end() && nodes.find(begin + 1) != nodes.end());
                        Node *_ = build_tree_cp2(_keys[begin], _keys[begin + 1], _layer, nodes[begin], nodes[begin + 1]);
                        memcpy(node, _, sizeof(Node));
                        delete_nodes(_, 1);
                    }
                }
            }
            else
            {
                T *keys = _keys + begin;
                P *values = _values + begin;
                const int size = end - begin;
                const int BUILD_GAP_CNT = compute_gap_count(size);

                node->is_two = 0;
                node->build_size = size;
                node->size = treesize;
                node->fixed = 0;
                node->num_inserts = node->num_insert_to_data = 0;
                node->layer = _layer;

                // FMCD method
                // Here the implementation is a little different with Algorithm 1 in our paper.
                // In Algorithm 1, U_T should be (keys[size-1-D] - keys[D]) / (L - 2).
                // But according to the derivation described in our paper, M.A should be less than 1 / U_T.
                // So we added a small number (1e-6) to U_T.
                // In fact, it has only a negligible impact of the performance.
                {
                    node->model.layer = node->layer;
                    const int L = size * static_cast<int>(BUILD_GAP_CNT + 1);
                    int i = 0;
                    int D = 1;
                    RT_ASSERT(D <= size - 1 - D);
                    double Ut = (static_cast<long double>(keys[size - 1 - D].to_model_key(node->layer)) - static_cast<long double>(keys[D].to_model_key(node->layer))) /
                                    (static_cast<double>(L - 2)) +
                                1e-6;
                    while (i < size - 1 - D)
                    {
                        while (i + D < size && keys[i + D].to_model_key(node->layer) - keys[i].to_model_key(node->layer) >= Ut)
                        {
                            i++;
                        }
                        if (i + D >= size)
                        {
                            break;
                        }
                        D = D + 1;
                        if (D * 3 > size)
                            break;
                        RT_ASSERT(D <= size - 1 - D);
                        Ut = (static_cast<long double>(keys[size - 1 - D].to_model_key(node->layer)) - static_cast<long double>(keys[D].to_model_key(node->layer))) /
                                 (static_cast<double>(L - 2)) +
                             1e-6;
                    }
                    if (D * 3 <= size)
                    {
                        stats.fmcd_success_times++;

                        node->model.a = 1.0 / Ut;
                        if (node->model.a < 0)
                        {
                            // printf("%lf\n", node->model.a);
                            // printf("%lf\n", Ut);
                            printf("D:%d\n", D);
                            printf("size:%d\n", size);
                            printf("i:%d\n", i);
                            printf("layer:%d\n", node->layer);
                            // for (int j = D; j <= size - 1 - D; j++)
                            // {
                            //     printf("%lld\n", (keys[j].to_model_key(node->layer)));
                            // }
                            // printf("%lld\n", (keys[D].to_model_key(node->layer)));
                            // printf("%lld\n", (keys[size - 1 - D].to_model_key(node->layer)));
                        }
                        node->model.b = (L - node->model.a * (static_cast<long double>(keys[size - 1 - D].to_model_key(node->layer)) +
                                                              static_cast<long double>(keys[D].to_model_key(node->layer)))) /
                                        2;
                        RT_ASSERT(isfinite(node->model.a));
                        RT_ASSERT(isfinite(node->model.b));
                        node->num_items = L;
                    }
                    else
                    {
                        stats.fmcd_broken_times++;

                        int mid1_pos = (size - 1) / 3;
                        int mid2_pos = (size - 1) * 2 / 3;

                        RT_ASSERT(0 <= mid1_pos);
                        RT_ASSERT(mid1_pos < mid2_pos);
                        RT_ASSERT(mid2_pos < size - 1);

                        const long double mid1_key = (static_cast<long double>(keys[mid1_pos].to_model_key(node->layer)) +
                                                      static_cast<long double>(keys[mid1_pos + 1].to_model_key(node->layer))) /
                                                     2;
                        const long double mid2_key = (static_cast<long double>(keys[mid2_pos].to_model_key(node->layer)) +
                                                      static_cast<long double>(keys[mid2_pos + 1].to_model_key(node->layer))) /
                                                     2;

                        node->num_items = size * static_cast<int>(BUILD_GAP_CNT + 1);
                        const double mid1_target = mid1_pos * static_cast<int>(BUILD_GAP_CNT + 1) + static_cast<int>(BUILD_GAP_CNT + 1) / 2;
                        const double mid2_target = mid2_pos * static_cast<int>(BUILD_GAP_CNT + 1) + static_cast<int>(BUILD_GAP_CNT + 1) / 2;
                        node->model.a = (mid2_target - mid1_target) / (mid2_key - mid1_key);
                        node->model.b = mid1_target - node->model.a * mid1_key;
                        RT_ASSERT(isfinite(node->model.a));
                        RT_ASSERT(isfinite(node->model.b));
                    }
                }
                RT_ASSERT(node->model.a >= 0);
                const int lr_remains = static_cast<int>(size * BUILD_LR_REMAIN);
                node->model.b += lr_remains;
                node->num_items += lr_remains * 2;

                if (size > 1e6)
                {
                    node->fixed = 1;
                }

                node->items = new_items(node->num_items);
                const int bitmap_size = BITMAP_SIZE(node->num_items);
                node->none_bitmap = new_bitmap(bitmap_size);
                node->child_bitmap = new_bitmap(bitmap_size);
                node->cp_bitmap = new_bitmap(bitmap_size);
                memset(node->none_bitmap, 0xff, sizeof(bitmap_t) * bitmap_size);
                memset(node->child_bitmap, 0, sizeof(bitmap_t) * bitmap_size);
                memset(node->cp_bitmap, 0, sizeof(bitmap_t) * bitmap_size);

                for (int item_i = PREDICT_POS(node, keys[0]), offset = 0; offset < size;)
                {
                    int next = offset + 1, next_i = -1;
                    while (next < size)
                    {
                        next_i = PREDICT_POS(node, keys[next]);
                        if (next_i == item_i)
                        {
                            next++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    if (next == offset + 1)
                    {
                        BITMAP_CLEAR(node->none_bitmap, item_i);
                        node->items[item_i].comp.data.key = keys[offset];

                        if (BITMAP_GET(is_cp, begin + offset) == 1)
                        {
                            RT_ASSERT(nodes.find(begin + offset) != nodes.end());
                            BITMAP_SET(node->child_bitmap, item_i);
                            BITMAP_SET(node->cp_bitmap, item_i);
                            node->items[item_i].comp.child = nodes[begin + offset];
                        }
                        else
                        {
                            node->items[item_i].comp.data.value = values[offset];
                        }
                    }
                    else
                    {
                        // ASSERT(next - offset <= (size+2) / 3);
                        BITMAP_CLEAR(node->none_bitmap, item_i);
                        BITMAP_SET(node->child_bitmap, item_i);
                        node->items[item_i].comp.child = new_nodes(1);
                        int ts = 0;
                        for (int i = begin + offset; i < begin + next; i++)
                        {
                            if (BITMAP_GET(is_cp, i) == 1)
                            {
                                RT_ASSERT(nodes.find(i) != nodes.end());
                                ts += nodes[i]->size;
                            }
                            else
                            {
                                ts++;
                            }
                        }
                        s.push((Segment){
                            begin + offset,
                            begin + next,
                            level + 1,
                            node->items[item_i].comp.child,
                            ts});
                    }
                    // if (next == offset + 1)
                    // {
                    //     BITMAP_CLEAR(node->none_bitmap, item_i);
                    //     node->items[item_i].comp.data.key = keys[offset];

                    //     if (BITMAP_GET(is_cp, begin + offset) == 1)
                    //     {
                    //         RT_ASSERT(!nodes.empty());
                    //         BITMAP_SET(node->child_bitmap, item_i);
                    //         BITMAP_SET(node->cp_bitmap, item_i);
                    //         node->items[item_i].comp.child = nodes.front();
                    //         nodes.pop();
                    //     }
                    //     else
                    //     {
                    //         node->items[item_i].comp.data.value = values[offset];
                    //     }
                    // }
                    // else
                    // {
                    //     // ASSERT(next - offset <= (size+2) / 3);
                    //     BITMAP_CLEAR(node->none_bitmap, item_i);
                    //     BITMAP_SET(node->child_bitmap, item_i);
                    //     node->items[item_i].comp.child = new_nodes(1);
                    //     int ts = 0;
                    //     for (int i = begin + offset; i < begin + next; i++)
                    //     {
                    //         if (BITMAP_GET(is_cp, i) == 1)
                    //         {
                    //             RT_ASSERT(!nodes.empty());
                    //             std::queue<Node *> _nodes(nodes);
                    //             ts += _nodes.front()->size;
                    //             _nodes.pop();
                    //         }
                    //         else
                    //         {
                    //             ts++;
                    //         }
                    //     }
                    //     s.push((Segment){
                    //         begin + offset,
                    //         begin + next,
                    //         level + 1,
                    //         node->items[item_i].comp.child,
                    //         ts});
                    // }
                    if (next >= size)
                    {
                        break;
                    }
                    else
                    {
                        item_i = next_i;
                        offset = next;
                    }
                }
            }
        }

        return ret;
    }

    void destory_pending()
    {
        while (!pending_two.empty())
        {
            Node *node = pending_two.top();
            pending_two.pop();

            delete_items(node->items, node->num_items);
            const int bitmap_size = BITMAP_SIZE(node->num_items);
            delete_bitmap(node->none_bitmap, bitmap_size);
            delete_bitmap(node->child_bitmap, bitmap_size);
            delete_bitmap(node->cp_bitmap, bitmap_size);
            delete_nodes(node, 1);
        }
    }

    void destroy_tree(Node *root)
    {
        std::stack<Node *> s;
        s.push(root);
        while (!s.empty())
        {
            Node *node = s.top();
            s.pop();

            for (int i = 0; i < node->num_items; i++)
            {
                if (BITMAP_GET(node->child_bitmap, i) == 1)
                {
                    s.push(node->items[i].comp.child);
                }
            }

            if (node->is_two)
            {
                RT_ASSERT(node->build_size == 2);
                RT_ASSERT(node->num_items == 8);
                node->size = 2;
                node->num_inserts = node->num_insert_to_data = 0;
                node->none_bitmap[0] = 0xff;
                node->child_bitmap[0] = 0;
                node->cp_bitmap[0] = 0;
                node->layer = 0;
                pending_two.push(node);
            }
            else
            {
                delete_items(node->items, node->num_items);
                const int bitmap_size = BITMAP_SIZE(node->num_items);
                delete_bitmap(node->none_bitmap, bitmap_size);
                delete_bitmap(node->child_bitmap, bitmap_size);
                delete_bitmap(node->cp_bitmap, bitmap_size);
                delete_nodes(node, 1);
            }
        }
    }
    int get_this_layer_size(Node *_root)
    {
        int this_layer_size = 0;
        int cp = 0;
        std::stack<Node *> s;
        s.push(_root);
        while (!s.empty())
        {
            Node *node = s.top();
            s.pop();
            for (int i = 0; i < node->num_items; i++)
            {
                if (BITMAP_GET(node->none_bitmap, i) == 0)
                {
                    if (BITMAP_GET(node->child_bitmap, i) == 0)
                    { // DATA
                        this_layer_size++;
                    }
                    else if (BITMAP_GET(node->cp_bitmap, i) == 0)
                    { // NODE
                        s.push(node->items[i].comp.child);
                    }
                    else
                    { // NODE_cp
                        cp++;
                    }
                }
            }
        }

        return this_layer_size + cp;
    }
    int get_cp(Node *_root)
    {
        int cp = 0;
        std::stack<Node *> s;
        s.push(_root);
        while (!s.empty())
        {
            Node *node = s.top();
            s.pop();
            for (int i = 0; i < node->num_items; i++)
            {
                if (BITMAP_GET(node->none_bitmap, i) == 0)
                {
                    if (BITMAP_GET(node->cp_bitmap, i) == 1)
                    {
                        cp++;
                    }
                    else if (BITMAP_GET(node->child_bitmap, i) == 1)
                    {
                        s.push(node->items[i].comp.child);
                    }
                }
            }
        }

        return cp;
    }

    void scan_and_destory_tree(Node *_root, T *keys, P *values, bitmap_t *is_cp, std::map<int, Node *> &nodes, bool destory = true)
    {
        typedef std::pair<int, Node *> Segment; // <begin, Node*>
        std::stack<Segment> s;

        s.push(Segment(0, _root));
        while (!s.empty())
        {
            int begin = s.top().first;
            Node *node = s.top().second;
            const int SHOULD_END_POS = begin + get_this_layer_size(node);
            s.pop();

            for (int i = 0; i < node->num_items; i++)
            {
                if (BITMAP_GET(node->none_bitmap, i) == 0)
                {
                    if (BITMAP_GET(node->child_bitmap, i) == 0)
                    { // DATA
                        keys[begin] = node->items[i].comp.data.key;
                        values[begin] = node->items[i].comp.data.value;
                        begin++;
                    }
                    else if (BITMAP_GET(node->cp_bitmap, i) == 0)
                    { // NODE
                        s.push(Segment(begin, node->items[i].comp.child));
                        begin += get_this_layer_size(node->items[i].comp.child);
                    }
                    else if (BITMAP_GET(node->cp_bitmap, i) == 1)
                    { // NODE_cp
                        keys[begin] = node->items[i].comp.data.key;
                        nodes[begin] = node->items[i].comp.child;
                        BITMAP_SET(is_cp, begin);
                        begin++;
                    }
                }
            }
            RT_ASSERT(SHOULD_END_POS == begin);

            if (destory)
            {
                if (node->is_two)
                {
                    RT_ASSERT(node->build_size == 2);
                    RT_ASSERT(node->num_items == 8);
                    node->size = 2;
                    node->num_inserts = node->num_insert_to_data = 0;
                    node->none_bitmap[0] = 0xff;
                    node->child_bitmap[0] = 0;
                    node->cp_bitmap[0] = 0;
                    node->layer = 0;
                    pending_two.push(node);
                }
                else
                {
                    delete_items(node->items, node->num_items);
                    const int bitmap_size = BITMAP_SIZE(node->num_items);
                    delete_bitmap(node->none_bitmap, bitmap_size);
                    delete_bitmap(node->child_bitmap, bitmap_size);
                    delete_bitmap(node->cp_bitmap, bitmap_size);
                    delete_nodes(node, 1);
                }
            }
        }
    }

    /*
        void scan_and_destory_tree(Node *_root, T *keys, P *values, bool destory = true)
        {
            typedef std::pair<int, Node *> Segment; // <begin, Node*>
            std::stack<Segment> s;

            s.push(Segment(0, _root));
            while (!s.empty())
            {
                int begin = s.top().first;
                Node *node = s.top().second;
                const int SHOULD_END_POS = begin + node->size;
                s.pop();

                for (int i = 0; i < node->num_items; i++)
                {
                    if (BITMAP_GET(node->none_bitmap, i) == 0)
                    {
                        if (BITMAP_GET(node->child_bitmap, i) == 0)
                        {
                            keys[begin] = node->items[i].comp.data.key;
                            values[begin] = node->items[i].comp.data.value;
                            begin++;
                        }
                        else
                        {
                            s.push(Segment(begin, node->items[i].comp.child));
                            begin += node->items[i].comp.child->size;
                        }
                    }
                }
                RT_ASSERT(SHOULD_END_POS == begin);

                if (destory)
                {
                    if (node->is_two)
                    {
                        RT_ASSERT(node->build_size == 2);
                        RT_ASSERT(node->num_items == 8);
                        node->size = 2;
                        node->num_inserts = node->num_insert_to_data = 0;
                        node->none_bitmap[0] = 0xff;
                        node->child_bitmap[0] = 0;
                        node->cp_bitmap[0] = 0;
                        pending_two.push(node);
                    }
                    else
                    {
                        delete_items(node->items, node->num_items);
                        const int bitmap_size = BITMAP_SIZE(node->num_items);
                        delete_bitmap(node->none_bitmap, bitmap_size);
                        delete_bitmap(node->child_bitmap, bitmap_size);
                        delete_bitmap(node->cp_bitmap, bitmap_size);
                        delete_nodes(node, 1);
                    }
                }
            }
        }
    */

    Node *insert_tree(Node *_node, const T &key, const P &value)
    {
        constexpr int MAX_DEPTH = 128;
        Node *path[MAX_DEPTH];
        int path_size = 0;
        int insert_to_data = 0;
        uint8_t insert_layer = 0;

        for (Node *node = _node;;)
        {
            RT_ASSERT(path_size < MAX_DEPTH);
            path[path_size++] = node;

            node->size++;
            node->num_inserts++;
            int pos = PREDICT_POS(node, key);
            if (BITMAP_GET(node->none_bitmap, pos) == 1)
            { // insert to NULL
                BITMAP_CLEAR(node->none_bitmap, pos);
                node->items[pos].comp.data.key = key;
                node->items[pos].comp.data.value = value;
                insert_layer = node->layer;
                break;
            }
            else if (BITMAP_GET(node->child_bitmap, pos) == 0)
            { // insert to DATA
                BITMAP_SET(node->child_bitmap, pos);
                uint8_t layer = key.distinguish_layer(node->items[pos].comp.data.key);
                node->items[pos].comp.child = build_tree_two(key, value, node->items[pos].comp.data.key, node->items[pos].comp.data.value, layer);
                insert_to_data = 1;
                if (layer != node->layer)
                {
                    node->items[pos].comp.data.key.LCP(key, layer - 1);
                    BITMAP_SET(node->cp_bitmap, pos); // NODE -> NODE_cp
                    insert_to_data = 0;
                }
                insert_layer = layer;
                break;
            }
            else if (BITMAP_GET(node->cp_bitmap, pos) == 0)
            { // insert to NODE
                node = node->items[pos].comp.child;
            }
            else
            { // insert to NODE_cp
                // (slipp)
                uint8_t next_node_layer = node->items[pos].comp.child->layer;
                uint8_t layer = key.distinguish_layer(node->items[pos].comp.data.key);
                if (layer == next_node_layer)
                {
                    node = node->items[pos].comp.child;
                }
                else
                {
                    node->items[pos].comp.child = build_tree_cp(key, value, node->items[pos].comp.data.key, layer, node->items[pos].comp.child);

                    if (layer == node->layer)
                    {
                        BITMAP_CLEAR(node->cp_bitmap, pos); // NODE_cp -> NODE
                        insert_to_data = 1;
                    }
                    else
                    {
                        node->items[pos].comp.data.key.LCP(key, layer - 1);
                    }
                    // insert_to_data = 1;
                    insert_layer = layer;
                    break;
                }
            }
        }
        for (int i = 0; i < path_size; i++)
        {
            path[i]->num_insert_to_data += insert_to_data;
        }

        for (int i = 0; i < path_size; i++)
        {
            Node *node = path[i];
            const int num_inserts = node->num_inserts;
            const int num_insert_to_data = node->num_insert_to_data;
            const bool need_rebuild = node->fixed == 0 && node->layer == insert_layer && node->size >= node->build_size * 4 && node->size >= 64 && num_insert_to_data * 10 >= num_inserts;

            if (need_rebuild)
            {
                const int ESIZE = get_this_layer_size(node);
                const int cpSIZE = get_cp(node);
                T *keys = new T[ESIZE];
                P *values = new P[ESIZE];
                bitmap_t *is_cp = new_bitmap(BITMAP_SIZE(ESIZE));
                memset(is_cp, 0, sizeof(bitmap_t) * BITMAP_SIZE(ESIZE));
                // std::queue<Node *> nodes;
                std::map<int, Node *> nodes;
                const uint8_t _layer = node->layer;
                const int _size = node->size;

#if COLLECT_TIME
                auto start_time_scan = std::chrono::high_resolution_clock::now();
#endif
                scan_and_destory_tree(node, keys, values, is_cp, nodes);
#if COLLECT_TIME
                auto end_time_scan = std::chrono::high_resolution_clock::now();
                auto duration_scan = end_time_scan - start_time_scan;
                stats.time_scan_and_destory_tree += std::chrono::duration_cast<std::chrono::nanoseconds>(duration_scan).count() * 1e-9;
#endif
                // for (int i = 0; i < ESIZE; i++)
                // {
                //     printf("%s\n", keys[i].to_string().c_str());
                //     printf("%lld\n", keys[i].to_model_key(node->layer));
                // }
                RT_ASSERT(nodes.size() == cpSIZE);

#if COLLECT_TIME
                auto start_time_build = std::chrono::high_resolution_clock::now();
#endif
                Node *new_node = build_tree_bulk(keys, values, ESIZE, is_cp, nodes, _layer, _size);
#if COLLECT_TIME
                auto end_time_build = std::chrono::high_resolution_clock::now();
                auto duration_build = end_time_build - start_time_build;
                stats.time_build_tree_bulk += std::chrono::duration_cast<std::chrono::nanoseconds>(duration_build).count() * 1e-9;
#endif

                delete[] keys;
                delete[] values;
                delete_bitmap(is_cp, BITMAP_SIZE(ESIZE));

                path[i] = new_node;
                if (i > 0)
                {
                    int pos = PREDICT_POS(path[i - 1], key);
                    path[i - 1]->items[pos].comp.child = new_node;
                }

                break;
            }
        }

        return path[0];
    }
};

#endif // __SLIPP_H__
