#pragma once

#include <QMetaType>
#include <QtGlobal>

struct Fps
{
    int num = 0; // frames
    int den = 1; // seconds

    Fps() = default;
    Fps(int n, int d)
        : num(n)
        , den(d)
    {
    }

    bool isValid() const
    {
        return num > 0 && den > 0;
    }

    bool operator==(const Fps& o) const
    {
        return num == o.num && den == o.den;
    }
    bool operator!=(const Fps& o) const
    {
        return !(*this == o);
    }

    double value() const
    {
        return isValid() ? double(num) / double(den) : 0.0;
    }
};

Q_DECLARE_METATYPE(Fps)