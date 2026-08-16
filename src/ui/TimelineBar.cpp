#include "TimelineBar.h"

#include <QMouseEvent>
#include <QPainter>

TimelineBar::TimelineBar(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(36);
    setMaximumHeight(56);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Klicken/Vorziehen zum Springen"));
}

void TimelineBar::setMedia(qint64 totalFrames, const Cutlist& cuts, const GopMap& gop)
{
    m_totalFrames = totalFrames;
    m_cuts = cuts;
    m_gop = gop;
    update();
}

void TimelineBar::updateCuts(const Cutlist& cuts)
{
    m_cuts = cuts;
    update();
}

void TimelineBar::setPositionFrame(qint64 frame)
{
    if (frame != m_pos) {
        m_pos = frame;
        update();
    }
}

int TimelineBar::xForFrame(qint64 frame) const
{
    if (m_totalFrames <= 1)
        return 0;
    return int(double(frame) / double(m_totalFrames - 1) * (width()));
}

qint64 TimelineBar::frameAtX(int x) const
{
    if (m_totalFrames <= 1)
        return 0;
    const qint64 f = qint64(double(qBound(0, x, width())) / double(width())
                             * (m_totalFrames - 1));
    return qBound<qint64>(0, f, m_totalFrames - 1);
}

void TimelineBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(40, 40, 40));
    if (m_totalFrames <= 1) {
        p.setPen(QColor(120, 120, 120));
        p.drawText(rect(), Qt::AlignCenter, tr("Kein Video geöffnet"));
        return;
    }

    const QRect bar(0, 10, width(), height() - 20);

    // kept ranges
    p.fillRect(bar, QColor(60, 60, 60));
    for (const Cut& c : m_cuts.cuts())
        p.fillRect(QRect(xForFrame(c.inFrame), bar.top(),
                         qMax(1, xForFrame(c.outFrame) - xForFrame(c.inFrame)),
                         bar.height()),
                   QColor(46, 125, 50));

    // keyframe ticks
    if (m_gop.valid)
        for (qint64 kf : m_gop.keyframes) {
            const int x = xForFrame(kf);
            p.fillRect(x, bar.bottom() - 6, 1, 6, QColor(90, 90, 90));
        }

    QPen pen(QColor(90, 90, 90));
    p.setPen(pen);
    p.drawRect(bar);

    // playhead
    if (m_pos >= 0) {
        const int x = xForFrame(m_pos);
        p.fillRect(x, 4, 2, height() - 8, QColor(220, 60, 50));
    }
}

void TimelineBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_totalFrames > 1) {
        m_dragging = true;
        emit seekRequested(frameAtX(event->position().toPoint().x()));
    }
}

void TimelineBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging)
        emit seekRequested(frameAtX(event->position().toPoint().x()));
}

void TimelineBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
}