#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QfileDialog>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>

MainWindow::MainWindow(Unalmas::Editor* editor, QWidget* parent)
    : QMainWindow(parent), _ui(new Ui::MainWindow), _editor { editor }
{
    _ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete _ui;
}

void MainWindow::LaunchCommunicationThreads()
{
    _editor->LaunchCommunicationThreads();
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
        _editor->StartPie(fileName.toStdString());

        auto* dialog = new QProgressDialog("Please wait...",
                                           QString(),
                                           0,
                                           0,
                                           this);

        dialog->setWindowModality(Qt::ApplicationModal);
        dialog->setCancelButton(nullptr);
        dialog->setMinimumDuration(0);
        dialog->setWindowTitle("Loading...");
        dialog->show();

        QFuture<void> future = QtConcurrent::run([&]
        {
            qDebug() << "Waiting for pie connection...\n";
            _editor->WaitForPieConnection();
            qDebug() << "Pie connected.\n";
        });

        //dialog->accept();
        //dialog->deleteLater();

        //_editor->LaunchCommunicationThreads();

        auto* watcher = new QFutureWatcher<void>();

        connect(watcher, &QFutureWatcher<void>::finished, dialog, &QDialog::accept);
        connect(watcher, &QFutureWatcher<void>::finished, this, &MainWindow::LaunchCommunicationThreads);
        connect(dialog, &QDialog::accepted, dialog, &QObject::deleteLater);
        connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

        watcher->setFuture(future);
    }
}

