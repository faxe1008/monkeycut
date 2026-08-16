#pragma once

#include <QtGlobal>

#include "Fps.h"

class Time
{
public:
    Time() = default;
    Time(qint64 ticks, int tbNum, int tbDen);

    bool isValid() const
    {
        return m_tbNum > 0 && m_tbDen > 0;
    }

    qint64 ticks() const
    {
        return m_ticks;
    }
    int tbNum() const
    {
        return m_tbNum;
    }
    int tbDen() const
    {
        return m_tbDen;
    }

    double toSeconds() const
    {
        return isValid() ? double(m_ticks) * double(m_tbNum) / double(m_tbDen) : 0.0;
    }

    static Time fromSeconds(double s);
    static Time fromFrames(qint64 frame, const Fps& fps);
    static Time fromFrames(qint64 frame, int tbNum, int tbDen);

    Time toTimebase(int tbNum, int tbDen) const;

    Time operator+(const Time& o) const;
    Time operator-(const Time& o) const;
    bool operator<(const Time& o) const;
    bool operator<=(const Time& o) const;
    bool operator==(const Time& o) const;
    bool operator!=(const Time& o) const
    {
        return !(*this == o);
    }
    bool operator>(const Time& o) const
    {
        return o < *this;
    }
    bool operator>=(const Time& o) const
    {
        return !(o > *this);
    }

private:
    Time(qint64 ns);

    qint64 m_ticks = 0;
    int m_tbNum = 1;
    int m_tbDen = 1;
};