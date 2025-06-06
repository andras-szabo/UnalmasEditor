#include <QApplication>

#include "mainwindow.h"
#include "unalmas_sockets.h"


void StartPieProcess(std::stop_token stopToken,
                     const std::string& gameProjectPath,
                     std::mutex& pieMutex,
                     OUT bool& didPieFinish)
{
    LPCWSTR applicationName = L"C:\\Users\\andra\\source\\repos\\Unalmas\\x64\\Debug\\PIE.exe";
    STARTUPINFO startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo;
    ZeroMemory(&processInfo, sizeof(processInfo));

    const unsigned long long newSizeW = gameProjectPath.size() + 1;
    std::size_t convertedChars = 0;
    wchar_t* wcString = new wchar_t[newSizeW];
    mbstowcs_s(&convertedChars, wcString, newSizeW, gameProjectPath.c_str(), _TRUNCATE);

    BOOL couldCreateProcess = CreateProcess(
        applicationName,		// application name
        wcString,				// command line arguments
        NULL,					// process security attributes
        NULL,					// primary thread security attributes
        FALSE,					// handles are not inherited
        0,						// creation flags
        NULL,					// use parent's environment block
        NULL,					// use parent's starting directory
        &startupInfo,
        &processInfo);

    delete[] wcString;

    if (couldCreateProcess)
    {
        constexpr DWORD piePollDurationMS = 1000;
        bool didProcessExit { false };
        while (!didProcessExit && !stopToken.stop_requested())
        {
            const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, piePollDurationMS);
            if (waitResult == WAIT_TIMEOUT)
            {
                continue;
            }

            if (waitResult == WAIT_OBJECT_0)
            {
                didProcessExit = true;
            }
            else
            {
                // TODO
                // WaitForSingleObject failed
            }
        }

        if (!didProcessExit)
        {
            // Try to terminate child process
            constexpr unsigned int exitCode = 1u;
            TerminateProcess(processInfo.hProcess, exitCode);
        }

        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);

        {
            std::lock_guard<std::mutex> lock(pieMutex);
            didPieFinish = true;
        }
    }
    else
    {
        qDebug() << "Couldn't create PIE process.";
    }
}

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

    // Start the PIE process, send it params via sockets
    std::mutex pieMutex;
    bool didPieFinish = false;
    std::stop_source pieStopSource;
    std::jthread thread_pie(StartPieProcess,
                            pieStopSource.get_token(),
                            "C:\\Users\\andra\\source\\repos\\Unalmas_Game\\x64\\Debug\\UnalmasGameRuntime.dll",
                            std::ref(pieMutex),
                            OUT std::ref(didPieFinish));

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

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
