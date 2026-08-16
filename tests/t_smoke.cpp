#include <QApplication>
#include <QStatusBar>
#include <QtTest/QtTest>

#include "ui/MainWindow.h"

class TSmoke : public QObject
{
    Q_OBJECT

private slots:
    void windowConstructs()
    {
        MainWindow w;
        QCOMPARE(w.windowTitle(), QString("MonkeyCut"));
        w.resize(900, 600);
        QCOMPARE(w.width(), 900);
        QVERIFY(w.statusBar()->isVisibleTo(&w));
    }
};

QTEST_MAIN(TSmoke)

#include "t_smoke.moc"