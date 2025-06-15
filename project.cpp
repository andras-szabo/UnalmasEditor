#include "project.h"

Project::Project(const Unalmas::DataFile &serialized)
{
    dllPath = QString::fromStdString(serialized["DllPath"].GetString());
    const unsigned long long startupSceneCount = serialized["StartupSceneCount"].GetULong();
    for (int i = 0; i < startupSceneCount; ++i)
    {
        const std::string key = "StartupScene_" + std::to_string(i);
        const QString path = QString::fromStdString(serialized[key].GetString());
        startupScenes.push_back(path);
    }
}

Unalmas::DataFile Project::Serialize()
{
    Unalmas::DataFile data;

    data["DllPath"].SetString(dllPath.toStdString());
    data["StartupSceneCount"].SetULong(startupScenes.size());

    for (int i = 0; i < startupScenes.size(); ++i)
    {
        const std::string key = "StartupScene_" + std::to_string(i);
        data[key].SetString(startupScenes[i].toStdString());
    }

    return data;
}
