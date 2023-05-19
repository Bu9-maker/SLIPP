#ifndef __LINEAR_MODEL_H__
#define __LINEAR_MODEL_H__
#include <array>
#include <limits>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <atomic>

// typedef std::array<uint64_t, len / KEY_SLICE_LEN> model_key_t;

template <class T>
class LinearModel
{
public:
    LinearModel() = default;
    LinearModel(double a, long double b, uint8_t layer) : a(a), b(b), layer(layer) {}

    explicit LinearModel(const LinearModel &other) : a(other.a), b(other.b), layer(other.layer) {}

    std::pair<double, long double> train(T *x, int len)
    {
        int n = len;
        long double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0, temp_x = 0;
        for (int i = 0; i < n; i++)
        {
            temp_x = x->to_model_key()[layer];
            sum_x += temp_x;
            sum_y += i;
            sum_xy += temp_x * i;
            sum_xx += temp_x * temp_x;
            x++;
        }
        double a = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
        double b = (sum_y - a * sum_x) / n;
        this->a = a;
        this->b = b;
        return std::pair<double, long double>(a, b);
    }
    inline int predict(T key) const
    {
        return std::floor(a * static_cast<long double>(key.to_model_key(layer)) + b);
    }
    inline double predict_double(T key) const
    {
        return a * static_cast<long double>(key.to_model_key(layer)) + b;
    }

    uint8_t layer = 0; // layer
    double a = 0;      // slope
    long double b = 0; // intercept
};

#endif