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
#include <unistd.h>

#endif

struct ByteBuffer {
protected:
	char *m_array = NULL;
	size_t m_position = 0;
	size_t m_capacity = 0;
	size_t m_limit = 0;

public:
	ByteBuffer() {}
	virtual ~ByteBuffer() = 0;

	void put(const char* from, size_t offset, size_t length);
	void put(std::string_view);
	void put(char byte);
	void put(uint16_t i);
	void put(uint32_t i);
	void put(uint64_t i);

	void get(const char* to, size_t offset, size_t length);
	void get(char& byte);
	void get(std::string_view& sv, size_t length);
	void get(std::string_view& sv);
	void get(uint16_t& i);
	void get(uint32_t& i);
	void get(uint64_t& i);

	std::string_view to_string_view() {
		return std::string_view(m_array + m_position, remaining());
	}

	char* array() {
		return m_array;
	}

	size_t position() {
		return m_position;
	}

	size_t limit() {
		return m_limit;
	}

	size_t capacity() {
		return m_capacity;
	}

	void position(size_t pos) {
		if (pos > m_limit) {
			throw std::out_of_range("Position is greater than limit.");
		}

		m_position = pos;
	}

	void limit(size_t lim) {
		if (lim > m_capacity) {
			throw std::out_of_range("Limit is greater than capacity.");
		}

		m_limit = lim;
	}

	size_t remaining() {
		return (m_limit - m_position);
	}

	/**
	 * @brief Checks if there are remaining elements.
	 * 
	 * This function determines whether there are any remaining elements
	 * by checking if the result of the `remaining()` function is greater than zero.
	 * 
	 * @return true if there are remaining elements, false otherwise.
	 */
	bool has_remaining() {
		return remaining() > 0;
	}

	void rewind() {
		m_position = 0;
	}

	void clear() {
		m_limit = m_capacity;
		m_position = 0;
	}

	void flip() {
		m_limit = m_position;
		m_position = 0;
	}

	//Disable copying
	ByteBuffer(const ByteBuffer&) = delete;
	ByteBuffer& operator=(const ByteBuffer&) = delete;

	// Disable moving
	ByteBuffer(ByteBuffer&&) = delete;
	ByteBuffer& operator=(ByteBuffer&&) = delete;
};

template<std::size_t SIZE>
struct StaticByteBuffer : ByteBuffer {
private:
	char storage[SIZE];
public:
	StaticByteBuffer() : storage{}  {
		m_array = storage;
		m_capacity = SIZE;
		m_limit = SIZE;
		m_position = 0;
	}
	~StaticByteBuffer() {}
};

struct HeapByteBuffer : ByteBuffer {
	HeapByteBuffer(size_t sz);
	~HeapByteBuffer();
};

struct WrappedByteBuffer : ByteBuffer {
	WrappedByteBuffer(char* data, size_t length);
	~WrappedByteBuffer() {}
};

struct MappedByteBuffer : ByteBuffer {
private:
#ifdef _WIN32
	HANDLE file_handle = INVALID_HANDLE_VALUE;
	HANDLE map_handle = NULL;
#else
	int file_handle = -1;
#endif
	void cleanup();

public:
	MappedByteBuffer(const char* file_name, bool read_only = true, size_t max_size=0);
	~MappedByteBuffer();
};


struct SocketAttachment {};

struct Socket {
private:
	std::bitset<9> m_io_flag;
	SOCKET m_fd;
	std::shared_ptr<SocketAttachment> m_attachment;

public:

	enum IOFlag {
		REPORT_ACCEPTABLE,
		REPORT_READABLE,
		REPORT_WRITABLE,
		IS_ACCEPTABLE,
		IS_READABLE,
		IS_WRITABLE,
		IS_CONN_PENDING,
		IS_CONN_FAILED,
		IS_CONN_SUCCESS
	};

	Socket(int domain, int type, int protocol);
	Socket(SOCKET fd);
	virtual ~Socket();

	SOCKET fd() {
		return m_fd;
	}

	void report_accpeptable(bool flag) {
		m_io_flag.set(IOFlag::REPORT_ACCEPTABLE, flag);
	}

	void report_readable(bool flag) {
		m_io_flag.set(IOFlag::REPORT_READABLE, flag);
	}

	void report_writable(bool flag) {
		m_io_flag.set(IOFlag::REPORT_WRITABLE, flag);
	}

	bool is_report_acceptable() {
		return m_io_flag.test(IOFlag::REPORT_ACCEPTABLE);
	}

	bool is_report_readable() {
		return m_io_flag.test(IOFlag::REPORT_READABLE);
	}

	bool is_report_writable() {
		return m_io_flag.test(IOFlag::REPORT_WRITABLE);
	}

	void set_acceptable(bool flag) {
		m_io_flag.set(IOFlag::IS_ACCEPTABLE, flag);
	}

	void set_readable(bool flag) {
		m_io_flag.set(IOFlag::IS_READABLE, flag);
	}

	void set_writable(bool flag) {
		m_io_flag.set(IOFlag::IS_WRITABLE, flag);
	}

	bool is_readable() {
		return m_io_flag.test(IOFlag::IS_READABLE);
	}

	bool is_writable() {
		return m_io_flag.test(IOFlag::IS_WRITABLE);
	}

	bool is_acceptable() {
		return m_io_flag.test(IOFlag::IS_ACCEPTABLE);
	}

	void set_connection_pending(bool flag) {
		m_io_flag.set(IOFlag::IS_CONN_PENDING, flag);
	}

	bool is_connection_pending() {
		return m_io_flag.test(IOFlag::IS_CONN_PENDING);
	}

	void set_connection_failed(bool flag) {
		m_io_flag.set(IOFlag::IS_CONN_FAILED, flag);
	}

	bool is_connection_failed() {
		return m_io_flag.test(IOFlag::IS_CONN_FAILED);
	}

	void set_connection_success(bool flag) {
		m_io_flag.set(IOFlag::IS_CONN_SUCCESS, flag);
	}

	bool is_connection_success() {
		return m_io_flag.test(IOFlag::IS_CONN_SUCCESS);
	}

	/**
	 * @brief Sets the socket attachment. The attachment is a shared pointer to a SocketAttachment object.
	 * It can be used to store additional information about the socket.
	 * 
	 * @param a A shared pointer to a SocketAttachment object.
	 */
	void attachment(std::shared_ptr<SocketAttachment>& a) {
		m_attachment = a;
	}

	template<class T>
	std::shared_ptr<T> attachment() {
		return std::static_pointer_cast<T>(m_attachment);
	}

	int read(ByteBuffer& b);
	int write(ByteBuffer& b);
	int recvfrom(ByteBuffer& b, sockaddr* from, int* from_len);
	int sendto(ByteBuffer& b, const struct sockaddr* to, int to_len);

	bool operator<(const Socket& other) const {
		return m_fd < other.m_fd;
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
	std::set<std::shared_ptr<Socket>> m_canceled_sockets;
	std::set<std::shared_ptr<Socket>> m_sockets;

public:

	std::shared_ptr<Socket> start_udp_server(int port, std::shared_ptr<SocketAttachment> attachment);
	std::shared_ptr<Socket> start_multicast_server(const char* group_address, int port, std::shared_ptr<SocketAttachment> attachment);
	/**
	 * @brief Starts a TCP server on the specified port with the given socket attachment.
	 * 
	 * This function initializes and starts a TCP server that listens on the specified port.
	 * It sets the attachment of the server's socket to the provided socket attachment.
	 * 
	 * @param port The port number on which the server will listen for incoming connections.
	 * @param attachment A shared pointer to a SocketAttachment object that will be set as the server's attachment.
	 * @return A shared pointer to a Socket object representing the started server.
	 */
	std::shared_ptr<Socket> start_server(int port, std::shared_ptr<SocketAttachment> attachment);
	std::shared_ptr<Socket> start_client(const char* address, int port, std::shared_ptr<SocketAttachment> attachment);
	std::shared_ptr<DatagramClientSocket> start_udp_client(const char* address, int port, std::shared_ptr<SocketAttachment> attachment);
	std::shared_ptr<Socket> accept(std::shared_ptr<Socket> server, std::shared_ptr<SocketAttachment> attachment);
	int select(long timeout=0);
	void cancel_socket(std::shared_ptr<Socket> socket);

	const std::set<std::shared_ptr<Socket>>& sockets() {
		return m_sockets;
	}
};
