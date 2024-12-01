#pragma once

#include <stdexcept>
#include <string.h>
#include <memory>
#include <bitset>
#include <set>

#ifdef _WIN32
#include <winsock2.h>
#else
using SOCKET = int;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

struct ByteBuffer {
	char *array = NULL;
	size_t position;
	size_t capacity;
	size_t limit;
	bool owned;

	ByteBuffer(size_t capacity) {
		array = (char*) ::malloc(capacity);
		this->capacity = capacity;
		position = 0;
		limit = capacity;
		owned = true;
	}

	ByteBuffer(char* data, size_t length) {
		array = data;
		capacity = length;
		position = 0;
		limit = length;
		owned = false;
	}

	~ByteBuffer() {
		if (owned && array != NULL) {
			free(array);

			array = NULL;
		}
	}

	void put(char* from, size_t offset, size_t length) {
		if (length > remaining()) {
			throw std::out_of_range("Insufficient space remaining.");
		}

		::memcpy(array + position, from + offset, length);

		position += length;
	}

	void put(char byte) {
		if (!has_remaining()) {
			throw std::out_of_range("Insufficient space remaining.");
		}

		array[position] = byte;

		++position;
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
		IS_WRITABLE
	};

	enum SocketType {
		CLIENT,
		SERVER
	};

	SOCKET fd;
	std::bitset<4> io_flag;
	std::unique_ptr<SocketAttachment> attachment;
	SocketType socket_type;

	Socket();
	~Socket();

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

	int read(ByteBuffer& b);

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

struct Selector {
private:
	void purge_sokets();
	void populate_fd_set(fd_set& read_fd_set, fd_set& write_fd_set);

public:
	std::set<std::shared_ptr<Socket>> sockets;
	//std::set<std::shared_ptr<Socket>> triggered_sockets;
	std::set<std::shared_ptr<Socket>> canceled_sockets;

	std::shared_ptr<Socket> start_server(int port, std::unique_ptr<SocketAttachment> attachment);
	std::shared_ptr<Socket> accept(std::shared_ptr<Socket> server, std::unique_ptr<SocketAttachment> attachment);
	void select(long timeout=0);
	void cancel_socket(std::shared_ptr<Socket>& socket);
};