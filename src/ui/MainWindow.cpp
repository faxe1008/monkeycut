#include "ui/MainWindow.h"

#include <QApplication>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("MonkeyCut");
    resize(1100, 720);

    auto* fileMenu = menuBar()->addMenu(tr("File"));
    fileMenu->addAction(tr("Open…"), this, [] {});
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Quit"), this, &QWidget::close);

    auto* central = new QLabel(this);
    central->setAlignment(Qt::AlignCenter);
    central->setText(tr("MonkeyCut %1 — M0 smoke").arg(QApplication::applicationVersion()));
    setCentralWidget(central);

    statusBar()->showMessage(tr("ready"));
}