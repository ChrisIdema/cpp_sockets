#include "simple_cpp_sockets.h"

#include <chrono>
using namespace std::chrono_literals;
#include <string>
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
    
    bool exit_test;

    bool valid;
};

Socket_waiter* server_p=nullptr;

static void server_thread_function(Server_params* params)
{
    std::this_thread::sleep_for(500ms);//delay so connecting client too early is tested

    Socket_waiter server(true);
    server.init(params->server_ip, params->server_port);

    if(params->exit_test)
    {
        server_p = &server;

        auto events = server.wait_for_events();
        for(const auto& event: events)
        {
            if(event.event_code == Socket_waiter::Event_code::exit)
            {
                params->valid = true;
            }
            else
            {                
                params->valid = false;
                break;
            }            
        }
    }
    else
    {
        int state = 0;

        for(int i=0;i<3;++i)
        {
            auto events = server.wait_for_events();
            for(const auto& event: events)
            {
                PRINT("state: %d, event: %s\n", state, event.to_string().c_str());  

                switch(state)
                {
                    case 0:
                        if(event.event_code == Socket_waiter::Event_code::client_connected)
                        {
                            state = 1;
                        }
                        else
                        {
                            state = 4;
                        }
                    break;
                    case 1:
                        if(event.event_code == Socket_waiter::Event_code::rx)
                        {
                            state = 4;
                            if(event.bytes_available==1)
                            {
                                char buffer[1+1] = "";
                                event.client.recv(buffer, sizeof(buffer)-1,0);
                                if(buffer[0]=='1')
                                {
                                    state = 2;
                                    event.client.send("2", 1, 0); // response
                                }
                            }
                        }
                        else
                        {
                            state = 4;
                        }
                    break;
                    case 2:
                        if(event.event_code == Socket_waiter::Event_code::client_disconnected)
                        {
                            state = 3;
                        }
                    break;
                }                     
            }
        }
        params->valid = state == 3;
    }
}

struct Client_params
{
    std::string server_ip;
    uint16_t server_port;

    int client_id;
    bool exit_test;

    bool valid;
};

static void client_thread_function(Client_params* params)
{
    if(params->exit_test)
    {
        std::this_thread::sleep_for(1000ms);
        if (server_p != nullptr)
        {
            PRINT("server_p->exit()\n");
            server_p->exit_message();
            params->valid = true;
        }
        else
        {
            PRINT("server pointer NOT assigned\n");
        }
    }
    else
    {
        Simple_socket client_socket(false);

	bool retry;
	do{
		bool success=false;
		retry=false;		

		while(!success)
		{
		    success = client_socket.init(params->server_ip.c_str(),params->server_port);
		    if(!success)
		    {
		    	PRINT("client_socket.init==false\n");
		        std::this_thread::sleep_for(100ms);
		    }
		}


		if (success)
		{
		    client_socket.send("1",1);
		    
		    int numbytes;  
		    char buf[10]="";

		    numbytes = client_socket.recv(buf, sizeof(buf)-1);
		    if (numbytes == -1) //error
		    {
		        perror("recv");
		        //return;
		        retry = true;
		        client_socket.close();	
		        std::this_thread::sleep_for(100ms);// not needed, but reduces number of init calls
		        continue;
		        	        
		    }
		    else if(numbytes == 0) // closed
		    {
    		    perror("recv");
		        //return;
		        retry = true;
		        client_socket.close();	
		        std::this_thread::sleep_for(100ms);// not needed, but reduces number of init calls
		        continue;
		    }
		    else
		    {
		        buf[numbytes] = '\0';
		        PRINT("client: received(%d) '%s'\n",numbytes, buf);  
		    }
		
		    if ((numbytes == 1) && (buf[0] == '2'))
		    {
		        params->valid = true;
		        break;     
		    }	    
		    
		}
        }while(retry);
        
    }


}


int test1()
{
       
    // Server_params server_params={"0.0.0.0", 60000, false};
    // Server_params server_params={"", 60000, false};
    Server_params server_params={"127.0.0.1", 60000, false, false};

    // Client_params client_params={"", 60000, 1, false};
    Client_params client_params={"127.0.0.1", 60000, 1, false, false};


    std::thread server_thread(server_thread_function, &server_params); 
    std::thread client_thread(client_thread_function, &client_params); 

    server_thread.join();
    client_thread.join();

    PRINT("server_params.valid: %d\n", server_params.valid);

    if (server_params.valid && client_params.valid)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int test2()
{       
    Server_params server_params={"127.0.0.1", 60000, true, false};
    Client_params client_params={"127.0.0.1", 60000, 1, true, false};

    std::thread server_thread(server_thread_function, &server_params); 
    std::thread client_thread(client_thread_function, &client_params); 
    

    server_thread.join();
    client_thread.join();

    PRINT("server_params.valid: %d\n", server_params.valid);

    if (server_params.valid && client_params.valid)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int test3()
{
    Socket_waiter server(true);
    server.init("127.0.0.1", 60000);

    const std::vector<const char*> to_server={"Hello world!", "Hello again!", "Bye!"};
    std::vector<const char*> received_by_server;

    std::thread server_thread([](Socket_waiter* p_server, std::vector<const char*>* p_received_by_server){
        bool running = true;

        while(running)
        {
            auto events = p_server->wait_for_events();
            PRINT("wait_for_events returned:\n");  
            for(const auto& event: events)
            {
                PRINT("event: %s\n", event.to_string().c_str());  
                if(event.event_code == Socket_waiter::Event_code::exit)
                {
                    running = false;
                    break;
                }
                if(event.event_code == Socket_waiter::Event_code::interrupt)            
                {     
                    auto message = reinterpret_cast<const char*>(event.message);                    
                    PRINT("message: \"%s\"\n", message);        
                    p_received_by_server->push_back(message);        
                }            
            }
        }
    }, &server, &received_by_server);

    //std::this_thread::sleep_for(100ms);
    server.custom_message(to_server[0]);
    server.custom_message(to_server[1]);
    server.custom_message(to_server[2]);
    std::this_thread::sleep_for(100ms);
    server.exit_message();
    server_thread.join();


    PRINT("to_server==received_by_server: %d\n", to_server==received_by_server);   

    if (to_server==received_by_server)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}


int main() 
{
    Simple_socket_library simple;

    int res;
    
    res = test1();

    if (res != 0)
    {
        fprintf(stderr, RED "[ERROR]" NC ": test1() failed!\n");
        return res;// Error: Process completed with exit code 255.
    }
    else
    {
        printf(GREEN "[SUCCESS]" NC ": test1() succeeded!\n");
    }
    

    res = test2();

    if (res != 0)
    {
        fprintf(stderr, RED "[ERROR]" NC ": test2() failed!\n");
        return res;// Error: Process completed with exit code 255.
    }
    else
    {
        printf(GREEN "[SUCCESS]" NC ": test2() succeeded!\n");
    }

    res = test3();

    if (res != 0)
    {
        fprintf(stderr, RED "[ERROR]" NC ": test3() failed!\n");
        return res;// Error: Process completed with exit code 255.
    }
    else
    {
        printf(GREEN "[SUCCESS]" NC ": test3() succeeded!\n");
    }



    return 0; 
}