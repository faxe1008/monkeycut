#include "Cutlist.h"

#include <algorithm>

Cutlist::Cutlist(qint64 totalFrames)
    : m_totalFrames(totalFrames)
{
}

void Cutlist::setTotalFrames(qint64 frames)
{
    m_totalFrames = frames;
    normalize();
}

void Cutlist::setCuts(QVector<Cut> cuts)
{
    m_cuts = cuts;
    normalize();
}

bool Cutlist::addCut(qint64 inFrame, qint64 outFrame)
{
    if (outFrame <= inFrame)
        return false;
    inFrame = qMax<qint64>(0, inFrame);
    if (m_totalFrames > 0)
        outFrame = qMin<qint64>(outFrame, m_totalFrames);
    if (outFrame <= inFrame)
        return false;

    for (const Cut& c : m_cuts)
        if (inFrame < c.outFrame && c.inFrame < outFrame)
            return false;

    m_cuts.append(Cut(inFrame, outFrame));
    std::sort(m_cuts.begin(), m_cuts.end(),
              [](const Cut& a, const Cut& b) { return a.inFrame < b.inFrame; });
    return true;
}

bool Cutlist::removeAt(int index)
{
    if (index < 0 || index >= m_cuts.size())
        return false;
    m_cuts.removeAt(index);
    return true;
}

bool Cutlist::removeOverlap(const Cut& cut)
{
    bool removed = false;
    for (int i = m_cuts.size() - 1; i >= 0; --i)
        if (m_cuts[i].inFrame < cut.outFrame && cut.inFrame < m_cuts[i].outFrame) {
            m_cuts.removeAt(i);
            removed = true;
        }
    return removed;
}

void Cutlist::clear()
{
    m_cuts.clear();
}

qint64 Cutlist::keepFrameCount() const
{
    qint64 n = 0;
    for (const Cut& c : m_cuts)
        n += c.frames();
    return n;
}

qint64 Cutlist::cutFrameCount() const
{
    qint64 n = m_totalFrames;
    for (const Cut& c : m_cuts)
        n -= c.frames();
    return n;
}

bool Cutlist::containsFrame(qint64 frame) const
{
    for (const Cut& c : m_cuts)
        if (frame >= c.inFrame && frame < c.outFrame)
            return true;
    return false;
}

int Cutlist::indexOf(const Cut& cut) const
{
    return m_cuts.indexOf(cut);
}

void Cutlist::normalize()
{
    std::sort(m_cuts.begin(), m_cuts.end(),
              [](const Cut& a, const Cut& b) { return a.inFrame < b.inFrame; });
    QVector<Cut> out;
    qint64 last = 0;
    for (const Cut& c : m_cuts) {
        const Cut clipped(qMax<qint64>(0, c.inFrame),
                          qMin<qint64>(m_totalFrames > 0 ? m_totalFrames : c.outFrame,
                                       c.outFrame));
        if (clipped.frames() > 0 && clipped.inFrame >= last) {
            out.append(clipped);
            last = clipped.outFrame;
        }
    }
    m_cuts = out;
}