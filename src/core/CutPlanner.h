#pragma once

#include <QMetaType>
#include <QVector>

#include "core/Cut.h"
#include "core/GopMap.h"

struct PlannedSegment
{
    qint64 inFrame = 0;  // effective start (keyframe aligned)
    qint64 outFrame = 0; // effective end (exact)
    qint64 inDelta = 0;  // effective start - requested start (<= 0)
};

Q_DECLARE_METATYPE(PlannedSegment)
Q_DECLARE_METATYPE(QVector<PlannedSegment>)

struct PlanResult
{
    bool ok = false;
    bool keyframeAligned = false; // true when the GOP map was known
    QVector<PlannedSegment> segments;
};

// Converts keep-ranges into segments the stream-copy cutter can honor:
// segment starts land on keyframes (video can only start there),
// segment ends stay exact (copy just stops).
PlanResult planCuts(const QVector<Cut>& cuts,
                    const GopMap& gop,
                    qint64 totalFrames = -1);