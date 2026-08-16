#include "RecordingMatcher.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

namespace
{

const QStringList kSuffixes = {
    QStringLiteral("mpg"),  QStringLiteral("mpeg"), QStringLiteral("avi"),
    QStringLiteral("ts"),   QStringLiteral("m2ts"), QStringLiteral("mkv"),
    QStringLiteral("mov"),  QStringLiteral("mp4"),  QStringLiteral("vob"),
    QStringLiteral("asf"),  QStringLiteral("wmv"),  QStringLiteral("flv"),
    QStringLiteral("webm"), QStringLiteral("m4v"),  QStringLiteral("hd"),
    QStringLiteral("hq"),   QStringLiteral("hiq"),  QStringLiteral("remux"),
};

QString stripSuffixes(QString s)
{
    bool changed = true;
    while (changed) {
        changed = false;
        for (const QString& suf : kSuffixes) {
            if (s.endsWith(QStringLiteral(".") + suf)) {
                s.chop(suf.size() + 1);
                changed = true;
                break;
            }
        }
    }
    return s;
}

bool isVideoFile(const QString& fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return ext == "ts" || ext == "m2ts" || ext == "avi" || ext == "mpg"
        || ext == "mpeg" || ext == "mkv" || ext == "mov" || ext == "mp4"
        || ext == "vob" || ext == "wmv" || ext == "flv" || ext == "webm"
        || ext == "m4v";
}

QSet<QString> toTokens(const QString& normalized)
{
    QSet<QString> tokens;
    const QStringList parts =
        normalized.split(QRegularExpression(QStringLiteral("[^a-z0-9]+")),
                         Qt::SkipEmptyParts);
    for (QString part : parts) {
        if (part.isEmpty() || part.size() < 2)
            continue;
        if (part.contains(QLatin1Char('.')) || part.contains(QLatin1Char('-')))
            continue; // separators
        QString t = part;
        // strip embedded quality markers like "hd", "hq"
        if (t == "hd" || t == "hq" || t == "hiq" || t == "the" || t == "and"
            || t == "or")
            continue;
        // pure numbers are handled by date/channel matching
        if (t == t.trimmed() && t.contains(QRegularExpression("^[0-9]+$")))
            continue;
        tokens.insert(t);
    }
    return tokens;
}

bool sharedChannel(const QSet<QString>& a, const QSet<QString>& b)
{
    static const QRegularExpression namedLetterDigit(
        QStringLiteral("^[a-z]{2,}[0-9]{1,2}$")); // orf1, pro7, sat1 …
    static const QSet<QString> named = {
        QStringLiteral("3sat"),      QStringLiteral("kabeleins"),
        QStringLiteral("arte"),      QStringLiteral("zdf"),
        QStringLiteral("one"),       QStringLiteral("phoenix"),
        QStringLiteral("super"),     QStringLiteral("sixx"),
        QStringLiteral("ntv"),       QStringLiteral("rtlplus"),
        QStringLiteral("prosieben"), QStringLiteral("voxa"),
    };
    for (const QString& t : a) {
        if (!b.contains(t))
            continue;
        if (namedLetterDigit.match(t).hasMatch() || named.contains(t))
            return true;
    }
    return false;
}

// Does the file name carry the air date? Order-tolerant: accepts
// YY.MM.DD / DD.MM.YY / YYYY-MM-DD / compact 8 digits (20260529).
bool containsAirDate(const QString& fileName, const QDate& airDate)
{
    if (!airDate.isValid())
        return false;

    auto matchesTriplet = [&](int y, int a, int b) -> bool {
        if (y == -1)
            return false;
        const int year = y < 100 ? 2000 + y : y;
        if (year != airDate.year())
            return false;
        const int d = airDate.day();
        const int mo = airDate.month();
        return (a == mo && b == d) || (a == d && b == mo);
    };

    const QRegularExpression dot3(
        QStringLiteral("([0-9]{1,4})\\.([0-9]{1,2})\\.([0-9]{1,4})"));
    const QRegularExpression hy3(
        QStringLiteral("([0-9]{4})-([0-9]{1,2})-([0-9]{1,2})"));
    const QRegularExpression compact8(QStringLiteral("([0-9]{8})"));

    for (const QRegularExpression& re : {dot3, hy3, compact8}) {
        QRegularExpressionMatchIterator it = re.globalMatch(fileName);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const QString g1 = m.captured(1);
            const QString g2 = m.captured(2);
            const QString g3 = m.captured(3);
            int y = -1, a = -1, b = -1;
            if (re == compact8) {
                const QString n = m.captured(1);
                const int first4 = n.mid(0, 4).toInt();
                const int last4 = n.mid(4).toInt();
                if (first4 >= 1970 && first4 <= 2099) {
                    y = first4;
                    a = n.mid(4, 2).toInt();
                    b = n.mid(6, 2).toInt();
                } else if (last4 >= 1970 && last4 <= 2099) {
                    y = last4;
                    a = n.mid(0, 2).toInt();
                    b = n.mid(2, 2).toInt();
                }
            } else if (g1.size() == 4) {
                y = g1.toInt(); // y.m.d or y-m-d
                a = g2.toInt();
                b = g3.toInt();
            } else if (g3.size() == 4) {
                y = g3.toInt(); // d.m.y or m.d.y
                a = g1.toInt();
                b = g2.toInt();
            } else if (g1.size() == 2 && g3.size() == 2) {
                y = g1.toInt(); // y.m.d (2-digit year)
                a = g2.toInt();
                b = g3.toInt();
            } else if (g2.size() <= 2) {
                // d.m.y or m.d.y with 2-digit year in the middle is odd;
                // assume first two are day/month and last is year
                y = g3.size() == 2 ? g3.toInt() : -1;
                a = g1.toInt();
                b = g2.toInt();
                if (y < 100 && y >= 0)
                    y = 2000 + y;
            }
            if (matchesTriplet(y, a, b))
                return true;
        }
    }
    return false;
}

}

QString normName(const QString& fileNameOrPath)
{

    QString s = QFileInfo(fileNameOrPath).fileName().toLower();
    s = stripSuffixes(s);
    s.replace('_', QStringLiteral(" "));
    s.replace('-', QStringLiteral(" "));
    s.replace(QRegularExpression("\\s+"), QStringLiteral(" "));
    return s.trimmed();

}

struct MatchDetail
{
    double base = 0.0;
    bool channel = false;
    bool date = false;
    bool video = false;
};

// A date (any supported layout) embedded in the name, if present.
QDate dateInName(const QString& name)
{
    const QRegularExpression dot3(
        QStringLiteral("([0-9]{1,4})\\.([0-9]{1,2})\\.([0-9]{1,4})"));
    const QRegularExpression hy3(
        QStringLiteral("([0-9]{4})-([0-9]{1,2})-([0-9]{1,2})"));
    const QRegularExpression compact8(QStringLiteral("([0-9]{8})"));

    auto fromTriplet = [](int y, int a, int b) -> QDate {
        if (y < 0 || a < 1 || a > 31 || b < 1 || b > 31)
            return QDate();
        const int year = y < 100 ? 2000 + y : y;
        QDate d1(year, a, b);
        QDate d2(year, b, a);
        if (d1.isValid())
            return d1;
        if (d2.isValid())
            return d2;
        return QDate();
    };

    const QRegularExpression* regexes[] = {&dot3, &hy3, &compact8};
    for (const QRegularExpression* re : regexes) {
        QRegularExpressionMatchIterator it = re->globalMatch(name);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            if (re == &compact8) {
                const QString n = m.captured(1);
                const int first4 = n.mid(0, 4).toInt();
                const int last4 = n.mid(4).toInt();
                QDate d;
                if (first4 >= 1970 && first4 <= 2099)
                    d = QDate::fromString(n, QStringLiteral("yyyyMMdd"));
                else if (last4 >= 1970 && last4 <= 2099)
                    d = QDate::fromString(n, QStringLiteral("ddMMyyyy"));
                if (d.isValid())
                    return d;
                continue;
            }
            const QString g1 = m.captured(1);
            const QString g2 = m.captured(2);
            const QString g3 = m.captured(3);
            QDate d;
            if (g1.size() == 4)
                d = fromTriplet(g1.toInt(), g2.toInt(), g3.toInt());
            else if (g3.size() == 4)
                d = fromTriplet(g3.toInt(), g1.toInt(), g2.toInt());
            else if (g1.size() == 2 && g3.size() == 2)
                d = fromTriplet(g1.toInt(), g2.toInt(), g3.toInt());
            else if (g3.size() >= 2)
                d = fromTriplet(g3.toInt(), g1.toInt(), g2.toInt());
            if (d.isValid())
                return d;
        }
    }
    return QDate();
}

MatchDetail matchDetail(const QString& fileName, const QString& culName,
                        const QDate& airDate)
{
    MatchDetail d;
    d.video = isVideoFile(fileName);
    const QSet<QString> a = toTokens(normName(culName));
    const QSet<QString> b = toTokens(normName(fileName));
    if (a.isEmpty() || b.isEmpty())
        return d;
    QSet<QString> inter = a;
    inter.intersect(b);
    d.base = double(inter.size() * 2) / double(a.size() + b.size());
    d.channel = sharedChannel(a, b);
    d.date = containsAirDate(QFileInfo(fileName).fileName(), airDate);
    return d;
}

QString RecordingMatcher::normalizeName(const QString& fileNameOrPath)
{
    return normName(fileNameOrPath);
}

double RecordingMatcher::score(const QString& fileName, const QString& culName,
                               const QDate& airDate)
{
    const MatchDetail d = matchDetail(fileName, culName, airDate);
    double s = d.base;
    if (d.channel)
        s += 0.25;
    if (d.date)
        s += 0.25;
    if (d.video)
        s += 0.1;
    return s;
}

bool RecordingMatcher::matches(const QString& fileName,
                               const QString& culName, const QDate& airDate)
{
    const MatchDetail d = matchDetail(fileName, culName, airDate);
    if (!d.video || d.base < 0.6)
        return false;
    const QDate culDate = dateInName(normName(culName));
    if (culDate.isValid() && !d.date)
        return false;
    return true;
}

QString RecordingMatcher::bestMatch(const QDir& dir, const QString& culName,
                                    const QDate& airDate, double threshold,
                                    double* bestScore)
{
    if (bestScore)
        *bestScore = 0.0;
    const QFileInfoList files =
        dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    const QDate culDate = dateInName(normName(culName));
    QString best;
    double bestS = 0.0;
    for (const QFileInfo& f : files) {
        const MatchDetail d = matchDetail(f.fileName(), culName, airDate);
        if (!d.video || d.base < 0.6)
            continue;
        // if the CUL name carries a date, the recording must show the same one
        if (culDate.isValid() && !d.date)
            continue;
        const double sc = d.base + (d.channel ? 0.25 : 0.0)
            + (d.date ? 0.25 : 0.0) + 0.1;
        if (sc > bestS) {
            bestS = sc;
            best = f.absoluteFilePath();
        }
    }
    if (!best.isEmpty() && bestS >= threshold) {
        if (bestScore)
            *bestScore = bestS;
        return best;
    }
    return QString();
}
