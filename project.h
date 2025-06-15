#ifndef PROJECT_H
#define PROJECT_H

#include <QString>
#include <QVector>

#include "unalmas_datafile.h"

struct Project
{
    Project(const Unalmas::DataFile& serialized);

    QString dllPath;
    QVector<QString> startupScenes;

    Unalmas::DataFile Serialize();
};

#endif // PROJECT_H
