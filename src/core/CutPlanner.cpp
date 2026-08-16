#include "CutPlanner.h"

#include "core/Cut.h"

PlanResult planCuts(const QVector<Cut>& cuts, const GopMap& gop, qint64 totalFrames)
{
    PlanResult result;
    result.keyframeAligned = gop.valid && !gop.keyframes.isEmpty();

    for (const Cut& c : cuts) {
        PlannedSegment seg;
        seg.inFrame = qMax<qint64>(0, c.inFrame);
        seg.outFrame = totalFrames > 0 ? qMin(c.outFrame, totalFrames) : c.outFrame;
        if (seg.outFrame <= seg.inFrame)
            continue;

        if (result.keyframeAligned) {
            if (seg.inFrame == 0) {
                // start of file: keep as-is, but the first keyframe may be
                // a few frames in (e.g. TS with late start)
                if (gop.keyframes.first() > 0)
                    seg.inFrame = gop.keyframes.first();
            } else {
                const qint64 kf = gop.snapBack(seg.inFrame);
                if (kf < 0) {
                    // in front of the first keyframe: skip to it
                    seg.inFrame = gop.keyframes.first();
                } else {
                    seg.inFrame = kf;
                }
            }
            if (seg.inFrame >= seg.outFrame)
                continue; // whole GOP got absorbed by a surrounding cut
        }

        seg.inDelta = seg.inFrame - qMax<qint64>(0, c.inFrame);
        result.segments.append(seg);
    }

    result.ok = !result.segments.isEmpty() || cuts.isEmpty();
    return result;
}