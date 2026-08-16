#pragma once

#include <QByteArray>
#include <QObject>
#include <QUrl>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

// One cutlist entry from the cutlist.at search API.
struct SearchItem
{
    qint64 id = 0;
    QString name;
    QString airDate;      // ISO 8601
    QString suggestedName;
    QString otrkey;       // original recording filename
    QString channel;
    QString author;
    QString duration;     // "HH:MM:SS"
    QString quality;      // "hd" | "hq"
    int cutCount = 0;
    int hits = 0;
};

// One page of `POST /api/search-by` results.
struct SearchPage
{
    QVector<SearchItem> items;
    bool hasMore = false;
    int page = 0;
};

// Minimal read-only client for the plain-HTTP cutlist.at API:
//   GET  /api/get-info
//   POST /api/search-by
//   GET  /getfile.php?raw=1&nohit=1&id=<id>
// (Site is HTTP-only; base URL configurable for mirrors.)
class CutlistAtClient : public QObject
{
    Q_OBJECT
public:
    explicit CutlistAtClient(QObject* parent = nullptr);

    void setBaseUrl(const QUrl& url);
    QUrl baseUrl() const;

    void getInfo();
    void search(const QString& query, int page = 0);
    void downloadCul(qint64 id);

    // Offline-testable JSON mapping.
    static SearchPage parseSearchPage(const QByteArray& json);
    static bool parseInfo(const QByteArray& json, QString* name, QString* version);

signals:
    void infoReady(const QString& name, const QString& version);
    void searchResults(const SearchPage& page);
    void culReady(qint64 id, const QByteArray& data);
    void requestError(const QString& message);

private:
    QNetworkReply* send(const QUrl& url, const QByteArray& body, bool retried);

    QNetworkAccessManager* m_nam = nullptr;
    QUrl m_base = QStringLiteral("http://cutlist.at");
};
