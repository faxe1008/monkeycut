#include "fixtures.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

bool FfmpegFixture::available()
{
    static const bool ok = !QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty();
    return ok;
}

QString FfmpegFixture::createTestVideo(QDir& tempDir, const QString& baseName, const QStringList& codecArgs)
{
    const QString exe = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    const QString out = tempDir.filePath(baseName);

    QStringList args = {
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-y"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("testsrc2=duration=5:size=320x240:rate=25"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("sine=frequency=440:duration=5:sample_rate=48000"),
        QStringLiteral("-ac"), QStringLiteral("2"),
        QStringLiteral("-shortest")
    };
    args += codecArgs;
    args.append(out);

    QProcess p;
    p.start(exe, args);
    if (!p.waitForFinished(60000)) {
        p.kill();
        return {};
    }
    if (p.exitCode() != 0) {
        qWarning() << "ffmpeg failed:" << QString::fromLocal8Bit(p.readAllStandardError()).trimmed();
        return {};
    }
    return out;
}