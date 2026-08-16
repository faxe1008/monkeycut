#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "cut/RecordingMatcher.h"

class TRecording : public QObject
{
    Q_OBJECT

private slots:
    void normalize()
    {
        QCOMPARE(
            RecordingMatcher::normalizeName(
                QStringLiteral(
                    "Die_Simpsons__Mission_Simpossible_26.05.29_05-05_orf1_20_"
                    "TVOON_DE.mpg.HD.avi")),
            QStringLiteral(
                "die simpsons mission simpossible 26.05.29 05 05 orf1 20 tvoon "
                "de"));
        // note: bare "_hd" without a dot is kept (it is a stopword token
        // and never affects scoring)
        QCOMPARE(RecordingMatcher::normalizeName("/some/dir/Show_HD.mkv"),
                 QStringLiteral("show hd"));
        QCOMPARE(RecordingMatcher::normalizeName("plain.ts"),
                 QStringLiteral("plain"));
    }

    void score()
    {
        const QString otrkey = QStringLiteral(
            "Die_Simpsons__Mission_Simpossible_26.05.29_05-05_orf1_20_TVOON_"
            "DE.mpg.HD.avi");
        const QDate air(QDate(2026, 5, 29));

        // exact recording: high score (tokens + channel + date + container)
        const double exact =
            RecordingMatcher::score(otrkey, otrkey, air);
        QVERIFY2(exact >= 1.0, qPrintable(QString::number(exact)));

        // unrelated show: low score
        const double unrelated = RecordingMatcher::score(
            QStringLiteral("Some_Other_Show_12.12.01_sat1_10_TVOON_DE.ts"),
            otrkey, air);
        QVERIFY(unrelated < 0.6);

        // same show, different episode/date: score looks high (shared show
        // tokens), which is exactly why bestMatch gates on the date.
        const double otherEp = RecordingMatcher::score(
            QStringLiteral(
                "Die_Simpsons__Frinkcoin_26.05.25_05-25_orf1_20_TVOON_DE.ts"),
            otrkey, air);
        QVERIFY(otherEp >= 1.0);
    }

    void matches()
    {
        const QString otrkey = QStringLiteral(
            "Die_Simpsons__Mission_Simpossible_26.05.29_05-05_orf1_20_TVOON_"
            "DE.mpg.HD.avi");
        const QDate air(QDate(2026, 5, 29));

        // exact recording: matches
        QVERIFY(RecordingMatcher::matches(otrkey, otrkey, air));

        // same show, different episode date: must NOT match
        QVERIFY(!RecordingMatcher::matches(
            QStringLiteral(
                "Die_Simpsons__Frinkcoin_26.05.25_05-25_orf1_20_TVOON_DE.ts"),
            otrkey, air));

        // CUL name without a date: date gate is not applied
        QVERIFY(RecordingMatcher::matches(
            QStringLiteral("Die_Simpsons__Mission_Simpossible_05-05_orf1_20.mpg"),
            QStringLiteral("Die_Simpsons__Mission_Simpossible_05-05_orf1_20.ts"),
            QDate()));
        // non-video file: never matches
        QVERIFY(!RecordingMatcher::matches("Some_Show_26.05.29_orf1.txt",
                                           otrkey, air));
    }

    void bestMatch()
    {
        QTemporaryDir tmp;
        const QString good = tmp.filePath(
            QStringLiteral("Die_Simpsons__Mission_Simpossible_26.05.29_05-05_"
                           "orf1_20_TVOON_DE.mpg.HD.avi"));
        const QString other = tmp.filePath(
            QStringLiteral("News_Radio_25.08.15_20-00_orf1_05_TVOON_DE.ts"));
        // same show, different episode (its own date): must NOT win
        const QString otherEp = tmp.filePath(
            QStringLiteral(
                "Die_Simpsons__Frinkcoin_26.05.25_05-25_orf1_20_TVOON_DE.ts"));
        for (const QString& f : {good, other, otherEp}) {
            QFile q(f);
            QVERIFY(q.open(QIODevice::WriteOnly));
            q.write("x");
            q.close();
        }

        const QString culName = QStringLiteral(
            "Die_Simpsons__Mission_Simpossible_26.05.29_05-05_orf1_20_TVOON_"
            "DE.mpg.HD.avi");
        double bestScore = 0.0;
        const QString best =
            RecordingMatcher::bestMatch(QDir(tmp.path()), culName,
                                        QDate(2026, 5, 29), 1.0, &bestScore);
        QCOMPARE(best, good);
        QVERIFY(bestScore >= 1.0);

        // no match at all when the recording is absent
        QCOMPARE(
            RecordingMatcher::bestMatch(QDir(tmp.path()),
                                        QStringLiteral("Totally_Known_Show"),
                                        QDate(2020, 1, 1), 1.0),
            QString());
    }
};

QTEST_MAIN(TRecording)

#include "t_recording.moc"
