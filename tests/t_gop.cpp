#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "av/GopCache.h"
#include "av/GopScanner.h"
#include "core/GopMap.h"

#include "fixtures.h"

class TGop : public QObject
{
    Q_OBJECT

private slots:
    void scanGop()
    {
        if (!FfmpegFixture::available())
            QSKIP("ffmpeg binary not available");

        QTemporaryDir tmp;
        QDir tmpDir(tmp.path());
        const QString ts = FfmpegFixture::createTestVideo(
            tmpDir,
            QStringLiteral("gop.ts"),
            {
                QStringLiteral("-c:v"), QStringLiteral("mpeg2video"),
                QStringLiteral("-g"), QStringLiteral("25"),
                QStringLiteral("-c:a"), QStringLiteral("mp2"),
                QStringLiteral("-b:a"), QStringLiteral("128k"),
            });
        QVERIFY(!ts.isEmpty());

        const GopMap map = scanGopMap(ts);
        QVERIFY(map.valid);
        QCOMPARE(map.frameCount, qint64(125));

        // 5s @ 25fps = 125 frames (index 0..124). GOP 25 -> idr at 0,25,50,75,100.
        QVector<qint64> expected = {0, 25, 50, 75, 100};
        QCOMPARE(map.keyframes, expected);

        QCOMPARE(map.snapBack(0), qint64(0));
        QCOMPARE(map.snapBack(1), qint64(0));
        QCOMPARE(map.snapBack(25), qint64(25));
        QCOMPARE(map.snapBack(26), qint64(25));
        QCOMPARE(map.snapBack(124), qint64(100));
        QCOMPARE(map.snapFwd(0), qint64(0));
        QCOMPARE(map.snapFwd(1), qint64(25));
        QCOMPARE(map.snapFwd(25), qint64(25));
        QCOMPARE(map.snapFwd(99), qint64(100));
        QCOMPARE(map.snapFwd(124), qint64(-1));
        QCOMPARE(map.snapFwd(200), qint64(-1));
        QCOMPARE(map.snapBack(-1), qint64(-1));
    }

    void cacheRoundTrip()
    {
        GopMap map;
        map.valid = true;
        map.frameCount = 125;
        map.keyframes = {0, 25, 50, 75, 100, 125};

        const QString videoPath = QStringLiteral("/cache/test/video/ts");
        GopCache::save(videoPath, map);

        GopMap loaded;
        QVERIFY(GopCache::load(videoPath, &loaded));
        QCOMPARE(loaded.frameCount, map.frameCount);
        QCOMPARE(loaded.keyframes, map.keyframes);
        QVERIFY(loaded.valid);
    }

    void cacheMissOnChangedFile()
    {
        QTemporaryDir tmp;
        QFile f(tmp.filePath(QStringLiteral("vid.ts")));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("0123456789");
        f.close();

        const QString path = f.fileName();
        GopMap map;
        map.valid = true;
        map.frameCount = 42;
        map.keyframes = {0, 21, 42};
        GopCache::save(path, map);

        GopMap loaded;
        QVERIFY(GopCache::load(path, &loaded));
        QCOMPARE(loaded.frameCount, 42);

        QVERIFY(f.open(QIODevice::Append));
        f.write("extra-bytes");
        f.close();

        GopMap missed;
        QVERIFY(!GopCache::load(path, &missed));
        QVERIFY(!missed.valid);
    }

    void missingFile()
    {
        const GopMap map = scanGopMap(QStringLiteral("/nonexistent/video.ts"));
        QVERIFY(!map.valid);
        QVERIFY(map.keyframes.isEmpty());
    }
};

QTEST_MAIN(TGop)

#include "t_gop.moc"