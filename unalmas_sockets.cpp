#include "unalmas_sockets.h"

//TODO remove if possible
#include <cassert>
#include <iostream>

#include <cstring>
#include <qdatetime.h>
#include <qdebug.h>
#include <qlogging.h>
#include <windows.h>

namespace Unalmas
{

bool TryExtractHeader(char* incomingBuffer,
                      int& readPosition,
                      int bytesReceived,
                      MessageHeader* incomingMessageHeader,
                      std::vector<char>& overflowBuffer)
{
    // TODO - how does _exactly_ this work? Overflow in what sense?
    if (overflowBuffer.size() > 0)
    {
        // No matter what, we expect the header to fit into 2 incoming buffers at most,
        // so if we are overflowing, then we have to use fewer number of bits.
        const int firstPartSize = overflowBuffer.size();
        const int leftoverSize = sizeof(MessageHeader) - firstPartSize;
        std::memcpy(incomingMessageHeader, overflowBuffer.data(), firstPartSize);
        std::memcpy(incomingMessageHeader + firstPartSize, incomingBuffer, leftoverSize);
        if (!incomingMessageHeader->IsValid())
        {
            throw std::runtime_error("Received invalid header after overflow.");
        }

        readPosition = leftoverSize;
        overflowBuffer.clear();
        return true;
    }

    if (readPosition + sizeof(MessageHeader) <= bytesReceived)
    {
        std::memcpy(incomingMessageHeader, incomingBuffer + readPosition, sizeof(MessageHeader));
        if (!incomingMessageHeader->IsValid())
        {
            throw std::runtime_error("Received invalid header.");
        }
        readPosition += sizeof(MessageHeader);
        return true;
    }

    // readPosition + sizeof(MessageHeader) > bytesReceived, meaning that
    // we did receive the first part of a header, but not all of it.
    // so in this case, we'll have to use what's right now called the overflow
    // buffer - perhaps a better name would be ... better
    assert(overflowBuffer.size() == 0);
    std::copy(incomingBuffer + readPosition, incomingBuffer + bytesReceived, overflowBuffer.begin());
    readPosition = bytesReceived;

    return false;
}

void CommunicationThread(std::stop_token stopToken,
                         SOCKET socket,
                         std::queue<Unalmas::TypedMessage>& inbox,
                         std::queue<Unalmas::TypedMessage>& outbox,
                         std::mutex& queueMutex)
{
    constexpr int incomingBufferSizeBytes = 8192;
    char incomingBuffer[incomingBufferSizeBytes];

    std::vector<char> overflowBuffer;
    overflowBuffer.reserve(incomingBufferSizeBytes);

    MessageHeader incomingMessageHeader;
    MessageHeader outgoingMessageHeader;

    int bytesSentSoFar { 0 };

    WSAPOLLFD pollfd;
    pollfd.fd = socket;
    pollfd.events = POLLRDNORM | POLLWRNORM;

    while (!stopToken.stop_requested())
    {
        const int result = WSAPoll(&pollfd, 1, 100);

        if (result == SOCKET_ERROR)
        {
            std::cerr << "WSAPoll failed: " << WSAGetLastError() << "\n";
            break;
        }

        if (result == 0)
        {
            continue;
        }

        if (pollfd.revents & POLLRDNORM)
        {
        int bytesReceived = recv(socket, incomingBuffer, incomingBufferSizeBytes, 0);
        if (bytesReceived == 0 && WSAGetLastError() != WSAEWOULDBLOCK)
        {
            std::cerr << "Receive error: " << WSAGetLastError() << "\n";
            break;
        }

        // First deal with bytes received
        if (bytesReceived > 0)
        {
            int readPosition = 0;
            while (readPosition < bytesReceived)
            {
                // Extract a header, if we need to.
                if (incomingMessageHeader.IsValid() == false)
                {
                    if (!TryExtractHeader(incomingBuffer,
                                          readPosition,
                                          bytesReceived,
                                          &incomingMessageHeader,
                                          overflowBuffer))
                    {
                        // Couldn't extract a header => will have to recv again.
                        assert(readPosition == bytesReceived);
                        break;
                    }
                }

                assert(incomingMessageHeader.IsValid());

                // If we are overflowing already, then we have to append to the overflow buffer
                if (overflowBuffer.size() > 0)
                {
                    const int outstandingBytes = incomingMessageHeader.messageSize - overflowBuffer.size();
                    const int unreadBytes = bytesReceived - readPosition;
                    const int messageBytes = outstandingBytes < unreadBytes ? outstandingBytes : unreadBytes;
                    overflowBuffer.insert(overflowBuffer.end(),
                                          incomingBuffer + readPosition,
                                          incomingBuffer + readPosition + messageBytes);

                    if (overflowBuffer.size() == incomingMessageHeader.messageSize)
                    {
                        {
                            std::lock_guard<std::mutex> lock(queueMutex);
                            inbox.push(TypedMessage(incomingMessageHeader.messageType,
                                                    std::string(overflowBuffer.begin(), overflowBuffer.end())));
                        }

                        overflowBuffer.clear();
                        incomingMessageHeader.Clear();
                    }

                    readPosition += messageBytes;
                }

                // It's possible that we just finished reading a previously overflown message,
                // in which case the header is expected to be invalid - so we'll need to read the next header,
                // before we can read the next payload.

                if (incomingMessageHeader.IsValid())
                {
                    if (readPosition + incomingMessageHeader.messageSize <= bytesReceived)
                    {
                        {
                            std::lock_guard<std::mutex> lock(queueMutex);
                            inbox.push(TypedMessage(incomingMessageHeader.messageType,
                                                    std::string(incomingBuffer + readPosition,
                                                                incomingMessageHeader.messageSize)));
                        }

                        readPosition += incomingMessageHeader.messageSize;
                        incomingMessageHeader.Clear();
                    }
                    else
                    {
                        // Couldn't extract a full message, so let's stick what we have into the overflow buffer
                        overflowBuffer.insert(overflowBuffer.end(),
                                              incomingBuffer + readPosition,
                                              incomingBuffer + bytesReceived);
                        readPosition = bytesReceived;
                    }
                }
            }
        }

        }
        // Then deal with bytes to send
        if (pollfd.revents & POLLWRNORM)
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!outbox.empty())
            {
                const auto& message = outbox.front();
                bool didSendHeader = true;
                if (outgoingMessageHeader.IsValid() == false)
                {
                    didSendHeader = false;
                    const int msgSize = message.payload.size();
                    const int headerSize = sizeof(MessageHeader);
                    outgoingMessageHeader.Setup(static_cast<int>(message.type), msgSize);

                    const char* headerPtr = reinterpret_cast<const char*>(&outgoingMessageHeader);
                    const int bytesSent = send(socket, headerPtr, headerSize, 0);

                    if (bytesSent > 0)
                    {
                        if (bytesSent != headerSize)
                        {
                            throw std::runtime_error("Couldn't send message header in one go.");
                        }

                        bytesSentSoFar = 0;
                        didSendHeader = true;
                    }
                    else
                    {
                        if (bytesSent == 0 && WSAGetLastError() != WSAEWOULDBLOCK)
                        {
                            std::cerr << "Send error == : " << WSAGetLastError() << "\n";
                            break;
                        }

                        // We couldn't send the message header, so we'll have to try again.
                        outgoingMessageHeader.Clear();
                    }
                }

                if (didSendHeader)
                {
                    const int bytesSent = send(socket,
                                               message.payload.c_str() + bytesSentSoFar,
                                               message.payload.size() - bytesSentSoFar,
                                               0);

                    if (bytesSent > 0)
                    {
                        bytesSentSoFar += bytesSent;
                        if (bytesSentSoFar == outgoingMessageHeader.messageSize)
                        {
                            outbox.pop();
                            outgoingMessageHeader.Clear();
                        }
                    }
                    else
                    {
                        if (bytesSent == 0 && WSAGetLastError() != WSAEWOULDBLOCK)
                        {
                            std::cerr << "Send error ==== : " << WSAGetLastError() << "\n";
                            break;
                        }
                    }
                }
            }
        }
    }


    qDebug() << "Communication thread shutting down.";
}

SocketWrapper::SocketWrapper(int port, bool isBlocking)
{
    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    _isCreated = _socket != INVALID_SOCKET;

    _socketAddress.sin_family = AF_INET;
    _socketAddress.sin_port = htons(port);

    if (!isBlocking)
    {
        unsigned long nonBlockingMode = 1u;
        const auto ioCtlError = ioctlsocket(_socket, FIONBIO, &nonBlockingMode);
        if (ioCtlError != 0)
        {
            _isCreated = false;
        }
    }

    BOOL flag = TRUE;
    setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));

    InetPton(AF_INET, L"127.0.0.1", &_socketAddress.sin_addr.s_addr);
}

SocketWrapper::SocketWrapper(SocketWrapper&& other)
{
    _socket = other._socket;
    _isCreated = other._isCreated;
    other._socket = INVALID_SOCKET;
    other._isCreated = false;
}

SocketWrapper& SocketWrapper::operator=(SocketWrapper&& other)
{
    if (this != &other)
    {
        _socket = other._socket;
        _isCreated = other._isCreated;
        other._socket = INVALID_SOCKET;
        other._isCreated = false;
    }

    return *this;
}

SocketWrapper::~SocketWrapper()
{
    if (_socket != INVALID_SOCKET)
    {
        closesocket(_socket);
    }
}

bool SocketWrapper::IsCreated() const
{
    return _isCreated;
}

SOCKET SocketWrapper::GetSocket() const
{
    return _socket;
}

ServerSocketWrapper::ServerSocketWrapper(int port, int maxSendSize, bool isBlocking)
    : SocketWrapper(port, isBlocking)
{
    if (_isCreated)
    {
        const int setSizeError = setsockopt(_socket,
                                            SOL_SOCKET,
                                            SO_SNDBUF,
                                            (char*)&maxSendSize,
                                            sizeof(maxSendSize));

        const int bindError = bind(_socket,
                                   (SOCKADDR*)&_socketAddress,
                                   sizeof(_socketAddress));

        _isCreated = setSizeError == 0 && bindError == 0;
    }
}

SOCKET ServerSocketWrapper::GetConnectedClientSocket() const
{
    return _connectedClientSocket;
}

bool ServerSocketWrapper::BlockUntilListenOrError(std::stop_token stopToken)
{
    if (!_isCreated || _socket == INVALID_SOCKET)
    {
        return false;
    }

    // TODO - if this is so, then perhaps we can get rid of the vector of client sockets?
    constexpr int maxIncomingConnections = 1;

    while (!stopToken.stop_requested())
    {
        const int listenResult = listen(_socket, maxIncomingConnections);
        if (listenResult == SOCKET_ERROR)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
            {
                return false;
            }
            else
            {
                continue;
            }
        }

        break;
    }

    return true;
}

SOCKET ServerSocketWrapper::BlockUntilAcceptConnectionOrError(std::stop_token stopToken)
{
    if (!_isCreated || _socket == INVALID_SOCKET)
    {
        return INVALID_SOCKET;
    }

    while (!stopToken.stop_requested())
    {
        SOCKET acceptSocket = accept(_socket, nullptr, nullptr);
        if (acceptSocket == INVALID_SOCKET)
        {
            const int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                return acceptSocket;
            }
            else
            {
                continue;
            }
        }

        //TODO
        // std::cout << "Yay, connected!\n";
        return acceptSocket;
    }

    return INVALID_SOCKET;
}

bool ServerSocketWrapper::ListenAndAccept(std::stop_token stopToken)
{
    _connectedClientSocket = INVALID_SOCKET;

    if (BlockUntilListenOrError(stopToken))
    {
        _connectedClientSocket = BlockUntilAcceptConnectionOrError(stopToken);
    }

    bool isOK = _connectedClientSocket != INVALID_SOCKET;

    qDebug() << "ListenAndAccept finished; success? " << isOK;

    return _connectedClientSocket != INVALID_SOCKET;
}

ClientSocketWrapper::ClientSocketWrapper(int port, bool isBlocking)
    : SocketWrapper(port, isBlocking)
{
    if (_isCreated)
    {
        const int connectResult = connect(
            _socket,
            (SOCKADDR*)&_socketAddress,
            sizeof(_socketAddress));

        if (isBlocking)
        {
            _isCreated = connectResult == 0;
        }
        else
        {
            if (connectResult == SOCKET_ERROR)
            {
                _isCreated = WSAGetLastError() == WSAEWOULDBLOCK;
            }
        }
    }
}

WinSockEntity::WinSockEntity()
{
    auto constexpr version = MAKEWORD(2, 2);
    int const startupError = WSAStartup(version, &_wsaData);
    _isCreated = startupError == 0;
}

WinSockEntity::~WinSockEntity()
{
    if (_isCreated)
    {
        _serverStopSource.request_stop();

        WSACleanup();
        _isCreated = false;
    }
}

bool WinSockEntity::IsCreated() const
{
    return _isCreated;
}

int WinSockEntity::GetLastError() const
{
    return WSAGetLastError();
}

SOCKET WinSockEntity::CreateServerSocket(const ServerSocketConfig &config)
{
    return CreateServerSocket(config.port,
                              config.sendBufSize,
                              config.isBlocking,
                              config.startListening);
}

/// <summary>
/// Create a server socket and optionally start listening for incoming connections.
/// </summary>
/// <param name="port">The port assigned to the socket.</param>
/// <param name="sendBufSize">Maximum send buffer size.</param>
/// <param name="isBlocking">Whether the socket should be blocking.</param>
/// <param name="startListening">Whether we the server should start listening immediately.</param>
/// <returns>The socket handle.</returns>
SOCKET WinSockEntity::CreateServerSocket(int port, int sendBufSize, bool isBlocking, bool startListening)
{
    if (_serverSocket.GetSocket() == INVALID_SOCKET)
    {
        _serverSocket = ServerSocketWrapper(port, sendBufSize, isBlocking);
    }

    if (_serverSocket.IsCreated())
    {
        if (startListening)
        {
            std::stop_source newStopSource;
            _serverStopSource.swap(newStopSource);
            _serverThread = std::jthread(&ServerSocketWrapper::ListenAndAccept,
                                         &_serverSocket,
                                         _serverStopSource.get_token());
        }
    }

    return _serverSocket.GetSocket();
}

SOCKET WinSockEntity::CreateClientSocket(int port, bool isBlocking)
{
    _clientSocket = ClientSocketWrapper(port, isBlocking);
    return _clientSocket.GetSocket();
}

bool WinSockEntity::TryGetConnectedClientSocket(SOCKET &socket) const
{
    socket = _serverSocket.GetConnectedClientSocket();
    return socket != INVALID_SOCKET;
}
} // namespace Unalmas
