#pragma once
#include <string>
#include <deque>
#include <algorithm>
#include <cmath>

inline std::wstring spark_braille(const std::deque<double> &vals, int width)
{
    if (vals.empty() || width <= 0)
        return L"";
    int cells = std::max(1, width);
    int take = std::min((int)vals.size(), cells * 2);
    auto it = vals.end() - take;

    double mn = *std::min_element(it, vals.end());
    double mx = *std::max_element(it, vals.end());
    double span = (mx - mn);
    if (span < 1e-9)
        span = 1.0;

    std::wstring out;
    out.reserve(cells);
    for (int c = 0; c < cells; ++c)
    {
        double v0 = mn, v1 = mn;
        if (it != vals.end())
            v0 = *it++;
        if (it != vals.end())
            v1 = *it++;

        auto level = [&](double v)
        {
            double norm = (v - mn) / span;
            if (norm < 0)
                norm = 0;
            if (norm > 1)
                norm = 1;
            return (int)std::round(norm * 3.0);
        };
        int l0 = level(v0), l1 = level(v1);

        int mask = 0;
        auto setCol = [&](int L, bool B)
        {
            if (L >= 0)
                mask |= B ? 8 : 1;
            if (L >= 1)
                mask |= B ? 16 : 2;
            if (L >= 2)
                mask |= B ? 32 : 4;
            if (L >= 3)
                mask |= B ? 128 : 64;
        };
        setCol(l0, false);
        setCol(l1, true);

        wchar_t ch = (wchar_t)(0x2800 + mask);
        out.push_back(ch);
    }
    return out;
}
