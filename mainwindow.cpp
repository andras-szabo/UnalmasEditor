#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QfileDialog>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>

MainWindow::MainWindow(Unalmas::Editor* editor, QWidget* parent)
    : QMainWindow(parent), _ui(new Ui::MainWindow), _editor { editor }
{
    _ui->setupUi(this);

    EnableSaveProjectMenu(false);

    connect(editor, &Unalmas::Editor::OnPieClosed,
            this, [&]{ EnableSaveProjectMenu(false); });

    Init();
}

MainWindow::~MainWindow()
{
    delete _ui;
}

void MainWindow::Init()
{
    _editor->CreateServerSocket();
}

void MainWindow::LaunchCommunicationThreads()
{
    _editor->LaunchCommunicationThreads();
}

void MainWindow::EnableSaveProjectMenu(bool state)
{
    _ui->actionSave_Project->setEnabled(state);
}

void MainWindow::on_actionSave_Project_triggered()
{
    _editor->SaveProject();
}

void MainWindow::on_actionOpen_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,		// parent
        "Open File",
        "", // "C:\\Users\\andra\\source\\repos\\Unalmas_Game\\x64\\Debug",			// initial directory
        "UDF files (*.udf);;Dll files (*.dll);;All files (*)" // file filter
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

        connect(dialog, &QDialog::accepted, dialog, &QObject::deleteLater);

        QFuture<void> future = QtConcurrent::run([&, fileName]()
        {
            //qInfo() << "Waiting for pie connection...\n";
            _editor->WaitForPieConnection();
            //qInfo() << "Pie connected.\n";
            _editor->SetGameDllPath(fileName);
        });

        auto* watcher = new QFutureWatcher<void>();

        connect(watcher, &QFutureWatcher<void>::finished, this,[&]{ EnableSaveProjectMenu(true); });
        connect(watcher, &QFutureWatcher<void>::finished, this, &MainWindow::LaunchCommunicationThreads);
        connect(watcher, &QFutureWatcher<void>::finished, dialog, &QObject::deleteLater);
        connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);

        watcher->setFuture(future);
    }
}

