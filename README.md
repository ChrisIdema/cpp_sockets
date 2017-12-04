# CPP Sockets
*Testing UDP / TCP Sockets in CPP*

This was a very small exploration into using sockets in C++. Specifically this tries to create a CLI to send and receive basic network messages.

This repo also uses classes and inheritance whereas most socket examples use only C code.

Really this is not that big of a deal, most sources suggest using a library like Boost because of the error-prone nature of programming sockets. 

Still it is a fun exercise and helpful to understand the differences in programming TCP and UDP.

## Usage
The CLI allows you to interactively make a:

UDP or TCP, 

server or client,

By asking for:
- protocol
- client or server

If client:
- destination IP address
- destination port

If server:
- port on which to listen

## Functionality
The UDP client sends a message and exits

The UDP server prints received messages to the console

The TCP client sends a message, waits for a response and exits

The TCP server prints a received message, responds `Your message has been received by client` and exits

## Things to do
- There is [a lot more that could be done](http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html) but this is outside the scope of this project
	
### License

:copyright: Willy Nolan 2017

[MIT License](http://en.wikipedia.org/wiki/MIT_License)
