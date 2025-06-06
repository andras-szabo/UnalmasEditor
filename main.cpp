#include <QApplication>

#include "mainwindow.h"
#include "unalmas_sockets.h"
#include "pieloader.h"

void SendHandshakeAndWaitForResponse(std::stop_token stopToken,
                                     std::queue<Unalmas::TypedMessage>& inbox,
                                     std::queue<Unalmas::TypedMessage>& outbox,
                                     std::mutex& queueMutex)
{
    // Step 1: Place the handshake message in the outbox
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        outbox.push(Unalmas::TypedMessage(Unalmas::MessageType::Handshake, "-"));
    }

    // Step 2: Periodically check the inbox for a response
    constexpr int pollIntervalMs = 10;
    bool handshakeComplete = false;

    while (!handshakeComplete && !stopToken.stop_requested())
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (!inbox.empty())
        {
            const auto& incomingMsg = inbox.front();
            if (incomingMsg.type == Unalmas::MessageType::Handshake)
            {
                {
                    //TODO - extract script database
                    // std::lock_guard<std::mutex> dbLock(scriptDbMutex);
                    qDebug() << "Would extract script database now.";
                }

                inbox.pop();
                handshakeComplete = true;

                // TODO - maybe we don't need this; or the !handshakeComplete check in the while
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
    }

    // Step 3: Handle stop request or handshake completion
    if (stopToken.stop_requested())
    {
        // TODO
        qDebug() << "Handshake interrupted by stop request.";
    }
    else if (handshakeComplete)
    {
        qDebug() << "Handshake complete.";
    }
    else
    {
        qDebug() << "Handshake failed or timed out.";
    }
}


int main(int argc, char *argv[])
{
    Unalmas::WinSockEntity wsEntity;
    if (wsEntity.GetLastError() != 0)
    {
        return EXIT_FAILURE;
    }

    // Create server socket and start listening
    constexpr int port = 5555;
    constexpr int sendBufSize = 65536;
    constexpr bool isBlocking = false;
    constexpr bool startListening = true;

    auto serverSocket = wsEntity.CreateServerSocket(port,
                                                    sendBufSize,
                                                    isBlocking,
                                                    startListening);

    qDebug() << "Started server";

    Unalmas::PieLoader pieLoader;

    QApplication a(argc, argv);
    MainWindow w(&pieLoader);

    w.show();

    auto err = a.exec();

    //networkStopSource.request_stop();

    return err;

    /*
    pieLoader.StartPie("C:\\Users\\andra\\source\\repos\\Unalmas_Game\\x64\\Debug\\UnalmasGameRuntime.dll");
    qDebug() << "PIE started.";

    // Wait for PIE to connect
    SOCKET connectedClientSocket { INVALID_SOCKET };
    while (!wsEntity.TryGetConnectedClientSocket(OUT connectedClientSocket))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    qDebug() << "PIE connected.";

    std::queue<Unalmas::TypedMessage> inbox;
    std::queue<Unalmas::TypedMessage> outbox;
    std::mutex message_queue_mutex;

    std::stop_source networkStopSource;

    std::jthread network_thread(Unalmas::CommunicationThread,
                                networkStopSource.get_token(),
                                connectedClientSocket,
                                std::ref(inbox),
                                std::ref(outbox),
                                std::ref(message_queue_mutex));

    qDebug() << "Launched communication thread.";

    std::jthread handshake_thread(SendHandshakeAndWaitForResponse,
                                  networkStopSource.get_token(),
                                  std::ref(inbox),
                                  std::ref(outbox),
                                  std::ref(message_queue_mutex));

    qDebug() << "Launched handshake thread.";

    // And now
    bool shouldDisconnect = false;
    bool didFinishHandshake = false;

    while (!shouldDisconnect)
    {
        {
            std::lock_guard<std::mutex> lock(message_queue_mutex);
            while (!inbox.empty())
            {
                const auto& incomingMsg = inbox.front();
                qDebug() << "[SERVER] Incoming msg type: " << static_cast<int>(incomingMsg.type);
                switch (incomingMsg.type)
                {
                case Unalmas::MessageType::Undefined:
                    qDebug() << "[SERVER] Incoming msg payload: " << incomingMsg.payload;
                    break;

                case Unalmas::MessageType::Handshake:
                    didFinishHandshake = true;
                    break;

                case Unalmas::MessageType::Disconnect:
                    shouldDisconnect = true;
                    break;
                }

                inbox.pop();
            }
        }

        if (pieLoader.DidFinish())
        {
            shouldDisconnect = true;
        }


       //TODO - does this make sense?
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
*/

}
