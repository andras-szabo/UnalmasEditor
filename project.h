#ifndef PROJECT_H
#define PROJECT_H

#include <QString>
#include <QVector>

#include "unalmas_datafile.h"

struct Project
{
    Project() = default;
    Project(const Unalmas::DataFile& serialized);

    QString dllPath;
    QVector<QString> startupScenes;

    bool IsEmpty() const;
    Unalmas::DataFile Serialize() const;
};

#endif // PROJECT_H
