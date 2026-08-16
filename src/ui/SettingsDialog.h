#pragma once

#include <QDialog>

class QComboBox;
class QLineEdit;

// v1 settings: UI language (German default / English) and cutlist.at base URL.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private:
    QComboBox* m_language = nullptr;
    QLineEdit* m_baseUrl = nullptr;
};
