#pragma once

#include <algorithm>

#include <QMetaType>
#include <QtGlobal>
#include <QVector>

struct GopMap
{
    bool valid = false;
    qint64 frameCount = 0;
    QVector<qint64> keyframes;

    qint64 snapBack(qint64 f) const
    {
        if (keyframes.isEmpty())
            return -1;
        auto it = std::upper_bound(keyframes.constBegin(), keyframes.constEnd(), f);
        if (it == keyframes.constBegin())
            return -1;
        return *(--it);
    }

    qint64 snapFwd(qint64 f) const
    {
        if (keyframes.isEmpty())
            return -1;
        auto it = std::lower_bound(keyframes.constBegin(), keyframes.constEnd(), f);
        if (it == keyframes.constEnd())
            return -1;
        return *it;
    }
};

Q_DECLARE_METATYPE(GopMap)