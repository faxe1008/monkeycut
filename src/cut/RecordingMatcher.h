#pragma once

#include <QDate>
#include <QDir>
#include <QString>

// Heuristic scoring of local recordings against the recording a CUL was
// made for (its ApplyToFile / otrkey name + air date).
class RecordingMatcher
{
public:
    // Lowercase, "_" / "-" to spaces, strips container & quality suffixes
    // (".mpg.HD.avi" -> ""), collapses whitespace.
    static QString normalizeName(const QString& fileNameOrPath);

    // 0..~1.5: token overlap (dominant) + channel +0.25 + air-date +0.25
    // + video container +0.1.
    static double score(const QString& fileName, const QString& culName,
                        const QDate& airDate);

    // Strict acceptance rule (higher bar than score()): video container,
    // token overlap >= 0.6, and - when the CUL name carries a date - the
    // file must show the same date.
    static bool matches(const QString& fileName, const QString& culName,
                        const QDate& airDate);

    // Highest-scoring video file in the folder; empty if nothing reaches
    // the threshold.
    static QString bestMatch(const QDir& dir, const QString& culName,
                             const QDate& airDate, double threshold = 1.0,
                             double* bestScore = nullptr);
};
