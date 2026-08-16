#include <QApplication>
#include <QSettings>
#include <QTranslator>

#include "ui/MainWindow.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("monkeycut");
    app.setOrganizationDomain("localhost");
    app.setApplicationName("MonkeyCut");
    app.setApplicationVersion("0.1.0");

    // German by default, English at parity (owner decision)
    QTranslator translator;
    const QString lang = QSettings{}.value(QStringLiteral("ui/language"),
                                            QStringLiteral("de")).toString();
    const QString resourceName = QStringLiteral(":/i18n/monkeycut_%1.qm").arg(lang);
    if (!translator.load(resourceName)) {
        const QString fallback = QCoreApplication::applicationDirPath()
            + QStringLiteral("/translations/monkeycut_%1.qm").arg(lang);
        translator.load(fallback);
    }
    app.installTranslator(&translator);

    MainWindow w;
    w.show();
    return app.exec();
}
