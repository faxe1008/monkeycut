#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "av/AvProbe.h"
#include "av/GopScanner.h"
#include "core/CutPlanner.h"
#include "cut/CuttingEngine.h"

#include "fixtures.h"

class TCutEngine : public QObject
{
    Q_OBJECT

private slots:
    void cutTwoSegments()
    {
        if (!FfmpegFixture::available())
            QSKIP("ffmpeg binary not available");

        QTemporaryDir tmp;
        QDir tmpDir(tmp.path());
        const QString input = FfmpegFixture::createTestVideo(
            tmpDir,
            QStringLiteral("full.ts"),
            {
                QStringLiteral("-c:v"), QStringLiteral("mpeg2video"),
                QStringLiteral("-g"), QStringLiteral("25"),
                QStringLiteral("-c:a"), QStringLiteral("mp2"),
                QStringLiteral("-b:a"), QStringLiteral("128k"),
            });
        QVERIFY(!input.isEmpty());

        const GopMap gop = scanGopMap(input);
        QVERIFY(gop.valid);
        QCOMPARE(gop.keyframes, (QVector<qint64>{0, 25, 50, 75, 100}));

        // Keep [0,30) and [50,80) -> 60 frames, ~2.4 s
        const QVector<Cut> cuts = {Cut{0, 30}, Cut{50, 80}};
        const PlanResult plan = planCuts(cuts, gop, gop.frameCount);
        QVERIFY(plan.ok);
        QCOMPARE(plan.segments.size(), 2);
        QCOMPARE(plan.segments[0].inFrame, qint64(0));
        QCOMPARE(plan.segments[0].outFrame, qint64(30));
        QCOMPARE(plan.segments[1].inFrame, qint64(50));
        QCOMPARE(plan.segments[1].outFrame, qint64(80));

        const QString output = tmp.filePath(QStringLiteral("cut.ts"));
        CuttingEngine engine;
        QSignalSpy finishedSpy(&engine, &CuttingEngine::finished);
        QSignalSpy progressSpy(&engine, &CuttingEngine::progress);
        QVERIFY(engine.start(input, output, plan.segments, Fps(25, 1)));
        QCOMPARE(finishedSpy.wait(60000), true);
        QCOMPARE(finishedSpy.count(), 1);
        const QList<QVariant> args = finishedSpy.first();
        QCOMPARE(args.at(0).toBool(), true);
        QCOMPARE(args.at(1).toString(), QString());
        QVERIFY(!progressSpy.isEmpty());

        QVERIFY(QFileInfo(output).exists());

        const MediaInfo outInfo = AvProbe().probe(output);
        QVERIFY(outInfo.ok);
        QCOMPARE(outInfo.totalFrames, qint64(60));
        QVERIFY(outInfo.durationSec > 2.0 && outInfo.durationSec < 2.8);
        QVERIFY(outInfo.firstVideo());
        QCOMPARE(outInfo.firstVideo()->fps, Fps(25, 1));
        QVERIFY(outInfo.firstAudio() != nullptr);
        QCOMPARE(outInfo.formatName, QStringLiteral("mpegts"));
    }

    void cutSnapsBackToKeyframe()
    {
        if (!FfmpegFixture::available())
            QSKIP("ffmpeg binary not available");

        QTemporaryDir tmp;
        QDir tmpDir(tmp.path());
        const QString input = FfmpegFixture::createTestVideo(
            tmpDir,
            QStringLiteral("src.ts"),
            {
                QStringLiteral("-c:v"), QStringLiteral("mpeg2video"),
                QStringLiteral("-g"), QStringLiteral("25"),
                QStringLiteral("-c:a"), QStringLiteral("mp2"),
                QStringLiteral("-b:a"), QStringLiteral("128k"),
            });
        QVERIFY(!input.isEmpty());

        const GopMap gop = scanGopMap(input);
        QVERIFY(gop.valid);

        // In at frame 10 is not a keyframe -> segment starts at 0
        const QVector<Cut> cuts = {Cut{10, 40}};
        const PlanResult plan = planCuts(cuts, gop, gop.frameCount);
        QVERIFY(plan.ok);
        QCOMPARE(plan.segments[0].inFrame, qint64(0));
        QCOMPARE(plan.segments[0].outFrame, qint64(40));

        const QString output = tmp.filePath(QStringLiteral("cut.ts"));
        CuttingEngine engine;
        QSignalSpy finishedSpy(&engine, &CuttingEngine::finished);
        QVERIFY(engine.start(input, output, plan.segments, Fps(25, 1)));
        QCOMPARE(finishedSpy.wait(60000), true);
        QCOMPARE(finishedSpy.first().at(0).toBool(), true);

        const MediaInfo outInfo = AvProbe().probe(output);
        QVERIFY(outInfo.ok);
        QCOMPARE(outInfo.totalFrames, qint64(40));
    }

    void doubleStartRejected()
    {
        if (!FfmpegFixture::available())
            QSKIP("ffmpeg binary not available");

        QTemporaryDir tmp;
        QDir tmpDir(tmp.path());
        const QString input = FfmpegFixture::createTestVideo(
            tmpDir,
            QStringLiteral("dup.ts"),
            {
                QStringLiteral("-c:v"), QStringLiteral("mpeg2video"),
                QStringLiteral("-g"), QStringLiteral("25"),
                QStringLiteral("-c:a"), QStringLiteral("mp2"),
                QStringLiteral("-b:a"), QStringLiteral("128k"),
            });
        QVERIFY(!input.isEmpty());

        CuttingEngine engine;
        QSignalSpy finishedSpy(&engine, &CuttingEngine::finished);
        QVERIFY(engine.start(input, tmp.filePath("a.ts"),
                             {PlannedSegment{0, 10}}, Fps(25, 1)));
        QVERIFY(engine.isRunning());
        QVERIFY(!engine.start(input, tmp.filePath("b.ts"),
                              {PlannedSegment{0, 10}}, Fps(25, 1)));
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toBool(), false);
        QVERIFY(finishedSpy.wait(60000));
        QCOMPARE(finishedSpy.count(), 2);
        QCOMPARE(finishedSpy.at(1).at(0).toBool(), true);
    }

    void startRejected()
    {
        CuttingEngine engine;
        QSignalSpy finishedSpy(&engine, &CuttingEngine::finished);

        // no segments
        QVERIFY(!engine.start("/in.ts", "/out.ts", {}, Fps(25, 1)));
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toBool(), false);
        QVERIFY(!finishedSpy.first().at(1).toString().isEmpty());
        finishedSpy.clear();

        // bad fps
        QVERIFY(!engine.start("/in.ts", "/out.ts", {PlannedSegment{0, 10}},
                              Fps()));
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toBool(), false);

    }
};

QTEST_MAIN(TCutEngine)

#include "t_cutengine.moc"
