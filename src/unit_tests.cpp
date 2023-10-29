//#include "simple_cpp_sockets.h"

//#include <thread> 


#include <cstdio>
#include <cstdint>

#define RED "\033[0;31m"
#define NC "\033[0m"


static void server_thread(int params)
{

}

static void client_thread(int params)
{
}

int main() {

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
    //verify results (passed by pointer)


    fprintf(stderr, RED "[ERROR]" NC ": test1() failed!\n");
    return -1; // Error: Process completed with exit code 255.
}
