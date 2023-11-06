# Simple C++ Sockets
[![build](https://github.com/ChrisIdema/cpp_sockets/actions/workflows/ci.yml/badge.svg?branch=non_blocking)](https://github.com/ChrisIdema/cpp_sockets/actions/workflows/ci.yml)


Simple library for TCP sockets in C++. Server uses select to receive events (such as connect, disconnect and receive)

## Description

Originally forked from https://github.com/computersarecool/cpp_sockets 
I reimplemented the library to be event-based using select(). This allows the server thread to simply wait for events instead of polling or blocking on certain calls. I removed UDP. 

## Tested On
- Linux
- Windows

## Build
- To build and run the test application on Linux open a terminal and run:
    - `g++ -std=c++17 test/main.cpp -o main -Isrc`
    - `./main`

- To build and run the test application on Windows either use Visual Studio directly or open the `Developer Command Prompt` and run:
    - `cl test/main.cpp -Isrc /EHsc /std:c++17`
    - `start main.exe`


## Project Structure
- `src/simple_cpp_sockets.h` is the library
- `src/main.cpp` is a demo application/unit test

## Links
- [Popular tutorial on sockets](https://beej.us/guide/bgnet/) 
- [list of WSA error codes (Windows only)](https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2)
	
### License

[MIT License](http://en.wikipedia.org/wiki/MIT_License)
