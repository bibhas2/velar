# Introduction

Velar is a cross platform asynchronous networking library written in C++. It uses ``select()`` for multiplexing and doesn't use any threads.

I created asynchronous network library before. First in C and then in C++. I never quite found them as easy to use as they could be. Then I used Java NIO's selector architecture. It showed me how to create an easy to use library. Velar was influenced by Java NIO's Selector and ByteBuffer.

# Main Features

- TCP client and server.
- UDP client and server.
- UDP multicast server.
- Simplified I/O using ByteBuffer. This is a simple and yet hugely useful data struture.

# Platform Support
- Linux
- Windows 11
- MacOS

# Building
## Linux
From the root of the repo run:

```
make
```

This will build the static library ``libvelar.a`` and all the test executables in test1, test2 etc. folders.

## Windows
Open the Visual Stidio solution ``velar.sln``. Build the solution (Control+Shift+B). This will create the static library ``velar.lib`` and various test executables in the ``x64/Debug`` folder.

# Using Velar
Set your compiler's C++ language support level to at least C++ 17.

You will need to include the header file.

```
#include <velar.h>
```

Link your executable to the static library ``libvelar.a`` (Linux and MacOS) or ``velar.lib`` (Windows).

# Programming Guide

## ByteBuffer
The ByteBuffer class makes it easy and safe to deal with asynchronous read and write. Non-blocking I/O requires repeated attempts to fully read or write the data. ByteBuffer internally manages the state of how much data is yet to be read or written.

We can create a ByteBuffer that allocates memory on the heap.

```
//Create a buffer with a capacity of 256 bytes.
ByteBuffer b(256);
```

You can also allocate your own memory and wrap it by a ByteBuffer. In that case, the buffer doesn't own the memory and is not responsible for freeing it.

```
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

std::string_view sv = "Hello ";

b.put(sv)

sv = "World";
b.put(sv);

b.put('!');

//This will print 12
std::cout << b.position << std::endl;
```

To read from the buffer first call ``flip()``. This will set the limit to the current position and set the position to 0.

```
std::string_view sv2 = b.to 
```