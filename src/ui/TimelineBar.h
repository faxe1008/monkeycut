#pragma once

#include <QWidget>

#include "core/Cutlist.h"
#include "core/GopMap.h"

class TimelineBar : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineBar(QWidget* parent = nullptr);

    void setMedia(qint64 totalFrames, const Cutlist& cuts, const GopMap& gop = GopMap());
    void updateCuts(const Cutlist& cuts);
    void setPositionFrame(qint64 frame);

signals:
    void seekRequested(qint64 frame);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    int xForFrame(qint64 frame) const;
    qint64 frameAtX(int x) const;

    qint64 m_totalFrames = 0;
    Cutlist m_cuts;
    GopMap m_gop;
    qint64 m_pos = -1;
    bool m_dragging = false;
};