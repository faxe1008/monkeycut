#include "CutlistAtClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

CutlistAtClient::CutlistAtClient(QObject* parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    m_nam->setTransferTimeout(10000);
}

void CutlistAtClient::setBaseUrl(const QUrl& url)
{
    m_base = url;
}

QUrl CutlistAtClient::baseUrl() const
{
    return m_base;
}

QNetworkReply* CutlistAtClient::send(const QUrl& url, const QByteArray& body,
                                     bool retried)
{
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "MonkeyCut/0.1");
    if (!body.isEmpty())
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    QNetworkReply* reply =
        body.isEmpty() ? m_nam->get(req) : m_nam->post(req, body);
    reply->setProperty("mcutRetried", retried);
    return reply;
}

void CutlistAtClient::getInfo()
{
    QNetworkReply* reply =
        send(m_base.resolved(QStringLiteral("/api/get-info")), {}, false);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        const bool retried = reply->property("mcutRetried").toBool();
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (!retried) {
                qDebug() << "cutlist.at request failed, retrying once:"
                         << reply->errorString();
                getInfo();
            } else {
                emit requestError(reply->errorString());
            }
            return;
        }
        QString name, version;
        if (parseInfo(data, &name, &version))
            emit infoReady(name, version);
        else
            emit requestError(
                QStringLiteral("Unexpected get-info response"));
    });
}

void CutlistAtClient::search(const QString& query, int page)
{
    QJsonObject cond;
    cond[QStringLiteral("query")] = query;
    cond[QStringLiteral("field")] = QStringLiteral("name");
    QJsonArray conds;
    conds.append(cond);
    QJsonObject root;
    root[QStringLiteral("conds")] = conds;
    root[QStringLiteral("isOrConnection")] = false;
    root[QStringLiteral("sortBy")] = QStringLiteral("date");
    root[QStringLiteral("isAsc")] = false;
    root[QStringLiteral("page")] = page;
    const QByteArray body = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QNetworkReply* reply =
        send(m_base.resolved(QStringLiteral("/api/search-by")), body, false);
    reply->setProperty("mcutQuery", query);
    reply->setProperty("mcutPage", page);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        const bool retried = reply->property("mcutRetried").toBool();
        const QString query = reply->property("mcutQuery").toString();
        const int page = reply->property("mcutPage").toInt();
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (!retried) {
                qDebug() << "cutlist.at search failed, retrying once:"
                         << reply->errorString();
                search(query, page);
            } else {
                emit requestError(reply->errorString());
            }
            return;
        }
        emit searchResults(parseSearchPage(data));
    });
}

void CutlistAtClient::downloadCul(qint64 id)
{
    const QUrl url = m_base.resolved(
        QStringLiteral("/getfile.php?raw=1&nohit=1&id=%1").arg(id));
    QNetworkReply* reply = send(url, {}, false);
    reply->setProperty("mcutCulId", id);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        const bool retried = reply->property("mcutRetried").toBool();
        const qint64 id = reply->property("mcutCulId").toLongLong();
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (!retried) {
                qDebug() << "cutlist.at download failed, retrying once:"
                         << reply->errorString();
                downloadCul(id);
            } else {
                emit requestError(reply->errorString());
            }
            return;
        }
        emit culReady(id, data);
    });
}

SearchPage CutlistAtClient::parseSearchPage(const QByteArray& json)
{
    SearchPage page;
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return page;
    const QJsonObject root = doc.object();
    page.hasMore = root.value(QStringLiteral("hasMore")).toBool();
    page.page = root.value(QStringLiteral("currentPage")).toInt();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    page.items.reserve(items.size());
    for (const auto& v : items) {
        const QJsonObject o = v.toObject();
        SearchItem it;
        it.id = o.value(QStringLiteral("id")).toVariant().toLongLong();
        it.name = o.value(QStringLiteral("name")).toString();
        it.airDate = o.value(QStringLiteral("airDate")).toString();
        it.suggestedName = o.value(QStringLiteral("suggestedName")).toString();
        it.otrkey = o.value(QStringLiteral("otrkey")).toString();
        it.channel = o.value(QStringLiteral("channel")).toString();
        it.author = o.value(QStringLiteral("author")).toString();
        it.duration = o.value(QStringLiteral("duration")).toString();
        it.quality = o.value(QStringLiteral("quality")).toString();
        it.cutCount = o.value(QStringLiteral("cutCount")).toInt(0);
        it.hits = o.value(QStringLiteral("hits")).toInt(0);
        page.items.append(it);
    }
    return page;
}

bool CutlistAtClient::parseInfo(const QByteArray& json, QString* name,
                                QString* version)
{
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return false;
    const QJsonObject o = doc.object();
    *name = o.value(QStringLiteral("name")).toString();
    *version = o.value(QStringLiteral("version")).toString();
    return !name->isEmpty();
}
