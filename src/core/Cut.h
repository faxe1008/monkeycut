#pragma once

#include <QList>
#include <QMetaType>
#include <QtGlobal>

// A KEEP-range: the frames [inFrame, outFrame) are kept, the gaps are cut.
// Semantics follow cutlist.at CUL files ("the following parts are kept").
struct Cut
{
    qint64 inFrame = 0;
    qint64 outFrame = 0;

    Cut() = default;
    Cut(qint64 in, qint64 out)
        : inFrame(in)
        , outFrame(out)
    {
    }

    qint64 frames() const
    {
        return outFrame - inFrame;
    }

    bool operator==(const Cut& o) const
    {
        return inFrame == o.inFrame && outFrame == o.outFrame;
    }
};

Q_DECLARE_METATYPE(Cut)
Q_DECLARE_METATYPE(QVector<Cut>)