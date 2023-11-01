//#include "simple_cpp_sockets.h"
#include "simple_cpp_sockets2.h"

#include <thread> 
#include <string>

#include <cstdio>
#include <cstring>
#include <cstdint>

#define RED   "\033[0;31m"
#define GREEN "\033[0;32m"
#define NC    "\033[0m"

//#include <windows.h>
#include <chrono>
using namespace std::chrono_literals;



struct Server_params
{
    std::string server_ip;
    uint16_t server_port;
    bool valid;
};

static void server_thread_function(Server_params* params)
{
    Server_socket server;
    server.init(params->server_ip, params->server_port);

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
                                std::this_thread::sleep_for(1000ms);//wait for tx to complete
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
    // server.init("", 60000);

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

static void client_thread_function(Client_params* params)
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
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct addrinfo *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    //wait for server to connect
    //Sleep(1000);

    std::this_thread::sleep_for(1000ms);

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
            #ifdef WIN32
                closesocket(sockfd);
            #else
                close(sockfd);
            #endif
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
    //give server chance to respond
    //Sleep(1000);
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1000ms);

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
        #ifdef WIN32
            closesocket(sockfd);
        #else
            close(sockfd);
        #endif
        return;
    }   
    

    #ifdef WIN32
        closesocket(sockfd);
    #else
        close(sockfd);
    #endif

    //params->valid = true;
  
}


static void foo(Server_params* params)
{
    printf("Hello %s!\n",params->server_ip.c_str());
}

int test1()
{
    // Server_params server_params={"0.0.0.0", 60000, false};
    // Server_params server_params={"", 60000, false};
    Server_params server_params={"127.0.0.1", 60000, false};

    // Client_params client_params={"", 60000, 1, false};
    Client_params client_params={"127.0.0.1", 60000, 1, false};


    std::thread server_thread(server_thread_function, &server_params); 
    std::thread client_thread(client_thread_function, &client_params); 

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

// char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
// {
//     switch(sa->sa_family) {
//         case AF_INET:
//             inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
//                     s, maxlen);
//             break;

//         case AF_INET6:
//             inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
//                     s, maxlen);
//             break;

//         default:
//             strncpy(s, "Unknown AF", maxlen);
//             return NULL;
//     }

//     return s;
// }

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


    // struct addrinfo hints, *res;
    // // first, load up address structs with getaddrinfo():
    // memset(&hints, 0, sizeof hints);
    // hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    // hints.ai_socktype = SOCK_STREAM;
    // hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    // uint16_t port = 60000;
    // getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &res);

    // for(struct addrinfo* r=res; r!=nullptr; r=r->ai_next)
    // {
    //     struct sockaddr ai_addr = *r->ai_addr;
    //     size_t ai_addrlen = r->ai_addrlen;

    //     char ip_string[100]="";
    //     get_ip_str(&ai_addr, ip_string, ai_addrlen);
    //     printf("my ip: %s\n", ip_string);
    // }

    return 0; 
}
