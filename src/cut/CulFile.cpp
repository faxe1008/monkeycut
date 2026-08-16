#include "CulFile.h"

#include <QFile>
#include <QTextStream>

qint64 CulFile::framesPerSecond() const
{
    const QString v = generalValue(QStringLiteral("FramesPerSecond"));
    bool ok = false;
    double fps = v.toDouble(&ok);
    return ok ? qint64(qRound(fps)) : 25;
}

double CulFile::secondsPerFrame() const
{
    qint64 fps = framesPerSecond();
    // honor fractional fps like "29.97"
    const QString v = generalValue(QStringLiteral("FramesPerSecond"));
    double parsed = v.toDouble();
    if (parsed > 0)
        return 1.0 / parsed;
    return 1.0 / double(fps);
}

qint64 CulFile::cutStartFrame(int i) const
{
    if (i < 0 || i >= cuts.size())
        return -1;
    const CutSeg& s = cuts[i];
    if (s.startFrame > 0)
        return s.startFrame;
    return qint64(qRound(s.startSec / secondsPerFrame()));
}

qint64 CulFile::cutEndFrame(int i) const
{
    if (i < 0 || i >= cuts.size())
        return -1;
    const CutSeg& s = cuts[i];
    if (s.durationFrames > 0)
        return s.startFrame > 0 ? s.startFrame + s.durationFrames
                                 : cutStartFrame(i) + s.durationFrames;
    return qint64(qRound((s.startSec + s.durationSec) / secondsPerFrame()));
}

namespace
{
enum class Section { None, General, Cut, Info };

void handleKey(Section section, int cutIndex, const QString& key, const QString& value,
               CulFile* out)
{
    if (section == Section::General) {
        out->general[key] = value;
    } else if (section == Section::Info) {
        out->info[key] = value;
    } else if (section == Section::Cut) {
        if (out->cuts.size() <= static_cast<size_t>(cutIndex))
            out->cuts.resize(cutIndex + 1);
        CulFile::CutSeg& seg = out->cuts[cutIndex];
        if (key == QLatin1String("Start"))
            seg.startSec = value.toDouble();
        else if (key == QLatin1String("StartFrame"))
            seg.startFrame = value.toLongLong();
        else if (key == QLatin1String("Duration"))
            seg.durationSec = value.toDouble();
        else if (key == QLatin1String("DurationFrames"))
            seg.durationFrames = value.toLongLong();
    } else {
        out->other[key]; // keep section name? see below
    }
}
}

CulFile parseCul(const QString& text)
{
    CulFile out;
    Section section = Section::None;
    int cutIndex = -1;
    QString sectionName;

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';'))
            || line.startsWith(QLatin1Char('#')))
            continue;

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            sectionName = line.mid(1, line.size() - 2).trimmed();
            if (sectionName.compare(QLatin1String("General"), Qt::CaseInsensitive) == 0) {
                section = Section::General;
                cutIndex = -1;
            } else if (sectionName.compare(QLatin1String("Info"), Qt::CaseInsensitive) == 0) {
                section = Section::Info;
                cutIndex = -1;
            } else if (sectionName.startsWith(QLatin1String("Cut"), Qt::CaseInsensitive)) {
                bool ok = false;
                const int idx = sectionName.mid(3).toInt(&ok);
                section = Section::Cut;
                cutIndex = ok ? idx : (out.cuts.size());
            } else {
                section = Section::None; // keys land in other[sectionName]
                out.other[sectionName];
                cutIndex = -1;
            }
            continue;
        }

        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();

        if (section == Section::None) {
            if (!sectionName.isEmpty())
                out.other[sectionName][key] = value;
        } else {
            handleKey(section, cutIndex, key, value, &out);
        }
    }
    return out;
}

bool loadCul(const QString& path, CulFile* out, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = path + QStringLiteral(": ") + f.errorString();
        return false;
    }
    *out = parseCul(QString::fromUtf8(f.readAll()));
    return true;
}

namespace
{
bool append(QTextStream& out, const QString& key, const QString& value)
{
    out << key << QLatin1Char('=') << value << QStringLiteral("\r\n");
    return true;
}
bool blank(QTextStream& out)
{
    out << QStringLiteral("\r\n");
    return true;
}
}

bool saveCul(const QString& path, const CulFile& file, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = path + QStringLiteral(": ") + f.errorString();
        return false;
    }
    QTextStream ts(&f);

    ts << QStringLiteral("[General]\r\n");
    for (auto it = file.general.constBegin(); it != file.general.constEnd(); ++it)
        append(ts, it.key(), it.value());
    blank(ts);

    for (int i = 0; i < file.cuts.size(); ++i) {
        const CulFile::CutSeg& s = file.cuts[i];
        ts << QStringLiteral("[Cut%1]\r\n").arg(i);
        append(ts, QStringLiteral("Start"),
               QString::number(s.startSec, 'f', 2));
        append(ts, QStringLiteral("StartFrame"), QString::number(s.startFrame));
        append(ts, QStringLiteral("Duration"),
               QString::number(s.durationSec, 'f', 2));
        append(ts, QStringLiteral("DurationFrames"), QString::number(s.durationFrames));
        blank(ts);
    }

    ts << QStringLiteral("[Info]\r\n");
    for (auto it = file.info.constBegin(); it != file.info.constEnd(); ++it)
        append(ts, it.key(), it.value());
    blank(ts);

    for (auto secIt = file.other.constBegin(); secIt != file.other.constEnd(); ++secIt) {
        ts << QStringLiteral("[") << secIt.key() << QStringLiteral("]\r\n");
        for (auto it = secIt.value().constBegin(); it != secIt.value().constEnd(); ++it)
            append(ts, it.key(), it.value());
        blank(ts);
    }

    f.flush();
    if (f.error() != QFile::NoError) {
        if (error)
            *error = f.errorString();
        return false;
    }
    return true;
}

Cutlist culToCutlist(const CulFile& cul, double targetFps)
{
    Cutlist list;
    double culFps = cul.framesPerSecond();
    if (culFps <= 0)
        culFps = 25.0;
    const double outFps = targetFps > 0 ? targetFps : culFps;
    QVector<QPair<qint64, qint64>> ranges;
    for (int i = 0; i < cul.cuts.size(); ++i) {
        const CulFile::CutSeg& s = cul.cuts[i];
        const double inSec =
            s.startFrame > 0 ? double(s.startFrame) / culFps : s.startSec;
        const double durSec =
            s.durationFrames > 0 ? double(s.durationFrames) / culFps
                                  : s.durationSec;
        if (durSec <= 0)
            continue;
        const qint64 in = qint64(qRound(inSec * outFps));
        const qint64 out = qint64(qRound((inSec + durSec) * outFps));
        if (out > in)
            ranges.append({in, out});
    }
    std::sort(ranges.begin(), ranges.end());
    for (const auto& r : ranges)
        list.addCut(r.first, r.second);
    return list;
}

CulFile cutlistToCul(const Cutlist& cutlist, Fps fps, const QString& applyToFile,
                     const QString& suggestedName, const QString& author)
{
    CulFile out;
    const double spf = fps.isValid() ? 1.0 / fps.value() : 0.04;

    out.general[QStringLiteral("Application")] = QStringLiteral("MonkeyCut");
    out.general[QStringLiteral("Version")] = QStringLiteral("1.0");
    out.general[QStringLiteral("FramesPerSecond")] = QString::number(fps.value(), 'f');
    out.general[QStringLiteral("NoOfCuts")] = QString::number(cutlist.cuts().size());
    if (!applyToFile.isEmpty())
        out.general[QStringLiteral("ApplyToFile")] = applyToFile;

    for (const Cut& c : cutlist.cuts()) {
        CulFile::CutSeg seg;
        seg.startFrame = c.inFrame;
        seg.startSec = c.inFrame * spf;
        seg.durationFrames = c.frames();
        seg.durationSec = c.frames() * spf;
        out.cuts.append(seg);
    }

    out.info[QStringLiteral("SuggestedMovieName")] = suggestedName;
    out.info[QStringLiteral("Author")] = author;
    out.info[QStringLiteral("MissingBeginning")] = QStringLiteral("0");
    out.info[QStringLiteral("MissingEnding")] = QStringLiteral("0");
    out.info[QStringLiteral("MissingVideo")] = QStringLiteral("0");
    out.info[QStringLiteral("MissingAudio")] = QStringLiteral("0");
    out.info[QStringLiteral("UserComment")] = QStringLiteral("Mit MonkeyCut geschnitten");
    return out;
}