#include <QApplication>
#include <QtTest/QtTest>

#include "core/Time.h"
#include "core/TimeCode.h"

class TTime : public QObject
{
    Q_OBJECT

private slots:
    void fpsBasics()
    {
        Fps pal(25, 1);
        QCOMPARE(pal.value(), 25.0);
        Fps ntsc(30000, 1001);
        QVERIFY(qAbs(ntsc.value() - 29.97) < 0.01);
        Fps bad(0, 1);
        QVERIFY(!bad.isValid());
    }

    void timeConversion()
    {
        const Time t = Time::fromFrames(50, Fps(25, 1));
        QCOMPARE(t.toSeconds(), 2.0);

        const Time t90k = t.toTimebase(1, 90000);
        QCOMPARE(t90k.ticks(), 180000);
        QCOMPARE(t90k.toSeconds(), 2.0);

        QVERIFY(t.toTimebase(1, 90000) == Time(180000, 1, 90000));
        QVERIFY(Time(180000, 1, 90000) == t);
    }

    void timeArith()
    {
        const Time a = Time::fromFrames(25, Fps(25, 1));
        const Time b = Time::fromFrames(10, Fps(25, 1));
        const Time sum = a + b;
        QCOMPARE(sum.toSeconds(), 1.4);
        QVERIFY((a - b) == Time::fromFrames(15, Fps(25, 1)));
        QVERIFY(b < a);
        QVERIFY(!(a < b));
    }

    void timeCodeFrameFormat()
    {
        QCOMPARE(TimeCode(0, Fps(25, 1)).toString(), QStringLiteral("00:00:00:00"));
        QCOMPARE(TimeCode(9250, Fps(25, 1)).toString(), QStringLiteral("00:06:10:00"));
        QCOMPARE(TimeCode(9251, Fps(25, 1)).toString(), QStringLiteral("00:06:10:01"));
        QCOMPARE(TimeCode(25, Fps(25, 1)).toString(), QStringLiteral("00:00:01:00"));

        const TimeCode tc(75, Fps(50, 1));
        QCOMPARE(tc.toString(), QStringLiteral("00:00:01:25"));
    }

    void timeCodeMsFormat()
    {
        QCOMPARE(TimeCode(0, Fps(25, 1)).toStringMs(), QStringLiteral("00:00:00.000"));
        QCOMPARE(TimeCode(16750, Fps(25, 1)).toStringMs(), QStringLiteral("00:11:10.000"));
        QCOMPARE(TimeCode(1, Fps(25, 1)).toStringMs(), QStringLiteral("00:00:00.040"));
    }

    void timeCodeParse()
    {
        const Fps fps(25, 1);
        QCOMPARE(TimeCode::fromString(QStringLiteral("00:06:10:05"), fps).frame(), qint64(9255));
        QCOMPARE(TimeCode::fromString(QStringLiteral("00:06:10"), fps).frame(), qint64(9250));
        QCOMPARE(TimeCode::fromString(QStringLiteral("00:06:10.400"), fps).frame(), 9260);
        QCOMPARE(TimeCode::fromString(QStringLiteral("01:00:00"), fps).frame(), 90000);
        QVERIFY(!TimeCode::fromString(QStringLiteral("garbage"), fps).isValid());
        QVERIFY(!TimeCode::fromString(QStringLiteral(""), fps).isValid());
    }

    void timeCodeArith()
    {
        const TimeCode tc(100, Fps(25, 1));
        QCOMPARE((tc + 15).frame(), qint64(115));
        QCOMPARE((tc - 5).frame(), qint64(95));
        QVERIFY((tc - 5) < tc);
    }

    void ntscRoundTrip()
    {
        const Fps fps(30000, 1001);
        const TimeCode tc(2997, fps);
        QVERIFY(qAbs(tc.toSeconds() - 100.0) < 0.01);
        QCOMPARE(TimeCode::fromString(tc.toString(), fps), tc);
    }
};

QTEST_MAIN(TTime)

#include "t_time.moc"