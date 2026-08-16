#pragma once

#include <QMetaType>
#include <QString>

#include "Fps.h"

class TimeCode
{
public:
    TimeCode() = default;
    TimeCode(qint64 frame, Fps fps);

    bool isValid() const
    {
        return m_fps.isValid() && m_frame >= 0;
    }

    qint64 frame() const
    {
        return m_frame;
    }
    const Fps& fps() const
    {
        return m_fps;
    }
    qint64 totalFrames() const
    {
        return m_frame;
    }
    double toSeconds() const;

    TimeCode operator+(qint64 frames) const;
    TimeCode operator-(qint64 frames) const;
    bool operator<(const TimeCode& o) const;
    bool operator==(const TimeCode& o) const;

    QString toString() const;
    QString toStringMs() const;

    static TimeCode fromString(const QString& str, Fps fps);

private:
    qint64 m_frame = 0;
    Fps m_fps;
};

Q_DECLARE_METATYPE(TimeCode)