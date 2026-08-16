#pragma once

#include <QMap>
#include <QString>
#include <QVector>

#include "core/Cut.h"
#include "core/Cutlist.h"
#include "core/Fps.h"

// A ColdCut / VirtualDub CUL file (cutlist.at download format).
// Keep-semantics: the listed parts are kept, the gaps are cut.
struct CulFile
{
    struct CutSeg
    {
        double startSec = 0;
        qint64 startFrame = 0;
        double durationSec = 0;
        qint64 durationFrames = 0;
    };

    QMap<QString, QString> general; // raw [General] values
    QVector<CutSeg> cuts;
    QMap<QString, QString> info;    // raw [Info] values
    QMap<QString, QMap<QString, QString>> other; // e.g. [Comment]

    qint64 framesPerSecond() const;
    double secondsPerFrame() const;

    qint64 cutStartFrame(int i) const;
    qint64 cutEndFrame(int i) const; // start + duration

    // raw helper for known keys
    QString generalValue(const QString& key) const
    {
        return general.value(key);
    }
    QString infoValue(const QString& key) const
    {
        return info.value(key);
    }
};

// Parses CUL text (CRLF or LF). Tolerant of unknown keys/sections.
CulFile parseCul(const QString& text);

bool loadCul(const QString& path, CulFile* out, QString* error = nullptr);
bool saveCul(const QString& path, const CulFile& file, QString* error = nullptr);

// Conversion: CUL keep segments -> Cutlist keep-ranges (frames).
// When targetFps differs from the CUL's own rate, frame positions are
// converted via seconds (a 50 fps CUL applied to a 25 fps recording).
Cutlist culToCutlist(const CulFile& cul, double targetFps = -1.0);

// Reverse: keep-ranges -> CUL (fps from the media, times derived from frames)
CulFile cutlistToCul(const Cutlist& cutlist, Fps fps, const QString& applyToFile,
                     const QString& suggestedName, const QString& author);