#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QSlider>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <functional>

#include "av/Player.h"
#include "ui/MainWindow.h"
#include "ui/VideoView.h"

#include "fixtures.h"

namespace
{
bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 20000)
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
}

class TUi : public QObject
{
    Q_OBJECT

private slots:
    void test_openStepPlayGop()
    {
        if (!FfmpegFixture::available())
            QSKIP("ffmpeg binary not available");

        QTemporaryDir tmp;
        QDir tmpDir(tmp.path());
        const QString video = FfmpegFixture::createTestVideo(
            tmpDir,
            QStringLiteral("ui.ts"),
            {
                QStringLiteral("-c:v"), QStringLiteral("mpeg2video"),
                QStringLiteral("-g"), QStringLiteral("25"),
                QStringLiteral("-c:a"), QStringLiteral("mp2"),
                QStringLiteral("-b:a"), QStringLiteral("128k"),
            });
        QVERIFY(!video.isEmpty());

        MainWindow w;
        w.show();
        QCoreApplication::processEvents();

        w.openVideo(video);
        QVERIFY(w.windowTitle().contains(QStringLiteral("— MonkeyCut")));

        // initial frame displayed
        QVERIFY(waitUntil([&] {
            for (auto* v : w.findChildren<VideoView*>())
                if (v->hasFrame())
                    return true;
            return false;
        }));

        // slider range + timecode updated
        for (auto* s : w.findChildren<QSlider*>()) {
            QVERIFY2(s->maximum() == 124, "slider range must be frames 0..124");
            break;
        }

        // frame step forward via keyboard
        QTest::keyClick(&w, Qt::Key_Right);
        QVERIFY(waitUntil([&] {
            for (auto* l : w.findChildren<QLabel*>())
                if (l->text() == QLatin1String("00:00:00:01"))
                    return true;
            return false;
        }));

        // space starts playback, position advances
        QTest::keyClick(&w, Qt::Key_Space);
        qint64 before = -1;
        for (auto* p : w.findChildren<Player*>())
            before = p->currentFrame();
        Q_UNUSED(before)
        QVERIFY(waitUntil([&] {
            for (auto* p : w.findChildren<Player*>())
                if (p->state() == Player::State::Playing && p->currentFrame() > 8)
                    return true;
            return false;
        }));

        // space pauses
        QTest::keyClick(&w, Qt::Key_Space);
        QVERIFY(waitUntil([&] {
            for (auto* p : w.findChildren<Player*>())
                if (p->state() == Player::State::Paused)
                    return true;
            return false;
        }));

        // background GOP scan result arrives (keyframe count in info label)
        QVERIFY(waitUntil([&] {
            for (auto* l : w.findChildren<QLabel*>())
                if (l->text().contains(QStringLiteral("keyframes")))
                    return true;
            return false;
        }));

        w.close();
    }

    void test_markCutsAndLoadCul()
    {
        if (!FfmpegFixture::available())
            QSKIP("ffmpeg binary not available");

        QTemporaryDir tmp;
        QDir tmpDir(tmp.path());
        const QString video = FfmpegFixture::createTestVideo(
            tmpDir,
            QStringLiteral("ui2.ts"),
            {
                QStringLiteral("-c:v"), QStringLiteral("mpeg2video"),
                QStringLiteral("-g"), QStringLiteral("25"),
                QStringLiteral("-c:a"), QStringLiteral("mp2"),
                QStringLiteral("-b:a"), QStringLiteral("128k"),
            });
        QVERIFY(!video.isEmpty());

        MainWindow w;
        w.show();
        QCoreApplication::processEvents();
        w.openVideo(video);
        QVERIFY(waitUntil([&] {
            for (auto* v : w.findChildren<VideoView*>())
                if (v->hasFrame())
                    return true;
            return false;
        }));

        QTableWidget* table = nullptr;
        for (auto* t : w.findChildren<QTableWidget*>())
            table = t;
        QVERIFY(table != nullptr);

        // mark I at frame 10, O at frame 60
        w.seekToFrame(10);
        QVERIFY(waitUntil([&] {
            for (auto* p : w.findChildren<Player*>())
                if (p->currentFrame() == 10)
                    return true;
            return false;
        }));
        QTest::keyClick(&w, Qt::Key_I);

        w.seekToFrame(60);
        QVERIFY(waitUntil([&] {
            for (auto* p : w.findChildren<Player*>())
                if (p->currentFrame() == 60)
                    return true;
            return false;
        }));
        QTest::keyClick(&w, Qt::Key_O);

        QVERIFY(waitUntil([&] { return table->rowCount() == 1; }));
        QCOMPARE(table->item(0, 1)->text(), QLatin1String("00:00:00:10"));
        QCOMPARE(table->item(0, 2)->text(), QLatin1String("00:00:02:10"));

        // load a CUL replacing the current cut: keep [10, 70)
        const QString culPath = tmpDir.filePath(QStringLiteral("test.cul"));
        {
            QFile f(culPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(
                "[General]\r\n"
                "FramesPerSecond=25\r\n"
                "NoOfCuts=1\r\n"
                "[Cut0]\r\n"
                "Start=0.4\r\n"
                "StartFrame=10\r\n"
                "Duration=2.4\r\n"
                "DurationFrames=60\r\n"
                "[Info]\r\n"
                "Author=tester\r\n");
        }
        w.loadCulFile(culPath);
        QVERIFY(waitUntil([&] {
            return table->rowCount() == 1
                && table->item(0, 2)->text() == QLatin1String("00:00:02:20");
        }));
        QCOMPARE(table->item(0, 1)->text(), QLatin1String("00:00:00:10"));

        w.close();
    }
};

QTEST_MAIN(TUi)

#include "t_ui.moc"