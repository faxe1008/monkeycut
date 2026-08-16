#include "Project.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

static const QString kAppId = QStringLiteral("monkeycut");

bool saveProject(const QString& path, const MonkeyProject& project, QString* error)
{
    QJsonObject root;
    root[QStringLiteral("app")] = kAppId;
    root[QStringLiteral("version")] = project.version;
    root[QStringLiteral("filePath")] = project.filePath;
    root[QStringLiteral("suggestedName")] = project.suggestedName;
    root[QStringLiteral("author")] = project.author;

    QJsonObject fps;
    fps[QStringLiteral("num")] = project.fps.num;
    fps[QStringLiteral("den")] = project.fps.den;
    root[QStringLiteral("fps")] = fps;

    root[QStringLiteral("totalFrames")] = double(project.totalFrames);
    root[QStringLiteral("durationSec")] = project.durationSec;

    QJsonArray cuts;
    for (const Cut& c : project.cuts) {
        QJsonObject o;
        o[QStringLiteral("in")] = double(c.inFrame);
        o[QStringLiteral("out")] = double(c.outFrame);
        cuts.append(o);
    }
    root[QStringLiteral("cuts")] = cuts;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (f.error() != QFile::NoError) {
        if (error)
            *error = f.errorString();
        return false;
    }
    return true;
}

bool loadProject(const QString& path, MonkeyProject* out, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("invalid JSON: %1").arg(pe.errorString());
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("app")).toString() != kAppId) {
        if (error)
            *error = QStringLiteral("not a MonkeyCut project");
        return false;
    }

    MonkeyProject p;
    p.version = root.value(QStringLiteral("version")).toInt(1);
    p.filePath = root.value(QStringLiteral("filePath")).toString();
    p.suggestedName = root.value(QStringLiteral("suggestedName")).toString();
    p.author = root.value(QStringLiteral("author")).toString();
    const QJsonObject fps = root.value(QStringLiteral("fps")).toObject();
    p.fps = Fps(fps.value(QStringLiteral("num")).toInt(),
                fps.value(QStringLiteral("den")).toInt(1));
    p.totalFrames = qint64(root.value(QStringLiteral("totalFrames")).toDouble());
    p.durationSec = root.value(QStringLiteral("durationSec")).toDouble();

    for (const QJsonValue& v : root.value(QStringLiteral("cuts")).toArray()) {
        const QJsonObject o = v.toObject();
        const qint64 in = qint64(o.value(QStringLiteral("in")).toDouble());
        const qint64 out = qint64(o.value(QStringLiteral("out")).toDouble());
        if (out > in)
            p.cuts.append(Cut(in, out));
    }

    if (!p.isValid()) {
        if (error)
            *error = QStringLiteral("project missing required fields");
        return false;
    }
    *out = p;
    return true;
}