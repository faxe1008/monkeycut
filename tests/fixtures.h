#pragma once

#include <QDir>
#include <QString>

class FfmpegFixture
{
public:
    static bool available();

    static QString createTestVideo(QDir& tempDir, const QString& baseName, const QStringList& codecArgs);
};