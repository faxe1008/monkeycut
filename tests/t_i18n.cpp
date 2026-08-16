#include <QCoreApplication>
#include <QFileInfo>
#include <QTranslator>
#include <QtTest/QtTest>

class TI18n : public QObject
{
    Q_OBJECT

    static void ensureBuilt()
    {
        if (!QFileInfo::exists(QStringLiteral(":/i18n/monkeycut_de.qm")))
            QSKIP("compiled translations unavailable (lrelease missing at build time)");
    }

private slots:
    void deCatalogLoadsAndTranslates()
    {
        ensureBuilt();
        QTranslator tr;
        QVERIFY(tr.load(QStringLiteral(":/i18n/monkeycut_de.qm")));
        QCOMPARE(tr.translate("SettingsDialog", "Settings"),
                 QString::fromUtf8("Einstellungen"));
        QCOMPARE(tr.translate("MainWindow", "Export complete: %1"),
                 QString::fromUtf8("Export abgeschlossen: %1"));
        QCOMPARE(tr.translate("TimelineBar", "Klicken/Vorziehen zum Springen"),
                 QString::fromUtf8("Klicken/Vorziehen zum Springen"));
    }

    void enCatalogTranslatesGermanSources()
    {
        ensureBuilt();
        QTranslator tr;
        QVERIFY(tr.load(QStringLiteral(":/i18n/monkeycut_en.qm")));
        QCOMPARE(tr.translate("VideoView", "Kein Video"),
                 QString::fromUtf8("No video"));
        QCOMPARE(tr.translate("MainWindow", "Clear"), QString("Clear"));
    }
};

QTEST_GUILESS_MAIN(TI18n)

#include "t_i18n.moc"