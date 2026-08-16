#pragma once

#include <QImage>
#include <QWidget>

class VideoView : public QWidget
{
    Q_OBJECT
public:
    explicit VideoView(QWidget* parent = nullptr);

    bool hasFrame() const
    {
        return !m_frame.isNull();
    }

public slots:
    void setFrame(const QImage& image);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_frame;
};