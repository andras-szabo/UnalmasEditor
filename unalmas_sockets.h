#ifndef UNALMAS_SOCKETS_H
#define UNALMAS_SOCKETS_H

#include <qthread.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <stop_token>
#include <thread>
#include <queue>

#define OUT

namespace Unalmas
{
enum class MessageType
{
    Undefined = 0,
    Handshake = 1,
    Disconnect = 2
};

struct MessageHeader
{
    int messageType { 0 };
    int messageSize { 0 };

    bool IsValid() const { return messageSize > 0; }
    void Clear() { messageType = 0; messageSize = 0; }
    void Setup(int type, int size) { messageType = type; messageSize = size; }
};

struct TypedMessage
{
    TypedMessage(Unalmas::MessageType type_, const std::string& payload_)
        : type {type_}, payload {payload_}
    {}

    TypedMessage(int type_, const std::string& payload_)
        : type {type_}, payload {payload_}
    {}

    MessageType type;
    std::string payload;
};

struct ServerSocketConfig
{
    int port { 5555 };
    int sendBufSize { 65536 };
    bool isBlocking { false };
    bool startListening { true };
};

void CommunicationThread(std::stop_token stopToken,
                         SOCKET socket,
                         std::queue<TypedMessage>& inbox,
                         std::queue<TypedMessage>& outbox,
                         std::mutex& queueMutex);

class SocketWrapper
{
public:
    SocketWrapper() = default;

    // Deleting copy ctor and copy assignment operator (for now).
    // Copying a socket wrapper - e.g. from a temporary - might
    // lead to invoking the dtor of the original one, which
    // will close the socket.

    SocketWrapper(const SocketWrapper& other) = delete;
    SocketWrapper& operator=(const SocketWrapper& other) = delete;

    SocketWrapper(SocketWrapper&& other);
    SocketWrapper& operator=(SocketWrapper&& other);

    SocketWrapper(int port, bool isBlocking);
    ~SocketWrapper();

    bool IsCreated() const;
    SOCKET GetSocket() const;

protected:
    bool _isCreated { false };
    SOCKET _socket { INVALID_SOCKET };
    sockaddr_in _socketAddress;
};

class ServerSocketWrapper : public SocketWrapper
{
public:
    ServerSocketWrapper() = default;
    ServerSocketWrapper(int port, int sendBufSize, bool isBlocking);

    // Non-copyable
    ServerSocketWrapper(const ServerSocketWrapper& other) = delete;
    ServerSocketWrapper& operator=(const ServerSocketWrapper& other) = delete;

    // But moving is fine
    ServerSocketWrapper(ServerSocketWrapper&& other);
    ServerSocketWrapper& operator=(ServerSocketWrapper&& other);

    bool BlockUntilListenOrError();
    SOCKET BlockUntilAcceptConnectionOrError();
    SOCKET GetConnectedClientSocket();
    bool ListenAndAccept();

protected:
    std::mutex _ccsMutex;
    SOCKET _connectedClientSocket { INVALID_SOCKET };
};

class ClientSocketWrapper : public SocketWrapper
{
public:
    ClientSocketWrapper() = default;
    ClientSocketWrapper(int port, bool isBlocking);

    // TODO no need to delete copy ctor, as in ServerSocketWrapper?
};

class WinSockEntity
{
public:
    WinSockEntity();
    ~WinSockEntity();

    bool IsCreated() const;
    int GetLastError() const;
    SOCKET CreateServerSocket(const ServerSocketConfig& config);
    SOCKET CreateServerSocket(int port, int sendBufSize, bool isBlocking, bool startListening);
    SOCKET CreateClientSocket(int port, bool isBlocking);
    bool TryGetConnectedClientSocket(OUT SOCKET& socket);

private:
    WSADATA _wsaData;
    bool _isCreated { false };
    ServerSocketWrapper _serverSocket;
    std::vector<SOCKET> _clientSocketsAsSeenByServer;
    ClientSocketWrapper _clientSocket;

    QThread* _serverThread { nullptr };
    std::stop_source _serverStopSource;
    std::mutex _serverSocketMutex;
};
} // namespace Unalmas
#endif // UNALMAS_SOCKETS_H
