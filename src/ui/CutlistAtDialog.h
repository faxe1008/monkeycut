#pragma once

#include <QByteArray>
#include <QDialog>

#include "net/CutlistAtClient.h"

class QLineEdit;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class CutlistAtClient;

// Search cutlist.at for a cutlist and download it as CUL data.
class CutlistAtDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CutlistAtDialog(QWidget* parent = nullptr);

    const QByteArray& culData() const;
    const QString& culName() const;

private slots:
    void doSearch();
    void nextPage();
    void onResults(const SearchPage& page);
    void onItemActivated(QTreeWidgetItem* item, int column);
    void onCulReady(qint64 id, const QByteArray& data);
    void onRequestError(const QString& message);

private:
    void setStatus(const QString& text);
    void appendResults(const SearchPage& page);

    CutlistAtClient* m_client = nullptr;
    QLineEdit* m_query = nullptr;
    QTreeWidget* m_table = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_okBtn = nullptr;

    QString m_queryStr;
    int m_page = 0;
    bool m_hasMore = false;
    QTreeWidgetItem* m_activeItem = nullptr;
    qint64 m_activeId = -1;
    QByteArray m_culData;
    QString m_culName;
};
