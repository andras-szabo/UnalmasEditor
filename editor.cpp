#include "editor.h"
#include <qdatetime.h>
#include <qdebug.h>
#include <qlogging.h>

namespace Unalmas
{
static void SendHandshake(std::stop_token stopToken,
                          std::queue<Unalmas::TypedMessage>& outbox,
                          std::mutex& queueMutex)
{
    std::lock_guard<std::mutex> lock(queueMutex);
    outbox.push(Unalmas::TypedMessage(Unalmas::MessageType::Handshake, "-"));
}

Editor::Editor()
{
}

void Editor::CreateServerSocket()
{
    _serverSocket = _wsEntity.CreateServerSocket(_serverSocketConfig);
}

void Editor::WaitForPieConnection()
{
    SOCKET connectedClientSocket { INVALID_SOCKET };
    while (!_wsEntity.TryGetConnectedClientSocket(OUT connectedClientSocket))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Editor::LaunchCommunicationThreads()
{
    SOCKET connectedClientSocket { INVALID_SOCKET };
    _wsEntity.TryGetConnectedClientSocket(OUT connectedClientSocket);

    std::stop_source newStopSource;
    _networkStopSource.swap(newStopSource);

    _network_thread = std::jthread(Unalmas::CommunicationThread,
                                   _networkStopSource.get_token(),
                                   connectedClientSocket,
                                   std::ref(_inbox),
                                   std::ref(_outbox),
                                   std::ref(_message_queue_mutex));

    _handshake_thread = std::jthread(SendHandshake,
                                     _networkStopSource.get_token(),
                                     std::ref(_outbox),
                                     std::ref(_message_queue_mutex));

    _message_thread = std::jthread(&Editor::HandleMessages, this,
                                   _networkStopSource.get_token());
}

void Editor::StartPie(const std::string& gameRuntimeDllPath)
{
    _pieLoader.StartPie(gameRuntimeDllPath);
}

void Editor::HandleMessages(std::stop_token stopToken)
{
    qDebug() << "Handling messages yeah";

    bool shouldDisconnect { false };
    bool didFinishHandshake { false };
    while (!shouldDisconnect)
    {
        {
            std::lock_guard<std::mutex> lock(_message_queue_mutex);
            while (!_inbox.empty())
            {
                const auto& incomingMsg = _inbox.front();
                //qDebug() << "[SERVER] Incoming msg type: " << static_cast<int>(incomingMsg.type) << "\n";
                switch (incomingMsg.type)
                {
                case Unalmas::MessageType::Undefined:
                    qDebug() << "[SERVER] Incoming msg payload: " << incomingMsg.payload << "at: " << QTime::currentTime() << "\n";
                    break;

                case Unalmas::MessageType::Handshake:
                    qDebug() << "[SERVER] Incoming handshake!";
                    ExtractScriptDatabase(incomingMsg.payload);
                    didFinishHandshake = true;
                    break;

                case Unalmas::MessageType::Disconnect:
                    shouldDisconnect = true;
                    break;
                }

                _inbox.pop();
            }
        }

        if (_pieLoader.DidFinish())
        {
            shouldDisconnect = true;
        }

        //TODO - does this make sense?
        if (!shouldDisconnect)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (shouldDisconnect)
    {
        _pieLoader.GetPieStopSource().request_stop();
        _networkStopSource.request_stop();
    }

    emit OnPieClosed();
}

void Editor::SetGameDllPath(const QString& path)
{
    _project.dllPath = path;
}

void Editor::SaveProject()
{
    if (!_project.IsEmpty())
    {
        // TODO
    }
}

void Editor::ExtractScriptDatabase(const std::string& handshakePayload)
{
    if (!handshakePayload.empty() && handshakePayload != "--empty--")
    {
        std::lock_guard<std::mutex> dbLock(_script_database_mutex);
        _scriptDatabase = Unalmas::DataFile::FromString(handshakePayload);
    }

    //TODO remove
    qDebug() << _scriptDatabase.ToString();
}

} // namespace Unalmas
