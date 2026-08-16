#include "GopCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

static QString cacheKeyFor(const QString& videoPath)
{
    const QFileInfo fi(videoPath);
    QByteArray meta;
    meta += fi.absoluteFilePath().toUtf8();
    meta += QByteArray::number(fi.size());
    meta += QByteArray::number(fi.lastModified().toSecsSinceEpoch());
    return QString::fromUtf8(QCryptographicHash::hash(meta, QCryptographicHash::Md5).toHex());
}

static QString cacheDir()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/gop");
    QDir().mkpath(dir);
    return dir;
}

bool GopCache::load(const QString& videoPath, GopMap* out)
{
    QFile f(cacheDir() + QStringLiteral("/") + cacheKeyFor(videoPath) + QStringLiteral(".json"));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonObject o = doc.object();
    if (!o.contains(QStringLiteral("keyframes")))
        return false;

    GopMap map;
    map.frameCount = o.value(QStringLiteral("frameCount")).toInt();
    const QJsonArray arr = o.value(QStringLiteral("keyframes")).toArray();
    map.keyframes.reserve(arr.size());
    for (const auto& v : arr)
        map.keyframes.append(v.toVariant().toLongLong());
    map.valid = !map.keyframes.isEmpty();
    if (out)
        *out = map;
    return map.valid;
}

void GopCache::save(const QString& videoPath, const GopMap& map)
{
    if (!map.valid)
        return;

    QJsonObject o;
    o[QStringLiteral("frameCount")] = map.frameCount;
    QJsonArray arr;
    for (const auto k : map.keyframes)
        arr.append(k);
    o[QStringLiteral("keyframes")] = arr;

    QFile f(cacheDir() + QStringLiteral("/") + cacheKeyFor(videoPath) + QStringLiteral(".json"));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
}