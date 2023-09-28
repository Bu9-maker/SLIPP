#ifndef __STRKEY_H__
#define __STRKEY_H__
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <random>
#include <cstring>
#include <string>
#include <vector>

#define KEY_SLICE_LEN 8 // Divide the string key into segments by KEY_SLICE_lEN bytes

template <size_t len>
class StrKey
{
#if (KEY_SLICE_LEN == 8)
    typedef uint64_t key_slice_t;
#endif
#if (KEY_SLICE_LEN == 4)
    typedef uint32_t key_slice_t;
#endif

public:
    static constexpr size_t buff_size() { return len; }

    static StrKey max()
    {
        static StrKey max_key;
        max_key.buf.fill(255);
        return max_key;
    }
    static StrKey min()
    {
        static StrKey min_key;
        min_key.buf.fill(0);
        return min_key;
    }

    StrKey() = default;
    StrKey(const std::string &s)
    {
        std::copy(s.begin(), s.end(), buf.begin());
    }
    StrKey(const int i)
    {
        buf[0] = 48 + i % 10;
    }
    StrKey(const StrKey &other) = default;
    void LCP(const StrKey &other, uint8_t layer)
    {
        buf.fill(0);
        std::copy(other.buf.cbegin(), other.buf.cbegin() + (layer + 1) * KEY_SLICE_LEN, buf.begin());
    }

    uint8_t distinguish_layer(const StrKey &s) const
    {
        auto it1 = this->buf.cbegin();
        auto it2 = s.buf.cbegin();
        uint8_t layer = 0;

        int count = 0;
        for (;
             it1 != this->buf.end() && it2 != s.buf.end() && *it1 == *it2;
             it1++, it2++)
        {
            count++;
            if (count == KEY_SLICE_LEN)
            {
                count = 0;
                layer++;
            }
        }
        if (it1 == this->buf.end() && it2 == s.buf.end())
            return 255;

        return layer;
    }

    StrKey &operator=(const StrKey &other) = default;

    key_slice_t to_model_key(int layer) const
    {
        layer = layer < 0 ? 0 : layer;
        key_slice_t temp = 0;
        // for (size_t i = 0; i < KEY_SLICE_LEN; i++)
        // {
        //     temp <<= 8;
        //     temp |= buf[KEY_SLICE_LEN * layer + i];
        // }
        temp = be64toh(*(uint64_t *)(buf.begin() + layer * 8));
        return temp;
    }

    bool less_than(const StrKey &other, size_t begin_i, size_t l) const
    {
        return std::memcmp(buf.data() + begin_i, other.buf.data() + begin_i, l) < 0;
    }

    friend bool operator<(const StrKey &l, const StrKey &r)
    {
        return std::memcmp(l.buf.data(), r.buf.data(), len) < 0;
    }
    friend bool operator>(const StrKey &l, const StrKey &r)
    {
        return std::memcmp(l.buf.data(), r.buf.data(), len) > 0;
    }
    friend bool operator>=(const StrKey &l, const StrKey &r)
    {
        return std::memcmp(l.buf.data(), r.buf.data(), len) >= 0;
    }
    friend bool operator<=(const StrKey &l, const StrKey &r)
    {
        return std::memcmp(l.buf.data(), r.buf.data(), len) <= 0;
    }
    friend bool operator==(const StrKey &l, const StrKey &r)
    {
        return std::memcmp(l.buf.data(), r.buf.data(), len) == 0;
    }
    friend bool operator!=(const StrKey &l, const StrKey &r)
    {
        return std::memcmp(l.buf.data(), r.buf.data(), len) != 0;
    }

    friend std::ostream &operator<<(std::ostream &os, const StrKey &key)
    {
        os << "key [" << std::hex << std::showbase;
        for (size_t i = 0; i < len; i++)
        {
            os << int(key.buf[i]) << " ";
        }
        os << "] (as byte)" << std::dec;
        return os;
    }

    std::string to_string()
    {
        std::string str(len, '\0');
        std::copy(buf.begin(), buf.end(), str.begin());
        return str;
    }

    std::array<uint8_t, len> buf{};
};

#endif