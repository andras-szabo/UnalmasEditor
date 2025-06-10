#include <QApplication>

#include "mainwindow.h"
#include "editor.h"


int main(int argc, char *argv[])
{
    Unalmas::Editor editorInstance;

    QApplication a(argc, argv);
    MainWindow w(&editorInstance);

    w.show();

    auto err = a.exec();

    return err;
}
