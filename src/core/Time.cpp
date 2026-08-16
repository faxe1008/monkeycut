#include "Time.h"

#include <limits>

Time::Time(qint64 ticks, int tbNum, int tbDen)
    : m_ticks(ticks)
    , m_tbNum(tbNum)
    , m_tbDen(tbDen)
{
    if (m_tbDen < 0) {
        m_ticks = -m_ticks;
        m_tbDen = -m_tbDen;
    }
}

Time::Time(qint64 ns)
    : m_ticks(ns)
    , m_tbNum(1)
    , m_tbDen(1000000000)
{
}

Time Time::fromSeconds(double s)
{
    return Time(static_cast<qint64>(s * 1000000000.0));
}

Time Time::fromFrames(qint64 frame, const Fps& fps)
{
    if (!fps.isValid())
        return {};
    return Time(frame * fps.den, 1, fps.num);
}

Time Time::fromFrames(qint64 frame, int tbNum, int tbDen)
{
    return Time(frame, tbNum, tbDen);
}

Time Time::toTimebase(int tbNum, int tbDen) const
{
    if (!isValid() || tbNum <= 0 || tbDen <= 0)
        return {};
    const __int128 n = static_cast<__int128>(m_ticks) * static_cast<__int128>(m_tbNum)
        * static_cast<__int128>(tbDen);
    const __int128 d = static_cast<__int128>(m_tbDen) * static_cast<__int128>(tbNum);
    if (d == 0)
        return {};
    const bool neg = n < 0;
    const __int128 a = neg ? -n : n;
    const __int128 maxQ = static_cast<__int128>(std::numeric_limits<qint64>::max()) * d;
    qint64 q = a > maxQ ? std::numeric_limits<qint64>::max() : static_cast<qint64>(a / d);
    if (neg)
        q = -q;
    return Time(q, tbNum, tbDen);
}

Time Time::operator+(const Time& o) const
{
    const Time oo = o.toTimebase(m_tbNum, m_tbDen);
    return Time(m_ticks + oo.ticks(), m_tbNum, m_tbDen);
}

Time Time::operator-(const Time& o) const
{
    const Time oo = o.toTimebase(m_tbNum, m_tbDen);
    return Time(m_ticks - oo.ticks(), m_tbNum, m_tbDen);
}

bool Time::operator<(const Time& o) const
{
    if (isValid() && o.isValid()) {
        const __int128 a = static_cast<__int128>(m_ticks) * o.m_tbNum * o.m_tbDen;
        const __int128 b = static_cast<__int128>(o.m_ticks) * m_tbNum * m_tbDen;
        return a < b;
    }
    return o.isValid();
}

bool Time::operator<=(const Time& o) const
{
    if (isValid() && o.isValid()) {
        const __int128 a = static_cast<__int128>(m_ticks) * o.m_tbNum * o.m_tbDen;
        const __int128 b = static_cast<__int128>(o.m_ticks) * m_tbNum * m_tbDen;
        return a <= b;
    }
    return !o.isValid();
}

bool Time::operator==(const Time& o) const
{
    if (isValid() && o.isValid()) {
        const __int128 a = static_cast<__int128>(m_ticks) * o.m_tbNum * o.m_tbDen;
        const __int128 b = static_cast<__int128>(o.m_ticks) * m_tbNum * m_tbDen;
        return a == b;
    }
    return !isValid() && !o.isValid();
}