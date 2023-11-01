//#include "simple_cpp_sockets.h"
#include "simple_cpp_sockets2.h"

#include <thread> 
#include <string>


#include <cstdio>
#include <cstdint>

#define RED   "\033[0;31m"
#define GREEN "\033[0;32m"
#define NC    "\033[0m"

#include <windows.h>





struct Server_params
{
    std::string server_ip;
    uint16_t server_port;
    bool valid;
};

static void server_thread(Server_params* params)
{
    Server_socket server;
    server.init(params->server_ip, params->server_port);
    //printf("initializing server...\n");
    //server.init("192.168.1.45", 60000);

    int state = 0;
    SOCKET client_socket = INVALID_SOCKET;

    for(int i=0;i<3;++i)
    {
        auto events = server.wait_for_events();
        for(const auto& event: events)
        {
            printf("state: %d, event: %s\n", state, event.to_string().c_str());
            switch(state)
            {
                case 0:
                    if(event.event_code == Server_socket::Event_code::client_connected)
                    {
                        state = 1;
                    }
                    else
                    {
                        state = 4;
                    }
                break;
                case 1:
                    if(event.event_code == Server_socket::Event_code::rx)
                    {
                        state = 4;
                        if(event.bytes_available==1)
                        {
                            char buffer[1+1] = "";
                            recv(event.client,buffer, sizeof(buffer)-1,0);
                            if(buffer[0]=='1')
                            {
                                state = 2;
                                send(event.client, "2", 1, 0); // response
                            }
                        }
                    }
                    else
                    {
                        state = 4;
                    }
                break;
                case 2:
                    if(event.event_code == Server_socket::Event_code::client_disconnected)
                    {
                        state = 3;
                    }
                break;
            }                     
        }
    }

    params->valid = state == 3;

    // Server_socket server;
    // server.init("192.168.1.45", 60000);

    // for(int i=0;i<4;++i)
    // {
    //     auto events = server.wait_for_events();
    //     for(const auto& event: events)
    //     {
    //         printf("event: %s\n", event.to_string().c_str());
    //         if (event.event_code == Server_socket::Event_code::rx)
    //         {
    //             char buffer[100+1] = "";
    //             recv(event.client,buffer, sizeof(buffer)-1,0);
    //             send(event.client, "Hi\n", 3, 0); // response
    //             printf("received (%d): %s\n", event.bytes_available, buffer);
    //             params->valid = event.bytes_available==1 && buffer[0]=='1';
    //         }
    //     }
    // }

    // // send to all clients:
    // auto connected_clients = server.get_client_list();
    // for(const auto& client: connected_clients)
    // {
    //     send(client, "bye\n", 4, 0); 
    // }

    // printf("valid: (%d)\n", params->valid);

}

struct Client_params
{
    std::string server_ip;
    uint16_t server_port;

    int client_id;
    bool valid;

    //semaphore
};

static void client_thread(Client_params* params)
{

    // sockaddr_in server_addr;

    // //set ip address:
    // inet_pton(AF_INET, params.server_ip.c_str(), &server_addr.sin_addr);
    // //set port:
    // server_addr.sin_port = htons(params.server_port);
    // // set protocol
    // server_addr.sin_family = AF_INET;    

    // SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);

    // connect(client_socket, res->ai_addr, res->ai_addrlen);



    // closesocket(client_socket)

    int sockfd, numbytes;  
    char buf[10]="";
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    Sleep(1000);//wait for server to connect
    printf("client thread started\n");


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(params->server_ip.c_str(), "60000", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            closesocket(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    send(sockfd, "1", 1, 0);
    Sleep(1000);//give server chance to respond

    if ((numbytes = recv(sockfd, buf, sizeof(buf)-1, 0)) == -1) {
        perror("recv");
        return;
    }

    buf[numbytes] = '\0';

    if ((numbytes == 1) && (buf[0] == '2'))
    {
        params->valid = true;
        printf("client: received '%s'\n",buf);        
    }
    else
    {
        printf("client: received '%s'\n",buf);  
        closesocket(sockfd);
        return;
    }   
    

    closesocket(sockfd);

    //params->valid = true;
  
}

int test1()
{

    Server_params server_params={"192.168.1.45", 60000, false};
    Client_params client_params={"192.168.1.45", 60000, 1, false};

    std::thread server_thread(server_thread, &server_params); 
    std::thread client_thread(client_thread, &client_params); 

    server_thread.join();
    client_thread.join();

    printf("server_params.valid: (%d)\n", server_params.valid);

    if (server_params.valid && client_params.valid)
    {
        return 0;
    }
    else
    {
        return -1;
    }



    return -1;
}

int main() {
    Simple_socket_instance simple;

    // create semaphore
    // start server thread
    // start client thread 1
    // start client thread 2
    // trigger both client threads waiting on semaphore using release(2)
    // clients should connect and send and receive some data
    // client 1 join
    // client 2 join
    // close server
    // server join
    // verify results (passed by pointer/reference)

    // Server_socket server;
    // server.init("192.168.1.45", 60000);

    // for(int i=0;i<4;++i)
    // {
    //     auto events = server.wait_for_events();
    //     for(const auto& event: events)
    //     {
    //         printf("event: %s\n", event.to_string().c_str());
    //         if (event.event_code == Server_socket::Event_code::rx)
    //         {
    //             char buffer[100+1] = "";
    //             recv(event.client,buffer, sizeof(buffer)-1,0);
    //             send(event.client, "Hi\n", 3, 0); // response
    //             printf("received: %s\n", buffer);
                
    //         }
    //     }
    // }

    // // send to all clients:
    // auto connected_clients = server.get_client_list();
    // for(const auto& client: connected_clients)
    // {
    //     send(client, "bye\n", 4, 0); 
    // }

    int res = test1();

    if (res != 0)
    {
        fprintf(stderr, RED "[ERROR]" NC ": test1() failed!\n");
        return res;// Error: Process completed with exit code 255.
    }
    else
    {
        printf(GREEN "[SUCCESS]" NC ": test1() succeeded!\n");
    }

    

    return 0; 
}
