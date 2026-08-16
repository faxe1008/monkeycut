#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <functional>

#include "av/AvProbe.h"
#include "av/Player.h"

#include "fixtures.h"

namespace
{
bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 15000)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        if (pred())
            return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return pred();
}

void spin(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}
}

class TPlayer : public QObject
{
    Q_OBJECT

private slots:
    void test_openPlayStepSeekClose()
    {
        if (!FfmpegFixture::available())
            QSKIP("ffmpeg binary not available");

        QTemporaryDir tmp;
        QDir tmpDir(tmp.path());
        const QString video = FfmpegFixture::createTestVideo(
            tmpDir,
            QStringLiteral("player.ts"),
            {
                QStringLiteral("-c:v"), QStringLiteral("mpeg2video"),
                QStringLiteral("-g"), QStringLiteral("25"),
                QStringLiteral("-c:a"), QStringLiteral("mp2"),
                QStringLiteral("-b:a"), QStringLiteral("128k"),
            });
        QVERIFY(!video.isEmpty());

        const MediaInfo info = AvProbe().probe(video);
        QVERIFY(info.ok);

        Player p;
        QVERIFY(!p.isOpen());
        QVERIFY(p.open(video, info));
        QVERIFY(p.isOpen());
        QCOMPARE(p.totalFrames(), qint64(125));
        QCOMPARE(p.fps().num, 25);
        QCOMPARE(p.fps().den, 1);
        QCOMPARE(p.state(), Player::State::Stopped);

        // initial exact seek to frame 0 must deliver a decoded image
        QSignalSpy frameSpy(&p, &Player::frameAvailable);
        QVERIFY(waitUntil([&] { return frameSpy.count() >= 1; }));
        const QImage img = frameSpy.first().at(0).value<QImage>();
        QVERIFY(!img.isNull());
        QCOMPARE(img.size(), QSize(320, 240));
        QCOMPARE(p.currentFrame(), qint64(0));

        // exact seek to a keyframe
        p.seekFrame(50, true);
        QVERIFY(waitUntil([&] { return p.currentFrame() == 50; }));
        QCOMPARE(p.state(), Player::State::Stopped);

        // frame stepping (pauses if playing, exact seeks)
        p.stepFrame(-1);
        QVERIFY(waitUntil([&] { return p.currentFrame() == 49; }));
        p.stepFrame(3);
        QVERIFY(waitUntil([&] { return p.currentFrame() == 52; }));

        // playback advances
        p.play();
        QCOMPARE(p.state(), Player::State::Playing);
        const qint64 start = p.currentFrame();
        QVERIFY(waitUntil([&] { return p.currentFrame() > start + 5; }));

        // pause freezes
        p.pause();
        QCOMPARE(p.state(), Player::State::Paused);
        spin(400);
        const qint64 f1 = p.currentFrame();
        spin(400);
        QCOMPARE(p.currentFrame(), f1);

        // close is idempotent
        p.close();
        QVERIFY(!p.isOpen());
        p.close();
        QVERIFY(!p.isOpen());
    }

    void test_openFailure()
    {
        Player p;
        MediaInfo info; // not ok, no streams
        QVERIFY(!p.open(QStringLiteral("/nonexistent/file.ts"), info));
        QVERIFY(!p.isOpen());
    }
};

QTEST_MAIN(TPlayer)

#include "t_player.moc"