#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QfileDialog>

MainWindow::MainWindow(Unalmas::PieLoader* pieLoader, QWidget* parent)
    : QMainWindow(parent), _ui(new Ui::MainWindow), _pieLoader { pieLoader }
{
    _ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete _ui;
}

void MainWindow::on_actionOpen_triggered()
{
    //pieLoader.
    //StartPie("C:\\Users\\andra\\source\\repos\\Unalmas_Game\\x64\\Debug\\UnalmasGameRuntime.dll");

    QString fileName = QFileDialog::getOpenFileName(
        this,		// parent
        "Open File",
        "C:\\Users\\andra\\source\\repos\\Unalmas_Game\\x64\\Debug",			// initial directory
        "Dll files (*.dll);;All files (*)" // file filter
        );

    if (!fileName.isEmpty())
    {
        _pieLoader->StartPie(fileName.toStdString());
    }
}

