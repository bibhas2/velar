# Introduction

Velar is a cross platform asynchronous networking library written in C++. It uses ``select()`` for multiplexing and doesn't use any threads.

I created asynchronous network library before. First in C and then in C++. I never quite found them as easy to use as they could be. Then I used Java NIO's selector architecture. It showed me how to create an easy to use library. Velar was influenced by Java NIO's Selector and ByteBuffer.

# Main Features

- TCP client and server.
- UDP client and server.
- UDP multicast server.
- Simplified I/O using ByteBuffer.

# Platform Support
- Linux
- Windows 11
- MacOS

# Quick Example

This is a server that listens on port 9080. When a client connects, it simply sends a ``"HELLO VELAR"`` message to the client and disconnects.

```c++
#include <velar.h>

int main()
{
    Selector sel;
    ByteBuffer out_buff(128);

    sel.start_server(9080, nullptr);

    while (true) {
        sel.select();

        for (auto& s : sel.sockets) {
            if (s->is_acceptable()) {
                //We have a new client connection
                auto client = sel.accept(s, nullptr);

                out_buff.clear();
                out_buff.put("HELLO VELAR\r\n");
                out_buff.flip();

                client->report_writable(true);
            }
            else if (s->is_writable()) {
                if (out_buff.has_remaining()) {
                    if (s->write(out_buff) < 0) {
                        //Client disconnected
                        sel.cancel_socket(s);
                    }
                }
                else {
                    //We're done sending message. 
                    //Disconnect.
                    sel.cancel_socket(s);
                }
            }
        }
    }

    return 0;
}
```

# Building
## Linux and macOS
From the root of the repo run:

```
make
```

This will build the static library ``libvelar.a`` and all the test executables in test1, test2 etc. folders.

## Windows
Open the Visual Stidio solution ``velar.sln``. Build the solution (Control+Shift+B). This will create the static library ``velar.lib`` and all the test executables in the ``x64/Debug`` folder.

# Using Velar
Set your compiler's C++ language support level to at least C++ 17.

Include the header file.

```
#include <velar.h>
```

Link your executable to the static library ``libvelar.a`` (Linux and MacOS) or ``velar.lib`` (Windows).

# Programming Guide

## ByteBuffer
The ByteBuffer class makes it easy and safe to deal with asynchronous read and write. Non-blocking I/O requires repeated attempts to fully read or write the data. ByteBuffer internally manages the state of how much data is yet to be read or written.

We can create a ByteBuffer that allocates memory on the heap.

```c++
//Create a buffer with a capacity of 256 bytes.
ByteBuffer b(256);
```

You can also allocate your own memory and wrap it by a ByteBuffer. In that case, the buffer doesn't own the memory and is not responsible for freeing it.

```c++
char data[256];
ByteBuffer b(data, 256);
```

Two most important properties of a ByteBuffer are ``limit`` and ``position``.

- limit - Total number of bytes available to read from or write to.
- position - Indicates how many bytes have been already read from the buffer or written into it.
- The ``remaining()`` method returns the number of bytes still available to write into the buffer or yet to be read from.

To write data into a buffer, first you call ``clear()`` to set the position to 0 and limit to the capcity. Then you use various put methods to repeatedly write data.

```
ByteBuffer b(256);

//Always call clear before starting to write into the buffer
b.clear();

//Now we can write in several batches
b.put("Hello ")
b.put("World");
b.put('!');

//This will print 12
std::cout << b.position << std::endl;
```

To read from the buffer first call ``flip()``. This will set the limit to the current position and set the position to 0.

```
b.flip();

chat c;

b.get(c);
assert(c == 'H');

b.get(c);
assert(c == 'e');

//10 bytes left to be read
assert(b.remaining() == 10);
```

## Selector
A ``Selector`` manages a set of sockets. It detects when a socket is ready to write to or read from and reports that to the application.

A most minimal application using a ``Selector`` will look like this. The code doesn't really do anything but shows you the basic boilerplate of all Velar applications. 

```c++
int main()
{
    Selector sel;

    while (true) {
        sel.select();
    }

    return 0;
}
```

You can optionally set a timeout in seconds for the ``select()`` method.

```c++
int main()
{
    Selector sel;

    while (true) {
        int num_events = sel.select(10);

        if (num_events == 0) {
            //Timeout
            return 0;
        }
    }

    return 0;
}
```

## TCP Server
The ``Selector::start_server()`` method starts a new TCP server. It registers the socket with the selector.

```c++
Selector sel;

auto server1 = sel.start_server(9080, nullptr);
auto server2 = sel.start_server(9081, nullptr);

while (true) {
    sel.select();
}
```

When a client connects to the server, the server socket's ``is_acceptable()`` method will return true.

```c++
Selector sel;

sel.start_server(9080, nullptr);

while (true) {
    sel.select();

    for (auto& s : sel.sockets) {
        if (s->is_acceptable()) {
            std::cout << "We have a new client" << std::endl;

            auto client = sel.accept(s, nullptr);
        }
    }
}
```

You should call ``Selector::accept()`` to accept the connection. This will register a new ``Socket`` for the client with the selector.

When you have multiple servers running, you need to find a way to manage their states. This will help you distinguish between the servers. This is done by setting an attachment to the server's socket.

```c++
struct ServerState : public SocketAttachment {
    std::string name;
    ServerState(const char* n) : name{n}{}
};

Selector sel;

sel.start_server(9080, std::make_shared<ServerState>("SERVER1"));
sel.start_server(9081, std::make_shared<ServerState>("SERVER2"));

while (true) {
    sel.select();

    for (auto& s : sel.sockets) {
        if (s->is_acceptable()) {
            auto state = s->get_attachment<ServerState>();

            std::cout << "Client for: " << state->name << std::endl;

            auto client = sel.accept(s, nullptr);
        }
    }
}
```
