#include <iostream>
#include "velar.h"

#ifdef _WIN32
//These are needed by IPV6
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
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

ByteBuffer::~ByteBuffer() {}

void ByteBuffer::put(const char* from, size_t offset, size_t length) {
    if (length > remaining()) {
        throw std::out_of_range("Insufficient space remaining.");
    }

    ::memcpy(array + position, from + offset, length);

    position += length;
}

void ByteBuffer::put(std::string_view sv) {
    put(sv.data(), 0, sv.length());
}

void ByteBuffer::put(char ch) {
    if (!has_remaining()) {
        throw std::out_of_range("Insufficient space remaining.");
    }

    array[position] = ch;

    ++position;
}

void ByteBuffer::put(uint16_t i) {
    uint16_t n = htons(i);

    put((const char*)&n, 0, sizeof(uint16_t));
}

void ByteBuffer::put(uint32_t i) {
    uint32_t n = htonl(i);

    put((const char*)&n, 0, sizeof(uint32_t));
}

void ByteBuffer::put(uint64_t i) {
#if defined _WIN32 || defined __APPLE__
    uint64_t n = htonll(i);
#else
    uint64_t n = ::htobe64(i);
#endif

    put((const char*)&n, 0, sizeof(uint64_t));
}

/*
* Copies length number of bytes from the current position of the buffer
* into the destination at a given offset of the destination.
* 
* If insufficient bytes remain in the buffer, that is, remaining() < length, then
* a std::out_of_range exception is thrown. Otherwise, the position is moved
* forward by length.
*/
void ByteBuffer::get(const char* to, size_t offset, size_t length) {
    if (remaining() < length) {
        throw std::out_of_range("Insufficient data remaining.");
    }

    ::memcpy((void*) (to + offset), array + position, length);

    position += length;
}

void ByteBuffer::get(char& ch) {
    if (!has_remaining()) {
        throw std::out_of_range("Insufficient data remaining.");
    }

    ch = array[position];

    ++position;
}

/*
* Shares length bytes from the buffer with the give string_view.
* No data is copied. That makes this one of the fastest ways to read
* data from the buffer.
* 
* If there's insufficient data left to be read, that is, remaining() < length,
* then a std::out_of_range exception is thrown. Otherwise, the position is moved
* forward by length.
*/
void ByteBuffer::get(std::string_view& sv, size_t length) {
    if (remaining() < length) {
        throw std::out_of_range("Insufficient data remaining.");
    }

    sv = { array + position, length };

    position += length;
}

/*
* Shares all the remaining data with the given string_view.
* No data is copied. That makes this one of the fastest ways to read
* data from the buffer.
* 
* If there's no remaining data left to be read, that is, has_remaining() == false,
* then a std::out_of_range exception is thrown. Otherwise, the position is moved
* to the end of the buffer.
*/
void ByteBuffer::get(std::string_view& sv) {
    if (!has_remaining()) {
        throw std::out_of_range("Insufficient data remaining.");
    }

    sv = { array + position, remaining()};

    position += remaining();
}

void ByteBuffer::get(uint16_t& i) {
    uint16_t n = 0;

    get((const char*)&n, 0, sizeof(uint16_t));

    i = ntohs(n);
}

void ByteBuffer::get(uint32_t& i) {
    uint32_t n = 0;

    get((const char*)&n, 0, sizeof(uint32_t));

    i = ntohl(n);
}

void ByteBuffer::get(uint64_t& i) {
    uint64_t n = 0;

    get((const char*)&n, 0, sizeof(uint64_t));

#if defined _WIN32 || defined __APPLE__
    i = ntohll(n);
#else
    i = be64toh(n);
#endif
}

HeapByteBuffer::HeapByteBuffer(size_t sz) {
    array = (char*) ::malloc(sz);

    if (array == NULL) {
        throw std::runtime_error("malloc() failed.");
    }

    capacity = sz;
    position = 0;
    limit = sz;
}

HeapByteBuffer::~HeapByteBuffer() {
    if (array != NULL) {
        ::free(array);

        array = NULL;
    }
}

WrappedByteBuffer::WrappedByteBuffer(char* data, size_t length) {
    array = data;
    capacity = length;
    limit = length;
    position = 0;
}

MappedByteBuffer::MappedByteBuffer(const char* file_name, boolean read_only) {
#ifdef _WIN32
    file_handle = ::CreateFileA(
        file_name, 
        (read_only ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE)),
        0, 
        NULL, 
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, 
        NULL);

    if (file_handle == INVALID_HANDLE_VALUE) {
        cleanup(); //Do manual cleanup. Dtor won't be called.

        throw std::runtime_error("CreateFileA failed.");
    }

    LARGE_INTEGER file_size;

    if (!::GetFileSizeEx(file_handle, &file_size)) {
        cleanup();

        throw std::runtime_error("GetFileSizeEx failed.");
    }

    if (file_size.QuadPart == 0) {
        cleanup();

        throw std::runtime_error("Zero length file cannot be mapped in Windows.");
    }

    map_handle = ::CreateFileMappingA(
        file_handle, 
        NULL, 
        (read_only ? PAGE_READONLY : PAGE_READWRITE),
        0, 
        0, 
        NULL);

    if (map_handle == NULL) {
        cleanup(); //Do manual cleanup. Dtor won't be called.

        throw std::runtime_error("CreateFileMappingA failed.");
    }

    array = (char*) ::MapViewOfFile(map_handle, 
        read_only ? FILE_MAP_READ : (FILE_MAP_WRITE),
        0, 
        0, 
        0);

    if (array == NULL) {
        cleanup();

        throw std::runtime_error("MapViewOfFile failed.");
    }

    capacity = file_size.QuadPart;
    limit = capacity;
    position = 0;
#else
#endif
}

MappedByteBuffer::~MappedByteBuffer() {
    cleanup();
}

void MappedByteBuffer::cleanup() {
#ifdef _WIN32
    BOOL status;

    if (map_handle != NULL) {
        if (array != NULL) {
            status = ::UnmapViewOfFile(array);
        }

        status = ::CloseHandle(map_handle);

        map_handle = NULL;
        array = NULL;
    }
    if (file_handle != INVALID_HANDLE_VALUE) {
        status = ::CloseHandle(file_handle);

        file_handle = INVALID_HANDLE_VALUE;
    }
#else
#endif
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
        ::perror(msg);

        throw std::runtime_error(msg);
    }
#endif
}

void free_addrinfo(struct addrinfo* p) {
    if (p != NULL) {
        freeaddrinfo(p);
    }
}

/*
* Creates a UDP socket. The socket remembers the given server address and port.
* Any subsequent call to sendto() will use this address. The address of the server
* can be a hostname, ipv4 or ipv6 address.
* 
* After the first call to sendto() a UDP socket gets bound to the server's address and port.
* Which means, if you call recvfrom() after that the data is read from the server.
* 
* Once you no longer need to communicate with the server cancel the socket.
*/
std::shared_ptr<DatagramClientSocket> Selector::start_udp_client(const char* address, int port, std::shared_ptr<SocketAttachment> attachment) {
    char port_str[128];

    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints {}, * res{};

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
    hints.ai_socktype = SOCK_DGRAM; //UDP

    int status = ::getaddrinfo(address, port_str, &hints, &res);

    /*
    * getaddrinfo() is strange in a way since it may return a positive
    * value in case of an error. Any non-zero value indicates an error.
    */
    if (status != 0 || res == NULL) {
        throw std::runtime_error("Failed to resolve address.");
    }

    /*
    * Creating the socket object here willmake sure res gets freed up
    * no matter what happens.
    */
    auto client = std::make_shared<DatagramClientSocket>(res);

    set_nonblocking(client->fd());

    client->attachment(attachment);

    m_sockets.insert(client);

    return client;
}

/*
* Creates a new TCP socket and connects it to a server listening at the given 
* address and port. The address can be a host name, ipv4 or ipv6 IP address.
* The attachment is set for the newly created client socket.
* 
* To disconnect from the server gracefully, cancel the client socket.
*/
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

    auto client = std::make_shared<Socket>(res->ai_family, res->ai_socktype, res->ai_protocol);

    client->attachment(attachment);
    client->set_connection_pending(true);

    set_nonblocking(client->fd());

    status = ::connect(client->fd(), res->ai_addr, res->ai_addrlen);

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
            throw std::runtime_error("Failed to connect.");
        }
#else
        if (errno != EINPROGRESS) {
            throw std::runtime_error("Failed to connect.");
        }
#endif
    }

    m_sockets.insert(client);

    return client;
}

/*
* Starts a UDP server and makes it join a multicast group identified by the group's
* IP address group_ip. The group's address can be any valid ipv4 or ipv6 IP address.
*/
std::shared_ptr<Socket> Selector::start_multicast_server(const char* group_ip, int port, std::shared_ptr<SocketAttachment> attachment) {
    auto receiver = start_udp_server(port, attachment);

    // Join the multicast group
    struct ipv6_mreq mreq6 {};
    struct ip_mreq mreq4 {};

    //Set the multicast group address
    if (inet_pton(AF_INET6, group_ip, &mreq6.ipv6mr_multiaddr) == 1) {
        mreq6.ipv6mr_interface = 0;

        //Join the group
        int status = ::setsockopt(receiver->fd(), IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char*)&mreq6, sizeof(mreq6));

        check_socket_error(status, "Failed to join ipv6 multicast group.");
    }
    else if (inet_pton(AF_INET, group_ip, &mreq4.imr_multiaddr.s_addr) == 1) {
        mreq4.imr_interface.s_addr = INADDR_ANY;

        //Join the group
        int status = ::setsockopt(receiver->fd(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq4, sizeof(mreq4));

        check_socket_error(status, "Failed to join ipv4 multicast group.");
    }
    else {
        throw std::runtime_error("The group IP address is not a valid ipv6 or ipv4 address.");
    }

    return receiver;
}

/*
* Starts a UDP socket and binds to the given port. Clients should be able to
* connect to it using either ipv4 or ipv6 address.
* 
* The socket's readbility event reporting is enabled by default. Which means,
* the server can start accepting request messages from clients right away.
*/
std::shared_ptr<Socket> Selector::start_udp_server(int port, std::shared_ptr<SocketAttachment> attachment) {
    // Create a UDP socket
    auto receiver = std::make_shared<Socket>(AF_INET6, SOCK_DGRAM, 0);

    receiver->attachment(attachment);

    /*
    * This will make the socket bind to both ipv6 and ipv4 address.
    * This way, a client can connect using either ipv4 or ipv6 address.
    */
    int optval = 0;

    int status = ::setsockopt(receiver->fd(), IPPROTO_IPV6, IPV6_V6ONLY, (char*)&optval, sizeof(optval));

    check_socket_error(status, "Failed to disable IPV6_V6ONLY.");

    int reuse = 1;

    status = ::setsockopt(receiver->fd(), SOL_SOCKET, SO_REUSEADDR, (const char*) & reuse, sizeof reuse);
    
    check_socket_error(status, "Failed to set SO_REUSEADDR.");

#ifndef _WIN32
    status = ::setsockopt(receiver->fd(), SOL_SOCKET, SO_REUSEPORT, (const char*) &reuse, sizeof reuse);
    
    check_socket_error(receiver->fd(), "Failed to set SO_REUSEPORT.");
#endif

    // Bind the socket to the multicast port
    struct sockaddr_in6 addr {};

    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);

    status = ::bind(receiver->fd(), (const struct sockaddr*) &addr, sizeof(addr));

    check_socket_error(status, "Failed to bind to port.");

    //Turn this on since all receivers need to read
    receiver->report_readable(true);

    m_sockets.insert(receiver);

    return receiver;
}

/*
* Starts a TCP socket and binds to the given port. Clients should be able to
* connect to it using either ipv4 or ipv6 address.
*
* The socket's readbility event reporting is enabled by default. Which means,
* the server can start accepting clients right away.
* 
* To shutdown the server just cancel the server socket.
*/
std::shared_ptr<Socket> Selector::start_server(int port, std::shared_ptr<SocketAttachment> attachment) {
    auto server = std::make_shared<Socket>(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    server->attachment(attachment);

    set_nonblocking(server->fd());

    int reuse = 1;

    int status = ::setsockopt(server->fd(), SOL_SOCKET, SO_REUSEADDR, (const char*) &reuse, sizeof reuse);
    
    check_socket_error(status, "Failed to set SO_REUSEADDR.");

#ifndef _WIN32
    status = ::setsockopt(server->fd(), SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof reuse);

    check_socket_error(status, "Failed to set SO_REUSEPORT.");
#endif

    /*
    * This will allow IPV4 mapped addresses.
    */
    int optval = 0;
    
    status = ::setsockopt(server->fd(), IPPROTO_IPV6, IPV6_V6ONLY, (char*)&optval, sizeof(optval));
    check_socket_error(status, "Failed to disable IPV6_V6ONLY.");

    struct sockaddr_in6 addr {}; //Important to zero out the address

    addr.sin6_family = AF_INET6;
    addr.sin6_addr = ::in6addr_any;
    addr.sin6_port = htons(port);

    status = ::bind(server->fd(), (struct sockaddr*) &addr, sizeof(addr));

    check_socket_error(status, "Failed to bind to port.");

    status = ::listen(server->fd(), 10);

    check_socket_error(status, "Failed to listen.");

    //Turn this on since all servers will need to catch accept event
    server->report_accpeptable(true);

    m_sockets.insert(server);

    return server;
}

/*
* Accepts a client that has connected to the given server's socket.
* A new client socket is created and the attachment is set for the client.
* The selector begins monitoring the client socket for events until the client
* socket is cancelled.
*/
std::shared_ptr<Socket> Selector::accept(std::shared_ptr<Socket> server, std::shared_ptr<SocketAttachment> attachment) {
    SOCKET client_fd = ::accept(server->fd(), NULL, NULL);

    if (client_fd == INVALID_SOCKET) {
        throw std::runtime_error("accept() failed.");
    }

    /*
    * Create the Socket early so RAII can clean it up in
    * case of a problem.
    */
    auto client = std::make_shared<Socket>(client_fd);

    client->attachment(attachment);

    set_nonblocking(client_fd);

    m_sockets.insert(client);

    return client;
}

void Selector::populate_fd_set(fd_set& read_fd_set, fd_set& write_fd_set, fd_set& except_fd_set) {
    FD_ZERO(&read_fd_set);
    FD_ZERO(&write_fd_set);
    FD_ZERO(&except_fd_set);

    for (auto& s : m_sockets) {
        if (s->is_report_readable() || s->is_report_acceptable()) {
            FD_SET(s->fd(), &read_fd_set);
        }
        if (s->is_report_writable()) {
            FD_SET(s->fd(), &write_fd_set);
        }
        if (s->is_connection_pending()) {
            /*
            * Detecting connect() completion status is platform dependent.
            * In Winsock, we use exception fd set for error and write fd set for success. 
            * In BSD socket, we query writable event and then test for SO_ERROR.
            */
#ifdef _WIN32
            FD_SET(s->fd(), &except_fd_set);
#endif
            FD_SET(s->fd(), &write_fd_set);

        }
    }
}

void Selector::purge_sokets() {
    for (auto& s : m_canceled_sockets) {
        m_sockets.erase(s);
    }

    m_canceled_sockets.clear();
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

    for (auto& s : m_sockets) {
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
            if (FD_ISSET(s->fd(), &write_fd_set)) {
                s->set_connection_success(true);
                s->set_connection_pending(false);
            }
            else if ((FD_ISSET(s->fd(), &except_fd_set))) {
                //connect() has failed
                s->set_connection_failed(true);
                s->set_connection_pending(false);
            }
#else
            if (FD_ISSET(s->fd(), &write_fd_set)) {
                int valopt;
                socklen_t lon = sizeof(int);

                if (::getsockopt(s->fd(), SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) {
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

            //For a server socket, readable means new client
            //waiting to be accepted
            if (s->is_report_acceptable()) {
                s->set_acceptable((FD_ISSET(s->fd(), &read_fd_set)));
            } else {
                s->set_readable((FD_ISSET(s->fd(), &read_fd_set)));
            }
            
            s->set_writable((FD_ISSET(s->fd(), &write_fd_set)));
        }
    }

    return num_events;
}

/*
* Removes this socket from the set of sockets monitored by the selector.
* The socket will be eventually closed and destroyed.
*/
void Selector::cancel_socket(std::shared_ptr<Socket> socket) {
    m_canceled_sockets.insert(socket);
}

Socket::Socket(SOCKET fd) : m_fd(fd) {

}

Socket::Socket(int domain, int type, int protocol) : m_fd(INVALID_SOCKET) {
    m_fd = ::socket(domain, type, protocol);

    if (m_fd == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create a socket.");
    }

    m_io_flag.reset();
}

Socket::~Socket() {
    if (m_fd != INVALID_SOCKET) {
#ifdef _WIN32
        ::closesocket(m_fd);
#else
        ::close(m_fd);
#endif

        m_fd = INVALID_SOCKET;
    }
}

DatagramClientSocket::DatagramClientSocket(addrinfo* res) : 
    server_address(res),
    Socket(res->ai_family, res->ai_socktype, res->ai_protocol)
{
}

DatagramClientSocket::~DatagramClientSocket() {
    if (server_address != NULL) {
        ::freeaddrinfo(server_address);

        server_address = NULL;
    }
}

/*
* Writes data from the buffer to the address and port that
* this socket was constructed with. The buffer's position is
* moved forward by the number of bytes written. The limit
* remains unchanged. If not all the data could be written,
* calling has_remaining() on the buffer will return true.
*/
int DatagramClientSocket::sendto(ByteBuffer& b) {
    return sendto(b, server_address->ai_addr, server_address->ai_addrlen);
}

/*
* After the first time sendto() is called for a UDP socket it
* gets implicitly bound to the destination server's address and port.
* You can then call recvfrom(). This will receive data from the server where the
* original sendto() request was sent.
* 
* The position of the buffer is moved forward by the number of bytes received.
* Limit is left unchanged. You should flip() the buffer before reading
* from it.
*/
int DatagramClientSocket::recvfrom(ByteBuffer& b) {
    return recvfrom(b, nullptr, nullptr);
}

/*
* Reads data from this socket into the supplied ByteBuffer at the current position of the buffer.
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
        m_fd,
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
        m_fd,
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


int Socket::recvfrom(ByteBuffer& b, sockaddr* from, int* from_len) {
    if (!b.has_remaining()) {
        throw std::runtime_error("Buffer is full.");
    }

#ifdef _WIN32
    int bytes_read = ::recvfrom(
        m_fd,
        b.array + b.position,
        b.remaining(),
        0,
        from,
        from_len);

    if (bytes_read == SOCKET_ERROR) {
        int err = ::WSAGetLastError();

        if (err == WSAECONNRESET) {
            //Ungraceful disconnect by the other party
            return -1;
        } else if (err == WSAEWOULDBLOCK) {
            //Not an error really.
            return 0;
        }
        else if (err == WSAEMSGSIZE) {
            /*
            * A partial read took place. In Windows this is an error.
            * But in Linux it is not an error.
            * 
            * We make our recvfrom()
            * behave the same way as in Linux by returning the number of bytes 
            * saved in the buffer.
            */
            
            bytes_read = b.remaining();
        }
        else {
            //A real error has taken place.
            return -1;
        }
    }
#else
    int bytes_read = ::recvfrom(
        m_fd,
        b.array + b.position,
        b.remaining(),
        0,
        from,
        (socklen_t*) 
        from_len);

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
* Writes any remaining data from this ByteBuffer into the socket.
* Upon a successful write the position of the buffer is incremented by the
* number of bytes actually written. The limit remains unchanged.
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
        m_fd,
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
        m_fd,
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

int Socket::sendto(ByteBuffer& b, const struct sockaddr* to, int to_len) {
    if (!b.has_remaining()) {
        throw std::runtime_error("Buffer is empty.");
    }

#ifdef _WIN32
    int bytes_written = ::sendto(
        m_fd,
        b.array + b.position,
        b.remaining(),
        0,
        to,
        to_len);

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
    int bytes_written = ::sendto(
        m_fd,
        b.array + b.position,
        b.remaining(),
        0,
        to,
        to_len);

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