#include "CutlistAtDialog.h"

#include "net/CutlistAtClient.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

CutlistAtDialog::CutlistAtDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("cutlist.at"));

    m_client = new CutlistAtClient(this);
    connect(m_client, &CutlistAtClient::searchResults, this,
            &CutlistAtDialog::onResults);
    connect(m_client, &CutlistAtClient::culReady, this,
            &CutlistAtDialog::onCulReady);
    connect(m_client, &CutlistAtClient::requestError, this,
            &CutlistAtDialog::onRequestError);

    auto* row = new QHBoxLayout;
    m_query = new QLineEdit(this);
    m_query->setPlaceholderText(tr("Show or episode name…"));
    auto* searchBtn = new QPushButton(tr("Search"), this);
    connect(searchBtn, &QPushButton::clicked, this, &CutlistAtDialog::doSearch);
    connect(m_query, &QLineEdit::returnPressed, this, &CutlistAtDialog::doSearch);
    row->addWidget(m_query, 1);
    row->addWidget(searchBtn);

    m_table = new QTreeWidget(this);
    m_table->setRootIsDecorated(false);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->header()->setStretchLastSection(true);
    m_table->setHeaderLabels({tr("Name"), tr("Channel"), tr("Air date"),
                              tr("Qual."), tr("Cuts"), tr("Duration")});
    m_table->header()->resizeSection(0, 420);
    connect(m_table, &QTreeWidget::itemActivated, this,
            &CutlistAtDialog::onItemActivated);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    setStatus(tr("Search the cutlist.at database. Double-click a result to "
                 "download its CUL file.\nNote: the site is HTTP-only, so the "
                 "transfer is not encrypted."));

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Close, Qt::Horizontal, this);
    m_okBtn = buttons->button(QDialogButtonBox::Ok);
    m_okBtn->setEnabled(false);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(row);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_status);
    layout->addWidget(buttons);
    resize(860, 420);
}

const QByteArray& CutlistAtDialog::culData() const
{
    return m_culData;
}

const QString& CutlistAtDialog::culName() const
{
    return m_culName;
}

QDate CutlistAtDialog::airDate() const
{
    return m_airDate;
}

void CutlistAtDialog::setStatus(const QString& text)
{
    m_status->setText(text);
}

void CutlistAtDialog::doSearch()
{
    const QString query = m_query->text().trimmed();
    if (query.isEmpty())
        return;
    m_queryStr = query;
    m_page = 0;
    m_hasMore = false;
    m_table->clear();
    setStatus(tr("Searching “%1”…").arg(query));
    m_client->search(query, 0);
}

void CutlistAtDialog::nextPage()
{
    if (m_page < 0 || !m_hasMore)
        return;
    ++m_page;
    m_client->search(m_queryStr, m_page);
}

void CutlistAtDialog::onResults(const SearchPage& page)
{
    appendResults(page);
    m_hasMore = page.hasMore;
    m_page = page.page;
    const int n = m_table->topLevelItemCount();
    if (n == 0)
        setStatus(tr("No cutlists found for “%1”.").arg(m_queryStr));
    else
        setStatus(tr("%1 cutlists found for “%2”. Double-click to download.")
                      .arg(n)
                      .arg(m_queryStr));
}

void CutlistAtDialog::appendResults(const SearchPage& page)
{
    m_table->setUpdatesEnabled(false);
    for (const SearchItem& it : page.items) {
        auto* item = new QTreeWidgetItem(m_table);
        item->setText(0, it.name);
        item->setText(1, it.channel);
        item->setText(2, it.airDate.mid(0, 10));
        item->setText(3, it.quality);
        item->setText(4, it.cutCount > 0 ? QString::number(it.cutCount)
                                          : QStringLiteral("–"));
        item->setText(5, it.duration);
        item->setData(0, Qt::UserRole, it.id);
        item->setData(0, Qt::UserRole + 1, it.suggestedName);
        item->setData(0, Qt::UserRole + 2, it.otrkey);
        item->setData(0, Qt::UserRole + 3, it.airDate);
    }
    // "Next page" entry at the bottom
    auto* more = new QTreeWidgetItem(m_table);
    more->setText(0, page.hasMore ? tr("… load more …")
                                  : tr("… no more results …"));
    more->setData(0, Qt::UserRole, page.hasMore ? -1 : 0);
    if (!page.hasMore)
        more->setFlags(more->flags() & ~Qt::ItemIsSelectable
                       & ~Qt::ItemIsEnabled);
    m_table->setUpdatesEnabled(true);
}

void CutlistAtDialog::onItemActivated(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    const qint64 id = item->data(0, Qt::UserRole).toLongLong();
    if (id == -1) {
        nextPage(); // "load more" row
        return;
    }
    if (id <= 0)
        return;
    m_activeItem = item;
    m_activeId = id;
    setStatus(tr("Downloading cutlist “%1”…")
                  .arg(item->text(0)));
    m_client->downloadCul(id);
}

void CutlistAtDialog::onCulReady(qint64 id, const QByteArray& data)
{
    Q_UNUSED(id)
    m_culData = data;
    m_airDate = m_activeItem
                    ? QDate::fromString(m_activeItem->data(0, Qt::UserRole + 3)
                                             .toString(),
                                         QStringLiteral("yyyy-MM-dd"))
                    : QDate();
    m_okBtn->setEnabled(true);
    m_culName = m_activeItem
                    ? (m_activeItem->data(0, Qt::UserRole + 1).toString()
                           .isEmpty()
                                 ? m_activeItem->text(0)
                                 : m_activeItem->data(0, Qt::UserRole + 1)
                                       .toString())
                    : QStringLiteral("cutlist.cul");
    setStatus(tr("Cutlist “%1” downloaded – click OK to apply it to the "
                 "open video.")
                  .arg(m_culName));
}

void CutlistAtDialog::onRequestError(const QString& message)
{
    setStatus(tr("cutlist.at error: %1").arg(message));
}
