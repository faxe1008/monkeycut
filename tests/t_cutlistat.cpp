#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "net/CutlistAtClient.h"

namespace
{
// Trimmed copy of an observed /api/search-by response.
const char* kSampleSearch =
    "{ \"items\": [ "
    "{ \"id\": 2077248, \"name\": \"Die Simpsons  Mission Simpossible\", "
    "\"airDate\": \"2026-05-29T05:05:00+02:00\", "
    "\"uploadDate\": \"2026-08-04T22:58:28+02:00\", "
    "\"otrkey\": \"Die_Simpsons__Mission_Simpossible.mpg.HD.avi\", "
    "\"comment\": \"Mit ColdCut geschnitten\", "
    "\"suggestedName\": \"Die_Simpsons__Mission_Simpossible\", "
    "\"channel\": \"orf1\", \"author\": \"b-andi-t\", "
    "\"rating\": {\"avg\": \"0.00\", \"avgRounded\": 0, \"ratings\": 0}, "
    "\"registeredDownloads\": \"0\", \"hits\": 26, "
    "\"duration\": \"00:00:00\", \"quality\": \"hd\", \"cutCount\": 1, "
    "\"errors\": {\"start\": true, \"end\": true} }, "
    "{ \"id\": 12, \"name\": \"OnlyName\" } "
    "], \"hasMore\": true, \"currentPage\": 0 }";
}

class TCutlistAt : public QObject
{
    Q_OBJECT

private slots:
    void parseSearchPage()
    {
        const SearchPage page =
            CutlistAtClient::parseSearchPage(QByteArray(kSampleSearch));
        QCOMPARE(page.items.size(), 2);
        QCOMPARE(page.hasMore, true);
        QCOMPARE(page.page, 0);

        const SearchItem& a = page.items[0];
        QCOMPARE(a.id, qint64(2077248));
        QCOMPARE(a.name, QString::fromUtf8("Die Simpsons  Mission Simpossible"));
        QCOMPARE(a.airDate, QStringLiteral("2026-05-29T05:05:00+02:00"));
        QCOMPARE(a.channel, QStringLiteral("orf1"));
        QCOMPARE(a.author, QStringLiteral("b-andi-t"));
        QCOMPARE(a.quality, QStringLiteral("hd"));
        QCOMPARE(a.cutCount, 1);
        QCOMPARE(a.hits, 26);
        QCOMPARE(a.suggestedName,
                 QStringLiteral("Die_Simpsons__Mission_Simpossible"));
        QCOMPARE(a.otrkey,
                 QStringLiteral("Die_Simpsons__Mission_Simpossible.mpg.HD.avi"));

        // sparse item: defaults stay sensible
        const SearchItem& b = page.items[1];
        QCOMPARE(b.id, qint64(12));
        QCOMPARE(b.name, QStringLiteral("OnlyName"));
        QCOMPARE(b.channel, QString());
        QCOMPARE(b.cutCount, 0);
    }

    void parseSearchPageGarbage()
    {
        const SearchPage page =
            CutlistAtClient::parseSearchPage(QByteArray("not json"));
        QVERIFY(page.items.isEmpty());
        QVERIFY(!page.hasMore);

        const SearchPage empty = CutlistAtClient::parseSearchPage("[]");
        QVERIFY(empty.items.isEmpty());
    }

    void parseInfo()
    {
        QString name, version;
        QVERIFY(CutlistAtClient::parseInfo(
            QByteArray("{\"name\":\"cutlist\",\"version\":\"1.0.54\"}"),
            &name, &version));
        QCOMPARE(name, QStringLiteral("cutlist"));
        QCOMPARE(version, QStringLiteral("1.0.54"));

        name.clear();
        QVERIFY(!CutlistAtClient::parseInfo("nope", &name, &version));
    }

    // Live round-trip; skipped when the site is unreachable (offline CI).
    void liveRoundTrip()
    {
        CutlistAtClient client;
        QSignalSpy infoSpy(&client, &CutlistAtClient::infoReady);
        QSignalSpy errSpy(&client, &CutlistAtClient::requestError);
        client.getInfo();
        if (!infoSpy.wait(15000)) {
            QSKIP("cutlist.at unreachable from this environment");
        }
        QCOMPARE(errSpy.count(), 0);
        QVERIFY(!infoSpy.first().at(0).toString().isEmpty());

        QSignalSpy searchSpy(&client, &CutlistAtClient::searchResults);
        client.search(QStringLiteral("Simpsons"));
        if (!searchSpy.wait(15000)) {
            QSKIP("cutlist.at search failed from this environment");
        }
        const SearchPage page =
            searchSpy.first().at(0).value<SearchPage>();
        QVERIFY(!page.items.isEmpty());

        QSignalSpy culSpy(&client, &CutlistAtClient::culReady);
        client.downloadCul(page.items.first().id);
        if (!culSpy.wait(15000)) {
            QSKIP("cutlist.at download failed from this environment");
        }
        const QByteArray cul = culSpy.first().at(1).toByteArray();
        QVERIFY(cul.contains("[General]"));
        QVERIFY(cul.contains("NoOfCuts="));
    }
};

QTEST_MAIN(TCutlistAt)

#include "t_cutlistat.moc"
