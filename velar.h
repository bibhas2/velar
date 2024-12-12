#pragma once

#include <stdexcept>
#include <bitset>
#include <set>
#include <string_view>
#include <memory>
#include <cstring>

#ifdef _WIN32
//This header adds support for ipv6 and
//includes the base header: winsock2.h.
#include <ws2tcpip.h>
#else
using SOCKET = int;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#endif

struct ByteBuffer {
	char *array = NULL;
	size_t position;
	size_t capacity;
	size_t limit;
	bool owned;

	ByteBuffer(size_t capacity);

	ByteBuffer(char* data, size_t length);

	~ByteBuffer();

	void put(const char* from, size_t offset, size_t length);
	void put(std::string_view);
	void put(char byte);

	void get(const char* to, size_t offset, size_t length);
	void get(char& byte);
	void get(std::string_view& sv, size_t length);
	void get(std::string_view& sv);

	std::string_view to_string_view() {
		return std::string_view(array + position, remaining());
	}

	size_t remaining() {
		return (limit - position);
	}

	bool has_remaining() {
		return remaining() > 0;
	}

	void rewind() {
		position = 0;
	}

	void clear() {
		limit = capacity;
		position = 0;
	}

	void flip() {
		limit = position;
		position = 0;
	}

	//Disable copying
	ByteBuffer(const ByteBuffer&) = delete;
	ByteBuffer& operator=(const ByteBuffer&) = delete;

	// Disable moving
	ByteBuffer(ByteBuffer&&) = delete;
	ByteBuffer& operator=(ByteBuffer&&) = delete;
};

struct SocketAttachment {};

struct Socket {
	enum IOFlag {
		REPORT_READABLE,
		REPORT_WRITABLE,
		IS_READABLE,
		IS_WRITABLE,
		IS_CONN_PENDING,
		IS_CONN_FAILED,
		IS_CONN_SUCCESS
	};

	enum SocketType {
		CLIENT,
		SERVER
	};

	SOCKET fd;
	std::bitset<7> io_flag;
	std::shared_ptr<SocketAttachment> attachment;
	SocketType socket_type;

	Socket();
	virtual ~Socket();

	bool is_server() {
		return socket_type == SocketType::SERVER;
	}

	bool is_client() {
		return socket_type == SocketType::CLIENT;
	}

	void report_readable(bool flag) {
		io_flag.set(IOFlag::REPORT_READABLE, flag);
	}

	void report_writable(bool flag) {
		io_flag.set(IOFlag::REPORT_WRITABLE, flag);
	}

	bool is_report_readable() {
		return io_flag.test(IOFlag::REPORT_READABLE);
	}

	bool is_report_writable() {
		return io_flag.test(IOFlag::REPORT_WRITABLE);
	}

	void set_readable(bool flag) {
		io_flag.set(IOFlag::IS_READABLE, flag);
	}

	void set_writable(bool flag) {
		io_flag.set(IOFlag::IS_WRITABLE, flag);
	}

	bool is_readable() {
		return io_flag.test(IOFlag::IS_READABLE);
	}

	bool is_writable() {
		return io_flag.test(IOFlag::IS_WRITABLE);
	}

	bool is_acceptable() {
		return is_server() && is_readable();
	}

	void set_connection_pending(bool flag) {
		io_flag.set(IOFlag::IS_CONN_PENDING, flag);
	}

	bool is_connection_pending() {
		return io_flag.test(IOFlag::IS_CONN_PENDING);
	}

	void set_connection_failed(bool flag) {
		io_flag.set(IOFlag::IS_CONN_FAILED, flag);
	}

	bool is_connection_failed() {
		return io_flag.test(IOFlag::IS_CONN_FAILED);
	}

	void set_connection_success(bool flag) {
		io_flag.set(IOFlag::IS_CONN_SUCCESS, flag);
	}

	bool is_connection_success() {
		return io_flag.test(IOFlag::IS_CONN_SUCCESS);
	}

	int read(ByteBuffer& b);
	int write(ByteBuffer& b);
	int recvfrom(ByteBuffer& b, sockaddr* from, int* from_len);
	int sendto(ByteBuffer& b, const struct sockaddr* to, int to_len);

	bool operator<(const Socket& other) const {
		return fd < other.fd;
	}

	//Disable copying
	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;

	// Disable moving
	Socket(Socket&&) = delete;
	Socket& operator=(Socket&&) = delete;
};

/*
* A socket class that remembers the destination server's
* address and port. This makes it easy to call sendto().
*/
struct DatagramClientSocket : public Socket {
	addrinfo *server_address;

	DatagramClientSocket(addrinfo* addr);
	~DatagramClientSocket();

	int sendto(ByteBuffer& b);
	using Socket::sendto;

	int recvfrom(ByteBuffer& b);
	using Socket::recvfrom;
};

struct Selector {
private:
	void purge_sokets();
	void populate_fd_set(fd_set& read_fd_set, fd_set& write_fd_set, fd_set& except_fd_set);

public:
	std::set<std::shared_ptr<Socket>> sockets;
	std::set<std::shared_ptr<Socket>> canceled_sockets;

	std::shared_ptr<Socket> start_udp_server(int port, std::shared_ptr<SocketAttachment> attachment);
	std::shared_ptr<Socket> start_multicast_server(const char* group_address, int port, std::shared_ptr<SocketAttachment> attachment);
	std::shared_ptr<Socket> start_server(int port, std::shared_ptr<SocketAttachment> attachment);
	std::shared_ptr<Socket> start_client(const char* address, int port, std::shared_ptr<SocketAttachment> attachment);
	std::shared_ptr<DatagramClientSocket> start_udp_client(const char* address, int port, std::shared_ptr<SocketAttachment> attachment);
	std::shared_ptr<Socket> accept(std::shared_ptr<Socket> server, std::shared_ptr<SocketAttachment> attachment);
	int select(long timeout=0);
	void cancel_socket(std::shared_ptr<Socket> socket);
};