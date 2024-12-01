// tcp-server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "tcp-server.h"

static void set_nonblocking(SOCKET socket) {
#ifdef _WIN32
    u_long non_block = 1;

    int status = ::ioctlsocket(socket, FIONBIO, &non_block);

    if (status != NO_ERROR) {
        throw std::runtime_error("Failed to make socket non-blocking.");
    }
#else
    int status = fcntl(sock, F_SETFL, O_NONBLOCK);

    if (status < != NO_ERROR> 0) {
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


std::shared_ptr<Socket> Selector::start_server(int port, std::unique_ptr<SocketAttachment> attachment) {
    int status;

    SOCKET sock = ::socket(PF_INET, SOCK_STREAM, 0);

    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create a socket.");
    }

    set_nonblocking(sock);

    char reuse = 1;

    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    status = bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    check_socket_error(status, "Failed to bind to port.");

    status = listen(sock, 10);

    check_socket_error(status, "Failed to listen.");

    auto server = std::make_shared<Socket>();

    server->fd = sock;
    server->socket_type = Socket::SocketType::SERVER;
    server->attachment = std::move(attachment);
    //Turn this on since all servers will need to catch accept event
    server->report_readable(true);

    sockets.insert(server);

    return server;
}

std::shared_ptr<Socket> Selector::accept(std::shared_ptr<Socket> server, std::unique_ptr<SocketAttachment> attachment) {
    SOCKET client_fd = ::accept(server->fd, NULL, NULL);

    if (client_fd == INVALID_SOCKET) {
        throw std::runtime_error("accept() failed.");
    }

    set_nonblocking(client_fd);

    auto client = std::make_shared<Socket>();

    client->fd = client_fd;
    client->socket_type = Socket::SocketType::CLIENT;
    client->attachment = std::move(attachment);

    sockets.insert(client);

    return client;
}

void Selector::populate_fd_set(fd_set& read_fd_set, fd_set& write_fd_set) {
    FD_ZERO(&read_fd_set);
    FD_ZERO(&write_fd_set);

    for (auto& s : sockets) {
        if (s->is_report_readable()) {
            FD_SET(s->fd, &read_fd_set);
        }
        if (s->is_report_writable()) {
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

void Selector::select(long timeout) {
    fd_set read_fd_set, write_fd_set;
    struct timeval t;

    t.tv_sec = timeout;
    t.tv_usec = 0;

    purge_sokets();

    populate_fd_set(read_fd_set, write_fd_set);

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
            return;
        }
        else {
            throw std::runtime_error("select() failed.");
        }
    }
#else
    if (num_events < 0) {
        if (errno == EINTR) {
            //A signal was handled
            return;
        }
        else {
            throw std::runtime_error("select() failed.");
        }
    }
#endif

    if (num_events == 0) {
        //Timeout
        return;
    }

    for (auto& s : sockets) {
        s->set_readable((FD_ISSET(s->fd, &read_fd_set)));
        s->set_writable((FD_ISSET(s->fd, &write_fd_set)));
    }
}

void Selector::cancel_socket(std::shared_ptr<Socket>& socket) {
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
            //Ungraceful disconnect
            return 0;
        }

        if (err != WSAEWOULDBLOCK) {
            throw std::runtime_error("recv() failed.");
        }
    }
#else
    int bytes_read = ::read(
        fd,
        b.array + b.position,
        b.remaining());

    if (bytes_read == SOCKET_ERROR) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            throw std::runtime_error("read() failed.");
        }
    }
#endif

    if (bytes_read == 0) {
        //Client disconnected gracefully
        return 0;
    }

    //Forward the position
    b.position += bytes_read;
}

int main()
{
    Selector sel;
    ByteBuffer buff(128);

    sel.start_server(9080, nullptr);

    while (true) {
        sel.select();

        for (auto s : sel.sockets) {
            if (s->is_readable()) {
                if (s->is_server()) {
                    auto client = sel.accept(s, nullptr);

                    client->report_readable(true);

                    std::cout << "Client connected" << std::endl;
                }
                else {
                    buff.clear();

                    int sz = s->read(buff);

                    if (sz == 0) {
                        std::cout << "Client disconnected" << std::endl;

                        sel.cancel_socket(s);
                    }
                    else {
                        buff.flip();

                        std::cout.write(buff.array, buff.limit);
                    }
                }
            }
        }
    }

    return 0;
}
