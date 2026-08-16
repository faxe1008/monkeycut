#include "SettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QVBoxLayout>

namespace
{
QString languageCodeFromIndex(const QComboBox& box, int index)
{
    return box.itemData(index).toString();
}
}

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));

    QSettings s;
    const QString lang = s.value(QStringLiteral("ui/language"),
                                 QStringLiteral("de")).toString();
    const QString url = s.value(
        QStringLiteral("ui/cutlistatBaseURL"),
        QStringLiteral("http://cutlist.at")).toString();

    m_language = new QComboBox(this);
    m_language->addItem(tr("German"), QStringLiteral("de"));
    m_language->addItem(tr("English"), QStringLiteral("en"));
    const int cur = m_language->findData(lang);
    m_language->setCurrentIndex(cur >= 0 ? cur : 0);

    m_baseUrl = new QLineEdit(url, this);

    auto* restartNote = new QLabel(
        tr("Language changes are applied after restarting MonkeyCut."),
        this);
    restartNote->setWordWrap(true);

    auto* form = new QFormLayout;
    form->addRow(tr("Language:"), m_language);
    form->addRow(tr("cutlist.at server:"), m_baseUrl);
    form->addRow(QString(), restartNote);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::accepted, this,
            [this]() {
                QSettings s;
                s.setValue(QStringLiteral("ui/language"),
                           languageCodeFromIndex(*m_language,
                                                 m_language->currentIndex()));
                s.setValue(QStringLiteral("ui/cutlistatBaseURL"),
                           m_baseUrl->text().trimmed());
                accept();
            });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    resize(460, 160);
}
