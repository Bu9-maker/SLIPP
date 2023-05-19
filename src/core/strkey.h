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

/*
template <size_t len>
class StrKey;

// typedef StrKey<64> index_key_t;

template <size_t len>
class StrKey
{
#if (KEY_SLICE_LEN == 8)
    typedef uint64_t key_slice_t;
    typedef std::array<uint64_t, len / KEY_SLICE_LEN> model_key_t;
#endif
#if (KEY_SLICE_LEN == 4)
    typedef uint32_t key_slice_t;
    typedef std::array<uint32_t, len / KEY_SLICE_LEN> model_key_t;
#endif

public:
    static constexpr size_t model_key_size() { return len; }

    static StrKey max()
    {
        static StrKey max_key;
        memset(&max_key.buf, 255, len);
        return max_key;
    }
    static StrKey min()
    {
        static StrKey min_key;
        memset(&min_key.buf, 0, len);
        return min_key;
    }

    StrKey() { memset(&buf, 0, len); }
    // StrKey(uint64_t key) { COUT_N_EXIT("str key no uint64"); }
    StrKey(const std::string &s)
    {
        memset(&buf, 0, len);
        memcpy(&buf, s.data(), s.size());
    }
    StrKey(const StrKey &other) { memcpy(&buf, &other.buf, len); }

    StrKey &operator=(const StrKey &other)
    {
        memcpy(&buf, &other.buf, len);
        return *this;
    }

    model_key_t to_model_key() const
    {
        model_key_t model_key;
        int j = 0;
        key_slice_t temp = 0;
        int count = 0;
        for (size_t i = 0; i < len; i++)
        {
            temp <<= 8;
            temp |= static_cast<uint8_t>(buf[i]);
            count++;
            if (count == KEY_SLICE_LEN)
            {
                model_key[j++] = temp;
                count = 0;
                temp = 0;
            }
        }
        return model_key;
    }

    // void get_model_key(size_t begin_f, size_t l, double *target) const
    // {
    //     for (size_t i = 0; i < l; i++)
    //     {
    //         target[i] = buf[i + begin_f];
    //     }
    // }

    bool less_than(const StrKey &other, size_t begin_i, size_t l) const
    {
        return memcmp(buf + begin_i, other.buf + begin_i, l) < 0;
    }

    friend bool operator<(const StrKey &l, const StrKey &r)
    {
        return memcmp(&l.buf, &r.buf, len) < 0;
    }
    friend bool operator>(const StrKey &l, const StrKey &r)
    {
        return memcmp(&l.buf, &r.buf, len) > 0;
    }
    friend bool operator>=(const StrKey &l, const StrKey &r)
    {
        return memcmp(&l.buf, &r.buf, len) >= 0;
    }
    friend bool operator<=(const StrKey &l, const StrKey &r)
    {
        return memcmp(&l.buf, &r.buf, len) <= 0;
    }
    friend bool operator==(const StrKey &l, const StrKey &r)
    {
        return memcmp(&l.buf, &r.buf, len) == 0;
    }
    friend bool operator!=(const StrKey &l, const StrKey &r)
    {
        return memcmp(&l.buf, &r.buf, len) != 0;
    }

    friend std::ostream &operator<<(std::ostream &os, const StrKey &key)
    {
        os << "key [" << std::hex << std::showbase;
        for (size_t i = 0; i < sizeof(StrKey); i++)
        {
            os << int(key.buf[i]) << " ";
        }
        os << "] (as byte)" << std::dec;
        return os;
    }

    uint8_t buf[len];
};
*/

template <size_t len>
class StrKey
{
#if (KEY_SLICE_LEN == 8)
    typedef uint64_t key_slice_t;
#endif
#if (KEY_SLICE_LEN == 4)
    typedef uint32_t key_slice_t;
#endif
    typedef std::array<key_slice_t, len / KEY_SLICE_LEN> model_key_t;

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
    // StrKey(const StrKey &s, const uint8_t layer) // return the 0~layer layers of s
    // {
    //     std::copy(s.buf.cbegin(), s.buf.cbegin() + (layer + 1) * KEY_SLICE_LEN, buf.begin());
    // }
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
        return layer;
    }

    StrKey &operator=(const StrKey &other) = default;

    model_key_t to_model_key() const
    {
        model_key_t model_key;
        int j = 0;
        key_slice_t temp = 0;
        int count = 0;
        for (size_t i = 0; i < len; i++)
        {
            temp <<= 8;
            temp |= buf[i];
            count++;
            if (count == KEY_SLICE_LEN)
            {
                model_key[j++] = temp;
                count = 0;
                temp = 0;
            }
        }
        return model_key;
    }
    key_slice_t to_model_key(int layer) const
    {
        layer = layer < 0 ? 0 : layer;
        key_slice_t temp = 0;
        for (size_t i = 0; i < KEY_SLICE_LEN; i++)
        {
            temp <<= 8;
            temp |= buf[KEY_SLICE_LEN * layer + i];
        }
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