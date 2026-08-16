#include "ui/MainWindow.h"

#include "av/AvProbe.h"
#include "av/GopScanner.h"
#include "av/Player.h"
#include "core/CutPlanner.h"
#include "core/TimeCode.h"
#include "cut/CuttingEngine.h"
#include "cut/CulFile.h"
#include "cut/Project.h"
#include "ui/CutlistAtDialog.h"
#include "ui/TimelineBar.h"
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
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QShortcut>
#include <QSlider>
#include <QStatusBar>
#include <QTableWidget>
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
    resize(1150, 780);
    setAcceptDrops(true);
    buildUi();
}

void MainWindow::buildUi()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* openAct = fileMenu->addAction(tr("&Open video…"), this, &MainWindow::openFile);
    openAct->setShortcut(QKeySequence::Open);
    auto* openProjAct = fileMenu->addAction(tr("Open &project…"), this,
                                            &MainWindow::openProject);
    openProjAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    fileMenu->addSeparator();
    auto* saveCulAct = fileMenu->addAction(tr("Save &cutlist…"), this,
                                           &MainWindow::saveCutlist);
    saveCulAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    auto* saveProjAct = fileMenu->addAction(tr("Save &project…"), this,
                                            &MainWindow::saveProject);
    saveProjAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    fileMenu->addSeparator();
    auto* exportAct = fileMenu->addAction(tr("&Export video…"), this,
                                          &MainWindow::exportVideo);
    exportAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), this, &QWidget::close);

    auto* clMenu = menuBar()->addMenu(tr("&cutlist.at"));
    auto* clSearchAct =
        clMenu->addAction(tr("Search for &cutlists…"), this,
                          &MainWindow::searchCutlistAt);
    Q_UNUSED(clSearchAct)

    auto* central = new QWidget(this);
    auto* lay = new QVBoxLayout(central);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

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

    auto* markInBtn = new QToolButton(transport);
    markInBtn->setText(QStringLiteral("I"));
    {
        QFont f = markInBtn->font();
        f.setPointSize(11);
        f.setBold(true);
        markInBtn->setFont(f);
    }
    markInBtn->setToolTip(tr("Mark keep-start (I)"));
    auto* markOutBtn = new QToolButton(transport);
    markOutBtn->setText(QStringLiteral("O"));
    {
        QFont f = markOutBtn->font();
        f.setPointSize(11);
        f.setBold(true);
        markOutBtn->setFont(f);
    }
    markOutBtn->setToolTip(tr("Mark keep-end (O)"));
    auto* delCutBtn = new QToolButton(transport);
    delCutBtn->setText(QStringLiteral("Del"));
    delCutBtn->setToolTip(tr("Remove selected keep-range (Del)"));
    auto* clearCutBtn = new QToolButton(transport);
    clearCutBtn->setText(tr("Clear"));
    clearCutBtn->setToolTip(tr("Remove all keep-ranges"));

    m_speedBox = new QComboBox(transport);
    m_speedBox->addItems({"0.25×", "0.5×", "1×", "1.5×", "2×", "4×"});
    m_speedBox->setItemText(2, tr("1 normal (1×)"));
    m_speedBox->setCurrentIndex(2);
    m_speedBox->setToolTip(tr("Playback speed"));

    btnRow->addWidget(m_stepBackBtn);
    btnRow->addWidget(m_playBtn);
    btnRow->addWidget(m_stepFwdBtn);
    btnRow->addSpacing(12);
    btnRow->addWidget(markInBtn);
    btnRow->addWidget(markOutBtn);
    btnRow->addWidget(delCutBtn);
    btnRow->addWidget(clearCutBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(m_speedBox);
    tlay->addLayout(btnRow);

    m_timeline = new TimelineBar(transport);
    tlay->addWidget(m_timeline);

    m_slider = new QSlider(Qt::Horizontal, transport);
    m_slider->setRange(0, 0);
    m_slider->setPageStep(50);
    tlay->addWidget(m_slider);

    auto* infoRow = new QHBoxLayout;
    m_tcLabel = new QLabel(QStringLiteral("00:00:00:00"), transport);
    QFont mono;
    mono.setStyleHint(QFont::Monospace);
    m_tcLabel->setFont(mono);
    m_infoLabel = new QLabel(transport);
    infoRow->addWidget(m_tcLabel);
    infoRow->addStretch(1);
    infoRow->addWidget(m_infoLabel);
    tlay->addLayout(infoRow);

    lay->addWidget(transport);

    m_cutTable = new QTableWidget(central);
    m_cutTable->setColumnCount(5);
    m_cutTable->setHorizontalHeaderLabels(
        {tr("#"), tr("In"), tr("Out"), tr("Duration"), tr("Δ keyframe")});
    m_cutTable->verticalHeader()->setVisible(false);
    m_cutTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_cutTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cutTable->setRowCount(0);
    m_cutTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_cutTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    lay->addWidget(m_cutTable, 1);

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
    connect(m_stepFwdBtn, &QToolButton::clicked, [this] { m_player->stepFrame(1); });
    connect(markInBtn, &QToolButton::clicked, this, &MainWindow::markIn);
    connect(markOutBtn, &QToolButton::clicked, this, &MainWindow::markOut);
    connect(delCutBtn, &QToolButton::clicked, this, &MainWindow::deleteSelectedCut);
    connect(clearCutBtn, &QToolButton::clicked, this, &MainWindow::clearCuts);
    connect(m_slider, &QSlider::sliderReleased, this, [this] {
        if (!m_sliderBusy)
            m_player->seekFrame(m_slider->value(), false);
    });
    connect(m_speedBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateSpeed);
    connect(m_timeline, &TimelineBar::seekRequested, this, [this](qint64 frame) {
        m_player->seekFrame(frame, false);
    });
    connect(m_cutTable, &QTableWidget::cellActivated, this,
            [this](int row, int) { onCutTableActivated(row); });

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
    auto* markInKey = new QShortcut(QKeySequence(Qt::Key_I), this);
    connect(markInKey, &QShortcut::activated, this, &MainWindow::markIn);
    auto* markOutKey = new QShortcut(QKeySequence(Qt::Key_O), this);
    connect(markOutKey, &QShortcut::activated, this, &MainWindow::markOut);
    auto* delKey = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    connect(delKey, &QShortcut::activated, this, &MainWindow::deleteSelectedCut);
}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open video"), QString(),
        tr("Video files (*.ts *.m2ts *.mts *.mpg *.mpeg *.mkv *.avi *.mp4 *.mov *.vob "
           "*.wmv *.flv);;All files (*)"));
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

    if (!m_player->open(path, info)) {
        m_player->close();
        showError(tr("The video in “%1” could not be opened.").arg(path));
        return;
    }

    const qint64 total = m_player->totalFrames();
    m_cutlist = Cutlist(total);
    m_markIn = -1;
    m_sliderBusy = true;
    m_slider->setRange(0, int(total > 0 ? total - 1 : 0));
    m_slider->setValue(0);
    m_sliderBusy = false;
    m_timeline->setMedia(total, m_cutlist, GopMap());
    refreshCutTable();

    setWindowTitle(QCoreApplication::translate(
                        "MainWindow", "%1 — MonkeyCut").arg(QFileInfo(path).fileName()));
    updateStatusInfo();
    updatePosition(0);

    const QString scanPath = path;
    std::thread([this, scanPath] {
        const GopMap map = scanGopMap(scanPath);
        emit gopReady(scanPath, map);
    }).detach();
}

static Cutlist cutlistFromCuts(const QVector<Cut>& cuts, qint64 total)
{
    Cutlist l(total);
    for (const Cut& c : cuts)
        l.addCut(c.inFrame, c.outFrame);
    return l;
}

void MainWindow::seekToFrame(qint64 frame)
{
    if (m_player->isOpen())
        m_player->seekFrame(frame, true);
}

void MainWindow::loadCulFile(const QString& path)
{
    CulFile cul;
    QString err;
    if (!loadCul(path, &cul, &err)) {
        showError(err);
        return;
    }
    applyCul(cul, QFileInfo(path).fileName());
}

void MainWindow::loadCulFromData(const QByteArray& data, const QString& name)
{
    applyCul(parseCul(QString::fromUtf8(data)), name);
}

void MainWindow::applyCul(const CulFile& cul, const QString& sourceName)
{
    if (!m_player->isOpen()) {
        showInfo(tr("Cutlist “%1”: no video open yet – open a video now.")
                     .arg(sourceName));
        return;
    }
    Cutlist l = culToCutlist(cul);
    l.setTotalFrames(m_player->totalFrames());
    if (l.cuts().isEmpty()) {
        showError(tr("No cut ranges found in “%1”.").arg(sourceName));
        return;
    }
    m_cutlist = l;
    m_markIn = -1;
    refreshCutTable();
    m_timeline->updateCuts(m_cutlist);
    showInfo(tr("Cutlist loaded: %1 keep-ranges from “%2” (replacing current).")
                 .arg(l.cuts().size())
                 .arg(sourceName));
}

void MainWindow::searchCutlistAt()
{
    CutlistAtDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted && !dlg.culData().isEmpty())
        loadCulFromData(dlg.culData(), dlg.culName());
}

void MainWindow::loadProjectFile(const QString& path)
{
    MonkeyProject p;
    QString err;
    if (!loadProject(path, &p, &err)) {
        showError(err);
        return;
    }
    m_cutlist = cutlistFromCuts(p.cuts, p.totalFrames);

    if (QFileInfo::exists(p.filePath)) {
        openVideo(p.filePath); // resets m_cutlist
        m_cutlist = cutlistFromCuts(p.cuts, p.totalFrames);
        refreshCutTable();
        m_timeline->updateCuts(m_cutlist);
    } else {
        showError(tr("Project “%1” references missing video “%2” – cut ranges "
                     "loaded, please open the video.")
                      .arg(QFileInfo(path).fileName(), p.filePath));
    }
}

void MainWindow::saveCutlist()
{
    if (!m_player->isOpen())
        return;
    if (m_cutlist.cuts().isEmpty()) {
        showInfo(tr("Nothing to save – no cut ranges yet."));
        return;
    }
    const QString suggested = QFileInfo(m_path).baseName() + QStringLiteral(".cul");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save cutlist"), suggested, tr("Cutlist (*.cul)"));
    if (path.isEmpty())
        return;
    const CulFile cul =
        cutlistToCul(m_cutlist, m_player->fps(), QFileInfo(m_path).fileName(),
                     QString(), QString());
    QString err;
    if (saveCul(path, cul, &err))
        showInfo(tr("Cutlist saved: %1").arg(path));
    else
        showError(err);
}

void MainWindow::saveProject()
{
    if (!m_player->isOpen())
        return;
    const QString suggested = QFileInfo(m_path).baseName() + QStringLiteral(".mproject");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save project"), suggested, tr("MonkeyCut project (*.mproject)"));
    if (path.isEmpty())
        return;
    MonkeyProject p;
    p.filePath = m_path;
    p.fps = m_player->fps();
    p.totalFrames = m_player->totalFrames();
    p.durationSec = m_info.durationSec;
    p.cuts = m_cutlist.cuts();
    QString err;
    if (::saveProject(path, p, &err))
        showInfo(tr("Project saved: %1").arg(path));
    else
        showError(err);
}

void MainWindow::openProject()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open project"), QString(),
        tr("MonkeyCut project (*.mproject);;All files (*)"));
    if (path.isEmpty())
        return;
    loadProjectFile(path);
}

void MainWindow::exportVideo()
{
    if (!m_player->isOpen())
        return;
    if (m_cutlist.cuts().isEmpty()) {
        showInfo(tr("Nothing to export – no cut ranges yet."));
        return;
    }
    const QFileInfo fi(m_path);
    const QString suggested =
        fi.baseName() + QStringLiteral("_cut.") + fi.suffix();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export video"), suggested,
        tr("Video (%1);;All files (*)").arg(QStringLiteral("*.") + fi.suffix()));
    if (path.isEmpty())
        return;
    startExport(path);
}

bool MainWindow::startExport(const QString& outputPath)
{
    if (!m_player->isOpen())
        return false;
    if (m_cutlist.cuts().isEmpty())
        return false;
    if (m_engine) {
        showInfo(tr("An export is already running."));
        return false;
    }
    const PlanResult plan =
        planCuts(m_cutlist.cuts(), m_gop, m_player->totalFrames());
    if (!plan.ok || plan.segments.isEmpty()) {
        showError(tr("Nothing to export for the selected cut ranges."));
        return false;
    }
    m_player->pause();
    m_engine = new CuttingEngine(this);
    connect(m_engine, &CuttingEngine::progress, this,
            [this](qint64 done, qint64 total) {
                statusBar()->showMessage(
                    tr("Exporting… %1 / %2").arg(done).arg(total));
            });
    connect(m_engine, &CuttingEngine::finished, this,
            [this, outputPath](bool ok, const QString& message) {
                statusBar()->clearMessage();
                m_engine = nullptr;
                if (ok)
                    showInfo(tr("Export complete: %1").arg(outputPath));
                else
                    showError(tr("Export failed: %1").arg(message));
            });
    return m_engine->start(m_path, outputPath, plan.segments, m_player->fps());
}

void MainWindow::markIn()
{
    if (!m_player->isOpen())
        return;
    m_markIn = m_player->currentFrame();
    const Fps fps = m_player->fps();
    showInfo(tr("I-point set at %1 (now set the O-point).")
                 .arg(TimeCode(m_markIn, fps).toString()));
}

void MainWindow::markOut()
{
    if (!m_player->isOpen())
        return;
    const qint64 out = m_player->currentFrame();
    const Fps fps = m_player->fps();
    if (m_markIn < 0) {
        showInfo(tr("No I-point set – press I first."));
        return;
    }
    if (out <= m_markIn) {
        showInfo(tr("The O-point must be after the I-point."));
        return;
    }
    if (!m_cutlist.addCut(m_markIn, out)) {
        if (!m_cutlist.removeOverlap(Cut(m_markIn, out))) {
            showInfo(tr("Range could not be added (overlap)."));
            return;
        }
        if (!m_cutlist.addCut(m_markIn, out)) {
            showInfo(tr("Range could not be added."));
            m_markIn = -1;
            return;
        }
    }
    const qint64 inFrame = m_markIn;
    m_markIn = -1;
    refreshCutTable();
    m_timeline->updateCuts(m_cutlist);
    showInfo(tr("Keep-range added: %1 – %2")
                 .arg(TimeCode(inFrame, fps).toString(), TimeCode(out, fps).toString()));
}

void MainWindow::deleteSelectedCut()
{
    const int row = m_cutTable->currentRow();
    if (row < 0 || row >= m_cutlist.cuts().size())
        return;
    m_cutlist.removeAt(row);
    refreshCutTable();
    m_timeline->updateCuts(m_cutlist);
    showInfo(tr("Keep-range removed."));
}

void MainWindow::clearCuts()
{
    m_cutlist.clear();
    m_markIn = -1;
    refreshCutTable();
    m_timeline->updateCuts(m_cutlist);
    showInfo(tr("All keep-ranges removed."));
}

void MainWindow::refreshCutTable()
{
    const Fps fps = m_player ? m_player->fps() : Fps(25, 1);
    m_cutTable->setRowCount(0);
    const bool aligned = m_gop.valid && !m_gop.keyframes.isEmpty();
    int i = 0;
    for (const Cut& c : m_cutlist.cuts()) {
        ++i;
        auto* num = new QTableWidgetItem(QString::number(i));
        auto* in = new QTableWidgetItem(TimeCode(c.inFrame, fps).toString());
        auto* out = new QTableWidgetItem(TimeCode(c.outFrame, fps).toString());
        auto* dur = new QTableWidgetItem(TimeCode(c.frames(), fps).toString());
        QString delta;
        if (aligned) {
            const qint64 eff = c.inFrame == 0
                ? (m_gop.keyframes.first() > 0 ? m_gop.keyframes.first() : 0)
                : m_gop.snapBack(c.inFrame);
            const qint64 d = (eff >= 0 ? eff : c.inFrame) - c.inFrame;
            delta = d == 0 ? QStringLiteral("0")
                           : QString::number(d) + QStringLiteral(" f");
        } else {
            delta = QStringLiteral("–");
        }
        auto* dlt = new QTableWidgetItem(delta);
        dlt->setForeground(delta.startsWith(QLatin1Char('-'))
                               ? QColor(230, 180, 60)
                               : Qt::gray);
        m_cutTable->insertRow(m_cutTable->rowCount());
        m_cutTable->setItem(m_cutTable->rowCount() - 1, 0, num);
        m_cutTable->setItem(m_cutTable->rowCount() - 1, 1, in);
        m_cutTable->setItem(m_cutTable->rowCount() - 1, 2, out);
        m_cutTable->setItem(m_cutTable->rowCount() - 1, 3, dur);
        m_cutTable->setItem(m_cutTable->rowCount() - 1, 4, dlt);
    }

    if (m_player && m_player->isOpen() && m_cutlist.totalFrames() > 0) {
        const double keepSec =
            double(m_cutlist.keepFrameCount()) / (fps.value() > 0 ? fps.value() : 25.0);
        const double cutSec =
            double(m_cutlist.cutFrameCount()) / (fps.value() > 0 ? fps.value() : 25.0);
        showInfo(tr("%1 keep-ranges · keep %2 s · cut %3 s")
                     .arg(m_cutlist.cuts().size())
                     .arg(QString::number(keepSec, 'f', 1))
                     .arg(QString::number(cutSec, 'f', 1)));
    }
}

void MainWindow::showError(const QString& message)
{
    statusBar()->showMessage(message, 15000);
    QMessageBox::critical(this, tr("MonkeyCut"), message);
}

void MainWindow::showInfo(const QString& message)
{
    statusBar()->showMessage(message, 8000);
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
    if (m_player->isOpen() && m_player->fps().isValid()) {
        m_tcLabel->setText(TimeCode(frame, m_player->fps()).toString());
        m_timeline->setPositionFrame(frame);
    }
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
    m_timeline->setMedia(m_player ? m_player->totalFrames() : 0, m_cutlist, m_gop);
    refreshCutTable();
    updateStatusInfo();
    if (map.valid)
        showInfo(tr("Keyframe index ready (%1 keyframes) – cut snapping active.")
                     .arg(map.keyframes.size()));
    else
        showInfo(tr("Keyframe index unavailable – cut snapping off."));
}

void MainWindow::onCutTableActivated(int row)
{
    if (row < 0 || row >= m_cutlist.cuts().size() || !m_player->isOpen())
        return;
    m_player->seekFrame(m_cutlist.cuts()[row].inFrame, true);
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

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (dragHasFile(event->mimeData()))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!dragHasFile(event->mimeData()))
        return;
    const QString p = event->mimeData()->urls().first().toLocalFile();
    const QString suffix = QFileInfo(p).suffix();
    if (suffix.compare(QStringLiteral("cul"), Qt::CaseInsensitive) == 0)
        loadCulFile(p);
    else if (suffix.compare(QStringLiteral("mproject"), Qt::CaseInsensitive) == 0)
        loadProjectFile(p);
    else
        openVideo(p);
    event->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_player->close();
    event->accept();
}