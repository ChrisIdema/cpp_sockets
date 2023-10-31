//#include "simple_cpp_sockets.h"
#include "simple_cpp_sockets2.h"

#include <thread> 
#include <string>


#include <cstdio>
#include <cstdint>

#define RED "\033[0;31m"
#define NC "\033[0m"



// struct Server_params
// {
//     std::string server_ip;
//     uint16_t server_port;
// };

// static void server_thread(Server_params& params)
// {
//     // Server_socket socket;

//     // socket.init(Server_socket::SOCK_STREAM, "192.168.1.45", 60000);

// }

// struct Client_params
// {
//     std::string server_ip;
//     uint16_t server_port;

//     int client_id;

//     //semaphore
// };

// static void client_thread(Client_params& params)
// {

// }

int main() {
    Simple_socket_instance simple;

    //create semaphore
    //start server thread
    //start client thread 1
    //start client thread 2
    //trigger both client threads waiting on semaphore using release(2)
    //clients should connect and send and receive some data
    //client 1 join
    //client 2 join
    //close server
    //server join
    //verify results (passed by pointer/reference)

    Server_socket server;
    server.init("192.168.1.45", 60000);

    for(int i=0;i<4;++i)
    {
        auto events = server.wait_for_events();
        for(const auto& event: events)
        {
            printf("event: %s\n", event.to_string().c_str());
            if (event.event_code == Server_socket::Event_code::rx)
            {
                char buffer[100+1] = "";
                recv(event.client,buffer, sizeof(buffer)-1,0);
                send(event.client, "Hi\n", 3, 0); // response
                printf("received: %s\n", buffer);
                
            }
        }
    }

    // send to all clients:
    auto connected_clients = server.get_client_list();
    for(const auto& client: connected_clients)
    {
        send(client, "bye\n", 4, 0); 
    }



    fprintf(stderr, RED "[ERROR]" NC ": test1() failed!\n");
    return -1; // Error: Process completed with exit code 255.
}
