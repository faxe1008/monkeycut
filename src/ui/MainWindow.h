#pragma once

#include <QMainWindow>

#include "av/Player.h"
#include "core/Cutlist.h"
#include "core/GopMap.h"
#include "core/MediaInfo.h"

class QSlider;
class QLabel;
class QToolButton;
class QComboBox;
class QTableWidget;
class QAction;
class Player;
class VideoView;
class TimelineBar;
class CuttingEngine;
class CutlistAtDialog;
struct CulFile;

class MainWindow : public QMainWindow
{
    Q_OBJECT
signals:
    void gopReady(const QString& path, const GopMap& map);

public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openVideo(const QString& path);
    void loadCulFile(const QString& path);
    void loadCulFromData(const QByteArray& data, const QString& name);
    void loadProjectFile(const QString& path);
    void seekToFrame(qint64 frame);
    bool startExport(const QString& outputPath);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openFile();
    void searchCutlistAt();
    void showSettings();
    void openProject();
    void saveCutlist();
    void saveProject();
    void togglePlayPause();
    void markIn();
    void markOut();
    void deleteSelectedCut();
    void clearCuts();
    void exportVideo();
    void updatePosition(qint64 frame);
    void updateState(Player::State state);
    void updateSpeed();
    void onGopReady(const QString& path, const GopMap& map);
    void onCutTableActivated(int row);

private:
    void buildUi();
    void updateStatusInfo();
    void refreshCutTable();
    void applyCul(const CulFile& cul, const QString& sourceName);
    void showError(const QString& message);
    void showInfo(const QString& message);

    Player* m_player = nullptr;
    VideoView* m_view = nullptr;
    QSlider* m_slider = nullptr;
    QLabel* m_tcLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
    QToolButton* m_playBtn = nullptr;
    QToolButton* m_stepBackBtn = nullptr;
    QToolButton* m_stepFwdBtn = nullptr;
    QComboBox* m_speedBox = nullptr;
    TimelineBar* m_timeline = nullptr;
    QTableWidget* m_cutTable = nullptr;
    CuttingEngine* m_engine = nullptr;

    QString m_path;
    MediaInfo m_info;
    GopMap m_gop;
    Cutlist m_cutlist;
    qint64 m_markIn = -1;
    bool m_sliderBusy = false;
};