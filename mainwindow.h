#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "editor.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Unalmas::Editor* editor, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionOpen_triggered();
    void LaunchCommunicationThreads();

private:
    Ui::MainWindow* _ui { nullptr };
    Unalmas::Editor* _editor { nullptr };
};
#endif // MAINWINDOW_H
