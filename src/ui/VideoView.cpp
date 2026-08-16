#include "VideoView.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

VideoView::VideoView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(480, 270);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(true);
    QPalette pal(palette());
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
}

void VideoView::setFrame(const QImage& image)
{
    if (image.isNull())
        return;
    m_frame = image;
    update();
}

void VideoView::clear()
{
    m_frame = QImage();
    update();
}

void VideoView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (!m_frame.isNull()) {
        const QSize scaled = m_frame.size()
            .scaled(size(), Qt::KeepAspectRatio);
        const int x = (width() - scaled.width()) / 2;
        const int y = (height() - scaled.height()) / 2;
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(QRect(QPoint(x, y), scaled), m_frame, m_frame.rect());
    } else {
        p.setPen(QColor(120, 120, 120));
        QFont f = p.font();
        f.setPointSize(14);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter,
                   QApplication::translate("VideoView", "Kein Video"));
    }
}