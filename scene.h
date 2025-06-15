#ifndef SCENE_H
#define SCENE_H

#include <QString>
#include "unalmas_guid.h"

class Scene
{
public:
    Scene();

    QString Path() const;

private:
    Unalmas::GUID guid;

    QString name;
    QString path;

};

#endif // SCENE_H
