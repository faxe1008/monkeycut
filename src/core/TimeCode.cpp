#include "TimeCode.h"

#include <QStringList>

TimeCode::TimeCode(qint64 frame, Fps fps)
    : m_frame(frame)
    , m_fps(fps)
{
}

double TimeCode::toSeconds() const
{
    if (!m_fps.isValid() || m_frame < 0)
        return 0.0;
    return double(m_frame) * double(m_fps.den) / double(m_fps.num);
}

TimeCode TimeCode::operator+(qint64 frames) const
{
    return TimeCode(m_frame + frames, m_fps);
}

TimeCode TimeCode::operator-(qint64 frames) const
{
    return TimeCode(m_frame - frames, m_fps);
}

bool TimeCode::operator<(const TimeCode& o) const
{
    if (m_frame == o.m_frame)
        return false;
    return toSeconds() < o.toSeconds();
}

bool TimeCode::operator==(const TimeCode& o) const
{
    return m_frame == o.m_frame && m_fps.num == o.m_fps.num && m_fps.den == o.m_fps.den;
}

static qint64 hmsToFrames(int h, int m, int s, Fps fps)
{
    const qint64 totalSec = qint64(h) * 3600 + qint64(m) * 60 + s;
    return totalSec * fps.num / fps.den;
}

QString TimeCode::toString() const
{
    if (!isValid())
        return QStringLiteral("00:00:00:00");
    const double fps = m_fps.value();
    const qint64 totalMs = qint64(m_frame / fps * 1000.0);
    const qint64 ms = totalMs % 1000;
    const qint64 totalSec = totalMs / 1000;
    const int h = int(totalSec / 3600);
    const int m = int((totalSec / 60) % 60);
    const int s = int(totalSec % 60);
    const int f = int(m_frame - qint64(qRound(totalSec * fps)));
    Q_UNUSED(ms)
    return QString::asprintf("%02d:%02d:%02d:%02d", h, m, s, f);
}

QString TimeCode::toStringMs() const
{
    if (!isValid())
        return QStringLiteral("00:00:00.000");
    const double sec = toSeconds();
    const qint64 totalMs = qint64(sec * 1000.0);
    const qint64 ms = totalMs % 1000;
    const qint64 totalSec = totalMs / 1000;
    const int h = int(totalSec / 3600);
    const int m = int((totalSec / 60) % 60);
    const int s = int(totalSec % 60);
    return QString::asprintf("%02d:%02d:%02d.%03d", h, m, s, int(ms));
}

TimeCode TimeCode::fromString(const QString& str, Fps fps)
{
    if (!fps.isValid())
        return {};
    const auto parts = str.trimmed().split(QStringLiteral(":"));
    bool ok1 = false, ok2 = false, ok3 = false;
    if (parts.size() == 4) {
        const int h = parts[0].toInt(&ok1);
        const int m = parts[1].toInt(&ok2);
        const int s = parts[2].toInt(&ok3);
        if (ok1 && ok2 && ok3) {
            qint64 f;
            const QString last = parts[3];
            if (!last.contains('.')) {
                f = last.toLongLong(&ok1);
            } else {
                f = qint64(last.toDouble(&ok1) * fps.value());
            }
            if (ok1 && h >= 0 && m >= 0 && s >= 0 && f >= 0)
                return TimeCode(hmsToFrames(h, m, s, fps) + f, fps);
        }
        return {};
    }
    if (parts.size() == 3) {
        const int h = parts[0].toInt(&ok1);
        const int m = parts[1].toInt(&ok2);
        const QString sPart = parts[2];
        double secF = 0;
        if (sPart.contains('.'))
            secF = sPart.toDouble(&ok3);
        else
            secF = sPart.toDouble(&ok3);
        if (ok1 && ok2 && ok3 && h >= 0 && m >= 0)
            return TimeCode(qint64((h * 3600 + m * 60 + secF) * fps.value()), fps);
    }
    return {};
}