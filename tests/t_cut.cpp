#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "core/Cut.h"
#include "core/Cutlist.h"
#include "core/CutPlanner.h"
#include "core/GopMap.h"
#include "cut/CulFile.h"
#include "cut/Project.h"

namespace
{
QString sampleCul()
{
    return QStringLiteral(
        "[General]\r\n"
        "Application=ColdCut\r\n"
        "Version=1.0.8.6\r\n"
        "FramesPerSecond=25\r\n"
        "DisplayAspectRatio=16:9\r\n"
        "NoOfCuts=2\r\n"
        "ApplyToFile=Die_Simpsons__Zu_Ehren_von_Murphy.ts\r\n"
        "OriginalFileSizeBytes=405016566\r\n"
        "\r\n"
        "[Cut0]\r\n"
        "Start=437.96\r\n"
        "StartFrame=10949\r\n"
        "Duration=615.04\r\n"
        "DurationFrames=15376\r\n"
        "\r\n"
        "[Cut1]\r\n"
        "Start=1490.24\r\n"
        "StartFrame=37256\r\n"
        "Duration=678.52\r\n"
        "DurationFrames=16963\r\n"
        "\r\n"
        "[Info]\r\n"
        "Author=b-andi-t\r\n"
        "RatingByAuthor=5\r\n"
        "EPGError=0\r\n"
        "SuggestedMovieName=Die Simpsons - Zu Ehren von Murphy\r\n"
        "UserComment=Mit ColdCut geschnitten\r\n");
}
}

class TCut : public QObject
{
    Q_OBJECT

private slots:
    void cutlistBasics()
    {
        Cutlist list(1000);
        QVERIFY(list.addCut(100, 200));
        QVERIFY(!list.addCut(150, 300)); // overlap
        QVERIFY(!list.addCut(300, 300)); // empty
        QVERIFY(list.addCut(0, 50));
        QCOMPARE(list.cuts().size(), 2);
        QCOMPARE(list.cuts()[0].inFrame, qint64(0)); // sorted
        QCOMPARE(list.keepFrameCount(), qint64(150));
        QCOMPARE(list.cutFrameCount(), qint64(850));
        QVERIFY(list.containsFrame(0));
        QVERIFY(list.containsFrame(49));
        QVERIFY(!list.containsFrame(50));
        QVERIFY(list.containsFrame(100));
        QVERIFY(!list.containsFrame(200));
        QVERIFY(list.removeAt(0));
        QCOMPARE(list.cuts().size(), 1);
        QVERIFY(!list.addCut(199, 300)); // now overlaps
        QVERIFY(list.addCut(200, 300));
    }

    void cutlistClipToTotal()
    {
        Cutlist list(100);
        QVERIFY(list.addCut(80, 500)); // clipped to total
        QCOMPARE(list.cuts()[0].outFrame, qint64(100));
        QVERIFY(!list.addCut(200, 300)); // beyond total
    }

    void plannerSnapsToKeyframes()
    {
        GopMap gop;
        gop.valid = true;
        gop.frameCount = 125;
        gop.keyframes = {0, 25, 50, 75, 100};

        QVector<Cut> cuts = {Cut(50, 60), Cut(110, 124)};
        const PlanResult r = planCuts(cuts, gop, 125);
        QVERIFY(r.ok);
        QVERIFY(r.keyframeAligned);
        QCOMPARE(r.segments.size(), 2);
        QCOMPARE(r.segments[0].inFrame, qint64(50)); // exact keyframe
        QCOMPARE(r.segments[0].outFrame, qint64(60));
        QCOMPARE(r.segments[0].inDelta, qint64(0));
        QCOMPARE(r.segments[1].inFrame, qint64(100)); // snapped back
        QCOMPARE(r.segments[1].inDelta, qint64(-10));
        QCOMPARE(r.segments[1].outFrame, qint64(124)); // end stays exact
    }

    void plannerWithoutGop()
    {
        QVector<Cut> cuts = {Cut(10, 20)};
        const PlanResult r = planCuts(cuts, GopMap(), 100);
        QVERIFY(r.ok);
        QVERIFY(!r.keyframeAligned);
        QCOMPARE(r.segments[0].inFrame, qint64(10)); // exact, no snapping
        QCOMPARE(r.segments[0].inDelta, qint64(0));
    }

    void culParse()
    {
        const CulFile cul = parseCul(sampleCul());
        QCOMPARE(cul.framesPerSecond(), qint64(25));
        QCOMPARE(qFuzzyCompare(cul.secondsPerFrame(), 0.04), true);
        QCOMPARE(cul.generalValue(QStringLiteral("ApplyToFile")),
                 QStringLiteral("Die_Simpsons__Zu_Ehren_von_Murphy.ts"));
        QCOMPARE(cul.cuts.size(), 2);
        QCOMPARE(cul.cutStartFrame(0), qint64(10949));
        QCOMPARE(cul.cutEndFrame(0), qint64(10949 + 15376));
        QCOMPARE(cul.cutStartFrame(1), qint64(37256));
        QCOMPARE(cul.cutEndFrame(1), qint64(37256 + 16963));
        QCOMPARE(cul.infoValue(QStringLiteral("Author")),
                 QStringLiteral("b-andi-t"));
        QCOMPARE(cul.infoValue(QStringLiteral("SuggestedMovieName")),
                 QStringLiteral("Die Simpsons - Zu Ehren von Murphy"));
    }

    void culRoundTrip()
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("sample.cul"));
        const CulFile in = parseCul(sampleCul());
        QString err;
        QVERIFY(saveCul(path, in, &err));

        CulFile out;
        QVERIFY(loadCul(path, &out, &err));
        QCOMPARE(out.general, in.general);
        QCOMPARE(out.info, in.info);
        QCOMPARE(out.cuts.size(), in.cuts.size());
        for (int i = 0; i < in.cuts.size(); ++i) {
            const auto& a = in.cuts[i];
            const auto& b = out.cuts[i];
            QCOMPARE(b.startFrame, a.startFrame);
            QCOMPARE(b.durationFrames, a.durationFrames);
            QVERIFY(qFuzzyCompare(b.startSec, a.startSec));
        }
    }

    void culToCutlistConversion()
    {
        const CulFile cul = parseCul(sampleCul());
        Cutlist list = culToCutlist(cul);
        list.setTotalFrames(60000);
        QCOMPARE(list.cuts().size(), 2);
        QCOMPARE(list.cuts()[0].inFrame, qint64(10949));
        QCOMPARE(list.cuts()[0].outFrame, qint64(26325));
        QCOMPARE(list.cuts()[1].inFrame, qint64(37256));
        QCOMPARE(list.cuts()[1].outFrame, qint64(54219));
    }

    void cutlistToCulConversion()
    {
        Cutlist list;
        list.addCut(0, 100);
        list.addCut(200, 300);
        const CulFile cul = cutlistToCul(list, Fps(25, 1), QStringLiteral("x.ts"),
                                         QStringLiteral("Some Movie"),
                                         QStringLiteral("tester"));
        QCOMPARE(cul.cuts.size(), 2);
        QCOMPARE(cul.cuts[0].startFrame, qint64(0));
        QCOMPARE(cul.cuts[0].durationFrames, qint64(100));
        QVERIFY(qFuzzyCompare(cul.cuts[0].startSec, 0.0));
        QVERIFY(qFuzzyCompare(cul.cuts[0].durationSec, 4.0));
        QCOMPARE(cul.cuts[1].startFrame, qint64(200));
        QCOMPARE(cul.cuts[1].durationFrames, qint64(100));
        QCOMPARE(cul.generalValue(QStringLiteral("NoOfCuts")), QStringLiteral("2"));
        QCOMPARE(cul.infoValue(QStringLiteral("SuggestedMovieName")),
                 QStringLiteral("Some Movie"));
    }

    void projectRoundTrip()
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("x.mproject"));

        MonkeyProject p;
        p.filePath = QStringLiteral("/tmp/recording.ts");
        p.suggestedName = QStringLiteral("Show S01E01");
        p.author = QStringLiteral("tester");
        p.fps = Fps(25, 1);
        p.totalFrames = 125000;
        p.durationSec = 5000.0;
        p.cuts = {Cut(0, 1000), Cut(2000, 3000)};

        QString err;
        QVERIFY(saveProject(path, p, &err));

        MonkeyProject q;
        QVERIFY(loadProject(path, &q, &err));
        QCOMPARE(q.filePath, p.filePath);
        QCOMPARE(q.suggestedName, p.suggestedName);
        QCOMPARE(q.author, p.author);
        QCOMPARE(q.fps, p.fps);
        QCOMPARE(q.totalFrames, p.totalFrames);
        QCOMPARE(q.durationSec, p.durationSec);
        QCOMPARE(q.cuts.size(), 2);
        QCOMPARE(q.cuts[0], p.cuts[0]);
        QCOMPARE(q.cuts[1], p.cuts[1]);
    }

    void projectRejectsForeignFile()
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("foreign.mproject"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("{\"app\":\"somethingelse\",\"version\":1}");
        }
        MonkeyProject q;
        QString err;
        QVERIFY(!loadProject(path, &q, &err));
        QVERIFY(!err.isEmpty());
    }

    void culFpsConversion()
    {
        // a 50 fps CUL applied to a 25 fps recording: frames halve
        CulFile cul;
        cul.general[QStringLiteral("FramesPerSecond")] = QStringLiteral("50");
        CulFile::CutSeg seg;
        seg.startFrame = 100; // = 2.0 s at 50 fps
        seg.startSec = 2.0;
        seg.durationFrames = 50; // = 1.0 s
        seg.durationSec = 1.0;
        cul.cuts.append(seg);

        Cutlist at50 = culToCutlist(cul);
        QCOMPARE(at50.cuts().size(), 1);
        QCOMPARE(at50.cuts()[0].inFrame, qint64(100));
        QCOMPARE(at50.cuts()[0].outFrame, qint64(150));

        Cutlist at25 = culToCutlist(cul, 25.0);
        QCOMPARE(at25.cuts().size(), 1);
        QCOMPARE(at25.cuts()[0].inFrame, qint64(50));
        QCOMPARE(at25.cuts()[0].outFrame, qint64(75));
    }
};


QTEST_MAIN(TCut)

#include "t_cut.moc"