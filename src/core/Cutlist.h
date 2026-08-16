#pragma once

#include <QVector>

#include "core/Cut.h"

class Cutlist
{
public:
    Cutlist() = default;
    explicit Cutlist(qint64 totalFrames);

    void setTotalFrames(qint64 frames);
    qint64 totalFrames() const
    {
        return m_totalFrames;
    }

    const QVector<Cut>& cuts() const
    {
        return m_cuts;
    }
    void setCuts(QVector<Cut> cuts); // sorted, clipped to [0, totalFrames]

    // adds a keep-range; returns false on invalid range or overlap
    bool addCut(qint64 inFrame, qint64 outFrame);
    bool removeAt(int index);
    bool removeOverlap(const Cut& cut); // removes cuts intersecting [in,out)
    void clear();

    qint64 keepFrameCount() const;
    qint64 cutFrameCount() const; // gaps between keep-ranges within [0,total]

    bool containsFrame(qint64 frame) const;
    int indexOf(const Cut& cut) const;

private:
    void normalize(); // sort, clip, drop empty

    qint64 m_totalFrames = 0;
    QVector<Cut> m_cuts;
};