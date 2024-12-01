// tcp-server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "tcp-server.h"

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

    if (bytes_read < 0) {
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

    return bytes_read;
}


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
            return 0;
        }

        if (err != WSAEWOULDBLOCK) {
            throw std::runtime_error("recv() failed.");
        }
    }
#else
    int bytes_written = ::write(
        fd,
        b.array + b.position,
        b.remaining());

    if (bytes_written < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            throw std::runtime_error("read() failed.");
        }
    }
#endif

    if (bytes_written == 0) {
        //Client disconnected gracefully
        return 0;
    }

    //Forward the position
    b.position += bytes_written;

    return bytes_written;
}

int main()
{
    Selector sel;
    ByteBuffer in_buff(128), out_buff(128);
    bool keep_running = true;

    sel.start_server(9080, nullptr);

    while (keep_running) {
        sel.select();

        for (auto s : sel.sockets) {
            if (s->is_acceptable()) {
                auto client = sel.accept(s, nullptr);

                const char* reply = "START SENDING\r\n";

                out_buff.clear();
                out_buff.put(reply, 0, strlen(reply));
                out_buff.flip();

                client->report_writable(true);

                std::cout << "Client connected" << std::endl;
            }
            else if (s->is_readable()) {
                in_buff.clear();

                int sz = s->read(in_buff);

                if (sz == 0) {
                    std::cout << "Client disconnected" << std::endl;

                    sel.cancel_socket(s);
                }
                else {
                    in_buff.flip();

                    auto sv = in_buff.to_string_view();

                    if (sv == "quit\n") {
                        sel.cancel_socket(s);
                    }
                    else if (sv == "shutdown\n") {
                        keep_running = false;
                    }
                    else if (sv == "list\n") {
                        for (auto s2 : sel.sockets) {
                            std::cout
                                << "Type: "
                                << (s2->is_server() ? "Server" : "Client")
                                << " Report readable: "
                                << (s2->is_report_readable() ? "Yes" : "No")
                                << " Report writable: "
                                << (s2->is_report_writable() ? "Yes" : "No")
                                << " Is readable: "
                                << (s2->is_readable() ? "Yes" : "No")
                                << " Is writable: "
                                << (s2->is_writable() ? "Yes" : "No")
                                << std::endl;
                        }
                    }
                }
            }
            else if (s->is_writable()) {
                if (out_buff.has_remaining()) {
                    s->write(out_buff);
                }
                else {
                    s->report_writable(false);
                    s->report_readable(true);
                }
            }
        }
    }

    return 0;
}
