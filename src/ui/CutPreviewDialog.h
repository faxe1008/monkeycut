#pragma once

#include <QDialog>

#include "av/Player.h"
#include "core/MediaInfo.h"
#include "core/Cut.h"

class VideoView;

class CutPreviewDialog : public QDialog
{
    Q_OBJECT
public:
    CutPreviewDialog(const QString& path, const MediaInfo& info,
                     const QVector<Cut>& windows, QWidget* parent = nullptr);
    ~CutPreviewDialog() override;

private:
    void advancePreview(qint64 frame);

    Player* m_player = nullptr;
    VideoView* m_view = nullptr;
    QString m_path;
    QVector<Cut> m_windows;
    int m_windowIndex = 0;
    bool m_waitingForSeek = false;
};
