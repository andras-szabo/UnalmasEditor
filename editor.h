#ifndef EDITOR_H
#define EDITOR_H

#include "unalmas_sockets.h"
#include "pieloader.h"

namespace Unalmas
{

class Editor
{
public:
    Editor();

    //TODO - this seems like it belongs to a different level of abstraction
    void CreateServerSocket();
    void WaitForPieConnection();
    void LaunchCommunicationThreads();

    void StartPie(const std::string& gameRuntimeDllPath);
    void HandleMessages(std::stop_token stopToken);

private:
    Unalmas::WinSockEntity _wsEntity;
    Unalmas::PieLoader _pieLoader;

    // TODO - these feel like they belong to a different level of abstraction
    Unalmas::ServerSocketConfig _serverSocketConfig;
    SOCKET _serverSocket;
    std::stop_source _networkStopSource;
    std::queue<Unalmas::TypedMessage> _inbox;
    std::queue<Unalmas::TypedMessage> _outbox;
    std::mutex _message_queue_mutex;

    std::jthread _network_thread;
    std::jthread _handshake_thread;
    std::jthread _message_thread;
};

} // namespace Unalmas

#endif // EDITOR_H
