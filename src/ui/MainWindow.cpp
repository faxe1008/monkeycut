#include "ui/MainWindow.h"

#include "av/AvProbe.h"
#include "av/GopScanner.h"
#include "av/Player.h"
#include "core/TimeCode.h"
#include "ui/VideoView.h"

#include <QApplication>
#include <QComboBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QShortcut>
#include <QSlider>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <thread>

namespace
{
bool dragHasFile(const QMimeData* d)
{
    return d->hasUrls() && d->urls().size() == 1 && d->urls().first().isLocalFile();
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("MonkeyCut"));
    resize(1100, 720);
    setAcceptDrops(true);
    buildUi();
}

void MainWindow::buildUi()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* openAct = fileMenu->addAction(tr("&Open video…"), this, &MainWindow::openFile);
    openAct->setShortcut(QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), this, &QWidget::close);

    auto* central = new QWidget(this);
    auto* lay = new QVBoxLayout(central);
    lay->setContentsMargins(4, 4, 4, 4);

    m_view = new VideoView(central);
    lay->addWidget(m_view, 1);

    auto* transport = new QWidget(central);
    auto* tlay = new QVBoxLayout(transport);
    tlay->setContentsMargins(0, 0, 0, 0);
    tlay->setSpacing(4);

    auto* btnRow = new QHBoxLayout;
    m_stepBackBtn = new QToolButton(transport);
    m_stepBackBtn->setText(QStringLiteral("◀︎|"));
    m_stepBackBtn->setToolTip(tr("One frame back (←)"));
    m_playBtn = new QToolButton(transport);
    m_playBtn->setText(QStringLiteral("▶"));
    m_playBtn->setToolTip(tr("Play / pause (Space)"));
    m_playBtn->setMinimumWidth(48);
    m_stepFwdBtn = new QToolButton(transport);
    m_stepFwdBtn->setText(QStringLiteral("|▶︎"));
    m_stepFwdBtn->setToolTip(tr("One frame forward (→)"));

    m_speedBox = new QComboBox(transport);
    m_speedBox->addItems({"0.25×", "0.5×", "1×", "1.5×", "2×", "4×"});
    m_speedBox->setItemText(2, tr("1 normal (1×)"));
    m_speedBox->setCurrentIndex(2);
    m_speedBox->setToolTip(tr("Playback speed"));

    btnRow->addWidget(m_stepBackBtn);
    btnRow->addWidget(m_playBtn);
    btnRow->addWidget(m_stepFwdBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(m_speedBox);
    tlay->addLayout(btnRow);

    m_slider = new QSlider(Qt::Horizontal, transport);
    m_slider->setRange(0, 0);
    m_slider->setPageStep(50);
    tlay->addWidget(m_slider);

    auto* infoRow = new QHBoxLayout;
    m_tcLabel = new QLabel(QStringLiteral("00:00:00:00"), transport);
    QFont mono(QStringLiteral("monospace"));
    mono.setStyleHint(QFont::Monospace);
    m_tcLabel->setFont(mono);
    m_infoLabel = new QLabel(transport);
    infoRow->addWidget(m_tcLabel);
    infoRow->addStretch(1);
    infoRow->addWidget(m_infoLabel);
    tlay->addLayout(infoRow);

    lay->addWidget(transport);
    setCentralWidget(central);

    statusBar()->showMessage(tr("Ready — please open a video file (or drag & drop)."));

    m_player = new Player(this);

    connect(this, &MainWindow::gopReady, this, &MainWindow::onGopReady);
    connect(m_player, &Player::frameAvailable, m_view, &VideoView::setFrame);
    connect(m_player, &Player::positionChanged, this, &MainWindow::updatePosition);
    connect(m_player, &Player::stateChanged, this, &MainWindow::updateState);
    connect(m_player, &Player::errorOccurred, this, [this](const QString& msg) {
        showError(msg);
    });
    connect(m_playBtn, &QToolButton::clicked, this, &MainWindow::togglePlayPause);
    connect(m_stepBackBtn, &QToolButton::clicked,
            [this] { m_player->stepFrame(-1); });
    connect(m_stepFwdBtn, &QToolButton::clicked,
            [this] { m_player->stepFrame(1); });
    connect(m_slider, &QSlider::sliderReleased, this, [this] {
        if (!m_sliderBusy)
            m_player->seekFrame(m_slider->value(), false);
    });
    connect(m_speedBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateSpeed);

    auto* playPause = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(playPause, &QShortcut::activated, this, &MainWindow::togglePlayPause);
    auto* back = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(back, &QShortcut::activated, [this] { m_player->stepFrame(-1); });
    auto* fwd = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(fwd, &QShortcut::activated, [this] { m_player->stepFrame(1); });
    auto* home = new QShortcut(QKeySequence(Qt::Key_Home), this);
    connect(home, &QShortcut::activated, [this] { m_player->seekFrame(0, false); });
    auto* end = new QShortcut(QKeySequence(Qt::Key_End), this);
    connect(end, &QShortcut::activated,
            [this] { m_player->seekFrame(m_player->totalFrames() - 1, false); });
}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open video"), QString(),
        tr("Video files (*.ts *.m2ts *.mts *.mpg *.mpeg *.mkv *.avi *.mp4 *.mov *.vob "
           "*.wmv *.flv *.ts);;All files (*)"));
    if (path.isEmpty())
        return;
    openVideo(path);
}

void MainWindow::openVideo(const QString& path)
{
    statusBar()->showMessage(tr("Reading file…"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const MediaInfo info = AvProbe().probe(path);
    QApplication::restoreOverrideCursor();

    if (!info.ok) {
        showError(tr("Cannot read “%1”:%2%3").arg(path, info.error));
        return;
    }

    m_info = info;
    m_gop = GopMap();
    m_path = path;

    if (m_player->open(path, info)) {
        const qint64 total = m_player->totalFrames();
        m_sliderBusy = true;
        m_slider->setRange(0, int(total > 0 ? total - 1 : 0));
        m_slider->setValue(0);
        m_sliderBusy = false;

        setWindowTitle(QCoreApplication::translate(
            "MainWindow", "%1 — MonkeyCut").arg(QFileInfo(path).fileName()));
        statusBar()->showMessage(QCoreApplication::translate(
            "MainWindow", "Opened %1 (%2)")
                                     .arg(QFileInfo(path).fileName(),
                                          QFileInfo(path).size() > (1 << 20)
                                              ? QStringLiteral("%1 MB")
                                                      .arg(QFileInfo(path).size() >> 20)
                                              : QStringLiteral("%1 KB")
                                                      .arg(QFileInfo(path).size() >> 10)),
                                 5000);
    } else {
        m_player->close();
        showError(tr("The video in “%1” could not be opened.").arg(path));
        return;
    }

    updateStatusInfo();
    updatePosition(0);

    // GOP scan in the background (keyframe index for cut snapping)
    const QString scanPath = path;
    std::thread([this, scanPath] {
        const GopMap map = scanGopMap(scanPath);
        emit gopReady(scanPath, map);
    }).detach();
}

void MainWindow::updateStatusInfo()
{
    if (!m_info.ok)
        return;
    const MediaStreamInfo* v = m_info.firstVideo();
    const MediaStreamInfo* a = m_info.firstAudio();

    QString info;
    info += tr("%1 frames · %2 s · %3 fps")
        .arg(m_info.totalFrames)
        .arg(m_info.durationSec, 0, 'f', 2)
        .arg(v ? v->fps.value() : 0.0, 0, 'f', 3);
    if (m_gop.valid)
        info += tr(" · %1 keyframes").arg(m_gop.keyframes.size());
    if (a)
        info += tr(" · %1 kHz/%2ch audio").arg(a->sampleRate / 1000).arg(a->channels);
    m_infoLabel->setText(info);

    QStringList warnings;
    if (m_info.videoStreamCount() > 1)
        warnings << tr("Multiple video streams – using the first one");
    if (m_info.audioStreamCount() > 1)
        warnings << tr("Multiple audio streams – using the first one");
    if (v && v->vfrSuspected)
        warnings << tr("Variable frame rate detected – cut accuracy is limited");
    if (!m_info.seekable)
        warnings << tr("File is not seekable – scrubbing may be slow");
    if (!warnings.isEmpty())
        statusBar()->showMessage(tr("⚠ %1").arg(warnings.join(QStringLiteral(" · "))),
                                 15000);
}

void MainWindow::showError(const QString& message)
{
    statusBar()->showMessage(message, 15000);
    QMessageBox::critical(this, tr("MonkeyCut"), message);
}

void MainWindow::togglePlayPause()
{
    if (!m_player->isOpen())
        return;
    if (m_player->state() == Player::State::Playing)
        m_player->pause();
    else
        m_player->play();
}

void MainWindow::updatePosition(qint64 frame)
{
    if (m_player->isOpen() && m_player->fps().isValid())
        m_tcLabel->setText(TimeCode(frame, m_player->fps()).toString());
    if (!m_sliderBusy) {
        m_sliderBusy = true;
        m_slider->setValue(int(frame));
        m_sliderBusy = false;
    }
}

void MainWindow::updateState(Player::State state)
{
    m_playBtn->setText(state == Player::State::Playing ? QStringLiteral("❚❚")
                                                       : QStringLiteral("▶"));
}

void MainWindow::updateSpeed()
{
    static const double speeds[] = {0.25, 0.5, 1.0, 1.5, 2.0, 4.0};
    m_player->setSpeed(speeds[m_speedBox->currentIndex()]);
}

void MainWindow::onGopReady(const QString& path, const GopMap& map)
{
    if (path != m_path)
        return;
    m_gop = map;
    updateStatusInfo();
    if (map.valid)
        statusBar()->showMessage(
            tr("Keyframe index ready (%1 keyframes).").arg(map.keyframes.size()), 5000);
    else
        statusBar()->showMessage(tr("Keyframe index unavailable – cut snapping off."),
                                 5000);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (dragHasFile(event->mimeData()))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!dragHasFile(event->mimeData()))
        return;
    openVideo(event->mimeData()->urls().first().toLocalFile());
    event->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_player->close();
    event->accept();
}