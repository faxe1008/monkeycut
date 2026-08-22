#include "CutPreviewDialog.h"

#include "ui/VideoView.h"

#include <QFile>
#include <QDialogButtonBox>
#include <QVBoxLayout>

CutPreviewDialog::CutPreviewDialog(const QString& path, const MediaInfo& info,
                                   const QVector<Cut>& windows, QWidget* parent)
    : QDialog(parent)
    , m_path(path)
    , m_windows(windows)
{
    setWindowTitle(tr("Cut preview"));
    resize(900, 600);

    auto* layout = new QVBoxLayout(this);
    m_view = new VideoView(this);
    layout->addWidget(m_view, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    m_player = new Player(this);
    connect(m_player, &Player::frameAvailable, m_view, &VideoView::setFrame);
    connect(m_player, &Player::frameDisplayed, this, &CutPreviewDialog::advancePreview);
    connect(m_player, &Player::errorOccurred, this, [this](const QString& message) {
        setWindowTitle(tr("Cut preview: %1").arg(message));
    });
    if (m_player->open(m_path, info)) {
        if (m_windows.isEmpty()) {
            // No join points to preview (e.g. a single keep-segment) -
            // just play the whole exported file.
            m_player->play();
        } else {
            m_waitingForSeek = true;
            m_player->seekFrame(m_windows.first().inFrame, true);
        }
    }
}

void CutPreviewDialog::advancePreview(qint64 frame)
{
    if (m_windowIndex >= m_windows.size())
        return;
    const Cut window = m_windows[m_windowIndex];
    if (m_waitingForSeek) {
        if (frame < window.inFrame)
            return;
        m_waitingForSeek = false;
        m_player->play();
        return;
    }
    if (frame + 1 < window.outFrame)
        return;

    m_player->pause();
    ++m_windowIndex;
    if (m_windowIndex >= m_windows.size()) {
        setWindowTitle(tr("Cut preview (finished)"));
        return;
    }
    m_waitingForSeek = true;
    m_player->seekFrame(m_windows[m_windowIndex].inFrame, true);
}

CutPreviewDialog::~CutPreviewDialog()
{
    if (m_player)
        m_player->close();
    QFile::remove(m_path);
}
