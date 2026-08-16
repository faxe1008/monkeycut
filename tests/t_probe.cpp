#include <QApplication>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "av/AvProbe.h"
#include "av/VfrDetect.h"
#include "core/MediaInfo.h"

#include "fixtures.h"

class TProbe : public QObject
{
    Q_OBJECT

private slots:
    void tsFixture()
    {
        if (!FfmpegFixture::available())
            QSKIP("ffmpeg binary not available");

        QTemporaryDir tmp;
        QDir tmpDir(tmp.path());
        const QString ts = FfmpegFixture::createTestVideo(
            tmpDir,
            QStringLiteral("fixture.ts"),
            {
                QStringLiteral("-c:v"), QStringLiteral("mpeg2video"),
                QStringLiteral("-g"), QStringLiteral("25"),
                QStringLiteral("-c:a"), QStringLiteral("mp2"),
                QStringLiteral("-b:a"), QStringLiteral("128k"),
            });
        QVERIFY(!ts.isEmpty());

        const MediaInfo info = AvProbe{}.probe(ts);
        QVERIFY2(info.ok, qPrintable(info.error));
        QCOMPARE(info.formatName, QStringLiteral("mpegts"));
        QVERIFY(info.seekable);
        QVERIFY(info.durationSec > 4.5 && info.durationSec < 5.5);
        QVERIFY(info.totalFrames >= 120 && info.totalFrames <= 130);

        const MediaStreamInfo* v = info.firstVideo();
        QVERIFY(v);
        QCOMPARE(v->codecName, QStringLiteral("mpeg2video"));
        QCOMPARE(v->width, 320);
        QCOMPARE(v->height, 240);
        QCOMPARE(v->fps, Fps(25, 1));
        QVERIFY(!v->vfrSuspected);

        const MediaStreamInfo* a = info.firstAudio();
        QVERIFY(a);
        QCOMPARE(a->codecName, QStringLiteral("mp2"));
        QCOMPARE(a->sampleRate, 48000);
        QCOMPARE(a->channels, 2);
        QCOMPARE(info.audioStreamCount(), 1);
        QCOMPARE(info.videoStreamCount(), 1);
    }

    void aviFixture()
    {
        if (!FfmpegFixture::available())
            QSKIP("ffmpeg binary not available");

        QTemporaryDir tmp;
        QDir tmpDir(tmp.path());
        const QString avi = FfmpegFixture::createTestVideo(
            tmpDir,
            QStringLiteral("fixture.avi"),
            {
                QStringLiteral("-c:v"), QStringLiteral("mpeg2video"),
                QStringLiteral("-g"), QStringLiteral("25"),
                QStringLiteral("-c:a"), QStringLiteral("mp2"),
                QStringLiteral("-b:a"), QStringLiteral("128k"),
            });
        QVERIFY(!avi.isEmpty());

        const MediaInfo info = AvProbe{}.probe(avi);
        QVERIFY2(info.ok, qPrintable(info.error));
        QCOMPARE(info.formatName, QStringLiteral("avi"));
        QVERIFY(info.seekable);

        const MediaStreamInfo* v = info.firstVideo();
        QVERIFY(v);
        QCOMPARE(v->codecName, QStringLiteral("mpeg2video"));
        QCOMPARE(v->fps, Fps(25, 1));

        const MediaStreamInfo* a = info.firstAudio();
        QVERIFY(a);
        QCOMPARE(a->codecName, QStringLiteral("mp2"));
    }

    void missingFile()
    {
        const MediaInfo info = AvProbe{}.probe(QStringLiteral("/nonexistent/video.ts"));
        QVERIFY(!info.ok);
        QVERIFY(!info.error.isEmpty());
    }

    void vfrDetection()
    {
        QVector<qint64> cfr;
        for (int i = 0; i < 50; ++i)
            cfr.append(i * 40);
        QVERIFY(!looksLikeVfr(cfr));

        QVector<qint64> vfr;
        for (int i = 0; i < 50; ++i)
            vfr.append(i * 40 + (i % 3 == 0 ? 15 : 0));
        QVERIFY(looksLikeVfr(vfr));

        QVERIFY(!looksLikeVfr({100, 140, 180}));
    }
};

QTEST_MAIN(TProbe)

#include "t_probe.moc"