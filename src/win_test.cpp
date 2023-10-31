#ifndef UNICODE
#define UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#include <stdint.h>

int main()
{

    //---------------------------------------
    // Declare variables
    WSADATA wsaData;

    SOCKET ListenSocket;
    sockaddr_in service;

    int iResult = 0;

    BOOL bOptVal = FALSE;
    int bOptLen = sizeof (BOOL);

    int iOptVal = 0;
    int iOptLen = sizeof (int);

    //---------------------------------------
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        wprintf(L"Error at WSAStartup()\n");
        return 1;
    }
    //---------------------------------------
    // Create a listening socket
    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        wprintf(L"socket function failed with error: %u\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    //---------------------------------------
    // Bind the socket to the local IP address
    // and port 27015
    hostent *thisHost;
    char *ip;
    u_short port;
    port = 27015;
    thisHost = gethostbyname("");
    ip = inet_ntoa(*(struct in_addr *) *thisHost->h_addr_list);

    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr(ip);
    service.sin_port = htons(port);

    iResult = bind(ListenSocket, (SOCKADDR *) & service, sizeof (service));
    if (iResult == SOCKET_ERROR) {
        wprintf(L"bind failed with error %u\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    //---------------------------------------
    // Initialize variables and call setsockopt. 
    // The SO_KEEPALIVE parameter is a socket option 
    // that makes the socket send keepalive messages
    // on the session. The SO_KEEPALIVE socket option
    // requires a boolean value to be passed to the
    // setsockopt function. If TRUE, the socket is
    // configured to send keepalive messages, if FALSE
    // the socket configured to NOT send keepalive messages.
    // This section of code tests the setsockopt function
    // by checking the status of SO_KEEPALIVE on the socket
    // using the getsockopt function.

    bOptVal = TRUE;

    iResult = getsockopt(ListenSocket, SOL_SOCKET, SO_KEEPALIVE, (char *) &iOptVal, &iOptLen);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"getsockopt for SO_KEEPALIVE failed with error: %u\n", WSAGetLastError());
    } else
        wprintf(L"SO_KEEPALIVE Value: %ld\n", iOptVal);

    iResult = setsockopt(ListenSocket, SOL_SOCKET, SO_KEEPALIVE, (char *) &bOptVal, bOptLen);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"setsockopt for SO_KEEPALIVE failed with error: %u\n", WSAGetLastError());
    } else
        wprintf(L"Set SO_KEEPALIVE: ON\n");

    iResult = getsockopt(ListenSocket, SOL_SOCKET, SO_KEEPALIVE, (char *) &iOptVal, &iOptLen);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"getsockopt for SO_KEEPALIVE failed with error: %u\n", WSAGetLastError());
    } else
        wprintf(L"SO_KEEPALIVE Value: %ld\n", iOptVal);

    
    listen(ListenSocket, 10);

    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    int client_socket_fd;
    addr_size = sizeof(their_addr);

    printf("waiting for connection\n");
    client_socket_fd = accept(ListenSocket, (struct sockaddr *)&their_addr, &addr_size);
    printf("accepted\n");

    uint8_t buffer[80+4]= "";//extra space for line ending and null terminator and one extra null terminator


    // recv(client_socket, (char*)buffer, sizeof(buffer)-1, 0);
    // printf("received: %s\n", buffer);
    // send(client_socket, "You send: \"", 11, 0);
    // send(client_socket, (const char*)buffer, strlen((const char*)buffer), 0);
    // send(client_socket, "\"", 1, 0);

    fd_set socket_fd_set;
    fd_set new_socket_fd_set; // is modified by select
    int socket_fd_max;

    FD_ZERO(&socket_fd_set); // clear set

    FD_SET(ListenSocket, &socket_fd_set);
    FD_SET(client_socket_fd, &socket_fd_set);
    socket_fd_max = client_socket_fd;
    
    new_socket_fd_set = socket_fd_set;
    int res = select(socket_fd_max+1, &new_socket_fd_set, NULL, NULL, NULL); // +1 to convert number to count
    printf("select res=%d\n", res);

    //loop through all potential sockets that have events
    for(int i = 0; i <= socket_fd_max; i++)
    {
        if (FD_ISSET(i, &new_socket_fd_set)) //new event 
        { 
            if (i == ListenSocket) //server event
            {
                printf("server event\n");

                // int newfd = accept(ListenSocket, (struct sockaddr *)&remoteaddr, &addrlen);

                // if (newfd == -1)
                // {
                //     perror("accept");
                // } 
                // else {
                //     FD_SET(newfd, &socket_fd_set); // add to master set
                //     // if (newfd > fdmax) {    // keep track of the max
                //     //     fdmax = newfd;
                //     // }

                //     int aap = inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),  remoteIP, INET6_ADDRSTRLEN),

                //     printf("selectserver: new connection from %s on "
                //         "socket %d\n",
                //         inet_ntop(remoteaddr.ss_family,
                //             get_in_addr((struct sockaddr*)&remoteaddr),
                //             remoteIP, INET6_ADDRSTRLEN),
                //         newfd);
                // }
                
            }
            else // client event
            {
                //memset(buffer,0,sizeof(buffer));
                int peek = recv(i, (char*)buffer, sizeof(buffer)-1, MSG_PEEK);
                memset(buffer,0,sizeof(buffer));

                if (peek == 0)
                {
                    printf("socket %d closed\n", i);
                }
                else
                {
                    printf("bytes available=%d\n", peek);
                    //memset(buffer,0,sizeof(buffer));
                    int bytes_received = recv(i, (char*)buffer, sizeof(buffer)-1, 0);
                    printf("received %s\n", buffer);
                }
                
            }
        }
    }

    closesocket(ListenSocket);
    WSACleanup();
    return 0;
}

