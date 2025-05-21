#include <iostream>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include "ticktock.h"
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_scan.h>
// TODO: 并行化所有这些 for 循环

template <class T, class Func>
std::vector<T> fill(std::vector<T> &arr, Func const &func)
{
    TICK(fill);
    tbb::parallel_for(tbb::blocked_range<size_t>(0, arr.size()),
                      [&](const tbb::blocked_range<size_t> &r)
                      {
                          for (size_t i = r.begin(); i < r.end(); i++)
                          {
                              arr[i] = func(i);
                          }
                      });
    TOCK(fill);
    return arr;
}

template <class T>
void saxpy(T a, std::vector<T> &x, std::vector<T> const &y)
{
    TICK(saxpy);
    tbb::parallel_for(tbb::blocked_range<size_t>(0, x.size()),
                      [&](const tbb::blocked_range<size_t> &r)
                      {
                          for (size_t i = r.begin(); i < r.end(); i++)
                          {
                              x[i] = a * x[i] + y[i];
                          }
                      });
    TOCK(saxpy);
}

template <class T>
T sqrtdot(std::vector<T> const &x, std::vector<T> const &y)
{
    TICK(sqrtdot);
    // for (size_t i = 0; i < std::min(x.size(), y.size()); i++)
    // {
    //     ret += x[i] * y[i];
    // }
    T ret = tbb::parallel_reduce(
        tbb::blocked_range<size_t>(0, std::min(x.size(), y.size())),
        T(0),
        [&](const tbb::blocked_range<size_t> &r, T sum)
        {
            for (size_t i = r.begin(); i < r.end(); i++)
                sum += x[i] * y[i];
            return sum;
        },
        [](T a, T b)
        { return a + b; });
    ret = std::sqrt(ret);
    TOCK(sqrtdot);
    return ret;
}

template <class T>
T minvalue(std::vector<T> const &x)
{
    TICK(minvalue);
    // T ret = x[0];
    // for (size_t i = 1; i < x.size(); i++)
    // {
    //     if (x[i] < ret)
    //         ret = x[i];
    // }
    T ret = tbb::parallel_reduce(tbb::blocked_range<size_t>(0, x.size()), x[0], [&](const tbb::blocked_range<size_t> &r, T min)
                                 {
                                     for (size_t i = r.begin(); i < r.end(); i++)
                                         if (x[i] < min)
                                             min = x[i];
                                     return min; }, [](T a, T b)
                                 { return std::min(a, b); });

    TOCK(minvalue);
    return ret;
}

template <class T>
std::vector<T> magicfilter(std::vector<T> const &x, std::vector<T> const &y)
{
    TICK(magicfilter);
    // std::vector<T> res;
    // for (size_t i = 0; i < std::min(x.size(), y.size()); i++)
    // {
    //     if (x[i] > y[i])
    //     {
    //         res.push_back(x[i]);
    //     }
    //     else if (y[i] > x[i] && y[i] > 0.5f)
    //     {
    //         res.push_back(y[i]);
    //         res.push_back(x[i] * y[i]);
    //     }
    // }
    tbb::concurrent_vector<T> res;
    tbb::parallel_for(tbb::blocked_range<size_t>(0, std::min(x.size(), y.size())),
                      [&](const tbb::blocked_range<size_t> &r)
                      {
                          std::vector<T> tmp;
                          tmp.reserve(r.size() * 2);
                          for (size_t i = r.begin(); i < r.end(); i++)
                          {
                              if (x[i] > y[i])
                              {
                                  tmp.push_back(x[i]);
                              }
                              else if (y[i] > x[i] && y[i] > 0.5f)
                              {
                                  tmp.push_back(y[i]);
                                  tmp.push_back(x[i] * y[i]);
                              }
                          }
                          if (!tmp.empty())
                          {
                              res.grow_by(tmp.begin(), tmp.end());
                          }
                      });

    TOCK(magicfilter);
    return std::vector<T>(res.begin(), res.end());
}

template <class T>
T scanner(std::vector<T> &x)
{
    TICK(scanner);
    // T ret = 0;
    // for (size_t i = 0; i < x.size(); i++)
    // {
    //     ret += x[i];
    //     x[i] = ret;
    // }
    // TOCK(scanner);
    // return ret;
    T total = tbb::parallel_scan(
        tbb::blocked_range<size_t>(0, x.size()), T(0),
        [&](const tbb::blocked_range<size_t> &r, T sum, bool isFinal) -> T
        {
            for (size_t i = r.begin(); i < r.end(); i++)
            {
                sum += x[i];
                if (isFinal)
                    x[i] = sum;
            }
            return sum;
        },
        [](T a, T b) -> T
        { return a + b; }
    );
    TOCK(scanner);
    return total;
}

int main()
{
    size_t n = 1 << 26;
    std::vector<float> x(n);
    std::vector<float> y(n);

    fill(x, [&](size_t i)
         { return std::sin(i); });
    fill(y, [&](size_t i)
         { return std::cos(i); });

    saxpy(0.5f, x, y);

    std::cout << sqrtdot(x, y) << std::endl;
    std::cout << minvalue(x) << std::endl;

    auto arr = magicfilter(x, y);
    std::cout << arr.size() << std::endl;

    scanner(x);
    std::cout << std::reduce(x.begin(), x.end()) << std::endl;

    return 0;
}
