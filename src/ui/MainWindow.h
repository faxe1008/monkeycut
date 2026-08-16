#pragma once

#include <QMainWindow>

#include "av/Player.h"
#include "core/GopMap.h"
#include "core/MediaInfo.h"

class Player;
class VideoView;
class QSlider;
class QLabel;
class QToolButton;
class QComboBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openVideo(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

signals:
    void gopReady(const QString& path, const GopMap& map);

private slots:
    void openFile();
    void togglePlayPause();
    void updatePosition(qint64 frame);
    void updateState(Player::State state);
    void updateSpeed();
    void onGopReady(const QString& path, const GopMap& map);

private:
    void buildUi();
    void updateStatusInfo();
    void showError(const QString& message);

    Player* m_player = nullptr;
    VideoView* m_view = nullptr;
    QSlider* m_slider = nullptr;
    QLabel* m_tcLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
    QToolButton* m_playBtn = nullptr;
    QToolButton* m_stepBackBtn = nullptr;
    QToolButton* m_stepFwdBtn = nullptr;
    QComboBox* m_speedBox = nullptr;

    QString m_path;
    MediaInfo m_info;
    GopMap m_gop;
    bool m_sliderBusy = false;
};