#include "simple_cpp_sockets.h"

#include <chrono>
using namespace std::chrono_literals;
#include <string>
#include <semaphore>
#include <thread> 


#include <cstdint>
#include <cstdio>
#include <cstring>

#define RED   "\033[0;31m"
#define GREEN "\033[0;32m"
#define NC    "\033[0m"


struct Server_params
{
    std::string server_ip;
    uint16_t server_port;
    bool valid;
    std::binary_semaphore* init_completed_sem;
};

static void server_thread_function(Server_params* params)
{
    Server_socket server;
    server.init(params->server_ip, params->server_port);

    params->init_completed_sem->release();

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
                                //std::this_thread::sleep_for(1000ms);//wait for tx to complete
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

    std::binary_semaphore* init_completed_sem;
    //semaphore
};

static void client_thread_function(Client_params* params)
{
    Simple_socket client_socket(false);

    bool success = client_socket.init(params->server_ip.c_str(),params->server_port);

    if (success)
    {
        client_socket.send("1",1);

        //give server chance to respond
        // using namespace std::chrono_literals;
        // std::this_thread::sleep_for(1000ms);
        params->init_completed_sem->acquire();
        
        int numbytes;  
        char buf[10]="";

        numbytes = client_socket.recv(buf, sizeof(buf)-1);
        if (numbytes == -1) 
        {
            perror("recv");
            return;
        }
        else
        {
            buf[numbytes] = '\0';
        }
       
        if ((numbytes == 1) && (buf[0] == '2'))
        {
            params->valid = true;     
        }
 
        printf("client: received '%s'\n",buf);       
    }
}


static void foo(Server_params* params)
{
    printf("Hello %s!\n",params->server_ip.c_str());
}

int test1()
{
    std::binary_semaphore init_completed_sem{0};

    // Server_params server_params={"0.0.0.0", 60000, false};
    // Server_params server_params={"", 60000, false};
    Server_params server_params={"127.0.0.1", 60000, false, &init_completed_sem};

    // Client_params client_params={"", 60000, 1, false};
    Client_params client_params={"127.0.0.1", 60000, 1, false, &init_completed_sem};


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


int main() {
    Simple_socket_library simple;

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

    return 0; 
}
