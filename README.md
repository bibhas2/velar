# Introduction

Velar is a cross platform asynchronous networking library written in C++. It uses ``select()`` for multiplexing and doesn't use any threads.

Velar was heavily influenced by Java NIO's Selector and ByteBuffer.

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
    StaticByteBuffer<128> out_buff;

    sel.start_server(9080, nullptr);

    while (true) {
        sel.select();

        for (auto& s : sel.sockets()) {
            if (s->is_acceptable()) {
                //We have a new client connection
                auto client = sel.accept(s, nullptr);

                out_buff.clear();
                out_buff.put("HELLO VELAR\r\n");
                out_buff.flip();

                //We want to know when the client
                //socket becomes writable.
                client->report_writable(true);
            }
            else if (s->is_writable()) {
                //Send message to the client
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

This will build the static library ``libvelar.a`` and all the test executables in ``test1/build``, ``test2/build`` etc. folders.

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
The ByteBuffer class and its derived classes make it easy and safe to deal with asynchronous read and write. Non-blocking I/O requires repeated attempts to fully read or write  data from a socket. ByteBuffer internally manages the state of how much data is yet to be read or written. Here's a quick example.

```c++
StaticByteBuffer<128> b;
uint64_t i1 = 10, j1 = 0;
char ch1 = 'A', ch2 = 0;

//Always clear the buffer before starting to write into it.
//This is like clearing all the pages of a notebook before
//you start to write a new story.
b.clear();
b.put(i1);
b.put(ch1);

//Flip the buffer before starting to read from it.
//This is like flipping the notebook back to the first page
//before you start to read the story.
b.flip();
b.get(j1);
b.get(ch2);

assert(i1 == j1);
assert(ch1 == ch2);
```

In most cases we can statically allocate memory for a ByteBuffer like this. This is the fastest option since very little work has to be done at runtime to allocate memory or free it.

```c++
//Capacity of 128 bytes
StaticByteBuffer<128> b;
```

We can also create a ByteBuffer that allocates memory on the heap.

```c++
//Create a buffer with a capacity of 256 bytes.
HeapByteBuffer b(256);
```

You can also allocate your own memory and wrap it by a ByteBuffer. In that case, the buffer doesn't own the memory and is not responsible for freeing it.

```c++
char data[256];
WrappedByteBuffer b(data, sizeof(data));
```

Two most important properties of a ByteBuffer are ``limit`` and ``position``.

- limit - Total number of bytes available to read from or write to.
- position - Indicates how many bytes have been already read from the buffer or written into it.
- The ``remaining()`` method returns the number of bytes still available to write into the buffer or yet to be read from.

To write data into a buffer, first you call ``clear()`` to set the position to 0 and limit to the capcity. Then you use various put methods to repeatedly write data.

```c++
StaticByteBuffer<256> b;

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

```c++
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
A ``Selector`` manages a set of sockets. It detects various events happening to a socket. Such as, a socket has become readable, writebale or has successfully completed a connection with a server. These events are then reported back to the application.

A most minimal application using a ``Selector`` will look like this. It shows you the basic boilerplate of all Velar applications. 

```c++
int main()
{
    Selector sel;

    while (true) {
        sel.select();

        //Loop through all the managed sockets
        //and see if anything interesting happened.
        for (auto& s : sel.sockets()) {
            if (s->is_acceptable()) {
                //A client has connected
            } else if (s->is_readable()) {
                //Data is available to be read
            } else if (s->is_writable()) {
                //We can write to this socket
            }
        }
    }

    return 0;
}
```

## TCP Server
The ``Selector::start_server()`` method starts a new TCP server. It registers the server's socket with the selector.

When a client connects to the server, the server socket's ``is_acceptable()`` method will return true.

```c++
Selector sel;

sel.start_server(9080, nullptr);

while (true) {
    sel.select();

    for (auto& s : sel.sockets()) {
        if (s->is_acceptable()) {
            //We have a new client. Accept the client.
            auto client = sel.accept(s, nullptr);

            //Opt in to detect readable event
            client->report_readable(true);
        } else if (s->is_readable()) {
            //Data is available to be read
        } else if (s->is_writable()) {
            //We can write to this socket
        }
    }
}
```

You call ``Selector::accept()`` to accept the connection. This will register a new ``Socket`` for the client with the selector.

Read and write events are reported only if opted in. You call ``report_readable(bool)`` and ``report_writable(bool)`` to opt in or out.

When you have multiple servers running, you need to find a way to manage their states. This will help you distinguish between the servers. This is done by setting an attachment to the server's socket.

```c++
struct ServerState : public SocketAttachment {
    std::string name;
    ServerState(const char* n) : name{n}{}
};

Selector sel;

//Set state data as attachment
sel.start_server(9080, std::make_shared<ServerState>("SERVER1"));
sel.start_server(9081, std::make_shared<ServerState>("SERVER2"));

while (true) {
    sel.select();

    for (auto& s : sel.sockets()) {
        if (s->is_acceptable()) {
            //Get the attachment
            auto state = s->attachment<ServerState>();

            std::cout << "Client for: " << state->name << std::endl;

            auto client = sel.accept(s, nullptr);
        }
    }
}
```
