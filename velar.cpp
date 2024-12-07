#include <iostream>
#include "velar.h"

#ifdef _WIN32
//These are needed by IPV6
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

/*
* Use RAII to initialize winsock. Any application using this library won't have to 
* worry about that.
*/
class WSInit {
public:
    WSInit() {
        WSADATA wsa;

        if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            throw std::runtime_error("WSAStartup() failed.");
        }
    }
    ~WSInit() {
        ::WSACleanup();
    }
};

static WSInit __wsa_init;
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

ByteBuffer::ByteBuffer(size_t capacity) {
    array = (char*) ::malloc(capacity);
    this->capacity = capacity;
    position = 0;
    limit = capacity;
    owned = true;
}

ByteBuffer::ByteBuffer(char* data, size_t length) {
    array = data;
    capacity = length;
    position = 0;
    limit = length;
    owned = false;
}

ByteBuffer::~ByteBuffer() {
    if (owned && array != NULL) {
        free(array);

        array = NULL;
    }
}

void ByteBuffer::put(const char* from, size_t offset, size_t length) {
    if (length > remaining()) {
        throw std::out_of_range("Insufficient space remaining.");
    }

    ::memcpy(array + position, from + offset, length);

    position += length;
}

void ByteBuffer::put(char byte) {
    if (!has_remaining()) {
        throw std::out_of_range("Insufficient space remaining.");
    }

    array[position] = byte;

    ++position;
}


static void set_nonblocking(SOCKET socket) {
#ifdef _WIN32
    u_long non_block = 1;

    int status = ::ioctlsocket(socket, FIONBIO, &non_block);

    if (status != NO_ERROR) {
        throw std::runtime_error("Failed to make socket non-blocking.");
    }
#else
    int status = ::fcntl(socket, F_SETFL, O_NONBLOCK);

    if (status < 0) {
        throw std::runtime_error("Failed to make socket non-blocking.");
    }
#endif
}

static void check_socket_error(int status, const char* msg) {
#ifdef _WIN32
    if (status == SOCKET_ERROR) {
        throw std::runtime_error(msg);
    }
#else
    if (status < 0) {
        throw std::runtime_error(msg);
    }
#endif
}

void free_addrinfo(struct addrinfo* p) {
    if (p != NULL) {
        freeaddrinfo(p);
    }
}

std::shared_ptr<Socket> Selector::start_client(const char* address, int port, std::shared_ptr<SocketAttachment> attachment) {
    char port_str[128];

    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints {}, *res{};

    /*
    * We take a numeric port number and not a 
    * service name like "http" or "ftp" for port.
    * This will tell getaddrinfo() not to do any
    * service name resolution making it slightly faster.
    */
    hints.ai_flags = AI_NUMERICSERV;
    /*
    * This will cause getaddrinfo() to return both ipv4 and ipv6
    * address if available.
    */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = ::getaddrinfo(address, port_str, &hints, &res);

    /*
    * getaddrinfo() is strange in a way since it may return a positive 
    * value in case of an error. Any non-zero value indicates an error.
    */
    if (status != 0 || res == NULL) {
        throw std::runtime_error("Failed to resolve address.");
    }

    /*
    * The addrinfo res object is a linked list. It has all
    * resolved addresses. For example, it will have both ipv4 and ipv6
    * addresses if available. We can iterate through the addresses using res->next.
    * Below, we go with the first address in the list, which can be either ipv4 or ipv6.
    * A more robust implementation will try the next address (res->next) if
    * connect() fails.
    */

    /*
    * Use RAII to free the address. This makes the code below much simpler.
    */
    auto addr_resource = std::unique_ptr<struct addrinfo, void(*)(struct addrinfo*)>(res, free_addrinfo);

    SOCKET sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create a socket.");
    }

    set_nonblocking(sock);

    status = ::connect(sock, res->ai_addr, res->ai_addrlen);

    /*
    * It is normal for a nonblocking socket to not complete connection immediately.
    * This is indicated by an error but we should not abort.
    * 
    * Checking for in progress connection differes between Winsock and BSD socket.
    */
    if (status < 0) {
#ifdef _WIN32
        auto err = ::WSAGetLastError();

        if (err != WSAEWOULDBLOCK) {
            ::closesocket(sock);

            throw std::runtime_error("Failed to connect.");
        }
#else
        if (errno != EINPROGRESS) {
            ::close(sock);

            throw std::runtime_error("Failed to connect.");
        }
#endif
    }

    auto client = std::make_shared<Socket>();

    client->fd = sock;
    client->socket_type = Socket::SocketType::CLIENT;
    client->attachment = attachment;
    client->set_connection_pending(true);

    sockets.insert(client);

    return client;
}

std::shared_ptr<Socket> Selector::start_multicast_receiver_ipv6(const char* group_ip, int port, std::shared_ptr<SocketAttachment> attachment) {
    auto receiver = start_udp_receiver_ipv6(port, attachment);

    // Join the multicast group
    struct ipv6_mreq mreq {};

    //Set the multicast group address
    if (inet_pton(AF_INET6, group_ip, &mreq.ipv6mr_multiaddr) != 1) {
        throw std::runtime_error("Failed to get IPV6 address.");
    }

    mreq.ipv6mr_interface = 0;

    //Join the group
    int status = ::setsockopt(receiver->fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char*)&mreq, sizeof(mreq));

    check_socket_error(status, "Failed to join multicast group.");

    return receiver;
}

std::shared_ptr<Socket> Selector::start_udp_receiver_ipv6(int port, std::shared_ptr<SocketAttachment> attachment) {
    // Create a UDP socket
    int sock = ::socket(AF_INET6, SOCK_DGRAM, 0);

    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create a socket.");
    }

    int on = 1;

    int status = ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    check_socket_error(status, "Failed to set SO_REUSEADDR.");

    // Bind the socket to the multicast port
    struct sockaddr_in6 addr {};

    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);

    status = ::bind(sock, (const struct sockaddr*) &addr, sizeof(addr));

    check_socket_error(status, "Failed to bind to port.");

    auto receiver = std::make_shared<Socket>();

    receiver->fd = sock;
    receiver->socket_type = Socket::SocketType::CLIENT;
    receiver->attachment = attachment;

    //Turn this on since all receivers need to read
    receiver->report_readable(true);

    sockets.insert(receiver);

    return receiver;
}

/*
* Starts a UDP multicast receiver (server). 
*/
std::shared_ptr<Socket> Selector::start_multicast_receiver_ipv4(const char* group_ip, int port, std::shared_ptr<SocketAttachment> attachment) {
    auto receiver = start_udp_receiver_ipv4(port, attachment);

    // Join the multicast group
    struct ip_mreq mreq {};

    //Set the multicast group address
    if (inet_pton(AF_INET, group_ip, &mreq.imr_multiaddr.s_addr) != 1) {
        throw std::runtime_error("Failed to get address.");
    }

    mreq.imr_interface.s_addr = INADDR_ANY;

    //Join the group
    int status = ::setsockopt(receiver->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));

    check_socket_error(status, "Failed to join multicast group.");

    return receiver;
}

std::shared_ptr<Socket> Selector::start_udp_receiver_ipv4(int port, std::shared_ptr<SocketAttachment> attachment) {
    // Create a UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create a socket.");
    }

    int on = 1;

    int status = ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*) &on, sizeof(on));

    check_socket_error(status, "Failed to set SO_REUSEADDR.");

    // Bind the socket to the multicast port
    struct sockaddr_in addr {};

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    status = bind(sock, (const struct sockaddr*)&addr, sizeof(addr));

    check_socket_error(status, "Failed to bind to port.");

    auto receiver = std::make_shared<Socket>();

    receiver->fd = sock;
    receiver->socket_type = Socket::SocketType::CLIENT;
    receiver->attachment = attachment;

    //Turn this on since all receivers need to read
    receiver->report_readable(true);

    sockets.insert(receiver);

    return receiver;
}

std::shared_ptr<Socket> Selector::start_server(int port, std::shared_ptr<SocketAttachment> attachment) {
    int status;

    SOCKET sock = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create a socket.");
    }

    set_nonblocking(sock);

    char reuse = 1;

    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
    
    /*
    * This will allow IPV4 mapped addresses.
    */
    int optval = 0;
    
    ::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&optval, sizeof(optval));

    struct sockaddr_in6 addr {}; //Important to zero out the address

    addr.sin6_family = AF_INET6;
    addr.sin6_addr = ::in6addr_any;
    addr.sin6_port = htons(port);

    status = ::bind(sock, (struct sockaddr*) &addr, sizeof(addr));

    check_socket_error(status, "Failed to bind to port.");

    status = listen(sock, 10);

    check_socket_error(status, "Failed to listen.");

    auto server = std::make_shared<Socket>();

    server->fd = sock;
    server->socket_type = Socket::SocketType::SERVER;
    server->attachment = attachment;

    //Turn this on since all servers will need to catch accept event
    server->report_readable(true);

    sockets.insert(server);

    return server;
}

std::shared_ptr<Socket> Selector::accept(std::shared_ptr<Socket> server, std::shared_ptr<SocketAttachment> attachment) {
    SOCKET client_fd = ::accept(server->fd, NULL, NULL);

    if (client_fd == INVALID_SOCKET) {
        throw std::runtime_error("accept() failed.");
    }

    set_nonblocking(client_fd);

    auto client = std::make_shared<Socket>();

    client->fd = client_fd;
    client->socket_type = Socket::SocketType::CLIENT;
    client->attachment = attachment;

    sockets.insert(client);

    return client;
}

void Selector::populate_fd_set(fd_set& read_fd_set, fd_set& write_fd_set, fd_set& except_fd_set) {
    FD_ZERO(&read_fd_set);
    FD_ZERO(&write_fd_set);
    FD_ZERO(&except_fd_set);

    for (auto& s : sockets) {
        if (s->is_report_readable()) {
            FD_SET(s->fd, &read_fd_set);
        }
        if (s->is_report_writable()) {
            FD_SET(s->fd, &write_fd_set);
        }
        if (s->is_connection_pending()) {
            /*
            * Detecting connect() completion status is platform dependent.
            * In Winsock, we use exception fd set for error and write fd set for success. 
            * In BSD socket, we query writable event and then test for SO_ERROR.
            */
#ifdef _WIN32
            FD_SET(s->fd, &except_fd_set);
#endif
            FD_SET(s->fd, &write_fd_set);

        }
    }
}

void Selector::purge_sokets() {
    for (auto& s : canceled_sockets) {
        sockets.erase(s);
    }

    canceled_sockets.clear();
}

int Selector::select(long timeout) {
    fd_set read_fd_set, write_fd_set, except_fd_set;
    struct timeval t;

    t.tv_sec = timeout;
    t.tv_usec = 0;

    purge_sokets();

    populate_fd_set(read_fd_set, write_fd_set, except_fd_set);

    int num_events = ::select(
        FD_SETSIZE,
        &read_fd_set,
        &write_fd_set,
        NULL,
        timeout > 0 ? &t : NULL);

#ifdef _WIN32
    if (num_events == SOCKET_ERROR) {
        int status = ::WSAGetLastError();

        if (status == WSAEINTR || status == WSAEINPROGRESS) {
            return num_events;
        }
        else {
            throw std::runtime_error("select() failed.");
        }
    }
#else
    if (num_events < 0) {
        if (errno == EINTR) {
            //A signal was handled
            return num_events;
        }
        else {
            throw std::runtime_error("select() failed.");
        }
    }
#endif

    if (num_events == 0) {
        //Timeout
        return num_events;
    }

    for (auto& s : sockets) {
        if (s->is_connection_pending()) {
            /*
            * Test for connect() completion status.
            */
#ifdef _WIN32
            /*
            * It appears that both write fd set and except fd set are set for 
            * the socket for a successful connection. We must test for the
            * write fd set before except fd set to determine success.
            */
            if (FD_ISSET(s->fd, &write_fd_set)) {
                s->set_connection_success(true);
                s->set_connection_pending(false);
            }
            else if ((FD_ISSET(s->fd, &except_fd_set))) {
                //connect() has failed
                s->set_connection_failed(true);
                s->set_connection_pending(false);
            }
#else
            if (FD_ISSET(s->fd, &write_fd_set)) {
                int valopt;
                socklen_t lon = sizeof(int);

                if (::getsockopt(s->fd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) {
                    throw std::runtime_error("Error in getsockopt().");
                }

                if (valopt) {
                    s->set_connection_failed(true);
                    s->set_connection_pending(false);
                }
                else {
                    s->set_connection_success(true);
                    s->set_connection_pending(false);
                }
            }
#endif
            if (s->is_connection_pending()) {
                //This should not happen.
                throw std::runtime_error("Invalid state.");
            }
        }
        else {
            s->set_connection_success(false);
            s->set_readable((FD_ISSET(s->fd, &read_fd_set)));
            s->set_writable((FD_ISSET(s->fd, &write_fd_set)));
        }
    }

    return num_events;
}

void Selector::cancel_socket(std::shared_ptr<Socket> socket) {
    canceled_sockets.insert(socket);
}

Socket::Socket() {
    fd = INVALID_SOCKET;

    io_flag.reset();
    socket_type = Socket::SocketType::CLIENT;
}

Socket::~Socket() {
    if (fd != INVALID_SOCKET) {
#ifdef _WIN32
        ::closesocket(fd);
#else
        ::close(fd);
#endif

        fd = INVALID_SOCKET;
    }
}

/*
* Reads data from this socket into the supplied ByteBuffer.
* Upon a successful read the position of the buffer is incremented but limit remains unchanged.
* Before you retrieve data from the buffer you should call flip().
* 
* When you try to read from a socket many things can happen. Winsock and BSD socket deal with
* these situations slightly differently. Here we normalize the situations by returning an 
* uniform value for both platforms. Here are the possible cases:
* 
* - Read was successful. In this case, the number of bytes read, a positive number, is returned.
* - An attempted read would lead to blocking. In this case 0 is returned.
* - The other party has disconnected gracefully, meaning they have closed their end of the socket. In this
* case a negative value is returned.
* - The other party has disconnected ungracefully, meaning somehow the connection was severed. In this case,
* we return a negative value.
* 
* If a negative value is returned, then applications should treat the socket as unusuable. 
* They should cancel the socket.
*/
int Socket::read(ByteBuffer& b) {
    if (!b.has_remaining()) {
        throw std::runtime_error("Buffer is full.");
    }

#ifdef _WIN32
    int bytes_read = ::recv(
        fd,
        b.array + b.position,
        b.remaining(),
        0);

    if (bytes_read == SOCKET_ERROR) {
        int err = ::WSAGetLastError();

        if (err == WSAECONNRESET) {
            //Ungraceful disconnect by the other party
            return -1;
        }

        if (err == WSAEWOULDBLOCK) {
            //Not an error really.
            return 0;
        }
        else {
            //A real error has taken place.
            return -1;
        }
    }
#else
    int bytes_read = ::read(
        fd,
        b.array + b.position,
        b.remaining());

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            //Not an error really.
            return 0;
        }
        else {
            //A real error has taken place.
            return -1;
        }
    }
#endif

    if (bytes_read == 0) {
        /*
        * The other party has disconnected:
        * Gracefully in Windows.
        * Gracefully or ungracefully in Linux.
        */
        return -1;
    }

    if (bytes_read < 0) {
        //This should have been handled above already.
        throw std::runtime_error("Invalid state.");
    }

    //Forward the position
    b.position += bytes_read;

    return bytes_read;
}

/*
* Writes any remaining data from this socket into the supplied ByteBuffer.
* Upon a successful write the position of the buffer is incremented but the limit remains unchanged.
* This way, the buffer keeps track of how much data was sent and how much is yet to be sent.
* In a nonblocking socket it is common that not all the data can be sent at the same time.
* You can call write() for the same buffer repetedly until all the data is fully sent.
*
* When you try to write to a socket many things can happen. Winsock and BSD socket deal with
* these situations slightly differently. Here we normalize the situations by returning an
* uniform value for both platforms. Here are the possible cases:
*
* - Write was successful. In this case, the number of bytes written, a positive number, is returned.
* - An attempted write would lead to blocking. In this case 0 is returned.
* - The other party has disconnected gracefully, meaning they have closed their end of the socket. In this
* case a negative value is returned.
* - The other party has disconnected ungracefully, meaning somehow the connection was severed. In this case,
* we return a negative value.
*
* If a negative value is returned, then applications should treat the socket as unusuable.
* They should cancel the socket.
*/
int Socket::write(ByteBuffer& b) {
    if (!b.has_remaining()) {
        throw std::runtime_error("Buffer is empty.");
    }

#ifdef _WIN32
    int bytes_written = ::send(
        fd,
        b.array + b.position,
        b.remaining(),
        0);

    if (bytes_written == SOCKET_ERROR) {
        int err = ::WSAGetLastError();

        if (err == WSAECONNRESET) {
            //Ungraceful disconnect
            return -1;
        }

        if (err == WSAEWOULDBLOCK) {
            //Not a real error
            return 0;
        }
        else {
            //A real error has taken place.
            return -1;
        }
    }
#else
    int bytes_written = ::write(
        fd,
        b.array + b.position,
        b.remaining());

    if (bytes_written < 0) {
        if (errno == EAGAIN && errno == EWOULDBLOCK) {
            //Not a real error
            return 0;
        }
        else {
            //A real error has taken place.
            return -1;
        }
    }
#endif

    if (bytes_written == 0) {
        /*
        * The other party has disconnected:
        * Gracefully in Windows.
        * Gracefully or ungracefully in Linux.
        */
        return -1;
    }

    if (bytes_written < 0) {
        //This should have been handled above already.
        throw std::runtime_error("Invalid state.");
    }

    //Forward the position
    b.position += bytes_written;

    return bytes_written;
}