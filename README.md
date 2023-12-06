# Simple C++ Sockets
[![build](https://github.com/ChrisIdema/cpp_sockets/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/ChrisIdema/cpp_sockets/actions/workflows/ci.yml)


Simple library for TCP sockets in C++. Server and client use select to receive events.

## Description

Originally forked from https://github.com/computersarecool/cpp_sockets 
I reimplemented the library to be event-based with a single blocking function using a select(). This allows the server or client thread to simply wait for events instead of manually polling. I removed UDP.
In addition to socket events such as connect, disconnect and receive Socket_waiter can also receive custom messages to interrupt the thread.

## Tested On
- Linux
- Windows

## Build
- To build and run the test application on Linux open a terminal and run:
    - `g++ -std=c++17 test/main.cpp -o main -Isrc -DSIMPLE_CPP_SOCKETS_DEBUG_PRINT`
    - `./main`

- To build and run the test application on Windows either use Visual Studio directly or open the `Developer Command Prompt` and run:
    - `cl test/main.cpp -Isrc /EHsc /std:c++17 -DSIMPLE_CPP_SOCKETS_DEBUG_PRINT`
    - `start main.exe`
    - If you want to include "windows.h" you need to define `WIN32_LEAN_AND_MEAN` or it will include the older winsock header file (winsock.h)

- defines
    - `SIMPLE_CPP_SOCKETS_DEBUG_PRINT` enables debug printing inside the library
    - `SIMPLE_CPP_SOCKETS_CLIENT_ADDRESS` adds client address to event

## Project Structure
Files:
- `src/simple_cpp_sockets.h` is the library
- `test/main.cpp` is a demo application/unit test

Types:
- `Simple_socket_library` instance to run this library (needed on Windows only, optional for Linux)
- `SOCKET` is not a class, but the native type for a socket. In Linux this is an int and in Windows a unsigned int containing a pointer (64-bit or 32-bit)
- `Raw_socket` is a class that is a basic wrapper for the native socket type to aid in type checking and easier syntax. It has no member other than the native socket. Purposely doesn't close upon destruction.
- `Simple_socket` is a class that provides easy initialization of its Raw_socket member for server or client. Closes upon destruction.
- `Socket_waiter` is a class that allows servers and clients to wait for events using wait_for_events()
- `Socket_waiter::Event` contains the event code (enum) and other relevant data, wait_for_events returns a vector of events


## keepalive for timeouts

In order to enable socket timeouts you need to manually set the socket options to enable keepalive:

```
int res = 0;
if (res != SOCKET_ERROR)
{
    //enable keep alive
    res = my_socket.setsockop_bool(SO_KEEPALIVE, true); 
}

if (res != SOCKET_ERROR)
{
    //number of retries
    res = my_socket.setsockop_int(TCP_KEEPCNT, 1, SOL_TCP); 
}

if (res != SOCKET_ERROR)
{
    // delay in seconds after idle to start sending keepalive
    res = my_socket.setsockop_int(TCP_KEEPIDLE, 1, SOL_TCP);
}

if (res != SOCKET_ERROR)
{
    // interval in seconds
    res = my_socket.setsockop_int(TCP_KEEPINTVL, 1, SOL_TCP);
}
```

After a timeout you will receive the event `Socket_waiter::Event_code::client_error`
It is recommended to enable socket timeouts on both sides. Since the timeout is a multiple of 1 second this method is not suitable for realtime systems. Alternatively you can create implement your own heartbeat packet between server and client.

## Detect unpluging or deleting IP of a server with no connected clients

TCP has no features to report failure of layers below it. Without any connected clients it cannot detect failures that prevent clients from connecting. One workarround is to connect a dummy client to the server on the same device as the server and enable keepalive. If the IP address gets removed if cable gets unplugged it gets a timeout and you can assume the connection is broken.

## Misc
- In order the break from select() you can add an unopened dummy socket in windows and close it, in Linux you can add a pipe and send a message. This is used to interrupt the thread of the server or client.
- Closing a socket in Windows before select() causes an error and also returns false events in the read set. The code handles this situation properly.
- if a client has multiple ip addresses connect will bind to a random ip. If you want to use a specific ip you need to specify it with the optional parameter client_bind_ip.
- a client socket will use a random port to connect to the specified server port. You can specify a port with the optional parameter client_bind_port

## Links
- [Popular tutorial on sockets](https://beej.us/guide/bgnet/) 
- [list of WSA error codes (Windows only)](https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2)
	
### License

[MIT License](http://en.wikipedia.org/wiki/MIT_License)

