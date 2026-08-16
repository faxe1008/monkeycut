#pragma once

#include <QString>

#include "core/Cut.h"
#include "core/Fps.h"

// A saved working state (.mproject): media reference + cut ranges.
struct MonkeyProject
{
    int version = 1;
    QString filePath;
    QString suggestedName;
    QString author;
    Fps fps;
    qint64 totalFrames = 0;
    double durationSec = 0;
    QVector<Cut> cuts;

    bool isValid() const
    {
        return !filePath.isEmpty() && fps.isValid();
    }
};

bool saveProject(const QString& path, const MonkeyProject& project, QString* error = nullptr);
bool loadProject(const QString& path, MonkeyProject* out, QString* error = nullptr);