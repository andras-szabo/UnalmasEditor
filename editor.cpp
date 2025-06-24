#include "editor.h"
#include <iostream>
#include <qdatetime.h>
#include <qdebug.h>
#include <qlogging.h>
#include <qthread.h>

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

Editor::~Editor()
{
    _networkStopSource.request_stop();
    qDebug() << "Editor destroyed.\n";
}

void Editor::CreateServerSocket()
{
    _serverSocket = _wsEntity.CreateServerSocket(_serverSocketConfig);
}

void Editor::WaitForPieConnection()
{
    qInfo() << "wait for pie connection";
    SOCKET connectedClientSocket { INVALID_SOCKET };
    while (!_wsEntity.TryGetConnectedClientSocket(OUT connectedClientSocket))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    qInfo() << "connected client socket got.";
}

void Editor::LaunchCommunicationThreads()
{
    SOCKET connectedClientSocket { INVALID_SOCKET };
    _wsEntity.TryGetConnectedClientSocket(OUT connectedClientSocket);

    std::stop_source newStopSource;
    _networkStopSource.swap(newStopSource);

    _network_jthread = std::jthread(Unalmas::CommunicationThread,
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
    qInfo() << "Handling messages yeah";

    bool shouldDisconnect { false };
    bool didFinishHandshake { false };
    while (!shouldDisconnect)
    {
        {
            std::lock_guard<std::mutex> lock(_message_queue_mutex);
            while (!_inbox.empty())
            {
                const auto& incomingMsg = _inbox.front();
                //qInfo() << "[SERVER] Incoming msg type: " << static_cast<int>(incomingMsg.type) << "\n";
                switch (incomingMsg.type)
                {
                case Unalmas::MessageType::Undefined:
                    qInfo() << "[SERVER] Incoming msg payload: " << incomingMsg.payload << "at: " << QTime::currentTime() << "\n";
                    break;

                case Unalmas::MessageType::Handshake:
                    qInfo() << "[SERVER] Incoming handshake!";
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

    qInfo() << "Pie closed.";
    emit OnPieClosed();
}

void Editor::SetGameDllPath(const QString& path)
{
    _project.dllPath = path;
}

void Editor::SaveProject()
{
    /*
    if (!_project.IsEmpty())
    {
        qDebug() << "Trying to save project.\n";
        const auto project = _project.Serialize();
        qDebug() << project.ToString();
    }
    else
    {
        qDebug() << "_project is empty.";
    }*/
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
