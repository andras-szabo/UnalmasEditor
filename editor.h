#ifndef EDITOR_H
#define EDITOR_H

#include <QObject>
#include <QVector>

#include "unalmas_sockets.h"
#include "unalmas_datafile.h"
#include "pieloader.h"
#include "scene.h"
#include "project.h"

namespace Unalmas
{

class Editor : public QObject
{
    Q_OBJECT

public:
    Editor();
    ~Editor();

    //TODO - this seems like it belongs to a different level of abstraction
    void CreateServerSocket();
    void WaitForPieConnection();
    void LaunchCommunicationThreads();

    void StartPie(const std::string& gameRuntimeDllPath);
    void HandleMessages(std::stop_token stopToken);

    void SetGameDllPath(const QString& path);
    void SaveProject();

signals:
    void OnPieClosed();

private:
    Unalmas::WinSockEntity _wsEntity;
    Unalmas::PieLoader _pieLoader;

    void ExtractScriptDatabase(const std::string& handshakePayload);
    std::mutex _script_database_mutex;
    Unalmas::DataFile _scriptDatabase;

    Project _project;
    QVector<Scene> _activeScenes;

    // TODO - these feel like they belong to a different level of abstraction
    Unalmas::ServerSocketConfig _serverSocketConfig;
    SOCKET _serverSocket;
    std::stop_source _networkStopSource;
    std::queue<Unalmas::TypedMessage> _inbox;
    std::queue<Unalmas::TypedMessage> _outbox;
    std::mutex _message_queue_mutex;

    QThread* _network_thread;

    std::jthread _network_jthread;
    std::jthread _handshake_thread;
    std::jthread _message_thread;
};

} // namespace Unalmas

#endif // EDITOR_H
